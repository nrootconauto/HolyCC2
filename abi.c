#include <IR2asm.h>
#include <IRLiveness.h>
#include <abi.h>
#include <asmEmitter.h>
#include <assert.h>
#include <cleanup.h>
#define DEBUG_PRINT_ENABLE 1
void *IR_ATTR_ABI_INFO = "ABI_INFO";
static void *IR_ATTR_FUNC = "FUNC";
static __thread char calledInsertArgs = 0;
static __thread char computedABIInfo = 0;
void IRAttrABIInfoDestroy(struct IRAttr *a) {
	struct IRAttrABIInfo *abi = (void *)a;
	strRegPDestroy(&abi->toPushPop);
	strRegPDestroy(&abi->liveIn);
	strRegPDestroy(&abi->liveOut);
}
static void *IR_ATTR_OLD_REG_SLICE = "OLD_REG_SLICE";
static void strX86AddrModeDestroy2(strX86AddrMode *str) {
	for (long i = 0; i != strX86AddrModeSize(*str); i++)
		X86AddrModeDestroy(&str[0][i]);
	strX86AddrModeDestroy(str);
}
struct IRATTRoldRegSlice {
	struct IRAttr base;
	graphNodeIR old;
};
struct IRAttrFunc {
	struct IRAttr base;
	struct parserFunction *func;
};
typedef int (*gnCmpType)(const graphNodeMapping *, const graphNodeMapping *);
static int ptrPtrCmp(const void *a, const void *b) {
	if (*(void **)a > *(void **)b)
		return 1;
	else if (*(void **)a < *(void **)b)
		return -1;
	return 0;
}
typedef int (*regCmpType)(const struct reg **, const struct reg **);
typedef int (*varCmpType)(const struct parserVar **, const struct parserVar **);
static int containsRegister(struct reg *par, struct reg *r) {
		if(par==r)
				return 1;
	for (long a = 0; a != strRegSliceSize(par->affects); a++) {
		if (par->affects[a].reg == r)
			return 1;
	}
	return 0;
}
static struct reg *__registerContainingBoth(struct reg *master, struct reg *a, struct reg *b) {
	assert(a->masterReg == b->masterReg);
	for (long c = 0; c != strRegSliceSize(master->affects); c++) {
		if (containsRegister(master->affects[c].reg, a) && containsRegister(master->affects[c].reg, b))
			return __registerContainingBoth(master->affects[c].reg, a, b);
	}
	return master;
}
/**
 * Finds (smallest) register containing both registers
 */
