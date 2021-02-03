#include <abi.h>
#include <IRLiveness.h>
#include <cleanup.h>
#include <assert.h>
#include <asmEmitter.h>
#include <IR2asm.h>>
#define DEBUG_PRINT_ENABLE 1
void *IR_ATTR_ABI_INFO="ABI_INFO";
void IRAttrABIInfoDestroy(struct IRAttr *a) {
		struct IRAttrABIInfo *abi=(void*)a;
		strRegPDestroy(&abi->toPushPop);
}
static void *IR_ATTR_OLD_REG_SLICE = "OLD_REG_SLICE";
struct IRATTRoldRegSlice {
		struct IRAttr base;
		graphNodeIR old;
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
static int containsRegister(struct reg *par,struct reg *r) {
		for(long a=0;a!=strRegSliceSize(par->affects);a++) {
				if(par->affects[a].reg==r)
						return 1;
		}
		return 0;
}
static struct reg *__registerContainingBoth(struct reg *master,struct reg * a,struct reg *b) {
		assert(a->masterReg==b->masterReg);
		for(long c=0;c!=strRegSliceSize(master->affects);c++) {
				if(containsRegister(master->affects[c].reg , a)&&containsRegister(master->affects[c].reg, b))
						return __registerContainingBoth(master->affects[c].reg,a,b);
		}
		return master;
}
/**
	* Finds (smallest) register containing both registers
	*/
static struct reg *smallestRegContainingBoth(struct reg * a,struct reg *b) {
		return __registerContainingBoth(a->masterReg, a, b);
}
static int killEdgeIfEq(void *a, void *b) {
		return a==b;
}
static void swapNode(graphNodeIR old,graphNodeIR with) {
		strGraphEdgeIRP in CLEANUP(strGraphEdgeIRPDestroy)=graphNodeIRIncoming(old);
		strGraphEdgeIRP out CLEANUP(strGraphEdgeIRPDestroy)=graphNodeIROutgoing(old);
		for(long i=0;i!=strGraphEdgeIRPSize(in);i++) {
				__auto_type from=graphEdgeIRIncoming(in[i]); 
				graphNodeIRConnect(from, with, *graphEdgeIRValuePtr(in[i]));
				graphEdgeIRKill(from, old, in[i],killEdgeIfEq,NULL);
		}
		for(long o=0;o!=strGraphEdgeIRPSize(out);o++) {
				__auto_type to=graphEdgeIROutgoing(out[o]); 
				graphNodeIRConnect(with,to, *graphEdgeIRValuePtr(out[o]));
				graphEdgeIRKill(old,to, out[o],killEdgeIfEq,NULL);
		}
}
static strRegP usedRegisters(strGraphNodeIRP nodes) {
	strRegP retVal = NULL;
	for (long i = 0; i != strGraphNodeIRPSize(nodes); i++) {
		struct IRNodeValue *value = (void *)graphNodeIRValuePtr(nodes[i]);
		if (value->base.type == IR_VALUE)
			if (value->val.type == IR_VAL_REG) {
				__auto_type reg = value->val.value.reg.reg;
				if (strRegPSortedFind(retVal, reg, (regCmpType)ptrPtrCmp))
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
		return strcpy(malloc(strlen(txt)+1), txt);
}
#define ARRAY_SIZE(array) (sizeof(array) / sizeof(*array))
PTR_MAP_FUNCS(struct parserVar *, struct reg *, Var2Reg);
static strRegP mergeConflictingRegs(strRegP conflicts) {
		//
		// If 2 items in conflicts have a common master register "merge" the registers and replace with the common register
		// 2 birds 1 stone.
		//
	mergeLoop:
		for(long c1=0;c1!=strRegPSize(conflicts);c1++) {
				for(long c2=0;c2!=strRegPSize(conflicts);c2++) {
						if(conflicts[c1]==conflicts[c2])
								continue;
						if(conflicts[c1]->masterReg!=conflicts[c2]->masterReg)
								continue;
						__auto_type common=smallestRegContainingBoth(conflicts[c1], conflicts[c2]);
						//Do a set differnece,then insert the common register
						strRegP dummy CLEANUP(strRegPDestroy)=NULL;
						dummy=strRegPSortedInsert(dummy, conflicts[c1], (regCmpType)ptrPtrCmp);
						dummy=strRegPSortedInsert(dummy, conflicts[c2], (regCmpType)ptrPtrCmp);
						conflicts=strRegPSetDifference(conflicts, dummy, (regCmpType)ptrPtrCmp);
						conflicts=strRegPSortedInsert(conflicts, common, (regCmpType)ptrPtrCmp);
						goto mergeLoop;
				}
		}

		return conflicts;
}
static void findRegisterLiveness(graphNodeIR start) {
	strGraphNodeIRP allNodes CLEANUP(strGraphNodeIRPDestroy) = graphNodeIRAllNodes(start);
	// Replace Registers with variables and do liveness analysis on the variables
	strRegP usedRegs CLEANUP(strRegPDestroy) = usedRegisters(allNodes);
	strVariable regVars CLEANUP(strVariableDestroy) = NULL;
	ptrMapVar2Reg var2Reg = ptrMapVar2RegCreate();
	for (long i = 0; i != strRegPSize(usedRegs); i++) {
		__auto_type newVar = IRCreateVirtVar(&typeI64i);
		newVar->name=strDup(usedRegs[i]->name);
		regVars = strVariableSortedInsert(regVars, newVar, (varCmpType)ptrPtrCmp);
		ptrMapVar2RegAdd(var2Reg, newVar, usedRegs[i]);
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
		__auto_type index = strRegPSortedFind(usedRegs, reg, (regCmpType)ptrPtrCmp) - usedRegs;
		__auto_type newNode = IRCreateVarRef(regVars[i]);
		
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
	strGraphNodeIRLiveP livenessGraphs CLEANUP(strGraphNodeIRLivePDestroy) = IRInterferenceGraphFilter(start, regVars, isSelectVariable);
	for (long n = 0; n != strGraphNodeIRPSize(allNodes); n++) {
		struct IRNodeFuncCall *call = (void *)graphNodeIRValuePtr(allNodes[n]);
		if (call->base.type != IR_FUNC_CALL)
			continue;
		__auto_type attr = llIRAttrFind(call->base.attrs, IR_ATTR_BASIC_BLOCK, IRAttrGetPred);
		assert(attr);
		struct IRAttrBasicBlock *block = (void *)llIRAttrValuePtr(attr);
		
		strVar inVars CLEANUP(strVarDestroy)=NULL;
		strVar outVars CLEANUP(strVarDestroy)=NULL;
		for(long i=0;i!=strVarSize(block->block->in);i++)
				inVars=strVarSortedInsert(inVars, block->block->in[i], IRVarCmp);
		for(long o=0;o!=strVarSize(block->block->out);o++)
				outVars=strVarSortedInsert(outVars, block->block->out[o], IRVarCmp);
		//Find insrection in/out registers that conflict
		strRegP conflicts =NULL;
		for(long i=0;i!=strVarSize(inVars);i++) {
				__auto_type iFind=*ptrMapVar2RegGet(var2Reg, inVars[i].var);
				for(long o=0;o!=strVarSize(outVars);o++) {
						__auto_type oFind=*ptrMapVar2RegGet(var2Reg, outVars[o].var);
						if(regConflict(iFind, oFind)) {
								__auto_type common=smallestRegContainingBoth(iFind, oFind);
								conflicts=strRegPSortedInsert(conflicts, common, (regCmpType)ptrPtrCmp);
						}
				}
		}

		conflicts=mergeConflictingRegs(conflicts);

		struct IRAttrABIInfo info;
		info.base.name=IR_ATTR_ABI_INFO;
		info.base.destroy=IRAttrABIInfoDestroy;
		info.toPushPop=conflicts;
		
		llIRAttr infoAttr=__llCreate(&info, sizeof(info));
		call->base.attrs=llIRAttrInsert(call->base.attrs, infoAttr, IRAttrInsertPred);
	}

	//Repalce temp variables with original registers
	for(long n = 0; n != strGraphNodeIRPSize(allNodes); n++) {
			struct IRNodeValue *node=(void*)graphNodeIRValuePtr(allNodes[n]);
			if(node->base.type!=IR_VALUE)
					continue;
			if(node->val.type!=IR_VAL_VAR_REF)
					continue;
			struct IRATTRoldRegSlice *find=(void*)llIRAttrFind(node->base.attrs, IR_ATTR_OLD_REG_SLICE, IRAttrGetPred);
			if(!find)
					continue;
			swapNode(allNodes[n], find->old);
			graphNodeIRKill(&allNodes[n], (void(*)(void*))IRNodeDestroy, NULL);
	}
	
	ptrMapVar2RegDestroy(var2Reg, NULL);
}
static strGraphNodeIRP getFuncArgs(graphNodeIR call) {
		strGraphEdgeIRP in CLEANUP(strGraphEdgeIRPDestroy)=IREdgesByPrec(call);
		strGraphNodeIRP args=NULL;
		for(long i=0;i!=strGraphEdgeIRPSize(in);i++) {
				switch(*graphEdgeIRValuePtr(in[i])) {
						default:
								break;
				case IR_CONN_FUNC_ARG_1...IR_CONN_FUNC_ARG_128:
						args=strGraphNodeIRPAppendItem(args, graphEdgeIRIncoming(in[i]));
				}
		}
		return args;
}
static void assembleInst(const char *name, strX86AddrMode args) {
	strOpcodeTemplate ops CLEANUP(strOpcodeTemplateDestroy) = X86OpcodesByArgs(name, args, NULL);
	assert(strOpcodeTemplateSize(ops));
	int err;
	X86EmitAsmInst(ops[0], args, &err);
	assert(!err);
}
static struct X86AddressingMode *nodePtr(graphNodeIR node) {
		struct IRNodeValue *val=(void*)graphNodeIRValuePtr(node);
		switch(val->val.type) {
		case __IR_VAL_MEM_FRAME:  {
				return X86AddrModeIndirSIB(0, NULL, X86AddrModeReg(basePointer()),
																															X86AddrModeSint(val->val.value.__frame.offset), val->val.value.__frame.type);
		}
		case IR_VAL_VAR_REF: {
				if(!val->val.value.var.var->isGlobal) {
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
static void strX86AddrModeDestroy2(strX86AddrMode *str)  {
	for (long i = 0; i != strX86AddrModeSize(*str); i++)
		X86AddrModeDestroy(&str[0][i]);
	strX86AddrModeDestroy(str);
}
static int conflictsWithRegisters(const void *_regs,const struct reg **r) {
		strRegP regs=(void*)_regs;
		for(long c=0;c!=strRegPSize(regs);c++)
				if(regConflict((struct reg*)*r, regs[c]))
						return 1;
		return 0;
}
//
// http://www.sco.com/developers/devspecs/abi386-4.pdf
//
void IR_ABI_I386_SYSV_2Asm(graphNodeIR start) {
		findRegisterLiveness(start);
		struct IRNodeFuncCall *call=(void*)graphNodeIRValuePtr(start);
		struct IRAttrABIInfo *info=(void*)llIRAttrFind(call->base.attrs, IR_ATTR_ABI_INFO, IRAttrGetPred);

		strGraphEdgeIRP in CLEANUP(strGraphEdgeIRPDestroy)=graphNodeIRIncoming(start);
		strGraphEdgeIRP inFunc CLEANUP(strGraphEdgeIRPDestroy)=IRGetConnsOfType(in, IR_CONN_FUNC);
		struct object *funcType= IRNodeType(graphEdgeIRIncoming(inFunc[0]));
		if(funcType->type==TYPE_PTR)
				funcType=((struct objectPtr *)funcType)->type;
		assert(funcType->type==TYPE_FUNCTION);
		struct objectFunction *func=(struct objectFunction*)funcType;
		strGraphNodeIRP args=getFuncArgs(start);
		assert(strGraphNodeIRPSize(args)==strFuncArgSize(func->args));

		strRegP clobbered CLEANUP(strRegPDestroy)=strRegPClone(info->toPushPop);
		//EAX is used as a scracth register here
		clobbered=strRegPSortedInsert(clobbered, &regX86EAX, (regCmpType)ptrPtrCmp);
		clobbered=mergeConflictingRegs(clobbered);

		strRegP preserved CLEANUP(strRegPDestroy)=NULL;
		preserved=strRegPSortedInsert(preserved, &regX86EBX, (regCmpType)ptrPtrCmp);
		preserved=strRegPSortedInsert(preserved, &regX86ESI, (regCmpType)ptrPtrCmp);
		preserved=strRegPSortedInsert(preserved, &regX86EDI, (regCmpType)ptrPtrCmp);
		preserved=strRegPSortedInsert(preserved, &regX86ESP, (regCmpType)ptrPtrCmp);
		preserved=strRegPSortedInsert(preserved, &regX86EBP, (regCmpType)ptrPtrCmp);
		clobbered=strRegPRemoveIf(clobbered, preserved, conflictsWithRegisters);

		long stackSize=0;
		long eaxOffset=0;

		//Passes a secret first arg if returns a struct/union(that isn't based on a primtive)
		//For example(I32 is treated as a I32i so dont return a union)
		__auto_type retType=objectBaseType(func->retType);
		int retsStruct=retType->type==TYPE_CLASS||retType->type==TYPE_UNION;
		int retsFloat=retType==&typeF64;

		//Is an int
		if(!retsStruct&&!retsFloat) {
				//Make a dummy area to store retVal when poping registers
				strX86AddrMode dpp CLEANUP(strX86AddrModeDestroy2)=strX86AddrModeAppendItem(NULL, X86AddrModeSint(0));
				assembleInst("PUSH", dpp);
				stackSize+=4;
		}
		
		//Push all the clobered
		for(long p=0;p!=strRegPSize(clobbered);p++) {
				if(clobbered[p]==&regX86EAX)
						eaxOffset=stackSize;
						
				strX86AddrMode r CLEANUP(strX86AddrModeDestroy2)=strX86AddrModeAppendItem(NULL, X86AddrModeReg(clobbered[p]));
				assembleInst("PUSH", r);
				stackSize+=clobbered[p]->size;
		}

		strGraphEdgeIRP out CLEANUP(strGraphEdgeIRPDestroy)=graphNodeIROutgoing(start);
		strGraphEdgeIRP dst CLEANUP(strGraphEdgeIRPDestroy)=IRGetConnsOfType(out, IR_CONN_DEST);
		if(retsStruct) {
				assert(strGraphEdgeIRPSize(dst)==1);
				__auto_type outNode=graphEdgeIROutgoing(dst[0]);
						
				strX86AddrMode leaArgs CLEANUP(strX86AddrModeDestroy2)=NULL;
				leaArgs=strX86AddrModeAppendItem(leaArgs, X86AddrModeReg(&regX86EAX));
				leaArgs=strX86AddrModeAppendItem(leaArgs, nodePtr(outNode));
				assembleInst("LEA",  leaArgs);
						
				strX86AddrMode pushEAXArgs CLEANUP(strX86AddrModeDestroy2)=NULL;
				pushEAXArgs=strX86AddrModeAppendItem(pushEAXArgs, X86AddrModeReg(&regX86EAX));
				assembleInst("PUSH", pushEAXArgs);
				stackSize+=4;
		}

		long stackSizeBeforeArgs=stackSize;
		for(long i=0;i!=strGraphNodeIRPSize(args);i++) {
				__auto_type type=objectBaseType(IRNodeType(args[i]));
				long itemSize=objectSize(type, NULL);
				if(type->type==TYPE_CLASS||type->type==TYPE_UNION) {
						struct X86AddressingMode *stack CLEANUP(X86AddrModeDestroy)=X86AddrModeIndirReg(stackPointer(), type);
						struct X86AddressingMode *val CLEANUP(X86AddrModeDestroy)=IRNode2AddrMode(args[i]);
						asmAssign(stack, val, itemSize);

						//Must be aligned to 4 bytes
						long aligned=itemSize/4*4;
						strX86AddrMode addSP CLEANUP(strX86AddrModeDestroy2)=NULL;
						addSP=strX86AddrModeAppendItem(addSP, X86AddrModeReg(stackPointer()));
						addSP=strX86AddrModeAppendItem(addSP, X86AddrModeSint(aligned));
						assembleInst("ADD", addSP);
				} else {
						strX86AddrMode pushArgs CLEANUP(strX86AddrModeDestroy2)=NULL;
						struct X86AddressingMode *mode CLEANUP(X86AddrModeDestroy)=IRNode2AddrMode(args[i]);
						//
						//EAX is trashed during the calling sequence,so retrieve EAX from the pushed registers if wants an EAX affecting register
						//
						int swapEaxWithStack=0;
						if(mode->type==X86ADDRMODE_REG)
								if(regConflict(mode->value.reg, &regX86EAX)) {
										swapEaxWithStack=1;
								}
						if(swapEaxWithStack) {
								strX86AddrMode xchgArgs CLEANUP(strX86AddrModeDestroy2)=NULL;
								xchgArgs=strX86AddrModeAppendItem(xchgArgs, X86AddrModeReg(&regX86EAX));
								xchgArgs=strX86AddrModeAppendItem(xchgArgs, X86AddrModeIndirSIB(0, NULL, X86AddrModeReg(stackPointer()), X86AddrModeSint(eaxOffset-stackSize), &typeU32i));
								assembleInst("XCHG", xchgArgs);
						}
								
						if(itemSize!=4) {
								struct X86AddressingMode *eax CLEANUP(X86AddrModeDestroy)=X86AddrModeReg(&regX86EAX);
								asmTypecastAssign(eax,  mode);
								pushArgs=strX86AddrModeAppendItem(pushArgs, X86AddrModeReg(&regX86EAX));
						} else {
								pushArgs=strX86AddrModeAppendItem(pushArgs, IRNode2AddrMode(args[i]));
						}
								
						if(swapEaxWithStack) {
								strX86AddrMode xchgArgs CLEANUP(strX86AddrModeDestroy2)=NULL;
								xchgArgs=strX86AddrModeAppendItem(xchgArgs, X86AddrModeReg(&regX86EAX));
								xchgArgs=strX86AddrModeAppendItem(xchgArgs, X86AddrModeIndirSIB(0, NULL, X86AddrModeReg(stackPointer()), X86AddrModeSint(eaxOffset-stackSize), &typeU32i));
								assembleInst("XCHG", xchgArgs);
						}
								
						assembleInst("PUSH", pushArgs);
						stackSize+=4;
				}
		}
		strX86AddrMode callArgs CLEANUP(strX86AddrModeDestroy2)=NULL;
		callArgs=strX86AddrModeAppendItem(callArgs, IRNode2AddrMode(graphEdgeIRIncoming(inFunc[0])));
		assembleInst("CALL",  callArgs);

		if(retType!=&typeU0&&strGraphEdgeIRPSize(dst)) {
				__auto_type outNode=graphEdgeIROutgoing(dst[0]);
				if(!retsStruct) {
						//We point to area we made on the stack for the return value eariler
						struct X86AddressingMode *outMode CLEANUP(X86AddrModeDestroy)=X86AddrModeIndirSIB(0, NULL,X86AddrModeReg(stackPointer()), X86AddrModeSint(-stackSize), IRNodeType(outNode));
						if(objectBaseType( outMode->valueType)!=&typeF64) {
								struct X86AddressingMode *eaxMode CLEANUP(X86AddrModeDestroy)=X86AddrModeReg(&regX86EAX); 
								asmTypecastAssign(outMode, eaxMode);
								goto end;
						} else {
								struct X86AddressingMode *outMode CLEANUP(X86AddrModeDestroy)=IRNode2AddrMode(outNode);
								struct X86AddressingMode *st0Mode CLEANUP(X86AddrModeDestroy)=X86AddrModeReg(&regX86ST0); 
								asmAssign(outMode, st0Mode, 8);
								goto end;
						} 
				} else {
						struct X86AddressingMode *outMode CLEANUP(X86AddrModeDestroy)=IRNode2AddrMode(outNode);
						struct X86AddressingMode *indirEaxMode CLEANUP(X86AddrModeDestroy)=X86AddrModeIndirReg(&regX86EAX,IRNodeType(outNode));
						asmAssign(outMode, indirEaxMode, objectSize(IRNodeType(outNode), NULL));
						goto end;
				}
				assert(0);
		}
	end:;
		strX86AddrMode stackSubArgs CLEANUP(strX86AddrModeDestroy2)=NULL;
		stackSubArgs=strX86AddrModeAppendItem(stackSubArgs, X86AddrModeReg(stackPointer()));
		stackSubArgs=strX86AddrModeAppendItem(stackSubArgs, X86AddrModeSint(stackSize-stackSizeBeforeArgs));
		assembleInst("SUB", stackSubArgs);
		stackSize=stackSizeBeforeArgs;
		
		//Pop all the clobered
		for(long p=strRegPSize(clobbered)-1;p>=0;p--) {
				if(clobbered[p]==&regX86EAX)
						eaxOffset=stackSize;
						
				strX86AddrMode r CLEANUP(strX86AddrModeDestroy2)=strX86AddrModeAppendItem(NULL, X86AddrModeReg(clobbered[p]));
				assembleInst("POP", r);
				stackSize-=clobbered[p]->size;
		}
		
		//Now we are left with the spot we made eariler for return values,let's pop it(if not a structure,all int non-structs get stuffed in EAX)
		if(!retsStruct&&!retsFloat) {
				__auto_type outNode=graphEdgeIROutgoing(dst[0]);
				strX86AddrMode outArgs CLEANUP(strX86AddrModeDestroy2)=strX86AddrModeAppendItem(NULL,IRNode2AddrMode(outNode));
				assembleInst("POP",  outArgs);
		}
}