static struct reg *smallestRegContainingBoth(struct reg *a, struct reg *b) {
	return __registerContainingBoth(a->masterReg, a, b);
}
static int killEdgeIfEq(void *a, void *b) {
		return *(enum IRConnType*)a == *(enum IRConnType*)b;
}
static void swapNode(graphNodeIR old, graphNodeIR with) {
	strGraphEdgeIRP in CLEANUP(strGraphEdgeIRPDestroy) = graphNodeIRIncoming(old);
	strGraphEdgeIRP out CLEANUP(strGraphEdgeIRPDestroy) = graphNodeIROutgoing(old);
	for (long i = 0; i != strGraphEdgeIRPSize(in); i++) {
		__auto_type from = graphEdgeIRIncoming(in[i]);
		graphNodeIRConnect(from, with, *graphEdgeIRValuePtr(in[i]));
		__auto_type val=*graphEdgeIRValuePtr(in[i]);
		graphEdgeIRKill(from, old, &val, killEdgeIfEq, NULL);
	}
	for (long o = 0; o != strGraphEdgeIRPSize(out); o++) {
		__auto_type to = graphEdgeIROutgoing(out[o]);
		graphNodeIRConnect(with, to, *graphEdgeIRValuePtr(out[o]));
		__auto_type val=*graphEdgeIRValuePtr(out[o]);
		graphEdgeIRKill(old, to, &val, killEdgeIfEq, NULL);
	}
}
static strRegP usedRegisters(strGraphNodeIRP nodes) {
	strRegP retVal = NULL;
	for (long i = 0; i != strGraphNodeIRPSize(nodes); i++) {
		struct IRNodeValue *value = (void *)graphNodeIRValuePtr(nodes[i]);
		if (value->base.type == IR_VALUE)
			if (value->val.type == IR_VAL_REG) {
				__auto_type reg = value->val.value.reg.reg;
				if (!strRegPSortedFind(retVal, reg, (regCmpType)ptrPtrCmp))
					retVal = strRegPSortedInsert(retVal, reg, (regCmpType)ptrPtrCmp);
			}
	}
	return retVal;
}
STR_TYPE_DEF(struct parserVar *, Variable);
STR_TYPE_FUNCS(struct parserVar *, Variable);
static int isSelectVariable(graphNodeIR node, const void *data) {
	const strVariable *select = data;
	struct IRNodeValue *val = (void *)graphNodeIRValuePtr(node);
	if (val->base.type == IR_VALUE)
		if (val->val.type == IR_VAL_VAR_REF)
			return NULL != strVariableSortedFind(*select, val->val.value.var.var, (varCmpType)ptrPtrCmp);

	return 0;
}
static char *strDup(const char *txt) {
	return strcpy(malloc(strlen(txt) + 1), txt);
}
#define ARRAY_SIZE(array) (sizeof(array) / sizeof(*array))
PTR_MAP_FUNCS(struct parserVar *, struct reg *, Var2Reg);
static strRegP mergeConflictingRegs(strRegP conflicts) {
	//
	// If 2 items in conflicts have a common master register "merge" the registers and replace with the common register
	// 2 birds 1 stone.
	//
mergeLoop:
	for (long c1 = 0; c1 != strRegPSize(conflicts); c1++) {
		for (long c2 = 0; c2 != strRegPSize(conflicts); c2++) {
			if (conflicts[c1] == conflicts[c2])
				continue;
			if (conflicts[c1]->masterReg != conflicts[c2]->masterReg)
				continue;
			__auto_type common = smallestRegContainingBoth(conflicts[c1], conflicts[c2]);
			// Do a set differnece,then insert the common register
			strRegP dummy CLEANUP(strRegPDestroy) = NULL;
			dummy = strRegPSortedInsert(dummy, conflicts[c1], (regCmpType)ptrPtrCmp);
			dummy = strRegPSortedInsert(dummy, conflicts[c2], (regCmpType)ptrPtrCmp);
			conflicts = strRegPSetDifference(conflicts, dummy, (regCmpType)ptrPtrCmp);
			conflicts = strRegPSortedInsert(conflicts, common, (regCmpType)ptrPtrCmp);
			goto mergeLoop;
		}
	}

	return conflicts;
}
PTR_MAP_FUNCS(struct reg *, struct parserVar *, Reg2Var);
void findRegisterLiveness(graphNodeIR start) {
	strGraphNodeIRP allNodes CLEANUP(strGraphNodeIRPDestroy) = graphNodeIRAllNodes(start);
	// Replace Registers with variables and do liveness analysis on the variables
	strRegP usedRegs CLEANUP(strRegPDestroy) = usedRegisters(allNodes);
	strVariable regVars CLEANUP(strVariableDestroy) = NULL;
	ptrMapVar2Reg var2Reg = ptrMapVar2RegCreate();
	ptrMapReg2Var reg2Var = ptrMapReg2VarCreate();
	for (long i = 0; i != strRegPSize(usedRegs); i++) {
		__auto_type newVar = IRCreateVirtVar(&typeI64i);
		newVar->name = strDup(usedRegs[i]->name);
		regVars = strVariableSortedInsert(regVars, newVar, (varCmpType)ptrPtrCmp);
		ptrMapVar2RegAdd(var2Reg, newVar, usedRegs[i]);
		ptrMapReg2VarAdd(reg2Var, usedRegs[i], newVar);
	}

	strGraphNodeIRP replacedNodes CLEANUP(strGraphNodeIRPDestroy) = NULL;
	strGraphNodeIRP addedNodes CLEANUP(strGraphNodeIRPDestroy) = NULL;
	for (long i = 0; i != strGraphNodeIRPSize(allNodes); i++) {
		struct IRNodeValue *value = (void *)graphNodeIRValuePtr(allNodes[i]);
		if (value->base.type != IR_VALUE)
			continue;
		if (value->val.type != IR_VAL_REG)
			continue;
		__auto_type reg = value->val.value.reg.reg;
		__auto_type newNode = IRCreateVarRef(*ptrMapReg2VarGet(reg2Var, reg));

		struct IRATTRoldRegSlice newAttr;
		newAttr.base.name = IR_ATTR_OLD_REG_SLICE;
		newAttr.base.destroy = NULL;
		newAttr.old = allNodes[i];
		IRAttrReplace(newNode, __llCreate(&newAttr, sizeof(newAttr)));
		swapNode(allNodes[i], newNode);
		replacedNodes = strGraphNodeIRPSortedInsert(replacedNodes, allNodes[i], (gnCmpType)ptrPtrCmp);
		addedNodes = strGraphNodeIRPSortedInsert(addedNodes, newNode, (gnCmpType)ptrPtrCmp);
	}
	allNodes = strGraphNodeIRPSetDifference(allNodes, replacedNodes, (gnCmpType)ptrPtrCmp);
	allNodes = strGraphNodeIRPSetUnion(allNodes, addedNodes, (gnCmpType)ptrPtrCmp);
	//
	// This has a side effect of attributing basic block attributes to nodes,which tell the in/out variables for each node in an expression
	//
	strGraphNodeIRLiveP livenessGraphs CLEANUP(strGraphNodeIRLivePDestroy) = IRInterferenceGraphFilter(start, &regVars, isSelectVariable);
	for (long n = 0; n != strGraphNodeIRPSize(allNodes); n++) {
		struct IRNodeFuncCall *call = (void *)graphNodeIRValuePtr(allNodes[n]);
		__auto_type attr = llIRAttrFind(call->base.attrs, IR_ATTR_BASIC_BLOCK, IRAttrGetPred);
		if(!attr)
				continue;
		
		assert(attr);
		struct IRAttrBasicBlock *block = (void *)llIRAttrValuePtr(attr);

		strVar inVars CLEANUP(strVarDestroy) = NULL;
		strVar outVars CLEANUP(strVarDestroy) = NULL;
		for (long i = 0; i != strVarSize(block->block->in); i++)
			inVars = strVarSortedInsert(inVars, block->block->in[i], IRVarCmp);
		for (long o = 0; o != strVarSize(block->block->out); o++)
			outVars = strVarSortedInsert(outVars, block->block->out[o], IRVarCmp);
		// Find insrection in/out registers that conflict
		strRegP conflicts = NULL;
		for (long i = 0; i != strVarSize(inVars); i++) {
			__auto_type iFind = *ptrMapVar2RegGet(var2Reg, inVars[i].var);
			for (long o = 0; o != strVarSize(outVars); o++) {
				__auto_type oFind = *ptrMapVar2RegGet(var2Reg, outVars[o].var);
				if (regConflict(iFind, oFind)) {
					__auto_type common = smallestRegContainingBoth(iFind, oFind);
					conflicts = strRegPSortedInsert(conflicts, common, (regCmpType)ptrPtrCmp);
				}
			}
		}

		strRegP inRegs =NULL;
		for (long i = 0; i != strVarSize(inVars); i++) {
				__auto_type iFind = *ptrMapVar2RegGet(var2Reg, inVars[i].var);
				inRegs=strRegPSortedInsert(inRegs, iFind, (regCmpType)ptrPtrCmp);
		}
		inRegs=mergeConflictingRegs(inRegs);

		strRegP outRegs =NULL;
		for (long i = 0; i != strVarSize(outVars); i++) {
				__auto_type oFind = *ptrMapVar2RegGet(var2Reg, outVars[i].var);
				outRegs=strRegPSortedInsert(outRegs, oFind, (regCmpType)ptrPtrCmp);
		}
		outRegs=mergeConflictingRegs(outRegs);
		
		conflicts = mergeConflictingRegs(conflicts);

		struct IRAttrABIInfo info;
		info.base.name = IR_ATTR_ABI_INFO;
		info.base.destroy = IRAttrABIInfoDestroy;
		info.toPushPop = conflicts;
		info.liveIn=inRegs;
		info.liveOut=outRegs;
		
		llIRAttr infoAttr = __llCreate(&info, sizeof(info));
		call->base.attrs = llIRAttrInsert(call->base.attrs, infoAttr, IRAttrInsertPred);
	}

	// Repalce temp variables with original registers
	for (long n = 0; n != strGraphNodeIRPSize(allNodes); n++) {
		struct IRNodeValue *node = (void *)graphNodeIRValuePtr(allNodes[n]);
		if (node->base.type != IR_VALUE)
			continue;
		if (node->val.type != IR_VAL_VAR_REF)
			continue;
		__auto_type find = llIRAttrFind(node->base.attrs, IR_ATTR_OLD_REG_SLICE, IRAttrGetPred);
		if (!find)
			continue;
		struct IRATTRoldRegSlice *attr = (void *)llIRAttrValuePtr(find);
		swapNode(allNodes[n], attr->old);
		graphNodeIRKill(&allNodes[n], (void (*)(void *))IRNodeDestroy, NULL);
	}

	ptrMapReg2VarDestroy(reg2Var, NULL);
	ptrMapVar2RegDestroy(var2Reg, NULL);
}
static strGraphNodeIRP getFuncArgs(graphNodeIR call) {
	strGraphEdgeIRP in CLEANUP(strGraphEdgeIRPDestroy) = IREdgesByPrec(call);
	strGraphNodeIRP args = NULL;
	for (long i = 0; i != strGraphEdgeIRPSize(in); i++) {
		switch (*graphEdgeIRValuePtr(in[i])) {
		default:
			break;
		case IR_CONN_FUNC_ARG_1 ... IR_CONN_FUNC_ARG_128:
			args = strGraphNodeIRPAppendItem(args, graphEdgeIRIncoming(in[i]));
		}
	}
	return args;
}
void IRComputeABIInfo(graphNodeIR start) {
	findRegisterLiveness(start);
	computedABIInfo = 1;
}
static void assembleInst(const char *name, strX86AddrMode args) {
	strOpcodeTemplate ops CLEANUP(strOpcodeTemplateDestroy) = X86OpcodesByArgs(name, args, NULL);
	assert(strOpcodeTemplateSize(ops));
	int err;
	X86EmitAsmInst(ops[0], args, &err);
	assert(!err);
}
static void pushReg(struct reg *r) {
		strX86AddrMode ppIndexArgs CLEANUP(strX86AddrModeDestroy2) = strX86AddrModeAppendItem(NULL, X86AddrModeReg(r));
		assembleInst("PUSH", ppIndexArgs);
}
static void popReg(struct reg *r) {
		strX86AddrMode ppIndexArgs CLEANUP(strX86AddrModeDestroy2) = strX86AddrModeAppendItem(NULL, X86AddrModeReg(r));
		assembleInst("POP", ppIndexArgs);
}
static struct X86AddressingMode *nodePtr(graphNodeIR node) {
	struct IRNodeValue *val = (void *)graphNodeIRValuePtr(node);
	switch (val->val.type) {
	case __IR_VAL_MEM_FRAME: {
		return X86AddrModeIndirSIB(0, NULL, X86AddrModeReg(basePointer()), X86AddrModeSint(-val->val.value.__frame.offset), val->val.value.__frame.type);
	}
	case IR_VAL_VAR_REF: {
		if (!val->val.value.var.var->isGlobal) {
			fprintf(stderr, "Convert local variables to frame addresses\n", NULL);
			abort();
		}
		X86AddrModeIndirLabel(val->val.value.var.var->name, objectPtrCreate(val->val.value.var.var->type));
	}
	default:
		assert(0);
	}
	return NULL;
}
static int conflictsWithRegisters(const void *_regs, const struct reg **r) {
	strRegP regs = (void *)_regs;
	for (long c = 0; c != strRegPSize(regs); c++)
		if (regConflict((struct reg *)*r, regs[c]))
			return 1;
	return 0;
}
//
// http://www.sco.com/developers/devspecs/abi386-4.pdf
//
static strVar IR_ABI_I386_SYS_InsertLoadArgs(graphNodeIR start);
static void IR_ABI_I386_SYSV_2Asm(graphNodeIR start) {
	if (!computedABIInfo) {
		fprintf(stderr, "CALL computedABIInfo before calling me!!!\n");
		assert(computedABIInfo); // Call IRComputeABIInfo!!!
	}
	struct IRNodeFuncCall *call = (void *)graphNodeIRValuePtr(start);
	llIRAttr llInfo = (void *)llIRAttrFind(call->base.attrs, IR_ATTR_ABI_INFO, IRAttrGetPred);
	assert(llInfo);
	struct IRAttrABIInfo *info = (void *)llIRAttrValuePtr(llInfo);

	strGraphEdgeIRP in CLEANUP(strGraphEdgeIRPDestroy) = graphNodeIRIncoming(start);
	strGraphEdgeIRP inFunc CLEANUP(strGraphEdgeIRPDestroy) = IRGetConnsOfType(in, IR_CONN_FUNC);
	struct object *funcType = IRNodeType(graphEdgeIRIncoming(inFunc[0]));
	if (funcType->type == TYPE_PTR)
		funcType = ((struct objectPtr *)funcType)->type;
	assert(funcType->type == TYPE_FUNCTION);
	struct objectFunction *func = (struct objectFunction *)funcType;
	strGraphNodeIRP args = getFuncArgs(start);
	assert(strGraphNodeIRPSize(args) == strFuncArgSize(func->args));

	strRegP clobbered CLEANUP(strRegPDestroy) = strRegPClone(info->toPushPop);
	// EAX is used as a scracth register here
	clobbered = strRegPSortedInsert(clobbered, &regX86EAX, (regCmpType)ptrPtrCmp);
	clobbered = mergeConflictingRegs(clobbered);

	strRegP preserved CLEANUP(strRegPDestroy) = NULL;
	preserved = strRegPSortedInsert(preserved, &regX86EBX, (regCmpType)ptrPtrCmp);
	preserved = strRegPSortedInsert(preserved, &regX86ESI, (regCmpType)ptrPtrCmp);
	preserved = strRegPSortedInsert(preserved, &regX86EDI, (regCmpType)ptrPtrCmp);
	preserved = strRegPSortedInsert(preserved, &regX86ESP, (regCmpType)ptrPtrCmp);
	preserved = strRegPSortedInsert(preserved, &regX86EBP, (regCmpType)ptrPtrCmp);
	clobbered = strRegPRemoveIf(clobbered, preserved, conflictsWithRegisters);

	long stackSize = 0;
	long eaxOffset = 0;

	// Passes a secret first arg if returns a struct/union(that isn't based on a primtive)
	// For example(I32 is treated as a I32i so dont return a union)
	__auto_type retType = objectBaseType(func->retType);
	int retsStruct = retType->type == TYPE_CLASS || retType->type == TYPE_UNION;
	int retsFloat = retType == &typeF64;

	// Is an int
	if (!retsStruct && !retsFloat && retType != &typeU0) {
		// Make a dummy area to store retVal when poping registers
		strX86AddrMode dpp CLEANUP(strX86AddrModeDestroy2) = strX86AddrModeAppendItem(NULL, X86AddrModeSint(0));
		assembleInst("PUSH", dpp);
		stackSize += 4;
	}

	// Push all the clobered
	for (long p = 0; p != strRegPSize(clobbered); p++) {
		if (clobbered[p] == &regX86EAX)
			eaxOffset = stackSize;

		strX86AddrMode r CLEANUP(strX86AddrModeDestroy2) = strX86AddrModeAppendItem(NULL, X86AddrModeReg(clobbered[p]));
		assembleInst("PUSH", r);
		stackSize += clobbered[p]->size;
	}

	strGraphEdgeIRP out CLEANUP(strGraphEdgeIRPDestroy) = graphNodeIROutgoing(start);
	strGraphEdgeIRP dst CLEANUP(strGraphEdgeIRPDestroy) = IRGetConnsOfType(out, IR_CONN_DEST);

	long stackSizeBeforeArgs = stackSize;
	for (long i = strGraphNodeIRPSize(args) - 1; i >= 0; i--) {
		__auto_type type = objectBaseType(IRNodeType(args[i]));
		long itemSize = objectSize(type, NULL);
		if (type->type == TYPE_CLASS || type->type == TYPE_UNION) {
			struct X86AddressingMode *stack CLEANUP(X86AddrModeDestroy) = X86AddrModeIndirReg(stackPointer(), type);
			struct X86AddressingMode *val CLEANUP(X86AddrModeDestroy) = IRNode2AddrMode(args[i]);
			asmAssign(stack, val, itemSize,0);

			// Must be aligned to 4 bytes
			long aligned = itemSize / 4 * 4 + ((itemSize % 4) ? 4 : 0);
			strX86AddrMode addSP CLEANUP(strX86AddrModeDestroy2) = NULL;
			addSP = strX86AddrModeAppendItem(addSP, X86AddrModeReg(stackPointer()));
			addSP = strX86AddrModeAppendItem(addSP, X86AddrModeSint(aligned));
			assembleInst("ADD", addSP);
		} else {
			strX86AddrMode pushArgs CLEANUP(strX86AddrModeDestroy2) = NULL;
			struct X86AddressingMode *mode CLEANUP(X86AddrModeDestroy) = IRNode2AddrMode(args[i]);
			//
			// EAX is trashed during the calling sequence,so retrieve EAX from the pushed registers if wants an EAX affecting register
			//
			int swapEaxWithStack = 0;
			if (mode->type == X86ADDRMODE_REG)
				if (regConflict(mode->value.reg, &regX86EAX)) {
					swapEaxWithStack = 1;
				}
			if (swapEaxWithStack) {
				strX86AddrMode xchgArgs CLEANUP(strX86AddrModeDestroy2) = NULL;
				xchgArgs = strX86AddrModeAppendItem(xchgArgs, X86AddrModeReg(&regX86EAX));
				xchgArgs =
				    strX86AddrModeAppendItem(xchgArgs, X86AddrModeIndirSIB(0, NULL, X86AddrModeReg(stackPointer()), X86AddrModeSint(eaxOffset - stackSize), &typeU32i));
				assembleInst("XCHG", xchgArgs);
			}

			if (itemSize != 4) {
				struct X86AddressingMode *eax CLEANUP(X86AddrModeDestroy) = X86AddrModeReg(&regX86EAX);
				eax->valueType=&typeI32i;
				asmTypecastAssign(eax, mode,0);
				pushArgs = strX86AddrModeAppendItem(pushArgs, X86AddrModeReg(&regX86EAX));
			} else {
				pushArgs = strX86AddrModeAppendItem(pushArgs, IRNode2AddrMode(args[i]));
			}

			if (swapEaxWithStack) {
				strX86AddrMode xchgArgs CLEANUP(strX86AddrModeDestroy2) = NULL;
				xchgArgs = strX86AddrModeAppendItem(xchgArgs, X86AddrModeReg(&regX86EAX));
				xchgArgs =
				    strX86AddrModeAppendItem(xchgArgs, X86AddrModeIndirSIB(0, NULL, X86AddrModeReg(stackPointer()), X86AddrModeSint(eaxOffset - stackSize), &typeU32i));
				assembleInst("XCHG", xchgArgs);
			}

			assembleInst("PUSH", pushArgs);
			stackSize += 4;
		}
	}

	//Structure pointer is last argument pushed
	if (retsStruct) {
		assert(strGraphEdgeIRPSize(dst) == 1);
		__auto_type outNode = graphEdgeIROutgoing(dst[0]);

		strX86AddrMode leaArgs CLEANUP(strX86AddrModeDestroy2) = NULL;
		leaArgs = strX86AddrModeAppendItem(leaArgs, X86AddrModeReg(&regX86EAX));
		leaArgs = strX86AddrModeAppendItem(leaArgs, nodePtr(outNode));
		leaArgs[1]->valueType=NULL;
		assembleInst("LEA", leaArgs);

		pushReg(&regX86EAX);
	}
	
	strX86AddrMode callArgs CLEANUP(strX86AddrModeDestroy2) = NULL;
	callArgs = strX86AddrModeAppendItem(callArgs, IRNode2AddrMode(graphEdgeIRIncoming(inFunc[0])));
	assembleInst("CALL", callArgs);

	if (retType != &typeU0 && strGraphEdgeIRPSize(dst)) {
		__auto_type outNode = graphEdgeIROutgoing(dst[0]);
		if (!retsStruct) {
			// We point to area we made on the stack for the return value eariler
			struct X86AddressingMode *outMode CLEANUP(X86AddrModeDestroy) =
			    X86AddrModeIndirSIB(0, NULL, X86AddrModeReg(stackPointer()), X86AddrModeSint(stackSize - 4), IRNodeType(outNode));
			if (objectBaseType(outMode->valueType) != &typeF64) {
				struct X86AddressingMode *eaxMode CLEANUP(X86AddrModeDestroy) = X86AddrModeReg(&regX86EAX);
				// Assign type of eaxMode
				eaxMode->valueType = retType;
				asmTypecastAssign(outMode, eaxMode,0);
				goto end;
			} else {
				struct X86AddressingMode *outMode CLEANUP(X86AddrModeDestroy) = IRNode2AddrMode(outNode);
				struct X86AddressingMode *st0Mode CLEANUP(X86AddrModeDestroy) = X86AddrModeReg(&regX86ST0);
				asmAssign(outMode, st0Mode, 8,0);
				goto end;
			}
		} else {				
				struct X86AddressingMode *outMode CLEANUP(X86AddrModeDestroy) = IRNode2AddrMode(outNode);
				struct X86AddressingMode *indirEaxMode CLEANUP(X86AddrModeDestroy) = X86AddrModeIndirReg(&regX86EAX, IRNodeType(outNode));
				asmAssign(outMode, indirEaxMode, objectSize(IRNodeType(outNode), NULL),0);
				goto end;
		}
		assert(0);
	}
end:;
	
	strX86AddrMode stackSubArgs CLEANUP(strX86AddrModeDestroy2) = NULL;
	stackSubArgs = strX86AddrModeAppendItem(stackSubArgs, X86AddrModeReg(stackPointer()));
	stackSubArgs = strX86AddrModeAppendItem(stackSubArgs, X86AddrModeSint(stackSize - stackSizeBeforeArgs));
	assembleInst("ADD", stackSubArgs);
	stackSize = stackSizeBeforeArgs;

	// Pop all the clobered
	for (long p = strRegPSize(clobbered) - 1; p >= 0; p--) {
		if (clobbered[p] == &regX86EAX)
			eaxOffset = stackSize;

		strX86AddrMode r CLEANUP(strX86AddrModeDestroy2) = strX86AddrModeAppendItem(NULL, X86AddrModeReg(clobbered[p]));
		assembleInst("POP", r);
		stackSize -= clobbered[p]->size;
	}
	
	// Now we are left with the spot we made eariler for return values,let's pop it(if not a structure,all int non-structs get stuffed in EAX)
	if (!retsStruct && !retsFloat && retType != &typeU0) {
		__auto_type outNode = graphEdgeIROutgoing(dst[0]);
		strX86AddrMode outArgs CLEANUP(strX86AddrModeDestroy2) = strX86AddrModeAppendItem(NULL, IRNode2AddrMode(outNode));
		assembleInst("POP", outArgs);
		stackSize -= 4;
	}
	assert(stackSize == 0);
}
static graphNodeIR abiI386AddrModeNode(struct objectFunction *func, long argI) {
	int returnsStruct = func->retType->type == TYPE_CLASS || func->retType->type == TYPE_UNION;
	long offset = returnsStruct ? -12 : -8;
	if (returnsStruct&&argI==0) {
		return IRCreateFrameAddress(-8, objectPtrCreate(func->retType));
	}
	for (long i = 0; i != strFuncArgSize(func->args); i++) {
		long size = objectSize(func->args[i].type, NULL);
		if (i == argI - (returnsStruct ? 1 : 0))
			return IRCreateFrameAddress(offset, func->args[i].type);
		offset -= size / 4 * 4 + ((size % 4) ? 4 : 0);
	}
	assert(argI < strFuncArgSize(func->args));
	return NULL;
}
static strVar IR_ABI_I386_SYS_InsertLoadArgs(graphNodeIR start) {
	calledInsertArgs = 0;

	strGraphNodeIRP nodes CLEANUP(strGraphNodeIRPDestroy) = graphNodeIRAllNodes(start);
	struct IRNodeFuncStart *funcStart = (void *)graphNodeIRValuePtr(start);
	struct objectFunction *fType = (void *)funcStart->func->type;

	// Returning a structure returns adds a hidden first argument pointing to the return location
	int returnsStruct = 0;
	if (fType->retType->type == TYPE_CLASS || fType->retType->type == TYPE_UNION)
		returnsStruct = 1;

	strVar args = strVarResize(NULL, strFuncArgSize(fType->args));

	//
	// This is a chain of assigning a function argument to it's variable
	// [arg1]->var1
	// [arg2]->var2
	//
	graphNodeIR defineChain = NULL, defineChainStart = NULL;
	strGraphNodeIRP argNodes CLEANUP(strGraphNodeIRPDestroy) = NULL;
	if (returnsStruct) {
		struct IRVar ir;
		ir.SSANum = 0;
		ir.addressedByPtr = 0;
		ir.var = IRCreateVirtVar(objectPtrCreate(fType->retType));
		args[0] = ir;

		__auto_type arg = abiI386AddrModeNode(fType, 0);
		__auto_type var = IRCreateVarRef(ir.var);
		graphNodeIRConnect(arg, var, IR_CONN_DEST);
		
		defineChainStart = arg;
		defineChain = var;
		argNodes = strGraphNodeIRPAppendItem(argNodes, arg);
	}

	for (long v = 0; v != strVarSize(args); v++) {
		struct IRVar ir;
		ir.SSANum = 0;
		ir.addressedByPtr = 0;
		ir.var = IRCreateVirtVar(fType->args[v].type);
		args[v] = ir;

		__auto_type arg = abiI386AddrModeNode(fType, v+((returnsStruct)?1:0));
		__auto_type var = IRCreateVarRef(ir.var);
		argNodes = strGraphNodeIRPAppendItem(argNodes, arg);
		if (!defineChainStart)
			defineChainStart = arg;
		if (defineChain)
			graphNodeIRConnect(defineChain, arg, IR_CONN_FLOW);
		graphNodeIRConnect(arg, var, IR_CONN_DEST);
		defineChain = var;
	}

	for (long n = 0; n != strGraphNodeIRPSize(nodes); n++) {
		struct IRNodeFuncArg *arg = (void *)graphNodeIRValuePtr(nodes[n]);
		if (arg->base.type != IR_FUNC_ARG)
			continue;
		__auto_type ref = IRCreateVarRef(args[arg->argIndex].var);
		strGraphNodeIRP toReplace CLEANUP(strGraphNodeIRPDestroy) = strGraphNodePAppendItem(NULL, nodes[n]);
		graphIRReplaceNodes(toReplace, ref, NULL, (void (*)(void *))IRNodeDestroy);
	}

	if (defineChainStart)
		IRInsertAfter(start, defineChainStart, defineChain, IR_CONN_FLOW);

	for (long n = 0; n != strGraphNodeIRPSize(argNodes); n++) {
		struct IRAttrFunc funcAttr;
		funcAttr.base.destroy = NULL;
		funcAttr.base.name = IR_ATTR_FUNC;
		funcAttr.func = funcStart->func;
		IRAttrReplace(argNodes[n], __llCreate(&funcAttr, sizeof(funcAttr)));
	}

	return args;
}
static void abiI386LoadPreservedRegs(long frameSize) {
		struct X86AddressingMode *ebpMode CLEANUP(X86AddrModeDestroy)=X86AddrModeReg(basePointer());
		struct X86AddressingMode *espMode CLEANUP(X86AddrModeDestroy)=X86AddrModeReg(stackPointer());
		asmAssign(espMode, ebpMode, ptrSize(), 0);

		strX86AddrMode subArgs CLEANUP(strX86AddrModeDestroy2)=NULL;
		subArgs=strX86AddrModeAppendItem(subArgs, X86AddrModeReg(stackPointer()));
		subArgs=strX86AddrModeAppendItem(subArgs, X86AddrModeSint(frameSize+4+4+4));
		assembleInst("SUB", subArgs);

		popReg(&regX86EDI);
		popReg(&regX86ESI);
		popReg(&regX86EBX);
}
static void IR_ABI_I386_SYSV_Return(graphNodeIR start,long frameSize) {
	strGraphEdgeIRP in CLEANUP(strGraphEdgeIRPDestroy) = graphNodeIRIncoming(start);
	strGraphEdgeIRP inSource CLEANUP(strGraphEdgeIRPDestroy) = IRGetConnsOfType(in, IR_CONN_SOURCE_A);
	if (strGraphEdgeIRPSize(inSource) != 0) {
		__auto_type source = graphEdgeIRIncoming(inSource[0]);
		struct X86AddressingMode *mode CLEANUP(X86AddrModeDestroy) = IRNode2AddrMode(source);
		__auto_type baseType = objectBaseType(mode->valueType);
		if (baseType == &typeF64) {
			if (mode->type == X86ADDRMODE_REG) {
				// X87 fpu stack must contain return ST(0) upon return
				assert(mode->value.reg == &regX86ST0);
				abiI386LoadPreservedRegs(frameSize);
				goto loadBasePtr;
			} else {
				struct X86AddressingMode *st0 CLEANUP(X86AddrModeDestroy) = X86AddrModeReg(&regX86ST0);
				asmTypecastAssign(NULL, mode,0);
				abiI386LoadPreservedRegs(frameSize);
				goto loadBasePtr;
			}
		} else if (baseType->type == TYPE_CLASS || baseType->type == TYPE_UNION) {

			/* SystemV ABI expects both struct return addess and func return address to be popped
			 * So let's exchange the  struct pointer and func return addr,pop into EAX then reutrn
			 */
			struct X86AddressingMode *retPtr CLEANUP(X86AddrModeDestroy) =
			    X86AddrModeIndirSIB(0, NULL, X86AddrModeReg(basePointer()), X86AddrModeSint(4), objectPtrCreate(baseType));
			struct X86AddressingMode *eaxNode CLEANUP(X86AddrModeDestroy) = X86AddrModeReg(&regX86EAX);
			asmAssign(eaxNode, retPtr, ptrSize(),0);
			
			strX86AddrMode xchgArgs CLEANUP(strX86AddrModeDestroy2) = NULL;
			__auto_type structPtr = X86AddrModeIndirSIB(0, NULL, X86AddrModeReg(basePointer()), X86AddrModeSint(8), objectPtrCreate(baseType));
			xchgArgs = strX86AddrModeAppendItem(xchgArgs, structPtr);
			xchgArgs = strX86AddrModeAppendItem(xchgArgs, X86AddrModeReg(&regX86EAX));
			assembleInst("XCHG", xchgArgs);

			struct X86AddressingMode *indirEaxMode CLEANUP(X86AddrModeDestroy) = X86AddrModeIndirReg(&regX86EAX, baseType);
			asmAssign(indirEaxMode, mode, objectSize(baseType, NULL),0);
			
			abiI386LoadPreservedRegs(frameSize);
			// Pop the extra 4 bytes from the return address,then return
			assembleInst("LEAVE", NULL);
			strX86AddrMode addArgs CLEANUP(strX86AddrModeDestroy2) = NULL;
			addArgs=strX86AddrModeAppendItem(addArgs, X86AddrModeReg(stackPointer()));
			addArgs=strX86AddrModeAppendItem(addArgs, X86AddrModeSint(4));
			assembleInst("ADD", addArgs);
			
			goto ret;
		} else if (baseType == &typeU0) {
				abiI386LoadPreservedRegs(frameSize);
				goto loadBasePtr;
		} else {
			// Isn't a struct/union/F64,so is an int/ptr
			struct X86AddressingMode *eaxMode CLEANUP(X86AddrModeDestroy) = X86AddrModeReg(subRegOfType(&regX86EAX, mode->valueType));
			eaxMode->valueType = mode->valueType;
			asmTypecastAssign(eaxMode, mode,0);
			
			abiI386LoadPreservedRegs(frameSize);
			goto loadBasePtr;
		}
	} else {
			abiI386LoadPreservedRegs(frameSize);
	}
loadBasePtr : { assembleInst("LEAVE", NULL); }
ret:
	assembleInst("RET", NULL);
}
strVar IRABIInsertLoadArgs(graphNodeIR start) {
	calledInsertArgs = 1;
	switch (getCurrentArch()) {
	case ARCH_TEST_SYSV:
	case ARCH_X86_SYSV: {
		return IR_ABI_I386_SYS_InsertLoadArgs(start);
	}
	case ARCH_X64_SYSV:
		assert(0);
		return NULL;
	}
}
void IRABIAsmPrologue(long frameSize) {
	switch (getCurrentArch()) {
	case ARCH_TEST_SYSV:
	case ARCH_X86_SYSV: {
		struct X86AddressingMode *esp CLEANUP(X86AddrModeDestroy) = X86AddrModeReg(stackPointer());
		struct X86AddressingMode *ebp CLEANUP(X86AddrModeDestroy) = X86AddrModeReg(basePointer());
		pushReg(basePointer());
		asmAssign(ebp, esp, ptrSize(),0);

		strX86AddrMode subArgs CLEANUP(strX86AddrModeDestroy2)=NULL;
		subArgs=strX86AddrModeAppendItem(subArgs, X86AddrModeReg(stackPointer()));
		subArgs=strX86AddrModeAppendItem(subArgs, X86AddrModeSint(frameSize));
		assembleInst("SUB", subArgs);

		pushReg(&regX86EBX);
		pushReg(&regX86ESI);
		pushReg(&regX86EDI);
		return;
	}
	case ARCH_X64_SYSV:
		assert(0);
		return;
	}
}
void IRABIReturn2Asm(graphNodeIR start,long frameSize) {
	switch (getCurrentArch()) {
	case ARCH_TEST_SYSV:
	case ARCH_X86_SYSV: {
			return IR_ABI_I386_SYSV_Return(start,frameSize);
	}
	case ARCH_X64_SYSV:
		assert(0);
	}
}
void IRABICall2Asm(graphNodeIR start) {
	switch (getCurrentArch()) {
	case ARCH_TEST_SYSV:
	case ARCH_X86_SYSV: {
		return IR_ABI_I386_SYSV_2Asm(start);
	}
	case ARCH_X64_SYSV:
		assert(0);
	}
}
