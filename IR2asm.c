#include <limits.h>
#include <IR.h>
#include <diagMsg.h>
#include <IR2asm.h>
#include <IRTypeInference.h>
#include <X86AsmSharedVars.h>
#include <abi.h>
#include <asmEmitter.h>
#include <assert.h>
#include <cleanup.h>
#include <ctype.h>
#include <frameLayout.h>
#include <hashTable.h>
#include <ieee754.h>
#include <parse2IR.h>
#include <parserA.h>
#include <parserB.h>
#include <ptrMap.h>
#include <regAllocator.h>
#include <registers.h>
#include <stdarg.h>
#include <stdio.h>
#include <x87fpu.h>
#define DEBUG_PRINT_ENABLE 1
static void *IR_ATTR_ADDR_MODE = "ADDR_MODE";
struct IRAttrAddrMode {
	struct IRAttr base;
	struct X86AddressingMode *mode;
};
static void IRAttrAddrModeDestroy(struct IRAttr *attr) {
	struct IRAttrAddrMode *Attr = (void *)attr;
	X86AddrModeDestroy(&Attr->mode);
}
typedef int (*regCmpType)(const struct reg **, const struct reg **);
typedef int (*gnCmpType)(const graphNodeIR *, const graphNodeIR *);
static int ptrPtrCmp(const void *a, const void *b) {
		return *(void**)a-*(void**)b;
}
STR_TYPE_DEF(char, Char);
STR_TYPE_FUNCS(char, Char);
PTR_MAP_FUNCS(graphNodeIR, strChar, LabelNames);
PTR_MAP_FUNCS(graphNodeIR, int, CompiledNodes);
PTR_MAP_FUNCS(struct parserFunction *, strChar, FuncNames);
static __thread long labelsCount;
static __thread ptrMapFuncNames funcNames;
static __thread ptrMapLabelNames asmLabelNames;
static __thread ptrMapCompiledNodes compiledNodes;
static __thread int insertLabelsForAsmCalled = 0;
static __thread long frameSize=0;
static void assembleInst(const char *name, strX86AddrMode args) {
	__auto_type template  = X86OpcodeByArgs(name, args);
 	assert(template);
	int err;
	X86EmitAsmInst(template, args, &err);
	assert(!err);
}
STR_TYPE_DEF(long,Long);
STR_TYPE_FUNCS(long,Long);
static int regIsAliveAtNode(graphNodeIR atNode,struct reg *r) {
		return 1;
}
static void strX86AddrModeDestroy2(strX86AddrMode *str) {
	for (long i = 0; i != strX86AddrModeSize(*str); i++)
		X86AddrModeDestroy(&str[0][i]);
	strX86AddrModeDestroy(str);
}
static void pushReg(struct reg *r) {
		const struct reg *fpu[]={
				&regX86ST0,
				&regX86ST1,
				&regX86ST2,
				&regX86ST3,
				&regX86ST4,
				&regX86ST5,
				&regX86ST6,
				&regX86ST7,
		};
		for(long i=0;i!=8;i++)
				if(fpu[i]==r) {
						strX86AddrMode addArgs CLEANUP(strX86AddrModeDestroy2) = NULL;
						addArgs = strX86AddrModeAppendItem(addArgs, X86AddrModeReg(stackPointer()));
						addArgs = strX86AddrModeAppendItem(addArgs, X86AddrModeSint(8));
						assembleInst("SUB", addArgs);
						
						strX86AddrMode movArgs CLEANUP(strX86AddrModeDestroy2) = NULL;
						movArgs = strX86AddrModeAppendItem(movArgs, X86AddrModeIndirReg(stackPointer(),objectPtrCreate(&typeU0)));
						movArgs[0]->valueType=&typeF64;
						assembleInst("FSTP", movArgs);
						return;
				}
	// Can't push 8-bit registers,so make room on the stack and assign
	if (r->size == 1) {
		strX86AddrMode subArgs CLEANUP(strX86AddrModeDestroy2) = NULL;
		subArgs = strX86AddrModeAppendItem(subArgs, X86AddrModeReg(stackPointer()));
		subArgs = strX86AddrModeAppendItem(subArgs, X86AddrModeSint(1));
		assembleInst("SUB", subArgs);

		strX86AddrMode movArgs CLEANUP(strX86AddrModeDestroy2) = NULL;
		movArgs = strX86AddrModeAppendItem(movArgs, X86AddrModeIndirSIB(0, NULL, X86AddrModeReg(stackPointer()), X86AddrModeSint(0), &typeU8i));
		movArgs = strX86AddrModeAppendItem(movArgs, X86AddrModeReg(r));
		assembleInst("MOV", movArgs);
		return;
	}
	strX86AddrMode ppIndexArgs CLEANUP(strX86AddrModeDestroy2) = strX86AddrModeAppendItem(NULL, X86AddrModeReg(r));
	assembleInst("PUSH", ppIndexArgs);
}
 void pushMode(struct X86AddressingMode *mode) {
	if (mode->type == X86ADDRMODE_REG) {
		pushReg(mode->value.reg);
		return;
	} else if(mode->valueType) {
			__auto_type baseType=objectBaseType(mode->valueType);
			if(baseType==&typeF64) {
					switch(getCurrentArch()) {
					case ARCH_TEST_SYSV:
					case ARCH_X86_SYSV: {
							strX86AddrMode subArgs CLEANUP(strX86AddrModeDestroy2)=NULL;
							subArgs=strX86AddrModeAppendItem(subArgs, X86AddrModeReg(stackPointer()));
							subArgs=strX86AddrModeAppendItem(subArgs, X86AddrModeSint(8));
							assembleInst("SUB", subArgs);

							struct X86AddressingMode *aMode CLEANUP(X86AddrModeDestroy)=X86AddrModeReg(&regX86ST0);
							asmAssign(aMode, mode, 8, ASM_ASSIGN_X87FPU_POP);
							struct X86AddressingMode *stackTop CLEANUP(X86AddrModeDestroy)=X86AddrModeIndirReg(stackPointer(), &typeF64);
							asmAssign(stackTop, aMode, 8, ASM_ASSIGN_X87FPU_POP);
							return;
					}
					case ARCH_X64_SYSV:
							return;
					}
			}
	}
	strX86AddrMode pushArgs CLEANUP(strX86AddrModeDestroy) = strX86AddrModeAppendItem(NULL, mode);
	assembleInst("PUSH", pushArgs);
}
static void popReg(struct reg *r) {
	// Can't pop 8-bit registers,so make room on the stack and assign
	if (r->size == 1) {
		strX86AddrMode movArgs CLEANUP(strX86AddrModeDestroy2) = NULL;
		movArgs = strX86AddrModeAppendItem(movArgs, X86AddrModeReg(r));
		movArgs = strX86AddrModeAppendItem(movArgs, X86AddrModeIndirSIB(0, NULL, X86AddrModeReg(stackPointer()), X86AddrModeSint(0), &typeU8i));
		assembleInst("MOV", movArgs);

		strX86AddrMode addArgs CLEANUP(strX86AddrModeDestroy2) = NULL;
		addArgs = strX86AddrModeAppendItem(addArgs, X86AddrModeReg(stackPointer()));
		addArgs = strX86AddrModeAppendItem(addArgs, X86AddrModeSint(1));
		assembleInst("ADD", addArgs);
		return;
	}

	strX86AddrMode ppIndexArgs CLEANUP(strX86AddrModeDestroy2) = strX86AddrModeAppendItem(NULL, X86AddrModeReg(r));
	assembleInst("POP", ppIndexArgs);
}
void popMode(struct X86AddressingMode *mode) {
	if (mode->type == X86ADDRMODE_REG) {
		popReg(mode->value.reg);
	} else {
		strX86AddrMode pushArgs CLEANUP(strX86AddrModeDestroy) = strX86AddrModeAppendItem(NULL, mode);
		assembleInst("POP", pushArgs);
	}
}
static struct object *getTypeForSize(long size) {
	switch (size) {
	case 1:
		return &typeI8i;
	case 2:
		return &typeI16i;
	case 4:
		return &typeI32i;
	case 8:
		return &typeI64i;
	}
	return &typeU0;
}
static strGraphNodeIRP removeNeedlessLabels(graphNodeIR start) {
	strGraphNodeIRP allNodes CLEANUP(strGraphNodeIRPDestroy) = graphNodeIRAllNodes(start);
	strGraphNodeIRP removed = NULL;
	for (long i = 0; i != strGraphNodeIRPSize(allNodes); i++) {
		if (allNodes[i] == start)
			continue;

		__auto_type val = graphNodeIRValuePtr(allNodes[i]);
		if (val->type != IR_LABEL)
			continue;
		
		// Dont remove if named
		if (llIRAttrFind(val->attrs, IR_ATTR_LABEL_NAME, IRAttrGetPred))
			continue;

		strGraphEdgeIRP in CLEANUP(strGraphEdgeIRPDestroy) = graphNodeIRIncoming(allNodes[i]);
		
		// Dont remvoe if connected to jump-table
		int connectedToJmpTab=0;
		for(long i=0;i!=strGraphEdgeIRPSize(in);i++) {
				if(graphNodeIRValuePtr(graphEdgeIRIncoming(in[i]))->type==IR_JUMP_TAB) {
						connectedToJmpTab=1;
						break;
				}
		}
		if(connectedToJmpTab)
				continue;

		strGraphEdgeIRP out CLEANUP(strGraphEdgeIRPDestroy) = graphNodeIROutgoing(allNodes[i]);
		if (strGraphEdgeIRPSize(in) != 1)
			continue;
		if (strGraphEdgeIRPSize(out) > 1)
			continue;
		//"Transparently" remove
		for (long i = 0; i != strGraphEdgeIRPSize(in); i++)
			for (long o = 0; o != strGraphEdgeIRPSize(out); o++)
				graphNodeIRConnect(graphEdgeIRIncoming(in[i]), graphEdgeIROutgoing(out[o]), *graphEdgeIRValuePtr(in[i]));
		graphNodeIRKill(&allNodes[i], (void (*)(void *))IRNodeDestroy, NULL);

		removed = strGraphNodeIRPSortedInsert(removed, allNodes[i], (gnCmpType)ptrPtrCmp);
	}

	return removed;
}
void IR2AsmInit() {
	labelsCount = 0;
	asmFuncNames = ptrMapFuncNamesCreate();
	asmLabelNames = ptrMapLabelNamesCreate();
	compiledNodes = ptrMapCompiledNodesCreate();
	insertLabelsForAsmCalled = 1;
}
/**
 * These are the consumed registers for the current operation,doesn't
 * include registers for variables
 */
static strRegP consumedRegisters = NULL;
strChar uniqueLabel(const char *head) {
	if (!head)
		head = "";
	long count = snprintf(NULL, 0, "%s_%li$", head, ++labelsCount);
	char buffer[count + 1];
	sprintf(buffer, "%s_%li$", head, labelsCount);
	return strCharAppendData(NULL, buffer, count + 1);
}
struct reg *regForTypeExcludingConsumed(struct object *type) {
	strRegP regs CLEANUP(strRegPDestroy) = regGetForType(type);
	for (long i = 0; i != strRegPSize(regs); i++) {
		for (long i2 = 0; i2 != strRegPSize(consumedRegisters); i2++) {
			if (regConflict(regs[i], consumedRegisters[i2]))
				goto next;
		}
		return regs[i];
	next:;
	}
	return NULL;
}
static graphNodeIR nodeDest(graphNodeIR node) {
	strGraphEdgeIRP out CLEANUP(strGraphEdgeIRPDestroy) = graphNodeIROutgoing(node);
	assert(strGraphEdgeIRPSize(out) == 1);
	return graphEdgeIROutgoing(out[0]);
}
static void binopArgs(graphNodeIR node, graphNodeIR *a, graphNodeIR *b) {
	strGraphEdgeIRP in CLEANUP(strGraphEdgeIRPDestroy) = graphNodeIRIncoming(node);
	strGraphEdgeIRP inA CLEANUP(strGraphEdgeIRPDestroy) = IRGetConnsOfType(in, IR_CONN_SOURCE_A);
	strGraphEdgeIRP inB CLEANUP(strGraphEdgeIRPDestroy) = IRGetConnsOfType(in, IR_CONN_SOURCE_B);
	if (strGraphEdgeIRPSize(inA) == 1)
		*a = graphEdgeIRIncoming(inA[0]);
	else
		assert(0);
	if (strGraphEdgeIRPSize(inB) == 1)
		*b = graphEdgeIRIncoming(inB[0]);
	else
		assert(0);
}
static int isReg(graphNodeIR node) {
	if (graphNodeIRValuePtr(node)->type == IR_VALUE) {
		struct IRNodeValue *val = (void *)graphNodeIRValuePtr(node);
		return val->val.type == IR_VAL_REG;
	}
	return 0;
}
#define ALLOCATE(val)                                                                                                                                              \
	({                                                                                                                                                               \
		typeof(val) *r = calloc(sizeof(val));                                                                                                                          \
		*r = val;                                                                                                                                                      \
		r;                                                                                                                                                             \
	})
static strChar strClone(const char *name) {
	return strCharAppendData(NULL, name, strlen(name) + 1);
}
static strChar unescapeString(const char *str) {
	char *otherValids = "[]{}\\|;:\"\'<>?,./`~!@#$%^&*()-_+=";
	long len = strlen(str);
	strChar retVal = NULL;
	for (long i = 0; i != len; i++) {
		if (isalnum(str[i])) {
			retVal = strCharAppendItem(retVal, str[i]);
		} else {
			if (strchr(otherValids, str[i])) {
				retVal = strCharAppendItem(retVal, str[i]);
			} else {
				long count = snprintf(NULL, 0, "\\%02x", ((uint8_t *)str)[i]);
				char buffer[count + 1];
				sprintf(buffer, "\\%02x", ((uint8_t *)str)[i]);
				retVal = strCharAppendData(retVal, buffer, strlen(buffer));
			}
		}
	}
	return retVal;
}
static const char *getLabelName(graphNodeIR node) {
loop:;
	__auto_type existing = ptrMapLabelNamesGet(asmLabelNames, node);
	if (existing)
		return *existing;

	__auto_type nv = graphNodeIRValuePtr(node);
	__auto_type find = llIRAttrFind(nv->attrs, IR_ATTR_LABEL_NAME, IRAttrGetPred);
	if (find) {
		struct IRAttrLabelName *name = (void *)llIRAttrValuePtr(find);
		ptrMapLabelNamesAdd(asmLabelNames, node, strClone(name->name));
	} else {
		ptrMapLabelNamesAdd(asmLabelNames, node, uniqueLabel(""));
	}
	goto loop;
}
static struct X86AddressingMode *__node2AddrMode(graphNodeIR start) {
	if (graphNodeIRValuePtr(start)->type == IR_VALUE) {
		struct IRNodeValue *value = (void *)graphNodeIRValuePtr(start);
		switch (value->val.type) {
		case __IR_VAL_MEM_FRAME: {
			if (getCurrentArch() == ARCH_TEST_SYSV || getCurrentArch() == ARCH_X86_SYSV || getCurrentArch() == ARCH_X64_SYSV) {
				return X86AddrModeIndirSIB(0, NULL, X86AddrModeReg(basePointer()), X86AddrModeSint(-value->val.value.__frame.offset), IRNodeType(start));
			} else {
				assert(0); // TODO  implement
			}
		}
		case __IR_VAL_MEM_GLOBAL: {
				__auto_type name=parserGetGlobalSymLinkageName(value->val.value.__global.symbol->name);
			struct X86AddressingMode *mode = X86AddrModeIndirLabel(name, value->val.value.__global.symbol->type);
			return mode;
		}
		case __IR_VAL_LABEL: {
			graphNodeIR label = value->val.value.__label;
			const char *name = NULL;
			if (ptrMapLabelNamesGet(asmLabelNames, label)) {
				name = *ptrMapLabelNamesGet(asmLabelNames, label);
			} else {
				long count = snprintf(NULL, 0, "LBL%li", ++labelsCount);
				char *name2 = calloc(count + 1,1);
				sprintf(name2, "LBL%li", labelsCount);
				ptrMapLabelNamesAdd(asmLabelNames, label, strClone(name2));
				name = name2;
			}
			return X86AddrModeLabel(name);
		}
		case IR_VAL_REG: {
			return X86AddrModeReg(value->val.value.reg.reg);
		}
		case IR_VAL_VAR_REF: {
			fprintf(stderr, "CONVERT VARIABLE REFERENCES TO FRAME ADDRESSES!!!");
			assert(0);
		}
		case IR_VAL_FUNC: {
			return X86AddrModeFunc(value->val.value.func);
		}
		case IR_VAL_INT_LIT: {
			struct X86AddressingMode mode;
			mode.valueType = NULL;
			if (value->val.value.intLit.type == INT_SLONG) {
				return X86AddrModeSint(value->val.value.intLit.value.sLong);
			} else if (value->val.value.intLit.type == INT_ULONG) {
				return X86AddrModeSint(value->val.value.intLit.value.uLong);
			}
		}
		case IR_VAL_STR_LIT: {
			strChar strLab CLEANUP(strCharDestroy) = uniqueLabel("STR");
			__auto_type lab = X86EmitAsmStrLit((char*)value->val.value.strLit,__vecSize(value->val.value.strLit));
			lab->valueType = objectPtrCreate(&typeU8i);
			return lab;
		}
		case IR_VAL_FLT_LIT: {
			struct X86AddressingMode *encoded CLEANUP(X86AddrModeDestroy) = X86AddrModeUint(IEEE754Encode(value->val.value.fltLit));
			strChar fltLab CLEANUP(strCharDestroy) = uniqueLabel("FLT");
			struct X86AddressingMode *lab CLEANUP(X86AddrModeDestroy) = X86EmitAsmDU64(&encoded, 1);
			return X86AddrModeIndirLabel(lab->value.label, &typeF64);
		}
		}
	} else if (graphNodeIRValuePtr(start)->type == IR_LABEL) {
		return X86AddrModeLabel(getLabelName(start));
	} else {
		fprintf(stderr, "Accessing member requires pointer source.\n");
		abort();
	}
	assert(0);
	return X86AddrModeSint(-1);
}

struct X86AddressingMode *IRNode2AddrMode(graphNodeIR start) {
	struct IRNode *node = graphNodeIRValuePtr(start);
	__auto_type find = llIRAttrFind(node->attrs, IR_ATTR_ADDR_MODE, IRAttrGetPred);
	if (find)
		return X86AddrModeClone(((struct IRAttrAddrMode *)llIRAttrValuePtr(find))->mode);

	__auto_type retVal = __node2AddrMode(start);
	if (!retVal->valueType)
		retVal->valueType = IRNodeType(start);

	struct IRAttrAddrMode modeAttr;
	modeAttr.base.name = IR_ATTR_ADDR_MODE;
	modeAttr.base.destroy = IRAttrAddrModeDestroy;
	modeAttr.mode = X86AddrModeClone(retVal);
	IRAttrReplace(start, __llCreate(&modeAttr, sizeof(modeAttr)));
	return retVal;
}
static struct reg *destReg() {
	switch (getCurrentArch()) {
	case ARCH_X64_SYSV:
		return &regAMD64RDI;
	case ARCH_TEST_SYSV:
	case ARCH_X86_SYSV:
		return &regX86EDI;
	}
}
static struct reg *sourceReg() {
	switch (getCurrentArch()) {
	case ARCH_X64_SYSV:
		return &regAMD64RSI;
	case ARCH_TEST_SYSV:
	case ARCH_X86_SYSV:
		return &regX86ESI;
	}
}
static int isX87FltReg(const struct reg *r) {
	const struct reg *regs[] = {
	    &regX86ST0, &regX86ST1, &regX86ST2, &regX86ST3, &regX86ST4, &regX86ST5, &regX86ST6, &regX86ST7,
	};
	for (long i = 0; i != sizeof(regs) / sizeof(*regs); i++)
		if (regs[i] == r)
			return 1;
	return 0;
}
static int isGPReg(const struct reg *r) {
	const struct reg *regs[] = {
	    &regAMD64RAX,   &regAMD64RBX,   &regAMD64RCX,    &regAMD64RDX,    &regAMD64RSP,    &regAMD64RBP,    &regAMD64RSI,    &regAMD64RDI,
	    &regAMD64R8u64, &regAMD64R9u64, &regAMD64R10u64, &regAMD64R11u64, &regAMD64R12u64, &regAMD64R13u64, &regAMD64R14u64, &regAMD64R15u64,
	};
	struct regSlice rSlice;
	rSlice.reg = (void *)r;
	rSlice.offset = 0;
	rSlice.type = NULL;
	rSlice.widthInBits = r->size * 8;
	for (long i = 0; i != sizeof(regs) / sizeof(*regs); i++) {
		struct regSlice rSlice2;
		rSlice2.reg = (void *)regs[i];
		rSlice2.offset = 0;
		rSlice2.type = NULL;
		rSlice2.widthInBits = regs[i]->size * 8;
		if (regSliceConflict(&rSlice, &rSlice2))
			return 1;
	}
	return 0;
}
static int isFuncEnd(const struct __graphNode *node, graphNodeIR end) {
	return node == end;
}
void consumeRegister(struct reg *reg) {
	consumedRegisters = strRegPSortedInsert(consumedRegisters, reg, (regCmpType)ptrPtrCmp);
}
static strRegP regsFromMode(struct X86AddressingMode *mode) {
	if (mode->type == X86ADDRMODE_REG) {
		return strRegPAppendItem(NULL, mode->value.reg);
	} else if (mode->type == X86ADDRMODE_MEM) {
		if (mode->value.m.type == x86ADDR_INDIR_REG) {
			return strRegPAppendItem(NULL, mode->value.m.value.indirReg);
		} else if (mode->value.m.type == x86ADDR_INDIR_SIB) {
			strRegP retVal = NULL;
			if (mode->value.m.value.sib.base)
				retVal = regsFromMode(mode->value.m.value.sib.base);
			if (mode->value.m.value.sib.index) {
				__auto_type tmp = strRegPSetUnion(regsFromMode(mode->value.m.value.sib.index), retVal, (regCmpType)ptrPtrCmp);
				strRegPDestroy(&retVal);
				retVal = tmp;
			}
			return retVal;
		}
	}
	return NULL;
}
static void consumeRegFromMode(struct X86AddressingMode *mode) {
	if (mode->type == X86ADDRMODE_REG) {
		consumeRegister(mode->value.reg);
	} else if (mode->type == X86ADDRMODE_MEM) {
		if (mode->value.m.type == x86ADDR_INDIR_REG)
			consumeRegister(mode->value.m.value.indirReg);
		else if (mode->value.m.type == x86ADDR_INDIR_SIB) {
			if (mode->value.m.value.sib.base)
				consumeRegFromMode(mode->value.m.value.sib.base);
			if (mode->value.m.value.sib.index)
				consumeRegFromMode(mode->value.m.value.sib.index);
		}
	}
}
void unconsumeRegister(struct reg *reg) {
	__auto_type find = strRegPSortedFind(consumedRegisters, reg, (regCmpType)ptrPtrCmp);
	strRegP tmp CLEANUP(strRegPDestroy) = strRegPAppendItem(NULL, reg);
	consumedRegisters = strRegPSetDifference(consumedRegisters, tmp, (regCmpType)ptrPtrCmp);
}
static void unconsumeRegFromMode(struct X86AddressingMode *mode) {
	if (mode->type == X86ADDRMODE_REG)
		unconsumeRegister(mode->value.reg);
	else if (mode->type == X86ADDRMODE_MEM) {
		if (mode->value.m.type == x86ADDR_INDIR_REG)
			consumeRegister(mode->value.m.value.indirReg);
		else if (mode->value.m.type == x86ADDR_INDIR_SIB) {
			if (mode->value.m.value.sib.base)
				unconsumeRegFromMode(mode->value.m.value.sib.base);
			if (mode->value.m.value.sib.index)
				unconsumeRegFromMode(mode->value.m.value.sib.index);
		}
	}
}
static void __unconsumeRegFromModeDestroy(struct X86AddressingMode **mode) {
	unconsumeRegFromMode(mode[0]);
	X86AddrModeDestroy(mode);
}
// https://mort.coffee/home/obscure-c-features/
#define CONCAT_(a, b) a##b
#define CONCAT(a, b) CONCAT_(a, b)
#define UNINAME CONCAT($, __COUNTER__)
#define __AUTO_LOCK_MODE_REGS(name, mode)                                                                                                                          \
	struct X86AddressingMode *name CLEANUP(__unconsumeRegFromModeDestroy) = X86AddrModeClone(mode);                                                                  \
	consumeRegFromMode(name);
#define AUTO_LOCK_MODE_REGS(mode) __AUTO_LOCK_MODE_REGS(UNINAME, (mode))
static int regConflictsWithOtherRegs(strRegP regs,struct reg* r) {
		for(long R=0;R!=strRegPSize(regs);R++)
				if(regConflict(regs[R], r))
						return 1;
		return 0;
}
static int ifInLive(const void *livenessInfo,const struct reg **r) {
		const struct IRAttrABIInfo *live=livenessInfo;
		for(long c=0;c!=strRegPSize(consumedRegisters);c++)
				if(regConflict(consumedRegisters[c], (struct reg*)*r))
						return 1;
		if(!live)
				return 0;
		
		for(long i=0;i!=strRegPSize(live->liveIn);i++) {
				if(regConflict(live->liveIn[i], (struct reg*)*r))
						return 1;
		}

		for(long o=0;o!=strRegPSize(live->liveOut);o++) {
						if(regConflict(live->liveOut[o], (struct reg*)*r))
						return 1;
		}
		
		return 0;
}
strRegP deadRegsAtPoint(graphNodeIR atNode,struct object *type) {
		if(!atNode)
				return NULL;
		
		strRegP regs=regGetForType(type);
		struct IRAttrABIInfo *livenessInfo=NULL;
		if(atNode) {
				__auto_type find=llIRAttrFind(graphNodeIRValuePtr(atNode)->attrs ,  IR_ATTR_ABI_INFO,IRAttrGetPred);
				if(find)
						livenessInfo=(void*)llIRAttrValuePtr(find);

				regs=strRegPRemoveIf(regs, livenessInfo, ifInLive);
		}
		regs=strRegPRemoveIf(regs, NULL, ifInLive);
		return regs;
}
static void assembleOpcode(graphNodeIR atNode,const char *name,strX86AddrMode args) {
		if(0==strcmp("MOV", name)) {
				if(args[0]->valueType)
				if(objectSize( args[0]->valueType,NULL)==8)
						abort();
		}
		long originalSize=strRegPSize(consumedRegisters);
		strX86AddrMode toPushPop CLEANUP(strX86AddrModeDestroy2)=NULL;
		strOpcodeTemplate opsByName CLEANUP(strOpcodeTemplateDestroy) = X86OpcodesByName(name);
		__auto_type template = X86OpcodeByArgs(name, args);
		if(template) {
				assembleInst(name, args);
				return;
		}

		for(long a=0;a!=strX86AddrModeSize(args);a++) {
				consumeRegFromMode(args[a]);
		}

		//
		// Cost is added if push/pop regiseter
		// or if making a temporary variable to push/pop on the stack
		//
		long templateCount=strOpcodeTemplateSize(opsByName);
		strLong costPerTemplate CLEANUP(strLongDestroy)=strLongResize(NULL, templateCount);
		strLong stackSizeTemplate CLEANUP(strLongDestroy)=strLongResize(NULL, templateCount);
		strLong templateIsValid CLEANUP(strLongDestroy)=strLongResize(NULL, templateCount);
		for(long t=0;t!=strOpcodeTemplateSize(opsByName);t++) {
				templateIsValid[t]=1;
				costPerTemplate[t]=0;
				stackSizeTemplate[t]=0;

				strRegP consumedRegs CLEANUP(strRegPDestroy)=NULL;
				for(long a=0;a!=strOpcodeTemplateArgSize(opsByName[t]->args);a++) {
						switch(args[a]->type) {
						case X86ADDRMODE_ITEM_ADDR:
						case X86ADDRMODE_SINT:
						case X86ADDRMODE_UINT:
						case X86ADDRMODE_LABEL:
						case X86ADDRMODE_STR: {
								if(args[a]->valueType)
										if(objectSize(args[a]->valueType,NULL)!=opcodeTemplateArgSize(opsByName[t]->args[a]))
												templateIsValid[t]=0;

								//Can be converted to memory or register
								long size=dataSize();
								if(args[a]->valueType)
										size=objectSize(args[a]->valueType, NULL);

								//Get list of dead registers,if has an availble dead register,add to consumed
								int pushPop=1;
								strRegP deadRegs CLEANUP(strRegPDestroy)=deadRegsAtPoint(atNode, getTypeForSize(size));
								if(strRegPSize(deadRegs)) {
										consumeRegister(deadRegs[0]);
										pushPop=0;
										consumedRegs=strRegPSortedInsert(consumedRegs, deadRegs[0], (regCmpType)ptrPtrCmp); 
								}
								
								switch(opsByName[t]->args[a].type) {
								case OPC_TEMPLATE_ARG_R8:
								case OPC_TEMPLATE_ARG_RM8:
										if(pushPop) {
										case OPC_TEMPLATE_ARG_M8:
												stackSizeTemplate[t]++;
										}
										costPerTemplate[t]++;
										if(size!=1) templateIsValid[t]=0;
										break;
								case OPC_TEMPLATE_ARG_R16:
								case OPC_TEMPLATE_ARG_RM16:
										if(pushPop) {
										case OPC_TEMPLATE_ARG_M16:
												stackSizeTemplate[t]+=2;
										}
										costPerTemplate[t]++;
										if(size!=2) templateIsValid[t]=0;
										break;
								case OPC_TEMPLATE_ARG_R32:
								case OPC_TEMPLATE_ARG_RM32:
										if(pushPop) {
										case OPC_TEMPLATE_ARG_M32:
												stackSizeTemplate[t]+=4;
										}
										costPerTemplate[t]++;
										if(size!=4) templateIsValid[t]=0;
										break;
								case OPC_TEMPLATE_ARG_R64:
								case OPC_TEMPLATE_ARG_RM64:
										if(pushPop) {
										case OPC_TEMPLATE_ARG_M64:
												stackSizeTemplate[t]+=8;
										}
										costPerTemplate[t]++;
										if(size!=8) templateIsValid[t]=0;
										break;
								default:
										templateIsValid[t]=0;
								}
								break;
						}
						case X86ADDRMODE_REG: {
								//Can be converted to memory
								long size=args[a]->value.reg->size;
								switch(opsByName[t]->args[a].type) {
								case OPC_TEMPLATE_ARG_M8:
										costPerTemplate[t]++;
										stackSizeTemplate[t]++;
								case OPC_TEMPLATE_ARG_R8:
								case OPC_TEMPLATE_ARG_RM8:
										if(size!=1) templateIsValid[t]=0;
										break;
								case OPC_TEMPLATE_ARG_M16:
										costPerTemplate[t]++;
										stackSizeTemplate[t]+=2;
								case OPC_TEMPLATE_ARG_R16:
								case OPC_TEMPLATE_ARG_RM16:
										if(size!=2) templateIsValid[t]=0;
										break;
								case OPC_TEMPLATE_ARG_M32:
										costPerTemplate[t]++;
										stackSizeTemplate[t]+=4;
								case OPC_TEMPLATE_ARG_R32:
								case OPC_TEMPLATE_ARG_RM32:
										if(size!=4) templateIsValid[t]=0;
										break;
								case OPC_TEMPLATE_ARG_M64:
										costPerTemplate[t]++;
										stackSizeTemplate[t]+=8;
								case OPC_TEMPLATE_ARG_R64:
								case OPC_TEMPLATE_ARG_RM64:
										if(size!=8) templateIsValid[t]=0;
										break;
								default:
										templateIsValid[t]=1;
								}
								break;
						}
						case X86ADDRMODE_MEM: {
								//Can be converted to register
								long size=dataSize();
								if(args[a]->valueType)
										size=objectSize(args[a]->valueType, NULL);

								//Get list of dead registers,if has an availble dead register,add to consumed
								int pushPop=1;
								strRegP deadRegs CLEANUP(strRegPDestroy)=deadRegsAtPoint(atNode, getTypeForSize(size));
								if(strRegPSize(deadRegs)) {
										consumeRegister(deadRegs[0]);
										pushPop=0;
										consumedRegs=strRegPSortedInsert(consumedRegs, deadRegs[0], (regCmpType)ptrPtrCmp); 
								}
								
								switch(opsByName[t]->args[a].type) {
								case OPC_TEMPLATE_ARG_R8:
										costPerTemplate[t]++;
										if(pushPop)
												stackSizeTemplate[t]++;
								case OPC_TEMPLATE_ARG_M8:
								case OPC_TEMPLATE_ARG_RM8:
										if(size!=1) templateIsValid[t]=0;
										break;
								case OPC_TEMPLATE_ARG_R16:
										costPerTemplate[t]++;
										if(pushPop)
												stackSizeTemplate[t]+=2;
								case OPC_TEMPLATE_ARG_M16:
								case OPC_TEMPLATE_ARG_RM16:
										if(size!=2) templateIsValid[t]=0;
										break;
								case OPC_TEMPLATE_ARG_R32:
										costPerTemplate[t]++;
										if(pushPop)
												stackSizeTemplate[t]+=4;
								case OPC_TEMPLATE_ARG_M32:
								case OPC_TEMPLATE_ARG_RM32:
										if(size!=4) templateIsValid[t]=0;
										break;
								case OPC_TEMPLATE_ARG_R64:
										costPerTemplate[t]++;
										if(pushPop)
												stackSizeTemplate[t]+=8;
								case OPC_TEMPLATE_ARG_M64:
								case OPC_TEMPLATE_ARG_RM64:
										if(size!=8) templateIsValid[t]=0;
										break;
								case OPC_TEMPLATE_ARG_STI:
										break;
								default:
										templateIsValid[t]=0;
								}
								break;
						}
						case X86ADDRMODE_FLT:
								assert(0);
						}
				}

				//Unconsume hypothetical consumed registers
				for(long c=0;c!=strRegPSize(consumedRegs);c++) {
						unconsumeRegister(consumedRegs[c]);
				}
				if(templateIsValid[t])
						break;
		}
		//Find index of lowest (valid) template
		long lowestValidI=-1;
		long lowestCost=LONG_MAX;
		for(long t=0;t!=strLongSize(templateIsValid);t++) {
				if(!templateIsValid[t])
						continue;
				if(costPerTemplate[t]<lowestCost) {
						lowestCost=costPerTemplate[t];
						lowestValidI=t;
				}else
						continue;
		}
		
		assert(lowestValidI!=-1);

		strLong argsToRestore CLEANUP(strLongDestroy)=NULL;
		
		strX86AddrMode args2 CLEANUP(strX86AddrModeDestroy2)=NULL;
		long stackOffset=stackSizeTemplate[lowestValidI];
		
		for(long a=0;a!=strOpcodeTemplateArgSize(opsByName[lowestValidI]->args);a++) {
				switch(args[a]->type) {
				case X86ADDRMODE_ITEM_ADDR:
				case X86ADDRMODE_SINT:
				case X86ADDRMODE_UINT:
				case X86ADDRMODE_LABEL:
				case X86ADDRMODE_STR: {
						if(args[a]->type==opsByName[lowestValidI]->args[a].type) {
								args2=strX86AddrModeAppendItem(args2, X86AddrModeClone(args[a]));
						} else {										
								//Can be converted to memory or register
								long size=dataSize();
								if(args[a]->valueType)
										size=objectSize(args[a]->valueType, NULL);

								//Get list of dead registers,if has an availble dead register,add to consumed
								struct reg* reg=NULL;
								int pushPop=0;
								strRegP deadRegs CLEANUP(strRegPDestroy)=deadRegsAtPoint(atNode, getTypeForSize(size));
								if(strRegPSize(deadRegs)) {
										reg=deadRegs[0];
								} else {
										reg=regForTypeExcludingConsumed(getTypeForSize(size));
										pushPop=1;
								}
								
								switch(opsByName[lowestValidI]->args[a].type) {
								case OPC_TEMPLATE_ARG_R8:
								case OPC_TEMPLATE_ARG_RM8:
								case OPC_TEMPLATE_ARG_R16:
								case OPC_TEMPLATE_ARG_RM16:
								case OPC_TEMPLATE_ARG_R32:
								case OPC_TEMPLATE_ARG_RM32:
								case OPC_TEMPLATE_ARG_R64:
								case OPC_TEMPLATE_ARG_RM64:;
										__auto_type mode=X86AddrModeReg(reg);
								if(pushPop) {
										//Push
										toPushPop=strX86AddrModeAppendItem(toPushPop,X86AddrModeReg(reg));
										pushMode(mode);
										stackOffset-=size;
								}
								asmAssign(mode, args[a], size, ASM_ASSIGN_X87FPU_POP);
								mode->valueType=getTypeForSize(size);
								args2=strX86AddrModeAppendItem(args2, mode);
								break;
								case OPC_TEMPLATE_ARG_M8:
								case OPC_TEMPLATE_ARG_M16:
								case OPC_TEMPLATE_ARG_M32:
								case OPC_TEMPLATE_ARG_M64:; {
										//Stack grows down
										stackOffset-=size;
										__auto_type mode=X86AddrModeIndirSIB(0, NULL, X86AddrModeReg(stackPointer()), X86AddrModeSint(stackOffset), getTypeForSize(size));
										pushMode(args[a]);
										args2=strX86AddrModeAppendItem(args2, mode);
								}
								break;
								default:
												abort();
								}
						}
						break;
				}
				case X86ADDRMODE_REG: {
						//Can be converted to memory
						long size=args[a]->value.reg->size;
						switch(opsByName[lowestValidI]->args[a].type) {
						case OPC_TEMPLATE_ARG_M8:
						case OPC_TEMPLATE_ARG_M16:				
						case OPC_TEMPLATE_ARG_M32:
						case OPC_TEMPLATE_ARG_M64:;
								argsToRestore=strLongAppendItem(argsToRestore, a);
								//Stack grows down
								stackOffset-=size;
								__auto_type mode=X86AddrModeIndirSIB(0, NULL, X86AddrModeReg(stackPointer()), X86AddrModeSint(stackOffset), getTypeForSize(size));
								pushMode(args[a]);
								toPushPop=strX86AddrModeAppendItem(toPushPop, X86AddrModeClone(args[a]));
								args2=strX86AddrModeAppendItem(args2, mode);
								break;
						case OPC_TEMPLATE_ARG_R8:
						case OPC_TEMPLATE_ARG_RM8:
						case OPC_TEMPLATE_ARG_R16:
						case OPC_TEMPLATE_ARG_RM16:
						case OPC_TEMPLATE_ARG_R32:
						case OPC_TEMPLATE_ARG_RM32:
						case OPC_TEMPLATE_ARG_R64:
						case OPC_TEMPLATE_ARG_RM64:
								args2=strX86AddrModeAppendItem(args2, X86AddrModeClone(args[a]));
								break;
						default:
								assert(0);
						}
						break;
				}
				case X86ADDRMODE_MEM: {
						//Can be converted to register
						long size=dataSize();
						if(args[a]->valueType)
								size=objectSize(args[a]->valueType, NULL);

						struct reg* reg=NULL;
						int pushPop=0;
						strRegP deadRegs CLEANUP(strRegPDestroy)=deadRegsAtPoint(atNode, getTypeForSize(size));
						if(strRegPSize(deadRegs)) {
								reg=deadRegs[0];
						} else {
								reg=regForTypeExcludingConsumed(getTypeForSize(size));
								pushPop=1;
						}
						
						switch(opsByName[lowestValidI]->args[a].type) {
						case OPC_TEMPLATE_ARG_R8:
						case OPC_TEMPLATE_ARG_R16:
						case OPC_TEMPLATE_ARG_R32:
						case OPC_TEMPLATE_ARG_R64:;
								__auto_type mode=X86AddrModeReg(reg);

								if(pushPop) {
										//Push
										toPushPop=strX86AddrModeAppendItem(toPushPop,X86AddrModeReg(reg));
										pushMode(mode);
										stackOffset-=size;
								}
								
								asmAssign(mode, args[a], size, ASM_ASSIGN_X87FPU_POP);

								mode->valueType=getTypeForSize(size);
								args2=strX86AddrModeAppendItem(args2, mode);
								argsToRestore=strLongAppendItem(argsToRestore, a);
								break;
						case OPC_TEMPLATE_ARG_M8:
						case OPC_TEMPLATE_ARG_RM8:
						case OPC_TEMPLATE_ARG_M16:
						case OPC_TEMPLATE_ARG_RM16:
						case OPC_TEMPLATE_ARG_M32:
						case OPC_TEMPLATE_ARG_RM32:
						case OPC_TEMPLATE_ARG_M64:
						case OPC_TEMPLATE_ARG_RM64:

								args2=strX86AddrModeAppendItem(args2, X86AddrModeClone(args[a]));
								break;
						case OPC_TEMPLATE_ARG_STI: {
								__auto_type mode=X86AddrModeReg(&regX86ST0);
								mode->valueType=&typeF64;
								args2=strX86AddrModeAppendItem(args2, mode);
								asmTypecastAssign(args2[strX86AddrModeSize(args2)-1], args[a], ASM_ASSIGN_X87FPU_POP);
								break;
						}
						default:
								assert(0);
						}
						break;
				}
				case X86ADDRMODE_FLT:
						assert(0);
				}
				consumeRegFromMode(args2[a]);
		}
		assembleInst(name, args2);

		for(long a=0;a!=strX86AddrModeSize(args);a++)
				unconsumeRegFromMode(args[a]);
		for(long a=0;a!=strX86AddrModeSize(args2);a++)
				unconsumeRegFromMode(args2[a]);

		for(long a=0;a!=strLongSize(argsToRestore);a++) {
				long i=argsToRestore[a];
				asmAssign(args[i], args2[i],objectSize(args2[i]->valueType, NULL) , ASM_ASSIGN_X87FPU_POP);
		}
		
		for(long p=strX86AddrModeSize(toPushPop)-1;p>=0;p--)
				popMode(toPushPop[p]);

		long endSize=strRegPSize(consumedRegisters);
		if(endSize!=originalSize) {
				printf("%li,%li\n",originalSize,endSize);
		}
		assert(originalSize==endSize);
}

static strGraphNodeIRP getFuncNodes(graphNodeIR startN) {
	struct IRNodeFuncStart *start = (void *)graphNodeIRValuePtr(startN);
	strGraphEdgeP allEdges CLEANUP(strGraphEdgePDestroy) = graphAllEdgesBetween(startN, start->end, (int (*)(const struct __graphNode *, const void *))isFuncEnd);
	strGraphNodeIRP allNodes = NULL;
	for (long i = 0; i != strGraphEdgePSize(allEdges); i++) {
		strGraphNodeIRP frontBack = strGraphNodeIRPAppendItem(NULL, graphEdgeIRIncoming(allEdges[i]));
		frontBack = strGraphNodeIRPSortedInsert(frontBack, graphEdgeIROutgoing(allEdges[i]), (gnCmpType)ptrPtrCmp);
		allNodes = strGraphNodeIRPSetUnion(allNodes, frontBack, (gnCmpType)ptrPtrCmp);
	}
	return allNodes;
}
STR_TYPE_DEF(struct parserVar *, PVar);
STR_TYPE_FUNCS(struct parserVar *, PVar);
typedef int (*PVarCmpType)(const struct parserVar **, const struct parserVar **);
static int isPrimitiveType(const struct object *obj) {
	obj = objectBaseType(obj);
	if (obj->type == TYPE_PTR)
		return 1;

	const struct object *prims[] = {
	    &typeU0, &typeU8i, &typeU16i, &typeU32i, &typeU64i, &typeI8i, &typeI16i, &typeI32i, &typeI64i, &typeF64,
	};
	for (long i = 0; i != sizeof(prims) / sizeof(*prims); i++) {
		if (prims[i] == obj)
			return 1;
	}
	return 0;
}
static int isNotNoreg(const struct parserVar *var, const void *data) {
	return !var->isNoreg;
}
static int frameEntryCmp(const void *a, const void *b) {
	const struct frameEntry *A = a;
	const struct frameEntry *B = b;
	return IRVarCmp(&A->var, &B->var);
}
static void debugShowGraphIR(graphNodeIR enter) {
		return;
#if DEBUG_PRINT_ENABLE
		const char *name = tmpnam(NULL);
	__auto_type map = graphNodeCreateMapping(enter, 1);
	IRGraphMap2GraphViz(map, "viz", name, NULL, NULL, NULL, NULL);
	char buffer[1024];
	sprintf(buffer, "sleep 0.1 &&dot -Tsvg %s > /tmp/dot.svg && firefox /tmp/dot.svg & ", name);

	system(buffer);
	#endif
}
static strGraphNodeIRP insertLabelsForAsm(strGraphNodeIRP nodes) {
	strGraphNodeIRP inserted = NULL;
	insertLabelsForAsmCalled = 1;
	for (long i = 0; i != strGraphNodeIRPSize(nodes); i++) {
		__auto_type node = nodes[i];
		if(graphNodeIRValuePtr(node)->type==IR_LABEL)
				continue;
		
		strGraphEdgeIRP in CLEANUP(strGraphEdgeIRPDestroy) = graphNodeIRIncoming(node);
		strGraphEdgeIRP inFlow CLEANUP(strGraphEdgeIRPDestroy) = IRGetConnsOfType(in, IR_CONN_FLOW);
		if (strGraphEdgeIRPSize(inFlow) > 1) {
		insertLabel:
			if (graphNodeIRValuePtr(node)->type == IR_LABEL)
				continue;
			__auto_type new = IRCreateLabel();
			IRInsertBefore(node, new, new, IR_CONN_FLOW);
			inserted = strGraphNodeIRPSortedInsert(inserted, new, (gnCmpType)ptrPtrCmp);
			continue;
		}
		for (long i = 0; i != strGraphEdgeIRPSize(in); i++) {
			switch (*graphEdgeIRValuePtr(in[i])) {
			case IR_CONN_CASE:
			case IR_CONN_COND_TRUE:
			case IR_CONN_COND_FALSE:
				goto insertLabel;
			default:;
			}
		}
	}
	return inserted;
}
static int isIntType(struct object *obj) {
	const struct object *intTypes[] = {
	    &typeI8i, &typeI16i, &typeI32i, &typeI64i, &typeU8i, &typeU16i, &typeU32i, &typeU64i,
	};
	for (long i = 0; i != sizeof(intTypes) / sizeof(*intTypes); i++)
		if (objectBaseType(obj) == intTypes[i])
			return 1;
	return 0;
}
static int typeIsSigned(struct object *obj) {
	const struct object *signedTypes[] = {
	    &typeI8i, &typeI16i, &typeI32i, &typeI64i, &typeF64,
	};
	for (int i = 0; i != sizeof(signedTypes) / sizeof(*signedTypes); i++) {
		if (objectBaseType(obj) == signedTypes[i])
			return 1;
	}
	return 0;
}
static struct X86AddressingMode *__mem2SIB(struct X86AddressingMode *a) {
		struct X86AddressingMode *aSIB =NULL;
		if(a->type==X86ADDRMODE_MEM) {
				if(a->value.m.type==x86ADDR_INDIR_LABEL)
						aSIB=X86AddrModeIndirSIB(0, NULL, X86AddrModeClone(a->value.m.value.label), NULL, a->valueType);
				else if(a->value.m.type==x86ADDR_INDIR_REG)
						aSIB=X86AddrModeIndirSIB(0, NULL, X86AddrModeReg(a->value.m.value.indirReg), NULL, a->valueType);
				else if(a->value.m.type==x86ADDR_INDIR_SIB)
						aSIB=X86AddrModeClone(a);
				else if(a->value.m.type==x86ADDR_MEM)
						aSIB=X86AddrModeIndirSIB(0, NULL, X86AddrModeUint(a->value.m.value.mem), 0, a->valueType);
				aSIB->value.m.segment=a->value.m.segment;
				return aSIB;
		} else if(a->type==X86ADDRMODE_REG) {
				aSIB=X86AddrModeIndirSIB(0, NULL, X86AddrModeClone(a), NULL, a->valueType);
				return aSIB;
		} else {
				fputs("Large assign must point to memory.", stderr);
				abort();
		}
		return NULL;
}
static int isFltType(struct object *obj);
void asmAssign(struct X86AddressingMode *a, struct X86AddressingMode *b, long size,enum asmAssignFlags flags) {
		if (a->type == X86ADDRMODE_REG) {
		if (isX87FltReg(a->value.reg)) {
				switch(b->type) {
				case X86ADDRMODE_FLT:
				case X86ADDRMODE_STR: {
						fputs("Can't assign this into floating point.\n", stderr);
						abort();
				}
				case X86ADDRMODE_ITEM_ADDR:
				case X86ADDRMODE_LABEL:
				case X86ADDRMODE_MEM: {
						int intLoad=isIntType(b->valueType);
						strX86AddrMode fldArgs CLEANUP(strX86AddrModeDestroy2)=NULL;
						fldArgs=strX86AddrModeAppendItem(fldArgs, X86AddrModeClone(b));
						const char *op=NULL;
						if(intLoad)
								op="FILD";
						else
								op="FLD";
						assembleOpcode(NULL,op,fldArgs);
						return ;
				}
				case X86ADDRMODE_REG: {
						if(isX87FltReg(b->value.reg)) {
								strX86AddrMode fldArgs CLEANUP(strX86AddrModeDestroy2)=NULL;
								fldArgs=strX86AddrModeAppendItem(fldArgs, X86AddrModeClone(b));
								const char *op="FLD";
								assembleOpcode(NULL,op,fldArgs);
								if(flags&ASM_ASSIGN_X87FPU_POP) {
										strX86AddrMode subArgs CLEANUP(strX86AddrModeDestroy2)=NULL;
										subArgs=strX86AddrModeAppendItem(subArgs, X86AddrModeReg(&regX86ST0));
										assembleOpcode(NULL, "FSTP", subArgs);
								}
						} else  {
								pushMode(b);
								strX86AddrMode fldArgs CLEANUP(strX86AddrModeDestroy2)=NULL;
								__auto_type bLoc=X86AddrModeIndirSIB(0, NULL, X86AddrModeReg(stackPointer()), X86AddrModeSint(size), &typeF64);
								fldArgs=strX86AddrModeAppendItem(fldArgs, bLoc);
								const char *op="FILD";
								assembleOpcode(NULL,op, fldArgs);
								popMode(b);
						}
						return;
				}
				case X86ADDRMODE_SINT:
				case X86ADDRMODE_UINT:		{
						strX86AddrMode  modes CLEANUP(strX86AddrModeDestroy2)=NULL;
						modes=strX86AddrModeAppendItem(modes, X86AddrModeClone(b));
						__auto_type mem= X86EmitAsmDU64(modes, 1);

						strX86AddrMode fildArgs CLEANUP(strX86AddrModeDestroy2)=NULL;
						fildArgs=strX86AddrModeAppendItem(fildArgs, X86AddrModeIndirSIB(0,NULL, mem, NULL, &typeI64i));
						const char *op="FILD";
						assembleOpcode(NULL,op, fildArgs);
						return;
				}
				}
		}
	}
	if (b->type == X86ADDRMODE_REG) {
			if (isX87FltReg(b->value.reg)){
					if(a->type==X86ADDRMODE_REG) {
							if(isX87FltReg(a->value.reg)) {
									assert(0);
							} else {
									pushMode(a);
									strX86AddrMode fistArgs CLEANUP(strX86AddrModeDestroy2)=NULL;
									struct X86AddressingMode *aLoc CLEANUP(X86AddrModeDestroy)=X86AddrModeIndirSIB(0, NULL, X86AddrModeReg(stackPointer()), NULL, getTypeForSize(size));
									fistArgs=strX86AddrModeAppendItem(fistArgs, X86AddrModeClone(aLoc));
									const char *op=(flags&ASM_ASSIGN_X87FPU_POP)?"FISTP":"FISTP";
									assembleOpcode(NULL,op, fistArgs);
									popMode(a);
							}
					} else if(a->type==X86ADDRMODE_ITEM_ADDR||a->type==X86ADDRMODE_MEM) {
							if(objectBaseType(a->valueType)==&typeF64) {
									const char *op=(flags&ASM_ASSIGN_X87FPU_POP)?"FSTP":"FST";
									strX86AddrMode fistArgs CLEANUP(strX86AddrModeDestroy2)=strX86AddrModeAppendItem(NULL, X86AddrModeClone(a));
									assembleOpcode(NULL,op, fistArgs);
							} else {
									//Ensure destination is 16/32/64 bits
									if(objectSize(a->valueType, NULL)>1) {
											const char *op=(flags&ASM_ASSIGN_X87FPU_POP)?"FISTP":"FISTP";
											strX86AddrMode fistArgs CLEANUP(strX86AddrModeDestroy2)=strX86AddrModeAppendItem(NULL, X86AddrModeClone(a));
											assembleOpcode(NULL,op, fistArgs);
									}  else {
											AUTO_LOCK_MODE_REGS(a);
											__auto_type toType=typeIsSigned(a->valueType)?&typeI16i:&typeU16i;
											__auto_type avail=regForTypeExcludingConsumed(toType);
											struct X86AddressingMode *availMode CLEANUP(X86AddrModeDestroy)=X86AddrModeReg(avail);
											availMode->valueType=toType;
											asmAssign(availMode, b, 2, flags);
											asmTypecastAssign(a, availMode, 0);
									}
							}
					} else {
							fputs("Can't assign floating point into this\n", stderr);
							abort();
					}
					return;
			}
	} else if(b->valueType) {
			if(isFltType(b->valueType)) {
					struct X86AddressingMode *st0Mode CLEANUP(X86AddrModeDestroy)=X86AddrModeReg(&regX86ST0);
					asmAssign(st0Mode, b, 8, flags);
					asmAssign(a, st0Mode, size, 0);
					return;
			}
	}
	strX86AddrMode args CLEANUP(strX86AddrModeDestroy) = NULL;
	strOpcodeTemplate ops CLEANUP(strOpcodeTemplateDestroy) = NULL;
	if (size == 1 || size == 2 || size == 4 || size == 8) {
		args = strX86AddrModeAppendItem(args, a);
		args = strX86AddrModeAppendItem(args, b);
	}

	if(a->valueType&&b->valueType)
	if(isFltType(a->valueType)&&isFltType(b->valueType)) {
			struct X86AddressingMode *st0Mode CLEANUP(X86AddrModeDestroy)=X86AddrModeReg(&regX86ST0);
			asmAssign(st0Mode, b, 8, flags);
			asmAssign(a, st0Mode, 8, ASM_ASSIGN_X87FPU_POP);
			return;
	}
	
	if (size == 1 || size == 2 || size == 4 || size == 8) {
		if (a->type == X86ADDRMODE_MEM) {
			if (b->type == X86ADDRMODE_MEM) {
					if(size>dataSize()) {
							goto memcpy;
					}
			}
		}
		if(args[0]->valueType) {
				__auto_type base=objectBaseType(args[0]->valueType);
				if(base->type==TYPE_CLASS||base->type==TYPE_UNION)
						goto memcpy;
		}
		if(args[1]->valueType) {
				__auto_type base=objectBaseType(args[1]->valueType);
				if(base->type==TYPE_CLASS||base->type==TYPE_UNION)
						goto memcpy;
		}

		assert(size!=8);
		assembleOpcode(NULL,"MOV",args);
		return;
	} else {
	memcpy:;
			struct X86AddressingMode *aSIB CLEANUP(X86AddrModeDestroy)=__mem2SIB(a);
			struct X86AddressingMode *bSIB CLEANUP(X86AddrModeDestroy)=__mem2SIB(b);
			for(;size!=0;) {
					long chunk=0;
					if(size>=8&&dataSize()>=8) {
							chunk=8;
					} else if(size>=4) {
							chunk=4;
					} else if(size==2) {
							chunk=2;
					} else
							chunk=1;
					
					aSIB->valueType=getTypeForSize(chunk);
					bSIB->valueType=getTypeForSize(chunk);
					asmAssign(aSIB, bSIB, chunk, 0);
					X86AddrModeIndirSIBAddOffset(aSIB, chunk);
					X86AddrModeIndirSIBAddOffset(bSIB, chunk);
					size-=chunk;
			}
	}
}
void asmAssignFromPtr(struct X86AddressingMode *a,struct X86AddressingMode *b,long size,enum asmAssignFlags flags) {
		struct X86AddressingMode *sib CLEANUP(X86AddrModeDestroy)=__mem2SIB(b);
		asmAssign(a, sib, size, flags);
}
struct IRVar2WeightAssoc {
		double weight;
		struct IRVar var;
};
STR_TYPE_DEF(struct IRVar2WeightAssoc,IRVar2WeightAssoc);
STR_TYPE_FUNCS(struct IRVar2WeightAssoc,IRVar2WeightAssoc);
static int IRVar2WeightAssocCmp(const struct IRVar2WeightAssoc *a,const struct IRVar2WeightAssoc *b) {
		return IRVarCmp(&a->var, &b->var);
}
static double __var2Weight(struct IRVar *var,void *data) {
		strIRVar2WeightAssoc assoc=data;
		struct IRVar2WeightAssoc dummy;
		dummy.var=*var;
		__auto_type find=strIRVar2WeightAssocSortedFind(assoc, dummy, IRVar2WeightAssocCmp);
		assert(find);
		return find->weight;
}
static strIRVar2WeightAssoc computeVarWeights(graphNodeIR start) {
		strIRVar2WeightAssoc retVal=NULL;

		strGraphNodeIRP allNodes CLEANUP(strGraphNodeIRPDestroy)=graphNodeIRAllNodes(start);
		for(long n=0;n!=strGraphNodeIRPSize(allNodes);n++) {
				if(graphNodeIRValuePtr(allNodes[n])->type==IR_VALUE) {
						struct IRNodeValue *val=(void*)graphNodeIRValuePtr(allNodes[n]);
						if(val->val.type!=IR_VAL_VAR_REF)
								continue;
				registerLoop:;
						struct IRVar2WeightAssoc dummy;
						dummy.var=val->val.value.var;
						__auto_type find=strIRVar2WeightAssocSortedFind(retVal, dummy, IRVar2WeightAssocCmp);
						if(!find) {
								dummy.weight=0;
								retVal=strIRVar2WeightAssocSortedInsert(retVal, dummy, IRVar2WeightAssocCmp);
								goto registerLoop;
						}
						find->weight+=1.0;
				}
		}
		return retVal;
}
void IRCompile(graphNodeIR start, int isFunc) {
		//debugShowGraphIR(start);
		if (isFunc) {
		struct IRNodeFuncStart *funcNode = (void *)graphNodeIRValuePtr(start);
		X86EmitAsmLabel(funcNode->func->name);
	}

	__auto_type originalStart = start;
	__auto_type entry = IRCreateLabel();
	graphNodeIRConnect(entry, start, IR_CONN_FLOW);
	start = entry;

	strGraphNodeIRP nodes CLEANUP(strGraphNodeIRPDestroy) = graphNodeIRAllNodes(start);
	{
		strGraphNodeIRP removed CLEANUP(strGraphNodeIRPDestroy) = removeNeedlessLabels(start);
		nodes = strGraphNodeIRPSetDifference(nodes, removed, (gnCmpType)ptrPtrCmp);
		}

	strGraphNodeIRP funcsWithin CLEANUP(strGraphNodeIRPDestroy) = NULL;
	// Get list of function nodes,then we remove them from the main
	for (long n = 0; n != strGraphNodeIRPSize(nodes); n++) {
		if (nodes[n] == originalStart && isFunc)
			continue;
		struct IRNodeFuncStart *start = (void *)graphNodeIRValuePtr(nodes[n]);
		if (start->base.type != IR_FUNC_START)
			continue;
		// Make a "transparent" label to route all traffic in/out of the function's "space" in the graph(pretend the function never existed and compile it later)
		__auto_type lab = IRCreateLabel();
		IRInsertBefore(nodes[n], lab, lab, IR_CONN_FLOW);

		// Kill all incoming and outgoing nodes to isolate the function
		strGraphNodeIRP in CLEANUP(strGraphNodeIRPDestroy) = graphNodeIRIncomingNodes(nodes[n]);
		for (long i = 0; i != strGraphNodeIRPSize(in); i++)
			graphEdgeIRKill(in[i], nodes[n], NULL, NULL, NULL);
		strGraphNodeIRP out CLEANUP(strGraphNodeIRPDestroy) = graphNodeIROutgoingNodes(start->end);
		for (long o = 0; o != strGraphNodeIRPSize(out); o++) {
			graphNodeIRConnect(lab, out[o], IR_CONN_FLOW);
			graphEdgeIRKill(start->end, out[o], NULL, NULL, NULL);
		}

		funcsWithin = strGraphNodeIRPSortedInsert(funcsWithin, nodes[n], (gnCmpType)ptrPtrCmp);
	}
	strGraphNodeIRPDestroy(&nodes);

	IRRemoveNeverFlows(start);
	IRInsertImplicitTypecasts(start);
	
	nodes = graphNodeIRAllNodes(start);

	// If start is a function-start,make a list of arguments and store them into variables,replace references to function argument node with variable

	// "Push" asmFuncArgVars
	__auto_type oldAsmFuncArgVars = asmFuncArgVars;

	// Get list of variables that will always be stored in memory
	// - variables that are referenced by ptr
	// - Classes/unions with primitive base that have members references(I64.u8[1] etc)
	// - Vars marked as noreg
	strPVar noregs CLEANUP(strPVarDestroy) = NULL;
	strPVar inRegs CLEANUP(strPVarDestroy) = NULL;
	for (long i = 0; i != strGraphNodeIRPSize(nodes); i++) {
		struct IRNodeValue *value = (void *)graphNodeIRValuePtr(nodes[i]);
		if (value->base.type != IR_VALUE)
			continue;
		if (value->val.type != IR_VAL_VAR_REF)
			continue;

		strGraphNodeIRP out CLEANUP(strGraphNodeIRPDestroy) = graphNodeIROutgoingNodes(nodes[i]);

		if (value->val.value.var.var->isGlobal)
			goto markAsNoreg;

		if (!isPrimitiveType(objectBaseType(IRNodeType(nodes[i]))))
			goto markAsNoreg;

		if (strGraphNodeIRPSize(out) == 1) {
				if (graphNodeIRValuePtr(out[0])->type == IR_ADDR_OF||graphNodeIRValuePtr(out[0])->type == IR_MEMBERS_ADDR_OF) {
						value->val.value.var.var->isRefedByPtr=1;
						goto markAsNoreg;
				}
				
			if (isPrimitiveType(objectBaseType(IRNodeType(nodes[i]))))
				if (graphNodeIRValuePtr(out[0])->type == IR_MEMBERS)
					goto markAsNoreg;
		}

		if (value->val.value.var.var->isNoreg)
			goto markAsNoreg;

		__auto_type var = value->val.value.var.var;
		if (!strPVarSortedFind(inRegs, var, (PVarCmpType)ptrPtrCmp))
			inRegs = strPVarSortedInsert(inRegs, var, (PVarCmpType)ptrPtrCmp);
		continue;
	markAsNoreg : {
		__auto_type var = value->val.value.var.var;
		inRegs=strPVarRemoveItem(inRegs, var, (PVarCmpType)ptrPtrCmp);
		if (!strPVarSortedFind(noregs, var, (PVarCmpType)ptrPtrCmp))
			noregs = strPVarSortedInsert(noregs, var, (PVarCmpType)ptrPtrCmp);
	}
	}
	for (long i = 0; i != strPVarSize(noregs); i++)
		noregs[i]->isNoreg = 1;

	//
	// ABI section requires args to be converted to variables for register allocator.
	// This will also load the function arguments into the variables.
	//
	if (isFunc) {
		IRABIInsertLoadArgs(originalStart);
	}

	//SYSV-i386 uses X87 registers for values,so if SYSV-i386 ABI,use a special register allocator for floating points
	int allocateX87fpuRegs=0;
	switch(getCurrentArch()) {
	case ARCH_TEST_SYSV:
	case ARCH_X86_SYSV:
	x87fpuRemoveLoop:
			for(long v=0;v!=strPVarSize(inRegs);v++) {
					if(objectBaseType(inRegs[v]->type)==&typeF64) {
							__auto_type var=inRegs[v];
							inRegs=strPVarRemoveItem(inRegs, var, (PVarCmpType)ptrPtrCmp);
							if (!strPVarSortedFind(noregs, var, (PVarCmpType)ptrPtrCmp))
									noregs = strPVarSortedInsert(noregs, var, (PVarCmpType)ptrPtrCmp);
							goto x87fpuRemoveLoop;
					}
			}
			allocateX87fpuRegs=1;
			break;
	case ARCH_X64_SYSV:;
	}

	IRInsertNodesBetweenExprs(start, NULL, NULL);

	strIRVar2WeightAssoc weights CLEANUP(strIRVar2WeightAssocDestroy)=computeVarWeights(start);
	IRRegisterAllocate(start, __var2Weight, weights, isNotNoreg, noregs);

	if(allocateX87fpuRegs) {
			IRRegisterAllocateX87(start);
	}
	
	strGraphNodeIRP regAllocedNodes CLEANUP(strGraphNodeIRPDestroy) = graphNodeIRAllNodes(start);
	{
		strGraphNodeIRP removedNodes CLEANUP(strGraphNodeIRPDestroy) = NULL;
		strGraphNodeIRP addedNodes CLEANUP(strGraphNodeIRPDestroy) = NULL;
		// Replace all spills/loads with variable,then we compute a frame layout
		for (long i = 0; i != strGraphNodeIRPSize(regAllocedNodes); i++) {
			if (graphNodeIRValuePtr(regAllocedNodes[i])->type == IR_SPILL_LOAD) {
				struct IRNodeSpill *spillLoad = (void *)graphNodeIRValuePtr(regAllocedNodes[i]);

				__auto_type var = spillLoad->item.value.var.var;
				strGraphNodeP toReplace CLEANUP(strGraphNodePDestroy) = strGraphNodePAppendItem(NULL, regAllocedNodes[i]);
				__auto_type newNode = IRCreateVarRef(var);
				graphReplaceWithNode(toReplace, newNode, NULL, (void (*)(void *))IRNodeDestroy, sizeof(enum IRConnType));

				removedNodes = strGraphNodeIRPSortedInsert(removedNodes, regAllocedNodes[i], (gnCmpType)ptrPtrCmp);
				addedNodes = strGraphNodeIRPSortedInsert(addedNodes, newNode, (gnCmpType)ptrPtrCmp);
			}
		}
		regAllocedNodes = strGraphNodeIRPSetDifference(regAllocedNodes, removedNodes, (gnCmpType)ptrPtrCmp);
		regAllocedNodes = strGraphNodeIRPSetUnion(regAllocedNodes, addedNodes, (gnCmpType)ptrPtrCmp);
	}

	//"Push" the old frame layout
	__auto_type oldOffsets = localVarFrameOffsets;
	long oldFrameSize=frameSize;
	
	// Frame allocate
	{
		strFrameEntry layout CLEANUP(strFrameEntryDestroy) = IRComputeFrameLayout(start, &frameSize);
		localVarFrameOffsets = ptrMapFrameOffsetCreate();
		for (long i = 0; i != strFrameEntrySize(layout); i++)
				ptrMapFrameOffsetAdd(localVarFrameOffsets, layout[i].var.var, layout[i].offset);

		strGraphNodeIRP removed CLEANUP(strGraphNodeIRPDestroy) = NULL;
		strGraphNodeIRP added CLEANUP(strGraphNodeIRPDestroy) = NULL;
		for (long n = 0; n != strGraphNodeIRPSize(regAllocedNodes); n++) {
			struct IRNodeValue *ir = (void *)graphNodeIRValuePtr(regAllocedNodes[n]);
			if (ir->base.type != IR_VALUE)
				continue;
			if (ir->val.type != IR_VAL_VAR_REF)
				continue;
			if (ir->val.value.var.var->isGlobal)
				continue;

			__auto_type find = ptrMapFrameOffsetGet(localVarFrameOffsets, ir->val.value.var.var);
			assert(find);
			removed = strGraphNodeIRPSortedInsert(removed, regAllocedNodes[n], (gnCmpType)ptrPtrCmp);
			__auto_type frameReference = IRCreateFrameAddress(*find, ir->val.value.var.var->type);
			added = strGraphNodeIRPSortedInsert(added, frameReference, (gnCmpType)ptrPtrCmp);

			strGraphNodeIRP dummy CLEANUP(strGraphNodeIRPDestroy) = strGraphNodeIRPAppendItem(NULL, regAllocedNodes[n]);
			graphIRReplaceNodes(dummy, frameReference, NULL, (void (*)(void *))IRNodeDestroy);
		}

		regAllocedNodes = strGraphNodeIRPSetDifference(regAllocedNodes, removed, (gnCmpType)ptrPtrCmp);
		regAllocedNodes = strGraphNodeIRPSetUnion(regAllocedNodes, added, (gnCmpType)ptrPtrCmp);
	}

	// Replace all global variables with
	{
		strGraphNodeIRP removed CLEANUP(strGraphNodeIRPDestroy) = NULL;
		strGraphNodeIRP added CLEANUP(strGraphNodeIRPDestroy) = NULL;
		for (long n = 0; n != strGraphNodeIRPSize(regAllocedNodes); n++) {
			struct IRNodeValue *val = (void *)graphNodeIRValuePtr(regAllocedNodes[n]);
			if (val->base.type != IR_VALUE)
				continue;
			if (val->val.type != IR_VAL_VAR_REF)
				continue;
			if (!val->val.value.var.var->isGlobal)
				continue;
			__auto_type sym = IRCreateGlobalVarRef(val->val.value.var.var);
			strGraphNodeIRP toReplace CLEANUP(strGraphNodeIRPDestroy) = strGraphNodeIRPAppendItem(NULL, regAllocedNodes[n]);
			graphIRReplaceNodes(toReplace, sym, NULL, (void (*)(void *))IRNodeDestroy);

			removed = strGraphNodeIRPSortedInsert(removed, regAllocedNodes[n], (gnCmpType)ptrPtrCmp);
			added = strGraphNodeIRPSortedInsert(added, sym, (gnCmpType)ptrPtrCmp);
		}
		regAllocedNodes = strGraphNodeIRPSetDifference(regAllocedNodes, removed, (gnCmpType)ptrPtrCmp);
		regAllocedNodes = strGraphNodeIRPSetUnion(regAllocedNodes, added, (gnCmpType)ptrPtrCmp);
	}
	// For all non-reg globals,dump them to global scope
	if (!isFunc)
		for (long p = 0; p != strPVarSize(noregs); p++) {
			if (!noregs[p]->isGlobal)
				continue;
			X86EmitAsmGlobalVar(noregs[p]);
		}

	if (isFunc) {
		IRABIAsmPrologue(frameSize);
	} else {
		// Make EBP equal to ESP
		struct X86AddressingMode *bp = X86AddrModeReg(basePointer());
		struct X86AddressingMode *sp = X86AddrModeReg(stackPointer());
		asmAssign(bp, sp, ptrSize(),0);
	}
	// This computes calling information for the ABI
	IRComputeABIInfo(start);

	if(!isFunc) {
			// Add to stack pointer to make room for locals
			strX86AddrMode addArgs CLEANUP(strX86AddrModeDestroy2) = NULL;
			addArgs = strX86AddrModeAppendItem(addArgs, X86AddrModeReg(stackPointer()));
			addArgs = strX86AddrModeAppendItem(addArgs, X86AddrModeSint(frameSize));
			assembleInst("SUB", addArgs);
	}

	{
			strGraphNodeIRP inserted CLEANUP(strGraphNodeIRPDestroy) = insertLabelsForAsm(regAllocedNodes);
		inserted = strGraphNodeIRPSetUnion(nodes, inserted, (gnCmpType)ptrPtrCmp);
		debugShowGraphIR(start);
	}

	IR2Asm(start);
	
	ptrMapFrameOffsetDestroy(localVarFrameOffsets, NULL);
	//"Pop" the old frame layout
	localVarFrameOffsets = oldOffsets;
	frameSize=oldFrameSize;
	
	for (long f = 0; f != strGraphNodeIRPSize(funcsWithin); f++) {
		IRCompile(funcsWithin[f], 1);
	}
}
static int isPtrType(struct object *obj) {
	__auto_type type = objectBaseType(obj)->type;
	return type == TYPE_PTR || type == TYPE_ARRAY||type==TYPE_FUNCTION;
}

static int isPtrNode(graphNodeIR start) {
	return isPtrType(IRNodeType(start));
}
static int isFltType(struct object *obj) {
	return objectBaseType(obj) == &typeF64;
}
static int isFltNode(graphNodeIR start) {
	return isFltType(IRNodeType(start));
}
static int isIntNode(graphNodeIR start) {
	return isIntType(IRNodeType(start));
}
static struct X86AddressingMode *demoteAddrMode(struct X86AddressingMode *addr, struct object *type) {
	__auto_type mode = X86AddrModeClone(addr);
	switch (mode->type) {
	case X86ADDRMODE_REG: {
		__auto_type subReg = subRegOfType(mode->value.reg, type);
		if (!subReg)
			return NULL;
		mode->value.reg = subReg;
		break;
	}
	case X86ADDRMODE_STR:
	case X86ADDRMODE_FLT:
	case X86ADDRMODE_ITEM_ADDR:
	case X86ADDRMODE_LABEL:
	case X86ADDRMODE_MEM:
	case X86ADDRMODE_SINT:
	case X86ADDRMODE_UINT:
		mode->valueType = type;
		break;
	}
	return mode;
}
static void setCond(graphNodeIR atNode,const char *cond, struct X86AddressingMode *oMode) {
	struct X86AddressingMode *zero CLEANUP(X86AddrModeDestroy) = X86AddrModeSint(0);
	struct X86AddressingMode *oMode2 CLEANUP(X86AddrModeDestroy) = demoteAddrMode(oMode, &typeI8i);
	// Not all modes can be demoted
	if (!oMode2) {
		char buffer[32];
		strChar t CLEANUP(strCharDestroy) = uniqueLabel(NULL);
		strChar end CLEANUP(strCharDestroy) = uniqueLabel(NULL);
		sprintf(buffer, "J%s", cond);
		strX86AddrMode tLab CLEANUP(strX86AddrModeDestroy2) = strX86AddrModeAppendItem(NULL, X86AddrModeLabel(t));
		strX86AddrMode endLab CLEANUP(strX86AddrModeDestroy2) = strX86AddrModeAppendItem(NULL, X86AddrModeLabel(end));
		struct X86AddressingMode *one CLEANUP(X86AddrModeDestroy) = X86AddrModeSint(1);
		// Jcc t
		// oMode=0
		// JMP end
		// t:
		// oMode=1
		// end:
		
		assembleOpcode(atNode,buffer,tLab);
		asmAssign(oMode, zero, objectSize(oMode->valueType, NULL),0);
		assembleInst("JMP", endLab);
		X86EmitAsmLabel(t);
		asmAssign(oMode, one, objectSize(oMode->valueType, NULL),0);
		X86EmitAsmLabel(end);
	} else {
		strX86AddrMode setccArgs CLEANUP(strX86AddrModeDestroy2) = strX86AddrModeAppendItem(NULL, X86AddrModeClone(oMode2));
		char buffer[32];
		sprintf(buffer, "SET%s", cond);

		asmAssign(oMode, zero, objectSize(oMode->valueType, NULL),0);
		assembleOpcode(atNode,buffer,setccArgs);
	}
}
static struct X86AddressingMode *includeHCRTFunc(const char *name);
static strX86AddrMode X87UnopArgs(graphNodeIR node) {
		strGraphEdgeIRP inArgs CLEANUP(strGraphEdgeIRPDestroy)=IREdgesByPrec(node);
				__auto_type srcNode=graphEdgeIRIncoming(inArgs[0]);
				strX86AddrMode retVal= strX86AddrModeAppendItem(NULL, X86AddrModeReg(&regX86ST0));
				struct X86AddressingMode *mode CLEANUP(X86AddrModeDestroy)=(void*)IRNode2AddrMode(srcNode);
				retVal[0]->valueType=&typeF64;
				asmTypecastAssign(retVal[0], mode, ASM_ASSIGN_X87FPU_POP);
				return retVal;
}
static strX86AddrMode X87BinopArgs(graphNodeIR node,struct reg *a,struct reg *b) {
		graphNodeIR sA,sB;
		binopArgs(node, &sA, &sB);
				strX86AddrMode retVal= strX86AddrModeAppendItem(NULL, X86AddrModeReg(a));
				retVal= strX86AddrModeAppendItem(retVal, X86AddrModeReg(b));
				struct X86AddressingMode *aMode CLEANUP(X86AddrModeDestroy)=(void*)IRNode2AddrMode(sA);
				struct X86AddressingMode *bMode CLEANUP(X86AddrModeDestroy)=(void*)IRNode2AddrMode(sB);
				if(a==&regX86ST0) {
						__auto_type tmp=aMode;
						aMode=bMode;
						bMode=tmp;
				}
				struct X86AddressingMode *st0 CLEANUP(X86AddrModeDestroy)=X86AddrModeReg(&regX86ST0);
				st0->valueType=&typeF64;
				asmTypecastAssign(st0, aMode, 0);
				asmTypecastAssign(st0, bMode, 0);
				return retVal;
}
static void X87StoreResult(graphNodeIR node) {
		strGraphEdgeIRP out CLEANUP(strGraphEdgeIRPDestroy) = graphNodeIROutgoing(node);
		strGraphEdgeIRP dst CLEANUP(strGraphEdgeIRPDestroy) = IRGetConnsOfType(out, IR_CONN_DEST);
		if(strGraphEdgeIRPSize(dst)) {
				struct X86AddressingMode *st0 CLEANUP(X86AddrModeDestroy)=X86AddrModeReg(&regX86ST0);
				st0->valueType=&typeF64;
				struct X86AddressingMode *oMode CLEANUP(X86AddrModeDestroy)=IRNode2AddrMode(graphEdgeIROutgoing(dst[0]));
				asmTypecastAssign(oMode, st0 , ASM_ASSIGN_X87FPU_POP);
		}
}
static void X87PopFPU() {
		strX86AddrMode fstpArgs CLEANUP(strX86AddrModeDestroy2)=strX86AddrModeAppendItem(NULL, X86AddrModeReg(&regX86ST0));
		assembleOpcode(NULL, "FSTP",  fstpArgs);
}
static void compileX87Expr(graphNodeIR node) {
		strGraphEdgeIRP out CLEANUP(strGraphEdgeIRPDestroy) = graphNodeIROutgoing(node);
		strGraphEdgeIRP dst CLEANUP(strGraphEdgeIRPDestroy) = IRGetConnsOfType(out, IR_CONN_DEST);
		switch (graphNodeIRValuePtr(node)->type) {
		case IR_MOD: {
				strX86AddrMode faddpArgs CLEANUP(strX86AddrModeDestroy2)=X87BinopArgs(node,&regX86ST0,&regX86ST1);
				assembleOpcode(node, "FPREM", NULL);
				X87StoreResult(node);
				X87PopFPU();
				return;
		}
		case IR_INC: {
				strX86AddrMode faddpArgs CLEANUP(strX86AddrModeDestroy2)=strX86AddrModeAppendItem(NULL, X86AddrModeReg(&regX86ST0));
				faddpArgs=strX86AddrModeConcat(faddpArgs, X87UnopArgs(node));
				assembleInst("FLD1", NULL);
				assembleOpcode(node,"FADDP",faddpArgs);
				X87StoreResult(node);
				return ;
		}
		case IR_DEC: {
				strX86AddrMode faddpArgs CLEANUP(strX86AddrModeDestroy2)=strX86AddrModeAppendItem(NULL, X86AddrModeReg(&regX86ST0));
				faddpArgs=strX86AddrModeConcat(faddpArgs, X87UnopArgs(node));
				assembleInst("FLD1", NULL);
				assembleOpcode(node,"FSUBP",faddpArgs);
				X87StoreResult(node);
			return;
		}
		case IR_ADD: {
				strX86AddrMode faddpArgs CLEANUP(strX86AddrModeDestroy2)=X87BinopArgs(node,&regX86ST1,&regX86ST0);
				assembleOpcode(node,"FADDP",faddpArgs);
				X87StoreResult(node);
			return;
		}
		case IR_SUB: {
				strX86AddrMode faddpArgs CLEANUP(strX86AddrModeDestroy2)=X87BinopArgs(node,&regX86ST1,&regX86ST0);
				assembleOpcode(node,"FSUBP",faddpArgs);
				X87StoreResult(node);
				return ;
		}
		case IR_POS: {
				//Multiply by 1
				strX86AddrMode faddpArgs CLEANUP(strX86AddrModeDestroy2)=strX86AddrModeAppendItem(NULL, X86AddrModeReg(&regX86ST0));
				faddpArgs=strX86AddrModeConcat(faddpArgs, X87UnopArgs(node));
				assembleInst("FLD1", NULL);
				assembleOpcode(node,"FMULP",faddpArgs);
				X87StoreResult(node);
				return;
		}
		case IR_NEG: {
				strX86AddrMode faddpArgs CLEANUP(strX86AddrModeDestroy2)= X87UnopArgs(node);
				assembleOpcode(node,"FCHS",faddpArgs);
				X87StoreResult(node);
				return;
		}
		case IR_MULT: {
				strX86AddrMode faddpArgs CLEANUP(strX86AddrModeDestroy2)=X87BinopArgs(node,&regX86ST1,&regX86ST0);
				assembleOpcode(node,"FMULP",faddpArgs);
				X87StoreResult(node);
				return;
		}
		case IR_DIV: {
				strX86AddrMode faddpArgs CLEANUP(strX86AddrModeDestroy2)=X87BinopArgs(node,&regX86ST1,&regX86ST0);
				assembleOpcode(node,"FDIVP",faddpArgs);
				X87StoreResult(node);
				return;
		}
		case IR_POW: {
				struct X86AddressingMode *oMode CLEANUP(X86AddrModeDestroy)=X86AddrModeReg(&regX86ST0);
				oMode->valueType=&typeF64;
				struct X86AddressingMode *powF64Mode CLEANUP(X86AddrModeDestroy)=includeHCRTFunc("PowF64");

				X87BinopArgs(node, &regX86ST1, &regX86ST0);
				
				strX86AddrMode args CLEANUP(strX86AddrModeDestroy2)=NULL;
				args=strX86AddrModeAppendItem(args, X86AddrModeReg(&regX86ST0));
				args=strX86AddrModeAppendItem(args, X86AddrModeReg(&regX86ST0));
				args[0]->valueType=&typeF64;
				args[1]->valueType=&typeF64;
				IRABICall2Asm(node, powF64Mode, args, oMode);
				X87StoreResult(node);
				return;
		}
		case IR_GT: {
				strX86AddrMode faddpArgs CLEANUP(strX86AddrModeDestroy2)=X87BinopArgs(node,&regX86ST0,&regX86ST1);
				assembleOpcode(node,"FCOMIP",faddpArgs);
				X87PopFPU();
				struct X86AddressingMode *oMode CLEANUP(X86AddrModeDestroy) = IRNode2AddrMode(graphEdgeIROutgoing(dst[0]));
				setCond(node,"G", oMode);
				return;
		}
		case IR_LT: {
				strX86AddrMode faddpArgs CLEANUP(strX86AddrModeDestroy2)=X87BinopArgs(node,&regX86ST0,&regX86ST1);
				assembleOpcode(node,"FCOMIP",faddpArgs);
				X87PopFPU();
				strX86AddrMode setccArgs CLEANUP(strX86AddrModeDestroy2) = NULL;
				struct X86AddressingMode *oMode CLEANUP(X86AddrModeDestroy) = IRNode2AddrMode(graphEdgeIROutgoing(dst[0]));
				setCond(node,"L", oMode);
				return;
		}
		case IR_GE: {
				strX86AddrMode faddpArgs CLEANUP(strX86AddrModeDestroy2)=X87BinopArgs(node,&regX86ST0,&regX86ST1);
				assembleOpcode(node,"FCOMIP",faddpArgs);
				X87PopFPU();
				strX86AddrMode setccArgs CLEANUP(strX86AddrModeDestroy2) = NULL;
			struct X86AddressingMode *oMode CLEANUP(X86AddrModeDestroy) = IRNode2AddrMode(graphEdgeIROutgoing(dst[0]));
			setCond(node,"GE", oMode);
			return;
		}
		case IR_LE: {
				strX86AddrMode faddpArgs CLEANUP(strX86AddrModeDestroy2)=X87BinopArgs(node,&regX86ST0,&regX86ST1);
				assembleOpcode(node,"FCOMIP",faddpArgs);
				X87PopFPU();
				strX86AddrMode setccArgs CLEANUP(strX86AddrModeDestroy2) = NULL;
				struct X86AddressingMode *oMode CLEANUP(X86AddrModeDestroy) = IRNode2AddrMode(graphEdgeIROutgoing(dst[0]));
				setCond(node,"LE", oMode);
				return;
		}
		case IR_EQ: {
				strX86AddrMode faddpArgs CLEANUP(strX86AddrModeDestroy2)=X87BinopArgs(node,&regX86ST0,&regX86ST1);
				assembleOpcode(node,"FCOMIP",faddpArgs);
				X87PopFPU();
				strX86AddrMode setccArgs CLEANUP(strX86AddrModeDestroy2) = NULL;
				struct X86AddressingMode *oMode CLEANUP(X86AddrModeDestroy) = IRNode2AddrMode(graphEdgeIROutgoing(dst[0]));
				setCond(node,"E", oMode);
				return;
		}
		case IR_NE: {
				strX86AddrMode faddpArgs CLEANUP(strX86AddrModeDestroy2)=X87BinopArgs(node,&regX86ST0,&regX86ST1);
				assembleOpcode(node,"FCOMIP",faddpArgs);
				X87PopFPU();
				strX86AddrMode setccArgs CLEANUP(strX86AddrModeDestroy2) = NULL;
				struct X86AddressingMode *oMode CLEANUP(X86AddrModeDestroy) = IRNode2AddrMode(graphEdgeIROutgoing(dst[0]));
				setCond(node,"NE", oMode);
			return;
		}
		case IR_VALUE: {
				strGraphEdgeIRP in CLEANUP(strGraphEdgeIRPDestroy)=graphNodeIRIncoming(node);
				strGraphEdgeIRP inDst CLEANUP(strGraphEdgeIRPDestroy)=IRGetConnsOfType(in, IR_CONN_DEST);
				strGraphEdgeIRP inDstPtrAsn CLEANUP(strGraphEdgeIRPDestroy)=IRGetConnsOfType(in, IR_CONN_ASSIGN_FROM_PTR);
				if(strGraphEdgeIRPSize(inDst)==1) {
						__auto_type inNode=graphEdgeIRIncoming(in[0]);
						if(graphNodeIRValuePtr(inNode)->type==IR_VALUE) {
								struct X86AddressingMode *iMode CLEANUP(X86AddrModeDestroy)=IRNode2AddrMode(inNode);
								struct X86AddressingMode *oMode CLEANUP(X86AddrModeDestroy)=IRNode2AddrMode(node);
								asmTypecastAssign(oMode,  iMode,  ASM_ASSIGN_X87FPU_POP);
						}
				} else if(strGraphEdgeIRPSize(inDstPtrAsn)==1) {
						__auto_type inNode=graphEdgeIRIncoming(in[0]);
						if(graphNodeIRValuePtr(inNode)->type==IR_VALUE) {
								struct X86AddressingMode *iMode CLEANUP(X86AddrModeDestroy)=IRNode2AddrMode(inNode);
								struct X86AddressingMode *oMode CLEANUP(X86AddrModeDestroy)=IRNode2AddrMode(node);
								
								asmTypecastAssign(oMode,  iMode ,  ASM_ASSIGN_X87FPU_POP);
						}
				}

				//If is a x87 fpu register whose value is not used,pop it
				struct IRNodeValue *val=(void*)graphNodeIRValuePtr(node);
				if(val->val.type==IR_VAL_REG) {
						if(val->val.value.reg.reg==&regX86ST0) {
								strGraphEdgeIRP out CLEANUP(strGraphEdgeIRPDestroy)=graphNodeIROutgoing(node);
								int usedInExpr=0;
								for(long o=0;o!=strGraphEdgeIRPSize(out);o++)
										usedInExpr|=IRIsExprEdge(*graphEdgeIRValuePtr(out[o]));
								if(!usedInExpr) {
										//Add st(0) and st(0) to pop stack
										strX86AddrMode faddpArgs CLEANUP(strX86AddrModeDestroy2)=NULL;
										faddpArgs=strX86AddrModeAppendItem(faddpArgs, X86AddrModeReg(&regX86ST0));
										faddpArgs=strX86AddrModeAppendItem(faddpArgs, X86AddrModeReg(&regX86ST0));
										assembleOpcode(node,"FADDP",faddpArgs);
								}
						}
				}
				return;
		}
		case IR_TYPECAST: {
			strGraphEdgeIRP in CLEANUP(strGraphEdgeIRPDestroy) = graphNodeIRIncoming(node);
			strGraphEdgeIRP src CLEANUP(strGraphEdgeIRPDestroy) = IRGetConnsOfType(in, IR_CONN_SOURCE_A);
			__auto_type srcNode=graphEdgeIRIncoming(src[0]);
			struct X86AddressingMode *iMode CLEANUP(X86AddrModeDestroy)=IRNode2AddrMode(srcNode);
			struct X86AddressingMode *oMode CLEANUP(X86AddrModeDestroy)=IRNode2AddrMode(node);
			asmTypecastAssign(oMode, iMode, ASM_ASSIGN_X87FPU_POP);
			return;
		}
		default:
			assert(0);
		}
}
static strGraphNodeIRP nextNodesToCompile(graphNodeIR node) {
	strGraphEdgeIRP out CLEANUP(strGraphEdgeIRPDestroy) = graphNodeIROutgoing(node);
	strGraphNodeIRP retVal = NULL;
	for (long e = 0; e != strGraphEdgeIRPSize(out); e++)
		if (*graphEdgeIRValuePtr(out[e]) != IR_CONN_NEVER_FLOW)
			retVal = strGraphNodeIRPAppendItem(retVal, graphEdgeIROutgoing(out[e]));
	qsort(retVal, strGraphNodeIRPSize(retVal), (sizeof *retVal), ptrPtrCmp);
	return retVal;
}

static strGraphNodeIRP compileX87IfNeded(graphNodeIR start, int *compiled) {
	if (compiled)
		*compiled = 0;
	if (isFltNode(start)) {
		switch (getCurrentArch()) {
		case ARCH_TEST_SYSV:
		case ARCH_X86_SYSV: {
			if (compiled)
				*compiled = 1;
			compileX87Expr(start);
			return nextNodesToCompile(start);
		}
		case ARCH_X64_SYSV:;
		}
	}
	return NULL;
}
static void __typecastSignExt(graphNodeIR atNode,struct X86AddressingMode *outMode, struct X86AddressingMode *inMode) {
	long iSize = objectSize(inMode->valueType, NULL);
	long oSize = objectSize(outMode->valueType, NULL);

	strX86AddrMode movArgs CLEANUP(strX86AddrModeDestroy2) = NULL;
	movArgs = strX86AddrModeAppendItem(movArgs, X86AddrModeClone(outMode));
	movArgs = strX86AddrModeAppendItem(movArgs, X86AddrModeClone(inMode));
	if (oSize == 8 && iSize == 4) {
			assembleOpcode(atNode,"MOVSXD", movArgs);
	} else {
			assembleOpcode(atNode,"MOVSX", movArgs);
	}
}
static int __ouputModeAffectsInput(struct X86AddressingMode *input, struct X86AddressingMode *out) {
		if(out->type==X86ADDRMODE_REG) {
				strRegP iModeRegs CLEANUP(strRegPDestroy) = regsFromMode(input);
				for (long A = 0; A != strRegPSize(iModeRegs); A++)
						if (regConflict(iModeRegs[A], out->value.reg))
								return 1;
		}
	return 0;
}
static graphNodeIR assembleOpIntShift(graphNodeIR start, const char *op) {
	graphNodeIR a, b;
	binopArgs(start, &a, &b);
	__auto_type out = nodeDest(start);
	struct X86AddressingMode *aMode CLEANUP(X86AddrModeDestroy) = IRNode2AddrMode(a);
	struct X86AddressingMode *bMode CLEANUP(X86AddrModeDestroy) = IRNode2AddrMode(b);
	struct X86AddressingMode *clMode CLEANUP(X86AddrModeDestroy) = X86AddrModeReg(&regX86CL);
	if(!out)
			return NULL;
	struct X86AddressingMode *oMode CLEANUP(X86AddrModeDestroy) = IRNode2AddrMode(out);
	int shift = 0;
	if (bMode->type == X86ADDRMODE_SINT) {
		if (bMode->value.sint == 1)
			goto one;
		shift = bMode->value.sint;
		goto imm;
	}
	if (bMode->type == X86ADDRMODE_UINT) {
		if (bMode->value.uint == 1)
			goto one;
		shift = bMode->value.uint;
		goto imm;
	}
	clMode->valueType = &typeI8i;
	{
			AUTO_LOCK_MODE_REGS(bMode);
			AUTO_LOCK_MODE_REGS(oMode);
			AUTO_LOCK_MODE_REGS(clMode);
			__auto_type tmpReg=regForTypeExcludingConsumed(oMode->valueType);
			struct X86AddressingMode *tmpRegMode CLEANUP(X86AddrModeDestroy)=X86AddrModeReg(tmpReg);
		tmpRegMode->valueType=oMode->valueType;
		pushReg(tmpReg);
		asmTypecastAssign(tmpRegMode,aMode, ASM_ASSIGN_X87FPU_POP);

		pushReg(&regX86CL);
		asmTypecastAssign(clMode, bMode,0);
		strX86AddrMode args CLEANUP(strX86AddrModeDestroy2) = NULL;
		args = strX86AddrModeAppendItem(args, X86AddrModeClone(tmpRegMode));
		args = strX86AddrModeAppendItem(args, X86AddrModeReg(&regX86CL));
		assembleOpcode(start,op,args);
		popReg(&regX86CL);
		
		asmTypecastAssign(oMode, tmpRegMode, ASM_ASSIGN_X87FPU_POP);
		popReg(tmpReg);
		return out;
	}
	one : {
				asmTypecastAssign(oMode, aMode, ASM_ASSIGN_X87FPU_POP);
	strX86AddrMode args CLEANUP(strX86AddrModeDestroy2) = NULL;
	args = strX86AddrModeAppendItem(args, X86AddrModeClone(oMode));
	args = strX86AddrModeAppendItem(args, X86AddrModeSint(1));
	assembleOpcode(start,op,args);
	return out;
}
imm : {
				asmTypecastAssign(oMode, aMode, ASM_ASSIGN_X87FPU_POP);
	strX86AddrMode args CLEANUP(strX86AddrModeDestroy2) = NULL;
	args = strX86AddrModeAppendItem(args, X86AddrModeClone(oMode));
	args = strX86AddrModeAppendItem(args, X86AddrModeSint(shift));
	assembleOpcode(start,op,args);
	return out;
}
}
static graphNodeIR assembleOpPtrArith(graphNodeIR start) {
		graphNodeIR a, b, out = nodeDest(start);
		binopArgs(start, &a, &b);
		struct X86AddressingMode *aMode CLEANUP(X86AddrModeDestroy) = IRNode2AddrMode(a);
		struct X86AddressingMode *bMode CLEANUP(X86AddrModeDestroy) = IRNode2AddrMode(b);
		struct X86AddressingMode *oMode CLEANUP(X86AddrModeDestroy) = IRNode2AddrMode(out);
		if(graphNodeIRValuePtr(start)->type==IR_SUB) {
				if(isPtrNode(a)&&isPtrNode(b)) {
						//Is checking the distance in items between 2 pointers.
						long scale=0;
						if(aMode->valueType->type==TYPE_PTR) {
								struct objectPtr *ptr=(void*)aMode->valueType;
								scale=objectSize(ptr->type, NULL);
						} else if(aMode->valueType->type==TYPE_ARRAY) {
								struct objectArray *arr=(void*)aMode->valueType;
								scale=objectSize(arr->type, NULL);
						}
						{
								AUTO_LOCK_MODE_REGS(aMode);
								AUTO_LOCK_MODE_REGS(bMode);
								AUTO_LOCK_MODE_REGS(oMode);
								__auto_type tmpReg=regForTypeExcludingConsumed(getTypeForSize(ptrSize()));
								pushReg(tmpReg);
								struct X86AddressingMode *tmpRegMode CLEANUP(X86AddrModeDestroy)=X86AddrModeReg(tmpReg);
								asmAssign(tmpRegMode, aMode, ptrSize(), 0);
								strX86AddrMode addArgs CLEANUP(strX86AddrModeDestroy2)=NULL;
								addArgs=strX86AddrModeAppendItem(addArgs,X86AddrModeReg(tmpReg));
								addArgs=strX86AddrModeAppendItem(addArgs,X86AddrModeClone(bMode));
								assembleInst("SUB", addArgs);

								long shift=3;
								switch(scale) {
								case 1:
										asmAssign(oMode, tmpRegMode, ptrSize(), 0);
										break;
								case 2:
										shift=1;
										goto shift;
								case 4:
										shift=2;
										goto shift;
								case 8:
										shift=3;
										goto shift;
								shift: {
												strX86AddrMode sarArgs CLEANUP(strX86AddrModeDestroy2)=NULL;
												sarArgs=strX86AddrModeAppendItem(sarArgs,X86AddrModeReg(tmpReg));
												sarArgs=strX86AddrModeAppendItem(sarArgs,X86AddrModeSint(shift));
												assembleOpcode(start,"SAR",sarArgs);
												asmAssign(oMode, tmpRegMode, ptrSize(), 0);
												break;
										}
								default: {
										__auto_type rdx=subRegOfType(&regAMD64RDX, getTypeForSize(ptrSize()));
										__auto_type rax=subRegOfType(&regAMD64RAX, getTypeForSize(ptrSize()));
										//Make room for dummy value
										pushReg(rdx);
										pushReg(rdx);
										pushReg(rax);
										struct X86AddressingMode *raxMode CLEANUP(X86AddrModeDestroy)=X86AddrModeReg(rax);
										struct X86AddressingMode *scaleMode CLEANUP(X86AddrModeDestroy)=X86AddrModeSint(scale);
										//rax=tmpReg
										//tmpReg=scale
										//IDIV tmpReg
										//mov [ESP+2*ptrSize],rax
										asmAssign(raxMode, tmpRegMode, ptrSize(), 0);
										asmAssign(tmpRegMode, scaleMode, ptrSize(), 0);
										
										strX86AddrMode idivArgs CLEANUP(strX86AddrModeDestroy2)=NULL;
										idivArgs=strX86AddrModeAppendItem(idivArgs,X86AddrModeReg(tmpReg));
										assembleInst("IDIV", idivArgs);

										struct X86AddressingMode *dummyLoc CLEANUP(X86AddrModeDestroy)=X86AddrModeIndirSIB(0, NULL, X86AddrModeReg(stackPointer()), X86AddrModeSint(2*ptrSize()), objectPtrCreate(&typeU0));
										asmAssign(dummyLoc, raxMode, ptrSize(), 0);
										
										popReg(rax);
										popReg(rdx);

										//Pop dummy location
										popReg(tmpReg);
										asmAssign(oMode, tmpRegMode, ptrSize(), 0);
										break;
								}
								}
								popReg(tmpReg);
						}
						return out;
				}
		}
		
	swapLoop:;
		long scale=0;
		if(aMode->valueType->type==TYPE_PTR) {
				struct objectPtr *ptr=(void*)aMode->valueType;
				scale=objectSize(ptr->type, NULL);
		} else if(aMode->valueType->type==TYPE_ARRAY) {
				struct objectArray *arr=(void*)aMode->valueType;
				scale=objectSize(arr->type, NULL);
		} else {
				__auto_type tmp=aMode;
				aMode=bMode;
				bMode=tmp;
				goto swapLoop;
		}
		int sub=graphNodeIRValuePtr(start)->type==IR_SUB;
		switch(scale) {
		case 1:
		case 2:
		case 4:
		case 8: {
				AUTO_LOCK_MODE_REGS(oMode);
				AUTO_LOCK_MODE_REGS(aMode);
				AUTO_LOCK_MODE_REGS(bMode);
				struct  reg *baseReg=NULL; int pushBase=0;
				struct  reg *indexReg=NULL; int pushIndex=0;
				if(aMode->type==X86ADDRMODE_REG) {
						baseReg=aMode->value.reg;
				} else {
						baseReg=regForTypeExcludingConsumed(objectPtrCreate(&typeU0));
						pushBase=1;
						pushReg(baseReg);
				}
				struct X86AddressingMode *baseRegMode CLEANUP(X86AddrModeDestroy)=X86AddrModeReg(baseReg);
				if(pushBase)
						asmAssign(baseRegMode, aMode , ptrSize() , 0 );
				AUTO_LOCK_MODE_REGS(baseRegMode);

				if(bMode->type==X86ADDRMODE_REG) {
						indexReg=bMode->value.reg;
				} else {
						indexReg=regForTypeExcludingConsumed(objectPtrCreate(&typeU0));
						pushIndex=1;
						pushReg(indexReg);
				}
				struct X86AddressingMode *indexRegMode CLEANUP(X86AddrModeDestroy)=X86AddrModeReg(indexReg);
				if(pushIndex)
						asmAssign(indexRegMode, bMode , ptrSize() , 0 );
				AUTO_LOCK_MODE_REGS(indexRegMode);

				if(sub) {
						strX86AddrMode negArgs CLEANUP(strX86AddrModeDestroy)=strX86AddrModeAppendItem(NULL, indexRegMode);
						assembleInst("NEG", negArgs);
				}
				
				__auto_type tmpReg=regForTypeExcludingConsumed(IRNodeType(out));
				struct X86AddressingMode *tmpRegMode CLEANUP(X86AddrModeDestroy)=X86AddrModeReg(tmpReg);				
				pushReg(tmpReg);
				strX86AddrMode leaArgs CLEANUP(strX86AddrModeDestroy2)=NULL;
				leaArgs=strX86AddrModeAppendItem(leaArgs, X86AddrModeReg(tmpReg));
				leaArgs=strX86AddrModeAppendItem(leaArgs, X86AddrModeIndirSIB(scale, X86AddrModeReg(indexReg), X86AddrModeReg(baseReg), NULL, oMode->valueType));
				assembleInst("LEA", leaArgs);
				asmAssign(oMode, tmpRegMode, ptrSize(), 0);
				popReg(tmpReg);

				//Restore register to original value(if we didnt already push the old value which we will restored)
				if(sub&&!pushIndex) {
						strX86AddrMode negArgs CLEANUP(strX86AddrModeDestroy)=strX86AddrModeAppendItem(NULL, indexRegMode);
						assembleInst("NEG", negArgs);
				}

				if(pushIndex)
						popReg(indexReg);
				if(pushBase)
						popReg(baseReg);
				return out;
		}
		default: {
				struct X86AddressingMode *zeroMode CLEANUP(X86AddrModeDestroy)=X86AddrModeSint(0);
				asmTypecastAssign(oMode, zeroMode, 0);
				
				strX86AddrMode imul2Args CLEANUP(strX86AddrModeDestroy2)=NULL;
				imul2Args=strX86AddrModeAppendItem(imul2Args, X86AddrModeClone(oMode));
				imul2Args=strX86AddrModeAppendItem(imul2Args, X86AddrModeSint(scale*(sub?-1:1)));
				assembleOpcode(start, "IMUL2", imul2Args);
				
				strX86AddrMode addArgs CLEANUP(strX86AddrModeDestroy2)=NULL;
				addArgs=strX86AddrModeAppendItem(addArgs, X86AddrModeClone(oMode));
				addArgs=strX86AddrModeAppendItem(addArgs,X86AddrModeClone(bMode));
				assembleOpcode(start, "ADD", addArgs);
				return out;
		}
		} 
}
static strX86AddrMode X87BinopArgs(graphNodeIR node,struct reg *a,struct reg *b);
static void X87PopFPU();
static graphNodeIR assembleOpCmp(graphNodeIR start) {
			graphNodeIR a, b, out = nodeDest(start);
	binopArgs(start, &a, &b);
	struct X86AddressingMode *aMode CLEANUP(X86AddrModeDestroy) = IRNode2AddrMode(a);
	struct X86AddressingMode *bMode CLEANUP(X86AddrModeDestroy) = IRNode2AddrMode(b);
		struct X86AddressingMode *oMode CLEANUP(X86AddrModeDestroy) = IRNode2AddrMode(out);
	
	if(objectBaseType(aMode->valueType)==&typeF64) {
			strX86AddrMode fcomiArgs CLEANUP(strX86AddrModeDestroy2)=X87BinopArgs(start,&regX86ST0,&regX86ST1);
			//
			// FCOMI takes ST(0) first always
			//
			assembleOpcode(start, "FCOMIP", fcomiArgs);
			X87PopFPU();
	} else {
			strX86AddrMode cmpArgs CLEANUP(strX86AddrModeDestroy2)=NULL;
			cmpArgs=strX86AddrModeAppendItem(cmpArgs, IRNode2AddrMode(a));
			cmpArgs=strX86AddrModeAppendItem(cmpArgs, IRNode2AddrMode(b));
			assembleOpcode(start, "CMP", cmpArgs);
	}
	return out;
}
static graphNodeIR assembleOpIntLogical(graphNodeIR start, const char *suffix) {
	graphNodeIR a, b, out = nodeDest(start);
	binopArgs(start, &a, &b);
	assembleOpCmp(start);
	struct X86AddressingMode *oMode CLEANUP(X86AddrModeDestroy)=IRNode2AddrMode(out);
	setCond(start, suffix, oMode);
	return out;
}
static graphNodeIR assembleOpInt(graphNodeIR start, const char *opName) {
		graphNodeIR a, b, out = nodeDest(start);
		binopArgs(start, &a, &b);
		switch(graphNodeIRValuePtr(start)->type) {
		case IR_ADD:
		case IR_SUB: {
				__auto_type  type=objectBaseType(IRNodeType(start));
				if(type->type==TYPE_ARRAY||type->type==TYPE_PTR||(isPtrNode(a)&&isPtrNode(b))) {
						return assembleOpPtrArith(start);
				}
		}
		default:;
		}
		struct X86AddressingMode *aMode CLEANUP(X86AddrModeDestroy) = IRNode2AddrMode(a);
		struct X86AddressingMode *bMode CLEANUP(X86AddrModeDestroy) = IRNode2AddrMode(b);
	struct X86AddressingMode *oMode CLEANUP(X86AddrModeDestroy) = IRNode2AddrMode(out);
	int useTmp=0;
	if(!__ouputModeAffectsInput(aMode, oMode)&&!__ouputModeAffectsInput(bMode, oMode)) {
			asmTypecastAssign(oMode, aMode, 0);
			
			strX86AddrMode args CLEANUP(strX86AddrModeDestroy2)=NULL;
			args=strX86AddrModeAppendItem(args, X86AddrModeClone(oMode));
			args=strX86AddrModeAppendItem(args, X86AddrModeClone(bMode));
			assembleOpcode(start, opName , args);
	}else {
			useTmp=1;
			//A mode's value with be loaded into a register if no others are avialable
			//AUTO_LOCK_MODE_REGS(aMode);
			AUTO_LOCK_MODE_REGS(bMode);
			AUTO_LOCK_MODE_REGS(oMode);
			__auto_type reg=regForTypeExcludingConsumed(aMode->valueType);

			pushReg(reg);
			struct X86AddressingMode *oMode2 CLEANUP(X86AddrModeDestroy) = X86AddrModeReg(reg);
			asmAssign(oMode2, aMode, objectSize(aMode->valueType,NULL), 0);
			
			strX86AddrMode args CLEANUP(strX86AddrModeDestroy2)=NULL;
			args=strX86AddrModeAppendItem(args, X86AddrModeClone(oMode2));
			args=strX86AddrModeAppendItem(args, X86AddrModeClone(bMode));
			assembleOpcode(start, opName , args);

			asmAssign(oMode ,oMode2, objectSize(aMode->valueType,NULL), 0);
			popReg(reg);
	}
	return out;
}
static int interferesWithConsumedReg(struct reg *r) {
	for (long i = 0; i != strRegPSize(consumedRegisters); i++)
		if (regConflict(r, consumedRegisters[i]))
			return 1;
	return 0;
}
void asmTypecastAssign(struct X86AddressingMode *outMode, struct X86AddressingMode *inMode,enum asmAssignFlags flags) {
	switch (inMode->type) {
	case X86ADDRMODE_FLT: {
		if (isIntType(outMode->valueType) || isPtrType(outMode->valueType)) {
			int64_t value = inMode->value.flt;
			asmAssign(outMode, X86AddrModeSint(value), objectSize(outMode->valueType, NULL),0);
		} else if (isFltType(outMode->valueType)) {
				asmAssign(outMode, inMode, objectSize(outMode->valueType, NULL),0);
		} else if(objectBaseType(outMode->valueType)==&typeBool) {
		flt2Bool:;
				struct X86AddressingMode *st0Mode CLEANUP(X86AddrModeDestroy)=X86AddrModeReg(&regX86ST0);
				asmAssign(st0Mode, inMode, 8, 0);
				assembleInst("FLDZ", NULL);
				
				strX86AddrMode fcomipArgs CLEANUP(strX86AddrModeDestroy2)=NULL;
				fcomipArgs=strX86AddrModeAppendItem(fcomipArgs, X86AddrModeReg(&regX86ST0));
				fcomipArgs=strX86AddrModeAppendItem(fcomipArgs, X86AddrModeReg(&regX86ST1));
				assembleInst((flags&ASM_ASSIGN_X87FPU_POP)?"FCOMIP":"FCOMI",  NULL);
				
				setCond(NULL, "NZ",outMode);
		} else
			assert(0);
		return;
	}
	case X86ADDRMODE_MEM:
	case X86ADDRMODE_LABEL:
	case X86ADDRMODE_ITEM_ADDR:
	case X86ADDRMODE_REG: {
		// If destination is bigger than source,sign extend if dest is signed
		if (isPtrType(outMode->valueType) || isIntType(outMode->valueType)) {
				if(isFltType(inMode->valueType)) {
						asmAssign(outMode, inMode,  objectSize(outMode->valueType, NULL), flags);
						return;
				}
			long iSize = objectSize(inMode->valueType, NULL);
			long oSize = objectSize(outMode->valueType, NULL);
			if (oSize > iSize) {
				if (typeIsSigned(inMode->valueType)) {
						__typecastSignExt(NULL,outMode, inMode);
				} else {
					strX86AddrMode ppArgs CLEANUP(strX86AddrModeDestroy2) = NULL;
					struct reg *dumpToReg = NULL;
					// PUSH reg if assigning to non-reg dest(movzx needs reg as dst)
					if (outMode->type != X86ADDRMODE_REG) {
						__auto_type tmpReg = regForTypeExcludingConsumed(outMode->valueType);
						ppArgs = strX86AddrModeAppendItem(ppArgs, X86AddrModeReg(tmpReg));
						dumpToReg = tmpReg;
						assembleInst("PUSH", ppArgs);
					} else {
						dumpToReg = outMode->value.reg;
					}

					strX86AddrMode movzxArgs CLEANUP(strX86AddrModeDestroy2) = NULL;
					struct X86AddressingMode *dumpToRegMode CLEANUP(X86AddrModeDestroy) = X86AddrModeReg(dumpToReg);
					AUTO_LOCK_MODE_REGS(dumpToRegMode);

					movzxArgs = strX86AddrModeAppendItem(movzxArgs, X86AddrModeClone(dumpToRegMode));
					movzxArgs = strX86AddrModeAppendItem(movzxArgs, X86AddrModeClone(inMode));
					assembleInst("MOVZX", movzxArgs);
					asmAssign(outMode, dumpToRegMode, objectSize(outMode->valueType, NULL),0);

					// POP reg if assigning to non-reg dest(movzx needs reg as dst)
					if (outMode->type != X86ADDRMODE_REG) {
						assembleInst("POP", ppArgs);
					}
				}
				return;
			} else if (iSize > oSize) {
				struct X86AddressingMode *mode CLEANUP(X86AddrModeDestroy) = demoteAddrMode(inMode, outMode->valueType);
				if (!mode) {
						AUTO_LOCK_MODE_REGS(outMode);
						AUTO_LOCK_MODE_REGS(inMode);
						__auto_type reg=regForTypeExcludingConsumed(inMode->valueType);
						// Cant demote current mode,so use RAX register as accumatior(which can be demoted)
					struct X86AddressingMode *rax CLEANUP(X86AddrModeDestroy) = NULL;
					switch (getCurrentArch()) {
					case ARCH_X64_SYSV:
						rax = X86AddrModeReg(reg);
						break;
					case ARCH_X86_SYSV:
					case ARCH_TEST_SYSV:
						rax = X86AddrModeReg(reg);
						break;
					}
					struct X86AddressingMode *demoted2Out CLEANUP(X86AddrModeDestroy) = demoteAddrMode(rax, outMode->valueType);
					struct X86AddressingMode *demoted2In CLEANUP(X86AddrModeDestroy) = X86AddrModeClone(rax);

					
					struct X86AddressingMode *zero CLEANUP(X86AddrModeDestroy) = X86AddrModeSint(0);
					if(demoted2Out) {
							pushMode(demoted2In);
							asmAssign(demoted2In, inMode, objectSize(inMode->valueType, NULL),0);
							asmAssign(outMode, demoted2Out, objectSize(outMode->valueType, NULL),0);
							popMode(demoted2In);
					} else {
							//Will never be RAX if cant be demoted,so use rax as a tmp reg
							struct X86AddressingMode *rax CLEANUP(X86AddrModeDestroy) = X86AddrModeReg(&regAMD64RAX);
							struct X86AddressingMode *demoted2In CLEANUP(X86AddrModeDestroy) = demoteAddrMode(rax, inMode->valueType);
							struct X86AddressingMode *demoted2Out CLEANUP(X86AddrModeDestroy) = demoteAddrMode(rax, outMode->valueType);
							pushMode(demoted2In);
							asmAssign(demoted2In, inMode, objectSize(inMode->valueType, NULL),0);
							asmAssign(outMode, demoted2Out, objectSize(outMode->valueType, NULL),0);
							popMode(demoted2In);
					}
				} else {
					asmAssign(outMode, mode, objectSize(outMode->valueType, NULL),0);
				}
				return;
			} else {
				asmAssign(outMode, inMode, iSize,0);
			}
		} else if (isFltType(outMode->valueType)) {
				if(inMode->type==X86ADDRMODE_REG&&isFltType(inMode->valueType)) {} else {
						const char *op=isFltType(inMode->valueType)?"FLD":"FILD";
						strX86AddrMode fildArgs CLEANUP(strX86AddrModeDestroy2)=strX86AddrModeAppendItem(NULL, X86AddrModeClone(inMode));
						assembleOpcode(NULL, op, fildArgs);
				}
				struct X86AddressingMode *st0Mode CLEANUP(X86AddrModeDestroy)=X86AddrModeReg(&regX86ST0);
				asmAssign(outMode, st0Mode, 8, ASM_ASSIGN_X87FPU_POP);
		} else if(objectBaseType(outMode->valueType)==&typeBool) {
				if(isIntType(inMode->valueType)||isPtrType(inMode->valueType)) {
						strX86AddrMode cmpArgs CLEANUP(strX86AddrModeDestroy2)=NULL;
						cmpArgs=strX86AddrModeAppendItem(cmpArgs, X86AddrModeClone(inMode));
						cmpArgs=strX86AddrModeAppendItem(cmpArgs, X86AddrModeSint(0));
						assembleInst("CMP", cmpArgs);

						setCond(NULL,"NZ", outMode);
				} else if(isFltType(inMode->valueType)) {
						goto flt2Bool;
				}
		} else {
			__auto_type base = objectBaseType(outMode->valueType);
			if (base->type == TYPE_CLASS || base->type == TYPE_UNION) {
				asmAssign(outMode, inMode, objectSize(base, NULL),0);
			} else
				assert(0);
		};
		return;
	}
	case X86ADDRMODE_UINT:
	case X86ADDRMODE_SINT:
			if(isFltType(outMode->valueType)&&outMode->type!=X86ADDRMODE_REG) {
					struct X86AddressingMode *st0Mode CLEANUP(X86AddrModeDestroy)=X86AddrModeReg(&regX86ST0);
					st0Mode->valueType=&typeF64;
					asmTypecastAssign(st0Mode, inMode, flags);
					asmTypecastAssign(outMode,st0Mode,ASM_ASSIGN_X87FPU_POP);
					return ;
			}
		asmAssign(outMode, inMode, objectSize(outMode->valueType, NULL),ASM_ASSIGN_X87FPU_POP);
		return;
	}
	return;
}
static struct reg *addrModeReg(struct X86AddressingMode *mode) {
	if (mode->type != X86ADDRMODE_REG)
		return NULL;
	return mode->value.reg;
}
static int IRTableRangeCmp(const struct IRJumpTableRange *a, const struct IRJumpTableRange *b) {
	if (a->start > b->start)
		return 1;
	else if (a->start < b->start)
		return -1;
	else
		return 0;
}
static void storeMemberPtrInReg(struct reg *memReg, graphNodeIR sourceNode, strObjectMember members) {
	struct X86AddressingMode *memRegMode CLEANUP(X86AddrModeDestroy) = X86AddrModeReg(memReg);
	strX86AddrMode leaArgs CLEANUP(strX86AddrModeDestroy2) = NULL;
	leaArgs = strX86AddrModeAppendItem(leaArgs, X86AddrModeReg(memReg));
	leaArgs = strX86AddrModeAppendItem(leaArgs, IRNode2AddrMode(sourceNode));
	leaArgs[1]->valueType = NULL;
	assembleInst("LEA", leaArgs);

	__auto_type currentType = IRNodeType(sourceNode);
	long currentOffset = 0;
	for (long m = 0; m != strObjectMemberSize(members); m++) {
		if (currentType->type == TYPE_PTR) {
			if (currentOffset) {
				strX86AddrMode addArgs CLEANUP(strX86AddrModeDestroy2) = NULL;
				addArgs = strX86AddrModeAppendItem(addArgs, X86AddrModeReg(memReg));
				addArgs = strX86AddrModeAppendItem(addArgs, X86AddrModeSint(currentOffset));
				assembleInst("ADD", addArgs);
				currentOffset = 0;
			}

			// De-reference
			struct X86AddressingMode *indir CLEANUP(X86AddrModeDestroy) = X86AddrModeIndirReg(memReg, objectPtrCreate(&typeU0));
			asmAssign(memRegMode, indir, ptrSize(),0);
		}
		memRegMode->valueType = objectPtrCreate(members[m].type);
		currentOffset += members[m].offset;
		currentType = objectBaseType(members[m].type);
	}
	if (currentOffset) {
		strX86AddrMode addArgs CLEANUP(strX86AddrModeDestroy2) = NULL;
		addArgs = strX86AddrModeAppendItem(addArgs, X86AddrModeReg(memReg));
		addArgs = strX86AddrModeAppendItem(addArgs, X86AddrModeSint(currentOffset));
		assembleInst("ADD", addArgs);
	}
}
static strGraphNodeIRP __IR2Asm(graphNodeIR start) {
	if (ptrMapCompiledNodesGet(compiledNodes, start)) {
		// If encountering already "compiled" label node,jump to it
		if (graphNodeIRValuePtr(start)->type == IR_LABEL) {
			strX86AddrMode jmpArgs CLEANUP(strX86AddrModeDestroy2) = strX86AddrModeAppendItem(NULL, X86AddrModeLabel(getLabelName(start)));
			assembleInst("JMP", jmpArgs);
		}
		return NULL;
	}
	ptrMapCompiledNodesAdd(compiledNodes, start, 1);

	strGraphEdgeIRP out CLEANUP(strGraphEdgeIRPDestroy) = graphNodeIROutgoing(start);
	switch (graphNodeIRValuePtr(start)->type) {
	case IR_SOURCE_MAPPING: {
			struct IRNodeSourceMapping *mapping=(void*)graphNodeIRValuePtr(start);		
			long line;
			diagLineCol(mapping->fn, mapping->start, &line, NULL);			

			const char *fmt=";;;   %s:%li:\"\"\"%s\"\"\"   ;;;";
			__auto_type f=fopen(mapping->fn, "r");
			fseek(f, mapping->start, SEEK_SET);

			long len=diagDumpQoutedText(mapping->start,mapping->len+mapping->start,NULL);
			char qouted[len+1];
			qouted[len]='\0';
			diagDumpQoutedText(mapping->start,mapping->len+mapping->start,qouted);
			if(strchr(qouted, '\n'))
					*strchr(qouted, '\n')='\0';

			len=snprintf(NULL, 0, fmt, mapping->fn,line+1,qouted);
			char buffer[len+1];
			sprintf(buffer, fmt, mapping->fn,line+1,qouted);

			X86EmitAsmComment(buffer);
			return nextNodesToCompile(start);	
	}
	case IR_MEMBERS_ADDR_OF: {
		strGraphEdgeIRP in CLEANUP(strGraphEdgeIRPDestroy) = graphNodeIRIncoming(start);
		strGraphEdgeIRP inSource CLEANUP(strGraphEdgeIRPDestroy) = IRGetConnsOfType(in, IR_CONN_SOURCE_A);
		__auto_type sourceNode = graphEdgeIRIncoming(inSource[0]);

		struct IRNodeMembersAddrOf *mems = (void *)graphNodeIRValuePtr(start);
		strGraphEdgeIRP outAssn CLEANUP(strGraphEdgeIRPDestroy) = IRGetConnsOfType(out, IR_CONN_DEST);
		if (strGraphEdgeIRPSize(outAssn) == 1) {
			struct X86AddressingMode *oMode CLEANUP(X86AddrModeDestroy) = IRNode2AddrMode(graphEdgeIROutgoing(outAssn[0]));
			if (oMode->type == X86ADDRMODE_REG) {
				storeMemberPtrInReg(oMode->value.reg, sourceNode, mems->members);
			} else {
				AUTO_LOCK_MODE_REGS(oMode);
				struct reg *r = regForTypeExcludingConsumed(objectPtrCreate(&typeU0));
				pushReg(r);
				storeMemberPtrInReg(r, sourceNode, mems->members);
				struct X86AddressingMode *regMode CLEANUP(X86AddrModeDestroy) = X86AddrModeReg(r);
				regMode->valueType = IRNodeType(graphEdgeIROutgoing(outAssn[0]));
				asmTypecastAssign(oMode, regMode,0);
				popReg(r);
			}
		}
			
		return nextNodesToCompile(start);
	}
	case IR_ADD: {
#define COMPILE_87_IFN                                                                                                                                             \
	({                                                                                                                                                               \
		int compiled;                                                                                                                                                  \
		__auto_type tmp = compileX87IfNeded(start, &compiled);                                                                                                         \
		if (compiled)                                                                                                                                                  \
			return tmp;                                                                                                                                                  \
	})
		COMPILE_87_IFN;
		graphNodeIR a, b;
		binopArgs(start, &a, &b);
		if (isIntNode(a) || isPtrNode(a))
			return nextNodesToCompile(assembleOpInt(start, "ADD"));
		else
			assert(0);
	}
	case IR_ADDR_OF: {
		strGraphEdgeIRP out CLEANUP(strGraphEdgeIRPDestroy) = graphNodeIROutgoing(start);
		{
				strGraphEdgeIRP dst CLEANUP(strGraphEdgeIRPDestroy) = IRGetConnsOfType(out, IR_CONN_DEST);
				if (strGraphEdgeIRPSize(dst) == 1) {
						strGraphEdgeIRP in CLEANUP(strGraphEdgeIRPDestroy) = graphNodeIRIncoming(start);
						strGraphEdgeIRP source CLEANUP(strGraphEdgeIRPDestroy) = IRGetConnsOfType(in, IR_CONN_SOURCE_A);
						struct X86AddressingMode *iMode CLEANUP(X86AddrModeDestroy) = IRNode2AddrMode(graphEdgeIRIncoming(source[0]));
						struct X86AddressingMode *oMode CLEANUP(X86AddrModeDestroy) = IRNode2AddrMode(graphEdgeIROutgoing(dst[0]));
						AUTO_LOCK_MODE_REGS(iMode);
						if (iMode->type == X86ADDRMODE_MEM || iMode->type == X86ADDRMODE_ITEM_ADDR) {
								strX86AddrMode leaArgs CLEANUP(strX86AddrModeDestroy2)=NULL;
								leaArgs=strX86AddrModeAppendItem(leaArgs, X86AddrModeClone(oMode));
								leaArgs=strX86AddrModeAppendItem(leaArgs, X86AddrModeClone(iMode));
								leaArgs[1]->valueType=NULL;
								assembleOpcode(start, "LEA",  leaArgs);
						} else if(iMode->type==X86ADDRMODE_LABEL) {
								asmTypecastAssign(oMode, iMode, ASM_ASSIGN_X87FPU_POP);
						} else {
								fputs("IR_ADDR_OF needs an item that points to something", stderr);
								abort();
						}
				}
		}
		return nextNodesToCompile(start);
	}
	case IR_ARRAY_DECL: {
			strGraphEdgeIRP in CLEANUP(strGraphEdgeIRPDestroy)=IREdgesByPrec(start);
			strGraphNodeIRP dims CLEANUP(strGraphNodeIRPDestroy)=NULL;

			//Assign the current stack pointer to the output as we will push the memory for the array on the stack
			__auto_type out=nodeDest(start);
			struct X86AddressingMode *outMode CLEANUP(X86AddrModeDestroy)=IRNode2AddrMode(out);
			AUTO_LOCK_MODE_REGS(outMode);
			__auto_type reg32=regForTypeExcludingConsumed(&typeI32i);
			pushReg(reg32);
			
			struct objectArray *type=(void*)IRNodeType(start);
			long stackSize=0;
			for(long i=0;i!=strGraphEdgeIRPSize(in);i++) {
					switch(*graphEdgeIRValuePtr(in[i])) {
					case IR_CONN_ARRAY_DIM_1...IR_CONN_ARRAY_DIM_16: {
							__auto_type inMode=IRNode2AddrMode(graphEdgeIRIncoming(in[i]));
							AUTO_LOCK_MODE_REGS(inMode);
							
							strX86AddrMode mulArgs CLEANUP(strX86AddrModeDestroy2)=NULL;
							struct X86AddressingMode *stackPos=X86AddrModeIndirSIB(0,NULL, X86AddrModeReg(stackPointer()), X86AddrModeSint(4), &typeI32i);
							
							struct X86AddressingMode *reg32Mode CLEANUP(X86AddrModeDestroy)=X86AddrModeReg(reg32);
							reg32Mode->valueType=objectPtrCreate(&typeU0);
							asmTypecastAssign(reg32Mode, inMode, ASM_ASSIGN_X87FPU_POP);

							//Add an area on the stack,,we will start 1 and multiply  by the itemsize of each nested array's dim then mutlply my the itemsize
							strX86AddrMode pushArgs CLEANUP(strX86AddrModeDestroy2)=strX86AddrModeAppendItem(NULL, X86AddrModeClone(reg32Mode));
							assembleInst("PUSH", pushArgs);
							
							type=(void*)type->type;
							stackSize++;
					}		
					default:
							continue;
					}
			}
			struct X86AddressingMode *stackTop CLEANUP(X86AddrModeDestroy)=X86AddrModeIndirReg(stackPointer(), &typeI32i);
			struct X86AddressingMode *itemSize CLEANUP(X86AddrModeDestroy)=X86AddrModeSint(objectSize((struct object*)type, NULL));
			struct X86AddressingMode *reg32Mode CLEANUP(X86AddrModeDestroy)=X86AddrModeReg(reg32);
			itemSize->valueType=&typeI32i;
			reg32Mode->valueType=&typeI32i;
			asmTypecastAssign(reg32Mode, itemSize, 0);

			for(long s=0;s!=stackSize;s++) {
					strX86AddrMode imul2Args CLEANUP(strX86AddrModeDestroy)=NULL;
					imul2Args=strX86AddrModeAppendItem(imul2Args, reg32Mode);
					imul2Args=strX86AddrModeAppendItem(imul2Args, stackTop);
					assembleInst("IMUL2", imul2Args);
					
					//Add 4 from the stack pointer to "pop"
					strX86AddrMode addArgs CLEANUP(strX86AddrModeDestroy2)=NULL;
					addArgs=strX86AddrModeAppendItem(addArgs, X86AddrModeReg(stackPointer()));
					addArgs=strX86AddrModeAppendItem(addArgs, X86AddrModeSint(4));
					assembleInst("ADD", addArgs);
			}
			
			//Exchange the location of reg32's old value with reg32,then add the  value stored on the stack to the stack pointer
			strX86AddrMode xchgArgs CLEANUP(strX86AddrModeDestroy2)=NULL;
			xchgArgs=strX86AddrModeAppendItem(xchgArgs, X86AddrModeReg(reg32));
			xchgArgs=strX86AddrModeAppendItem(xchgArgs, X86AddrModeClone(stackTop));
			assembleInst("XCHG", xchgArgs);

			//Substract the data on top of the stack to the stack pointer to make room for the array
			strX86AddrMode subArgs CLEANUP(strX86AddrModeDestroy2)=NULL;
			subArgs=strX86AddrModeAppendItem(subArgs,X86AddrModeReg(stackPointer()));
			subArgs=strX86AddrModeAppendItem(subArgs, X86AddrModeClone(stackTop));
			assembleInst("SUB", subArgs);
			
			//Assign the stack pointer to the output position which will point to the area on the stack
			struct X86AddressingMode *spMode CLEANUP(X86AddrModeDestroy)=X86AddrModeReg(stackPointer());
			asmAssign(outMode, spMode , ptrSize(), 0);
			return nextNodesToCompile(start);
	}
	case IR_TYPECAST: {
			strGraphEdgeIRP in CLEANUP(strGraphEdgeIRPDestroy) = graphNodeIRIncoming(start);
			__auto_type inNode = graphEdgeIRIncoming(in[0]);
			strGraphEdgeIRP out CLEANUP(strGraphEdgeIRPDestroy) = graphNodeIROutgoing(start);
			assert(strGraphEdgeIRPSize(out) == 1);
			__auto_type outNode = graphEdgeIROutgoing(out[0]);
		struct X86AddressingMode *aMode CLEANUP(X86AddrModeDestroy) = IRNode2AddrMode(inNode);
		struct X86AddressingMode *oMode CLEANUP(X86AddrModeDestroy) = IRNode2AddrMode(outNode);
		asmTypecastAssign(oMode, aMode,0);
		return nextNodesToCompile(outNode);
	}
	case IR_STATEMENT_START: {
		struct IRNodeStatementStart *stmtStart = (void *)graphNodeIRValuePtr(start);
		IR2Asm(stmtStart->end);
		return nextNodesToCompile(stmtStart->end);
	}
	case IR_STATEMENT_END: {
		strGraphNodeIRP nodes CLEANUP(strGraphNodeIRPDestroy) = graphNodeIRIncomingNodes(start);
		assert(strGraphNodeIRPSize(nodes) == 1);
		assert(graphNodeIRValuePtr(nodes[0])->type != IR_STATEMENT_START);
		IR2Asm(nodes[0]);
		return nextNodesToCompile(nodes[0]);
	}
	case IR_INC: {
		COMPILE_87_IFN;

		strGraphEdgeIRP in CLEANUP(strGraphEdgeIRPDestroy) = graphNodeIRIncoming(start);
		__auto_type inNode = graphEdgeIRIncoming(in[0]);
		if (isIntNode(inNode)) {
			strX86AddrMode args CLEANUP(strX86AddrModeDestroy2) = strX86AddrModeAppendItem(NULL, IRNode2AddrMode(inNode));
			assembleInst("INC", args);
		} else if (isPtrNode(inNode)) {
			struct objectPtr *ptr = (void *)IRNodeType(inNode);
			// ADD ptr,ptrSize
			struct X86AddressingMode *iMode CLEANUP(X86AddrModeDestroy) = IRNode2AddrMode(start);
			AUTO_LOCK_MODE_REGS(iMode);

			strX86AddrMode addArgs CLEANUP(strX86AddrModeDestroy2) = strX86AddrModeAppendItem(NULL, X86AddrModeClone(iMode));
			addArgs = strX86AddrModeAppendItem(addArgs, X86AddrModeUint(objectSize(ptr->type, NULL)));
			assembleOpcode(start, "ADD", addArgs);
		} else
			assert(0);

		if (strGraphEdgeIRPSize(out))
				asmAssign(IRNode2AddrMode(graphEdgeIROutgoing(out[0])), IRNode2AddrMode(inNode), objectSize(IRNodeType(start), NULL),0);
		return strGraphNodeIRPAppendItem(NULL, nodeDest(start));
	}
	case IR_DEC: {
		COMPILE_87_IFN;

		strGraphEdgeIRP in CLEANUP(strGraphEdgeIRPDestroy) = graphNodeIRIncoming(start);
		__auto_type inNode = graphEdgeIRIncoming(in[0]);
		if (isIntNode(inNode)) {
			strX86AddrMode args CLEANUP(strX86AddrModeDestroy2) = strX86AddrModeAppendItem(NULL, IRNode2AddrMode(inNode));
			assembleInst("DEC", args);
		} else if (isPtrNode(inNode)) {
			struct objectPtr *ptr = (void *)IRNodeType(inNode);
			// ADD ptr,ptrSize
			struct X86AddressingMode *iMode CLEANUP(X86AddrModeDestroy) = IRNode2AddrMode(start);

			strX86AddrMode addArgs CLEANUP(strX86AddrModeDestroy2) = strX86AddrModeAppendItem(NULL, X86AddrModeClone(iMode));
			addArgs = strX86AddrModeAppendItem(addArgs, X86AddrModeUint(objectSize(ptr->type, NULL)));
			assembleOpcode(start, "SUB", addArgs);
		} else
			assert(0);

		if (strGraphEdgeIRPSize(out))
				asmAssign(IRNode2AddrMode(graphEdgeIROutgoing(out[0])), IRNode2AddrMode(inNode), objectSize(IRNodeType(start), NULL),0);
		return strGraphNodeIRPAppendItem(NULL, nodeDest(start));
	}
	case IR_SUB: {
		int compiled;
		__auto_type tmp = compileX87IfNeded(start, &compiled);
		if (compiled)
			return tmp;
		graphNodeIR a, b;
		binopArgs(start, &a, &b);
		// Are assumed to be same type if valid IR graph
		if (isIntNode(a) || isPtrNode(a))
			return strGraphNodeIRPAppendItem(NULL, assembleOpInt(start, "SUB"));
		assert(0);
		return NULL;
	}
	case IR_POS: {
		COMPILE_87_IFN;

		strGraphEdgeIRP in CLEANUP(strGraphEdgeIRPDestroy) = graphNodeIRIncoming(start);
		__auto_type inNode = graphEdgeIRIncoming(in[0]);
		asmAssign(IRNode2AddrMode(graphEdgeIROutgoing(out[0])), IRNode2AddrMode(inNode), objectSize(IRNodeType(start), NULL),0);
		return strGraphNodeIRPAppendItem(NULL, nodeDest(start));
	}
	case IR_NEG: {
		COMPILE_87_IFN;

		strGraphEdgeIRP in CLEANUP(strGraphEdgeIRPDestroy) = graphNodeIRIncoming(start);
		__auto_type inNode = graphEdgeIRIncoming(in[0]);
		if (isIntNode(inNode) || isPtrNode(inNode)) {
			// MOV dest,source
			// NOT dest
			__auto_type outNode = graphEdgeIROutgoing(out[0]);
			struct X86AddressingMode *oMode CLEANUP(X86AddrModeDestroy) = IRNode2AddrMode(outNode);
			struct X86AddressingMode *iMode CLEANUP(X86AddrModeDestroy) = IRNode2AddrMode(inNode);
			
			asmAssign(oMode, iMode, objectSize(IRNodeType(inNode), NULL),0);
			strX86AddrMode nArgs CLEANUP(strX86AddrModeDestroy2) = strX86AddrModeAppendItem(NULL, X86AddrModeClone(oMode));
			assembleOpcode(start,"NEG", nArgs);
			
			return strGraphNodeIRPAppendItem(NULL, nodeDest(start));
		}
		assert(0);
	}
	case IR_MULT: {
		COMPILE_87_IFN;

		graphNodeIR a, b;
		binopArgs(start, &a, &b);
		// Are assumed to be same type if valid IR graph

		if (isIntNode(a) || isPtrNode(a)) {
			if (typeIsSigned(IRNodeType(a))) {
				assembleOpInt(start, "IMUL2");
			} else {
				struct X86AddressingMode *aMode CLEANUP(X86AddrModeDestroy) = IRNode2AddrMode(a);
				struct X86AddressingMode *bMode CLEANUP(X86AddrModeDestroy) = IRNode2AddrMode(b);
				//
				// RAX is assigned into by the first operand,so if the second operand if RAX,swap the order of a and b
				//
				if (bMode->type == X86ADDRMODE_REG) {
					if (regConflict(&regAMD64RAX, bMode->value.reg)) {
						__auto_type tmp = aMode;
						aMode = bMode;
						bMode = tmp;
					}
				}

				strX86AddrMode mulArgs CLEANUP(strX86AddrModeDestroy2) = NULL;
				strX86AddrMode ppArgsA CLEANUP(strX86AddrModeDestroy2) = NULL;
				strX86AddrMode ppArgsD CLEANUP(strX86AddrModeDestroy2) = NULL;

				strRegP pushPopRegs CLEANUP(strRegPDestroy) = NULL;
				struct reg *outReg = NULL;

				long outSize = objectSize(IRNodeType(a), NULL);
				switch (outSize) {
				case 1: {
					struct X86AddressingMode *alMode CLEANUP(X86AddrModeDestroy) = X86AddrModeReg(&regX86AL);
					asmAssign(alMode, aMode, 1,0);
					mulArgs = strX86AddrModeAppendItem(mulArgs, X86AddrModeClone(bMode));
					pushPopRegs = strRegPAppendItem(pushPopRegs, &regX86AX);
					outReg = &regX86AL;
					break;
				}
				case 2: {
					struct X86AddressingMode *axMode CLEANUP(X86AddrModeDestroy) = X86AddrModeReg(&regX86AX);
					asmAssign(axMode, aMode, 2,0);
					mulArgs = strX86AddrModeAppendItem(mulArgs, X86AddrModeClone(bMode));
					pushPopRegs = strRegPAppendItem(pushPopRegs, &regX86AX);
					pushPopRegs = strRegPAppendItem(pushPopRegs, &regX86DX);
					outReg = &regX86AX;
					break;
				}
				case 4: {
					struct X86AddressingMode *eaxMode CLEANUP(X86AddrModeDestroy) = X86AddrModeReg(&regX86EAX);
					asmAssign(eaxMode, aMode, 4,0);
					mulArgs = strX86AddrModeAppendItem(mulArgs, X86AddrModeClone(bMode));
					pushPopRegs = strRegPAppendItem(pushPopRegs, &regX86EAX);
					pushPopRegs = strRegPAppendItem(pushPopRegs, &regX86EDX);
					outReg = &regX86EAX;
					break;
				}
				case 8: {
					struct X86AddressingMode *raxMode CLEANUP(X86AddrModeDestroy) = X86AddrModeReg(&regAMD64RAX);
					asmAssign(raxMode, aMode, 8,0);
					mulArgs = strX86AddrModeAppendItem(mulArgs, X86AddrModeClone(bMode));
					pushPopRegs = strRegPAppendItem(pushPopRegs, &regAMD64RAX);
					pushPopRegs = strRegPAppendItem(pushPopRegs, &regAMD64RDX);
					outReg = &regAMD64RAX;
					break;
				}
					assert(0);
				}

				// Make room on stack for  result
				strX86AddrMode addSPArgs CLEANUP(strX86AddrModeDestroy2) = NULL;
				addSPArgs = strX86AddrModeAppendItem(addSPArgs, X86AddrModeSint(outSize));
				addSPArgs = strX86AddrModeAppendItem(addSPArgs, X86AddrModeReg(stackPointer()));
				assembleInst("ADD", addSPArgs);

				// Push uncomsumed registers that are affected
				for (long r = 0; r != strRegPSize(pushPopRegs); r++)
					pushReg(pushPopRegs[r]);

				struct X86AddressingMode *resRegMode CLEANUP(X86AddrModeDestroy) = X86AddrModeReg(outReg);
				assembleInst("MUL", mulArgs);

				// Assign to area below pushed args
				long offset = 0;
				for (long r = 0; r != strRegPSize(pushPopRegs); r++)
					offset += pushPopRegs[r]->size;
				struct X86AddressingMode *resultPointer =
				    X86AddrModeIndirSIB(0, 0, X86AddrModeReg((ptrSize() == 4 ? &regX86ESP : &regAMD64RSP)), X86AddrModeSint(-offset), IRNodeType(nodeDest(start)));
				asmAssign(resultPointer, resRegMode, outSize,0);

				// Pop uncomsumed registers that are affected
				for (long r = strRegPSize(pushPopRegs) - 1; r >= 0; r--)
					popReg(pushPopRegs[r]);

				struct X86AddressingMode *oMode CLEANUP(X86AddrModeDestroy) = IRNode2AddrMode(nodeDest(start));
				asmAssign(oMode, X86AddrModeIndirReg(stackPointer(), IRNodeType(nodeDest(start))), outSize,0);
				// Free room for result on stack
				assembleInst("SUB", addSPArgs);
			}
		}
		return strGraphNodeIRPAppendItem(NULL, nodeDest(start));
	}
	case IR_MOD:
	case IR_DIV: {
		COMPILE_87_IFN;

		graphNodeIR a, b;
		binopArgs(start, &a, &b);
		struct X86AddressingMode *outMode CLEANUP(X86AddrModeDestroy) = IRNode2AddrMode(graphEdgeIROutgoing(out[0]));
		AUTO_LOCK_MODE_REGS(outMode);
		struct reg *outReg = NULL;
		if (outMode->type == X86ADDRMODE_REG)
			outReg = outMode->value.reg;

			//
			// We will either use EAX or RAX if x86 or x86 respeectivly
			//
			// Also,we will not push/pop rdx or rax if it conflicts with the result register
			//
#define IF_NOT_CONFLICT(reg, code)                                                                                                                                 \
	({                                                                                                                                                               \
		if (out) {                                                                                                                                                     \
			if (outReg) {                                                                                                                                                \
				if (!regConflict(&reg, outReg)) {                                                                                                                          \
					code;                                                                                                                                                    \
				}                                                                                                                                                          \
			} else                                                                                                                                                       \
				code;                                                                                                                                                      \
		} else {                                                                                                                                                       \
			code;                                                                                                                                                        \
		}                                                                                                                                                              \
	})

		strX86AddrMode ppRAX CLEANUP(strX86AddrModeDestroy2) = NULL;
		strX86AddrMode ppRDX CLEANUP(strX86AddrModeDestroy2) = NULL;
		switch (objectSize(IRNodeType(start), NULL)) {
		case 1:
			IF_NOT_CONFLICT(regX86AH, ppRAX = strX86AddrModeAppendItem(NULL, X86AddrModeReg(&regX86AH)));
			IF_NOT_CONFLICT(regX86AL, ppRDX = strX86AddrModeAppendItem(NULL, X86AddrModeReg(&regX86AL)));
			break;
		case 2:
			IF_NOT_CONFLICT(regX86AX, ppRAX = strX86AddrModeAppendItem(NULL, X86AddrModeReg(&regX86AX)));
			IF_NOT_CONFLICT(regX86DX, ppRDX = strX86AddrModeAppendItem(NULL, X86AddrModeReg(&regX86DX)));
			break;
		case 4:
			IF_NOT_CONFLICT(regX86EAX, ppRAX = strX86AddrModeAppendItem(NULL, X86AddrModeReg(&regX86EAX)));
			IF_NOT_CONFLICT(regX86EDX, ppRDX = strX86AddrModeAppendItem(NULL, X86AddrModeReg(&regX86EDX)));
			break;
		case 8:
			IF_NOT_CONFLICT(regAMD64RAX, ppRAX = strX86AddrModeAppendItem(NULL, X86AddrModeReg(&regAMD64RAX)));
			IF_NOT_CONFLICT(regAMD64RDX, ppRDX = strX86AddrModeAppendItem(NULL, X86AddrModeReg(&regAMD64RDX)));
			break;
		}
		if (ppRAX)
			assembleInst("PUSH", ppRAX), consumeRegFromMode(ppRAX[0]);
		if (ppRDX)
			assembleInst("PUSH", ppRDX), consumeRegFromMode(ppRDX[0]);
		strX86AddrMode movRaxArgs CLEANUP(strX86AddrModeDestroy2) = NULL;
		strX86AddrMode dxorArgs CLEANUP(strX86AddrModeDestroy2) = NULL;
		strX86AddrMode divArgs CLEANUP(strX86AddrModeDestroy2) = NULL;

		struct X86AddressingMode *aMode CLEANUP(X86AddrModeDestroy) = IRNode2AddrMode(a);
		AUTO_LOCK_MODE_REGS(aMode);
		struct X86AddressingMode *bMode CLEANUP(X86AddrModeDestroy) = IRNode2AddrMode(b);
		AUTO_LOCK_MODE_REGS(bMode);

		const char *op = typeIsSigned(IRNodeType(start)) ? "IDIV" : "DIV";
		int isDivOrMod = graphNodeIRValuePtr(start)->type == IR_DIV;


		//We will pop them into rax/rdx
		pushMode(bMode);
		pushMode(aMode);
		switch (objectSize(IRNodeType(start), NULL)) {
		case 1:
				popReg(&regX86AX);
				divArgs = strX86AddrModeAppendItem(divArgs, X86AddrModeIndirReg(stackPointer(), &typeI8i));
				assembleOpcode(start,op, divArgs);
				//Dummy pop
				popReg(&regX86AX);
			if (isDivOrMod) {
				struct X86AddressingMode *al CLEANUP(X86AddrModeDestroy) = X86AddrModeReg(&regX86AL);
				asmAssign(outMode, al, 1,0);
			} else {
				struct X86AddressingMode *ah CLEANUP(X86AddrModeDestroy) = X86AddrModeReg(&regX86AH);
				asmAssign(outMode, ah, 1,0);
			}
			break;
		case 2:
				popReg(&regX86AX);
				divArgs = strX86AddrModeAppendItem(divArgs, X86AddrModeIndirReg(stackPointer(), &typeI16i));
			dxorArgs = strX86AddrModeAppendItem(dxorArgs, X86AddrModeReg(&regX86DX));
			dxorArgs = strX86AddrModeAppendItem(dxorArgs, X86AddrModeReg(&regX86DX));;
			assembleInst("XOR", dxorArgs);
			assembleOpcode(start,op, divArgs);
			//Dummy pop
				popReg(&regX86AX);
			if (isDivOrMod) {
				struct X86AddressingMode *ax CLEANUP(X86AddrModeDestroy) = X86AddrModeReg(&regX86AX);
				asmAssign(outMode, ax, 1,0);
			} else {
				struct X86AddressingMode *dx CLEANUP(X86AddrModeDestroy) = X86AddrModeReg(&regX86DX);
				asmAssign(outMode, dx, 1,0);
			}
			break;
		case 4:
				popReg(&regX86EAX);
				divArgs = strX86AddrModeAppendItem(divArgs, X86AddrModeIndirReg(stackPointer(), &typeI32i));
			dxorArgs = strX86AddrModeAppendItem(dxorArgs, X86AddrModeReg(&regX86EDX));
			dxorArgs = strX86AddrModeAppendItem(dxorArgs, X86AddrModeReg(&regX86EDX));
			assembleInst("XOR", dxorArgs);
			assembleOpcode(start,op, divArgs);
			//Dummy pop
			popReg(&regX86EAX);
			if (isDivOrMod) {
				struct X86AddressingMode *eax CLEANUP(X86AddrModeDestroy) = X86AddrModeReg(&regX86EAX);
				asmAssign(outMode, eax, 1,0);
			} else {
				struct X86AddressingMode *edx CLEANUP(X86AddrModeDestroy) = X86AddrModeReg(&regX86EDX);
				asmAssign(outMode, edx, 1,0);
			}
			break;
		case 8:
				popReg(&regAMD64RAX);
			divArgs = strX86AddrModeAppendItem(divArgs, X86AddrModeIndirReg(stackPointer(), &typeI64i));
			dxorArgs = strX86AddrModeAppendItem(dxorArgs, X86AddrModeReg(&regAMD64RDX));
			dxorArgs = strX86AddrModeAppendItem(dxorArgs, X86AddrModeReg(&regAMD64RDX));
			assembleInst("XOR", dxorArgs);
			assembleOpcode(start,op, divArgs);
			//Dummy pop
			popReg(&regAMD64RAX);
			if (isDivOrMod) {
				struct X86AddressingMode *rdx CLEANUP(X86AddrModeDestroy) = X86AddrModeReg(&regAMD64RDX);
				asmAssign(outMode, rdx, 1,0);
			} else {
				struct X86AddressingMode *rax CLEANUP(X86AddrModeDestroy) = X86AddrModeReg(&regAMD64RAX);
				asmAssign(outMode, rax, 1,0);
			}
			break;
		}
		
		if (ppRDX)
			assembleInst("POP", ppRDX), unconsumeRegFromMode(ppRDX[0]);
		if (ppRAX)
			assembleInst("POP", ppRAX), unconsumeRegFromMode(ppRAX[0]);
		return strGraphNodeIRPAppendItem(NULL, nodeDest(start));
	}
	case IR_POW: {
			COMPILE_87_IFN;
			graphNodeIR a,b,out=nodeDest(start);
			binopArgs(start,&a,&b);
			struct X86AddressingMode *aMode CLEANUP(X86AddrModeDestroy)=IRNode2AddrMode(a);
			struct X86AddressingMode *bMode CLEANUP(X86AddrModeDestroy)=IRNode2AddrMode(b);
			struct X86AddressingMode *oMode CLEANUP(X86AddrModeDestroy)=IRNode2AddrMode(out);
			strX86AddrMode args CLEANUP(strX86AddrModeDestroy)=NULL;
			args=strX86AddrModeAppendItem(args, aMode);
			args=strX86AddrModeAppendItem(args, bMode);
			switch(getCurrentArch()) {
			case ARCH_TEST_SYSV:
			case ARCH_X86_SYSV: {
					struct X86AddressingMode *powMode CLEANUP(X86AddrModeDestroy)=includeHCRTFunc("PowI32i");
					IRABICall2Asm(start, powMode, args, oMode);
					break;
			}
			case ARCH_X64_SYSV:;
					// TODO
					assert(0);
			};
			return nextNodesToCompile(start);
	}
	case IR_LOR: {
		COMPILE_87_IFN;

		struct X86AddressingMode *outMode CLEANUP(X86AddrModeDestroy) = IRNode2AddrMode(graphEdgeIROutgoing(out[0]));
		graphNodeIR a, b;
		binopArgs(start, &a, &b);
		struct X86AddressingMode *aMode CLEANUP(X86AddrModeDestroy) = IRNode2AddrMode(a);
		struct X86AddressingMode *bMode CLEANUP(X86AddrModeDestroy) = IRNode2AddrMode(b);
		// CMP a,0
		// JE next1
		// MOV out,1
		// next1:
		// CMP b,0
		// JE next2
		// MOV out,1
		// JMP end
		// next2:
		// MOV out,0
		// end:
		strChar endLab CLEANUP(strCharDestroy) = uniqueLabel("LOR");
		struct X86AddressingMode *one CLEANUP(X86AddrModeDestroy) = X86AddrModeSint(1);
		for (int i = 0; i != 2; i++) {
			strX86AddrMode cmpArgs CLEANUP(strX86AddrModeDestroy2) = strX86AddrModeAppendItem(NULL, X86AddrModeClone(i ? aMode : bMode));
			cmpArgs = strX86AddrModeAppendItem(cmpArgs, X86AddrModeSint(0));
			assembleOpcode(start,"CMP", cmpArgs);

			strChar nextLab CLEANUP(strCharDestroy) = uniqueLabel("LOR");
			strX86AddrMode jneArgs CLEANUP(strX86AddrModeDestroy2) = strX86AddrModeAppendItem(NULL, X86AddrModeLabel(nextLab));
			assembleOpcode(start,"JE", jneArgs);
			asmAssign(outMode, one, objectSize(IRNodeType(start), NULL),0);

			strX86AddrMode jmpArgs CLEANUP(strX86AddrModeDestroy2) = strX86AddrModeAppendItem(NULL, X86AddrModeLabel(endLab));
			assembleOpcode(start,"JMP", jmpArgs);

			// Returns copy of label name
			free(X86EmitAsmLabel(nextLab));
		}
		// MOV out,0
		struct X86AddressingMode *zero CLEANUP(X86AddrModeDestroy) = X86AddrModeSint(0);
		asmAssign(outMode, zero, objectSize(IRNodeType(start), NULL),0);

		// Returns copy of label name
		free(X86EmitAsmLabel(endLab));
		return strGraphNodeIRPAppendItem(NULL, nodeDest(start));
	}
	case IR_LXOR: {
		if (isFltNode(start)) {
			switch (getCurrentArch()) {
			case ARCH_TEST_SYSV:
			case ARCH_X86_SYSV: {
				compileX87Expr(start);
				return nextNodesToCompile(start);
			}
			case ARCH_X64_SYSV:;
			}
		}

		struct X86AddressingMode *outMode CLEANUP(X86AddrModeDestroy) = IRNode2AddrMode(graphEdgeIROutgoing(out[0]));
		graphNodeIR a, b;
		binopArgs(start, &a, &b);
		struct X86AddressingMode *aMode CLEANUP(X86AddrModeDestroy) = IRNode2AddrMode(a);
		struct X86AddressingMode *bMode CLEANUP(X86AddrModeDestroy) = IRNode2AddrMode(b);
		// CMP aMode,0
		// SETNE outMode
		// CMP bMode,0
		// JNE END
		// XOR outNode,1
		// end:
		struct X86AddressingMode *zero CLEANUP(X86AddrModeDestroy) = X86AddrModeSint(0);
		struct X86AddressingMode *one CLEANUP(X86AddrModeDestroy) = X86AddrModeSint(1);

		strX86AddrMode cmpAArgs CLEANUP(strX86AddrModeDestroy2) = strX86AddrModeAppendItem(NULL, X86AddrModeClone(aMode));
		cmpAArgs = strX86AddrModeAppendItem(cmpAArgs, X86AddrModeSint(0));
		assembleOpcode(start,"CMP", cmpAArgs);

		setCond(start,"NE", outMode);

		strX86AddrMode cmpBArgs CLEANUP(strX86AddrModeDestroy2) = strX86AddrModeAppendItem(NULL, X86AddrModeClone(bMode));
		cmpBArgs = strX86AddrModeAppendItem(cmpBArgs, X86AddrModeSint(0));
		assembleOpcode(start,"CMP", cmpBArgs);

		strChar endLabel CLEANUP(strCharDestroy) = uniqueLabel("XOR");
		strX86AddrMode jmpeArgs CLEANUP(strX86AddrModeDestroy2) = strX86AddrModeAppendItem(NULL, X86AddrModeLabel(endLabel));
		assembleOpcode(start,"JE", jmpeArgs);

		strX86AddrMode xorArgs CLEANUP(strX86AddrModeDestroy2) = strX86AddrModeAppendItem(NULL, X86AddrModeClone(outMode));
		xorArgs = strX86AddrModeAppendItem(xorArgs, X86AddrModeClone(one));
		assembleOpcode(start,"XOR", xorArgs);
		
		X86EmitAsmLabel(endLabel);
		return strGraphNodeIRPAppendItem(NULL, nodeDest(start));
	}
	case IR_LAND: {
		COMPILE_87_IFN;

		__auto_type outNode = graphEdgeIROutgoing(out[0]);
		struct X86AddressingMode *outMode CLEANUP(X86AddrModeDestroy) = IRNode2AddrMode(outNode);
		graphNodeIR a, b;
		binopArgs(start, &a, &b);
		struct X86AddressingMode *aMode CLEANUP(X86AddrModeDestroy) = IRNode2AddrMode(a);
		struct X86AddressingMode *bMode CLEANUP(X86AddrModeDestroy) = IRNode2AddrMode(b);
		// CMP aMode,0
		// JE emd
		// CMP bMode,0
		// JE end
		// MOV outMode,1
		// JUMP 
		// end:
		// MOV outMode,0
		// final:
		struct X86AddressingMode *zero CLEANUP(X86AddrModeDestroy) = X86AddrModeSint(0);
		struct X86AddressingMode *one CLEANUP(X86AddrModeDestroy) = X86AddrModeSint(1);
		strX86AddrMode cmpAArgs CLEANUP(strX86AddrModeDestroy2) = strX86AddrModeAppendItem(NULL, X86AddrModeClone(aMode));
		cmpAArgs = strX86AddrModeAppendItem(cmpAArgs, X86AddrModeSint(0));
		assembleOpcode(start,"CMP", cmpAArgs);

		strChar endLabel CLEANUP(strCharDestroy) = uniqueLabel("AND");
		strChar finalLabel CLEANUP(strCharDestroy) = uniqueLabel("AND_FINAL");
		strX86AddrMode jmpeArgs CLEANUP(strX86AddrModeDestroy2) = strX86AddrModeAppendItem(NULL, X86AddrModeLabel(endLabel));
		assembleOpcode(start,"JE", jmpeArgs);

		strX86AddrMode cmpBArgs CLEANUP(strX86AddrModeDestroy2) = strX86AddrModeAppendItem(NULL, X86AddrModeClone(bMode));
		cmpBArgs = strX86AddrModeAppendItem(cmpBArgs, X86AddrModeSint(0));
		assembleOpcode(start,"CMP", cmpBArgs);
		assembleOpcode(start,"JE", jmpeArgs);

		asmAssign(outMode, one, objectSize(IRNodeType(outNode), NULL),0);
		strX86AddrMode jmpFinal CLEANUP(strX86AddrModeDestroy2) = strX86AddrModeAppendItem(NULL, X86AddrModeLabel(finalLabel));
		assembleOpcode(start, "JMP", jmpFinal);
		
		X86EmitAsmLabel(endLabel);
		asmAssign(outMode, zero, objectSize(IRNodeType(outNode), NULL),0);
		X86EmitAsmLabel(finalLabel);

		return strGraphNodeIRPAppendItem(NULL, nodeDest(start));
	}
	case IR_LNOT: {
		COMPILE_87_IFN;

		strGraphEdgeIRP in CLEANUP(strGraphEdgeIRPDestroy) = graphNodeIRIncoming(start);
		__auto_type inNode = graphEdgeIRIncoming(in[0]);
		__auto_type outNode = graphEdgeIROutgoing(out[0]);
		// MOV outMode,0
		// CMP inMode,0
		// SETE outMode

		struct X86AddressingMode *zero CLEANUP(X86AddrModeDestroy) = X86AddrModeSint(0);
		struct X86AddressingMode *oMode CLEANUP(X86AddrModeDestroy) = IRNode2AddrMode(outNode);
		struct X86AddressingMode *iMode CLEANUP(X86AddrModeDestroy) = IRNode2AddrMode(inNode);
		
		strX86AddrMode cmpArgs CLEANUP(strX86AddrModeDestroy2) = strX86AddrModeAppendItem(NULL, X86AddrModeClone(iMode));
		cmpArgs=strX86AddrModeAppendItem(cmpArgs, X86AddrModeSint(0));
		assembleOpcode(start,"CMP", cmpArgs);

		setCond(start,"E", oMode);

		return strGraphNodeIRPAppendItem(NULL, nodeDest(start));
	}
	case IR_BNOT: {
		COMPILE_87_IFN;

		strGraphEdgeIRP in CLEANUP(strGraphEdgeIRPDestroy) = graphNodeIRIncoming(start);
		__auto_type inNode = graphEdgeIRIncoming(in[0]);
		__auto_type outNode = graphEdgeIROutgoing(out[0]);
		struct X86AddressingMode *oMode CLEANUP(X86AddrModeDestroy) = IRNode2AddrMode(outNode);
		struct X86AddressingMode *iMode CLEANUP(X86AddrModeDestroy) = IRNode2AddrMode(inNode);

		// MOv out,in
		// NOT out
		asmAssign(oMode, iMode, objectSize(IRNodeType(outNode), NULL),0);

		strX86AddrMode notArgs CLEANUP(strX86AddrModeDestroy2) = strX86AddrModeAppendItem(NULL, X86AddrModeClone(oMode));
		assembleOpcode(start,"NOT", notArgs);
		return strGraphNodeIRPAppendItem(NULL, nodeDest(start));
	}
	case IR_BAND: {
		COMPILE_87_IFN;

		if (isIntNode(start) || isPtrNode(start)) {
			assembleOpInt(start, "AND");
		} else
			assert(0);
		return strGraphNodeIRPAppendItem(NULL, nodeDest(start));
	}
	case IR_BXOR: {
		COMPILE_87_IFN;

		if (isIntNode(start) || isPtrNode(start)) {
			assembleOpInt(start, "XOR");
		} else
			assert(0);
		return strGraphNodeIRPAppendItem(NULL, nodeDest(start));
	}
	case IR_BOR: {
		COMPILE_87_IFN;

		if (isIntNode(start) || isPtrNode(start)) {
			assembleOpInt(start, "OR");
		} else
			assert(0);
		return strGraphNodeIRPAppendItem(NULL, nodeDest(start));
	}
	case IR_LSHIFT: {
		COMPILE_87_IFN;

		if (isIntNode(start) || isPtrNode(start)) {
			graphNodeIR a, b;
			binopArgs(start, &a, &b);
			if (typeIsSigned(IRNodeType(start)))
				assembleOpIntShift(start, "SAL");
			else
				assembleOpIntShift(start, "SHL");
		} else
			assert(0);
		return strGraphNodeIRPAppendItem(NULL, nodeDest(start));
	}
	case IR_RSHIFT: {
		COMPILE_87_IFN;

		if (isIntNode(start) || isPtrNode(start)) {
			graphNodeIR a, b;
			binopArgs(start, &a, &b);
			if (typeIsSigned(IRNodeType(start)))
				assembleOpIntShift(start, "SAR");
			else
				assembleOpIntShift(start, "SHR");
		} else
			assert(0);
		return strGraphNodeIRPAppendItem(NULL, nodeDest(start));
	}
	case IR_SIMD: {
		assert(0);
	}
	case IR_GT: {
		COMPILE_87_IFN;

		struct X86AddressingMode *oMode CLEANUP(X86AddrModeDestroy) = IRNode2AddrMode(nodeDest(start));
		graphNodeIR a, b;
		binopArgs(start, &a, &b);
		assembleOpCmp(start);
		if(isFltType(IRNodeType(a))) {
				setCond(start, "A", oMode);
		} else if (typeIsSigned(IRNodeType(a)) || typeIsSigned(IRNodeType(b))) {
				setCond(start, "G", oMode);
		} else {
				setCond(start, "A", oMode);
		}
		return strGraphNodeIRPAppendItem(NULL, nodeDest(start));
	}
	case IR_LT: {
		COMPILE_87_IFN;

		struct X86AddressingMode *oMode CLEANUP(X86AddrModeDestroy) = IRNode2AddrMode(nodeDest(start));
		graphNodeIR a, b;
		binopArgs(start, &a, &b);
		assembleOpCmp(start);
		if(isFltType(IRNodeType(a))) {
				setCond(start, "B", oMode);
		} else if (typeIsSigned(IRNodeType(a)) || typeIsSigned(IRNodeType(b))) {
				setCond(start, "L",oMode);
		} else {
				setCond(start, "B",oMode);
		}
		return strGraphNodeIRPAppendItem(NULL, nodeDest(start));
	}
	case IR_GE: {
		COMPILE_87_IFN;

		struct X86AddressingMode *oMode CLEANUP(X86AddrModeDestroy) = IRNode2AddrMode(nodeDest(start));
		graphNodeIR a, b;
		binopArgs(start, &a, &b);
		assembleOpCmp(start);
		if(isFltType(IRNodeType(a))) {
				setCond(start, "AE", oMode);
		} else  if (typeIsSigned(IRNodeType(a)) || typeIsSigned(IRNodeType(b))) {
				setCond(start, "GE",oMode);
		} else {
				setCond(start, "AE",oMode);
		}
		return strGraphNodeIRPAppendItem(NULL, nodeDest(start));
	}
	case IR_LE: {
		COMPILE_87_IFN;

		struct X86AddressingMode *oMode CLEANUP(X86AddrModeDestroy) = IRNode2AddrMode(nodeDest(start));
		graphNodeIR a, b;
		binopArgs(start, &a, &b);
		assembleOpCmp(start);
		if(isFltType(IRNodeType(a))) {
				setCond(start, "BE", oMode);
		} else if (typeIsSigned(IRNodeType(a)) || typeIsSigned(IRNodeType(b))) {
				setCond(start, "LE",oMode);
		} else {
				setCond(start, "BE",oMode);
		}
		return strGraphNodeIRPAppendItem(NULL, nodeDest(start));
	}
	case IR_EQ: {
		COMPILE_87_IFN;

		struct X86AddressingMode *oMode CLEANUP(X86AddrModeDestroy) = IRNode2AddrMode(nodeDest(start));
		assembleOpCmp(start);
		setCond(start, "E",oMode);
		return strGraphNodeIRPAppendItem(NULL, nodeDest(start));
	}
	case IR_NE: {
		COMPILE_87_IFN;

		struct X86AddressingMode *oMode CLEANUP(X86AddrModeDestroy) = IRNode2AddrMode(nodeDest(start));
		assembleOpCmp(start);
		setCond(start, "NE",oMode);
		return strGraphNodeIRPAppendItem(NULL, nodeDest(start));
	}
	case IR_COND_JUMP: {
		strGraphEdgeIRP in CLEANUP(strGraphEdgeIRPDestroy) = graphNodeIRIncoming(start);
		strGraphEdgeIRP inSource CLEANUP(strGraphEdgeIRPDestroy) = IRGetConnsOfType(in, IR_CONN_SOURCE_A);
		__auto_type inNode = graphEdgeIRIncoming(inSource[0]);
		struct X86AddressingMode *inMode CLEANUP(X86AddrModeDestroy) = IRNode2AddrMode(inNode);

		strGraphEdgeIRP out CLEANUP(strGraphEdgeIRPDestroy) = graphNodeIROutgoing(start);
		strGraphEdgeIRP outT CLEANUP(strGraphEdgeIRPDestroy) = IRGetConnsOfType(out, IR_CONN_COND_TRUE);
		strGraphEdgeIRP outF CLEANUP(strGraphEdgeIRPDestroy) = IRGetConnsOfType(out, IR_CONN_COND_FALSE);

		__auto_type trueNode = graphEdgeIROutgoing(outT[0]);
		__auto_type falseNode = graphEdgeIROutgoing(outF[0]);
		struct X86AddressingMode *trueLab CLEANUP(X86AddrModeDestroy) = IRNode2AddrMode(trueNode);
		struct X86AddressingMode *falseLab CLEANUP(X86AddrModeDestroy) = IRNode2AddrMode(falseNode);

		strX86AddrMode cmpArgs CLEANUP(strX86AddrModeDestroy2) = NULL;
		cmpArgs = strX86AddrModeAppendItem(cmpArgs, X86AddrModeClone(inMode));
		cmpArgs = strX86AddrModeAppendItem(cmpArgs, X86AddrModeSint(0));
		assembleOpcode(inNode,"CMP",cmpArgs);
		
		strX86AddrMode jmpTArgs CLEANUP(strX86AddrModeDestroy2) = NULL;
		jmpTArgs = strX86AddrModeAppendItem(jmpTArgs, X86AddrModeClone(trueLab));
		assembleOpcode(start,"JNE", jmpTArgs);

		strX86AddrMode jmpFArgs CLEANUP(strX86AddrModeDestroy2) = NULL;
		jmpFArgs = strX86AddrModeAppendItem(jmpFArgs, X86AddrModeClone(falseLab));
		assembleOpcode(start,"JE", jmpFArgs);

		__auto_type retVal = strGraphNodeIRPAppendItem(NULL, trueNode);
		retVal = strGraphNodeIRPAppendItem(retVal, falseNode);
		return retVal;
	}
	case IR_JUMP_TAB: {
		strGraphEdgeIRP in CLEANUP(strGraphEdgeIRPDestroy) = graphNodeIRIncoming(start);
		strGraphEdgeIRP inSource CLEANUP(strGraphEdgeIRPDestroy) = IRGetConnsOfType(in, IR_CONN_SOURCE_A);
		__auto_type inNode = graphEdgeIRIncoming(inSource[0]);
		struct X86AddressingMode *inMode CLEANUP(X86AddrModeDestroy) = IRNode2AddrMode(inNode);

		struct IRNodeJumpTable *table = (void *)graphNodeIRValuePtr(start);
		strIRTableRange ranges CLEANUP(strIRTableRangeDestroy) = strIRTableRangeClone(table->labels);
		qsort(ranges, strIRTableRangeSize(ranges), sizeof(*ranges), (int (*)(const void *, const void *))IRTableRangeCmp);
		int64_t smallest = ranges[0].start;
		int64_t largest = ranges[strIRTableRangeSize(ranges) - 1].end;
		int64_t diff = 1+largest - smallest;

		strGraphEdgeIRP out CLEANUP(strGraphEdgeIRPDestroy) = graphNodeIROutgoing(start);
		strGraphEdgeIRP outDft CLEANUP(strGraphEdgeIRPDestroy) = IRGetConnsOfType(out, IR_CONN_DFT);
		assert(strGraphEdgeIRPSize(outDft) == 1);
		__auto_type dftNode = graphEdgeIROutgoing(outDft[0]);

		strX86AddrMode jmpTable CLEANUP(strX86AddrModeDestroy2) = strX86AddrModeResize(NULL, diff);
		for (long i = 0; i != diff; i++) {
			jmpTable[i] = IRNode2AddrMode(dftNode);
		}
		for (long i = 0; i != strIRTableRangeSize(ranges); i++) {
			for (long j = ranges[i].start; j != 1+ranges[i].end ; j++) {
				X86AddrModeDestroy(&jmpTable[j-smallest]);
				jmpTable[j-smallest] = IRNode2AddrMode(ranges[i].to);
			}
		}

		// Returns copy of label
		struct X86AddressingMode *jmpTabLabMode CLEANUP(X86AddrModeDestroy) = NULL;
		switch (ptrSize()) {
		case 2:
			fprintf(stderr, "HolyC on a commodore 64 isn't possible yet,bro.\n");
			assert(0);
		case 4: {
			jmpTabLabMode=X86EmitAsmDU32(jmpTable, strX86AddrModeSize(jmpTable));
			break;
		}
		case 8: {
			jmpTabLabMode=X86EmitAsmDU64(jmpTable, strX86AddrModeSize(jmpTable));
			break;
		}
		default:
			fprintf(stderr, "Are you in the future where 64bit address space is obselete?\n");
			assert(0);
		}

		//IF out of bounds of jump table,go to defualt
		strX86AddrMode cmpArgs1 CLEANUP(strX86AddrModeDestroy2) = NULL;
		cmpArgs1 = strX86AddrModeAppendItem(cmpArgs1, X86AddrModeClone(inMode));
		cmpArgs1 = strX86AddrModeAppendItem(cmpArgs1, X86AddrModeSint(smallest));
		assembleOpcode(NULL,"CMP", cmpArgs1);
		
		strX86AddrMode jmpDftArgs CLEANUP(strX86AddrModeDestroy2) = NULL;
		jmpDftArgs = strX86AddrModeAppendItem(jmpDftArgs, IRNode2AddrMode(dftNode));
		assembleInst("JL", jmpDftArgs);

		strX86AddrMode cmpArgs2 CLEANUP(strX86AddrModeDestroy2) = NULL;
		cmpArgs2 = strX86AddrModeAppendItem(cmpArgs2, X86AddrModeClone(inMode));
		cmpArgs2 = strX86AddrModeAppendItem(cmpArgs2, X86AddrModeSint(largest));
		assembleInst("CMP", cmpArgs2);

		assembleInst("JG", jmpDftArgs);

		AUTO_LOCK_MODE_REGS(inMode);
		struct reg *b = regForTypeExcludingConsumed((struct object *)getTypeForSize(ptrSize()));
		struct X86AddressingMode *regMode CLEANUP(X86AddrModeDestroy) = X86AddrModeReg(b);
		AUTO_LOCK_MODE_REGS(regMode);
		pushReg(b);

		//Load the jump-table address onto the stack,then pop
		asmAssign(regMode, jmpTabLabMode, ptrSize(),0);

		struct reg *indexReg = regForTypeExcludingConsumed((struct object *)getTypeForSize(ptrSize()));
		pushReg(indexReg);
		struct X86AddressingMode *indexRegMode CLEANUP(X86AddrModeDestroy) = X86AddrModeReg(indexReg);
		indexRegMode->valueType=&typeI32i;
		asmTypecastAssign(indexRegMode, inMode, 0);
		

		strX86AddrMode subArgs CLEANUP(strX86AddrModeDestroy2)=NULL;
		subArgs=strX86AddrModeAppendItem(subArgs, X86AddrModeReg(indexReg));
		subArgs=strX86AddrModeAppendItem(subArgs, X86AddrModeSint(smallest));
		assembleOpcode(start, "SUB", subArgs);
		
		//Add the offset of the input to b for the offset
		strX86AddrMode leaArgs CLEANUP(strX86AddrModeDestroy2) = NULL;
		leaArgs=strX86AddrModeAppendItem(leaArgs,X86AddrModeReg(b));
		leaArgs=strX86AddrModeAppendItem(leaArgs,X86AddrModeIndirSIB(ptrSize(), X86AddrModeReg(indexReg), X86AddrModeReg(b), NULL, objectPtrCreate(&typeU0)));
		assembleOpcode(start,"LEA", leaArgs);
		popReg(indexReg);
		
		strX86AddrMode movArgs CLEANUP(strX86AddrModeDestroy2) = NULL;
		movArgs=strX86AddrModeAppendItem(movArgs,X86AddrModeReg(b));
		movArgs = strX86AddrModeAppendItem(movArgs, X86AddrModeIndirSIB(ptrSize(), NULL, X86AddrModeReg(b), NULL, (struct object *)getTypeForSize(ptrSize())));
		assembleOpcode(start,"MOV", movArgs);

		strX86AddrMode xchgArgs CLEANUP(strX86AddrModeDestroy2) = NULL;
		xchgArgs=strX86AddrModeAppendItem(xchgArgs,X86AddrModeReg(b));
		xchgArgs=strX86AddrModeAppendItem(xchgArgs,X86AddrModeIndirReg(stackPointer(), objectPtrCreate(&typeU0)));																																		
		assembleOpcode(start,"XCHG", xchgArgs);
		
		assembleInst("RET", NULL); 

		strGraphNodeIRP retVal = strGraphNodeIRPAppendItem(NULL, graphEdgeIROutgoing(outDft[0]));
		strGraphEdgeIRP outCases CLEANUP(strGraphEdgeIRPDestroy) = IRGetConnsOfType(out, IR_CONN_CASE);
		for (long i = 0; i!=strGraphEdgeIRPSize(outCases); i++)
			retVal = strGraphNodeIRPAppendItem(retVal, graphEdgeIROutgoing(outCases[i]));
		return retVal;
	}
	case IR_VALUE: {
			if(objectBaseType(IRNodeType(start))==&typeF64) {
					compileX87Expr(start);
					return strGraphNodeIRPAppendItem(NULL, nodeDest(start));
			}
			struct X86AddressingMode *currMode CLEANUP(X86AddrModeDestroy) = IRNode2AddrMode(start);
		AUTO_LOCK_MODE_REGS(currMode);

		strGraphEdgeIRP incoming CLEANUP(strGraphEdgeIRPDestroy) = graphNodeIRIncoming(start);
		strGraphEdgeIRP assigns CLEANUP(strGraphEdgeIRPDestroy) = IRGetConnsOfType(incoming, IR_CONN_DEST);
		if (strGraphEdgeIRPSize(assigns) == 1) {
			// Operator automatically assign into thier destination,so ensure isn't an operator.
			if (graphNodeIRValuePtr(graphEdgeIRIncoming(assigns[0]))->type == IR_VALUE)
				asmAssign(currMode, IRNode2AddrMode(graphEdgeIRIncoming(assigns[0])), objectSize(IRNodeType(start), NULL),0);
		}
		strGraphEdgeIRP fromPtrAssigns CLEANUP(strGraphEdgeIRPDestroy) = IRGetConnsOfType(incoming, IR_CONN_ASSIGN_FROM_PTR);
		if (strGraphEdgeIRPSize(fromPtrAssigns) == 1) {
			__auto_type incoming = graphEdgeIRIncoming(fromPtrAssigns[0]);
			struct X86AddressingMode *inMode CLEANUP(X86AddrModeDestroy) = IRNode2AddrMode(incoming);
			asmAssignFromPtr(currMode, inMode,objectSize(inMode->valueType, NULL),0);
		}
		return nextNodesToCompile(start);
	}
	case IR_LABEL_LOCAL:
	case IR_LABEL: {
		X86EmitAsmLabel(getLabelName(start));
		return nextNodesToCompile(start);
	}
	case IR_X86_INST: {
		struct IRNodeX86Inst *inst = (void *)graphNodeIRValuePtr(start);
		assembleInst(inst->name, inst->args);
		return nextNodesToCompile(start);
	}
	case IR_ASM_DU8: {
		struct IRNodeAsmDU8 *du8 = (void *)graphNodeIRValuePtr(start);
		strX86AddrMode addrModes CLEANUP(strX86AddrModeDestroy2) = NULL;
		for (long i = 0; i != du8->count; i++)
			addrModes = strX86AddrModeAppendItem(addrModes, X86AddrModeUint(du8->data[i]));
		X86EmitAsmDU8(addrModes, du8->count);
		return nextNodesToCompile(start);
	}
	case IR_ASM_DU16: {
		struct IRNodeAsmDU16 *du16 = (void *)graphNodeIRValuePtr(start);
		strX86AddrMode addrModes CLEANUP(strX86AddrModeDestroy2) = NULL;
		for (long i = 0; i != du16->count; i++)
			addrModes = strX86AddrModeAppendItem(addrModes, X86AddrModeUint(du16->data[i]));
		X86EmitAsmDU16(addrModes, du16->count);
		return nextNodesToCompile(start);
	}
	case IR_ASM_DU32: {
		struct IRNodeAsmDU32 *du32 = (void *)graphNodeIRValuePtr(start);
		strX86AddrMode addrModes CLEANUP(strX86AddrModeDestroy2) = NULL;
		for (long i = 0; i != du32->count; i++)
			addrModes = strX86AddrModeAppendItem(addrModes, X86AddrModeUint(du32->data[i]));
		X86EmitAsmDU32(addrModes, du32->count);
		return nextNodesToCompile(start);
	}
	case IR_ASM_DU64: {
		struct IRNodeAsmDU64 *du64 = (void *)graphNodeIRValuePtr(start);
		strX86AddrMode addrModes CLEANUP(strX86AddrModeDestroy2) = NULL;
		for (long i = 0; i != du64->count; i++)
			addrModes = strX86AddrModeAppendItem(addrModes, X86AddrModeUint(du64->data[i]));
		X86EmitAsmDU64(addrModes, du64->count);
		return nextNodesToCompile(start);
	}
	case IR_ASM_IMPORT: {
		struct IRNodeAsmImport *import = (void *)graphNodeIRValuePtr(start);
		X86EmitAsmIncludeBinfile(import->fileName);
		return nextNodesToCompile(start);
	}
	case IR_FUNC_CALL: {
		IRABIFuncNode2Asm(start);
		return nextNodesToCompile(start);
	}
	case IR_FUNC_ARG: {
	}
	case IR_FUNC_RETURN:
	case IR_FUNC_END: {
			IRABIReturn2Asm(start,frameSize);
		return NULL;
	}
	case IR_FUNC_START: {
		return nextNodesToCompile(start);
	}
	case IR_DERREF: {
		strGraphEdgeIRP in CLEANUP(strGraphEdgeIRPDestroy) = graphNodeIRIncoming(start);
		strGraphEdgeIRP source CLEANUP(strGraphEdgeIRPDestroy) = IRGetConnsOfType(in, IR_CONN_SOURCE_A);
		strGraphEdgeIRP out CLEANUP(strGraphEdgeIRPDestroy) = graphNodeIROutgoing(start);
		strGraphEdgeIRP dst CLEANUP(strGraphEdgeIRPDestroy) = IRGetConnsOfType(out, IR_CONN_DEST);
		struct X86AddressingMode *iMode CLEANUP(X86AddrModeDestroy) = IRNode2AddrMode(graphEdgeIRIncoming(source[0]));
		if(IRNodeType(start)->type==TYPE_ARRAY) {
				struct X86AddressingMode *oMode CLEANUP(X86AddrModeDestroy) = IRNode2AddrMode(graphEdgeIROutgoing(dst[0]));				
				asmTypecastAssign(oMode, iMode, 0);
				return nextNodesToCompile(start);
		}

		//Dont derefence function pointer
		if(IRNodeType(start)->type==TYPE_FUNCTION) {
				struct X86AddressingMode *oMode CLEANUP(X86AddrModeDestroy) = IRNode2AddrMode(graphEdgeIROutgoing(dst[0]));
				asmTypecastAssign(oMode, iMode, 0);
				return nextNodesToCompile(start);
		}

		AUTO_LOCK_MODE_REGS(iMode);
		__auto_type regAddr = regForTypeExcludingConsumed(objectPtrCreate(&typeU0));
		pushReg(regAddr);
		struct X86AddressingMode *regAddrMode CLEANUP(X86AddrModeDestroy) = X86AddrModeReg(regAddr);
		asmAssign(regAddrMode, iMode, ptrSize(),0);

		strGraphEdgeIRP inAsn CLEANUP(strGraphEdgeIRPDestroy) = IRGetConnsOfType(in, IR_CONN_DEST);
		if (strGraphEdgeIRPSize(inAsn) == 1) {
			struct X86AddressingMode *oMode CLEANUP(X86AddrModeDestroy) = X86AddrModeIndirReg(regAddr, IRNodeType(start));
			struct X86AddressingMode *iMode2 CLEANUP(X86AddrModeDestroy) = IRNode2AddrMode(graphEdgeIRIncoming(inAsn[0]));
			asmTypecastAssign(oMode, iMode2,0);
		}

		strGraphEdgeIRP inAsnFromPtr CLEANUP(strGraphEdgeIRPDestroy) = IRGetConnsOfType(in, IR_CONN_ASSIGN_FROM_PTR);
		if (strGraphEdgeIRPSize(inAsnFromPtr) == 1) {
			struct X86AddressingMode *oMode CLEANUP(X86AddrModeDestroy) = X86AddrModeIndirReg(regAddr, IRNodeType(start));
			struct X86AddressingMode *iMode2 CLEANUP(X86AddrModeDestroy) = IRNode2AddrMode(graphEdgeIRIncoming(inAsnFromPtr[0]));
			asmTypecastAssign(oMode, iMode2,0);
		}

		
		if (strGraphEdgeIRPSize(dst) == 1) {
			struct X86AddressingMode *oMode CLEANUP(X86AddrModeDestroy) = IRNode2AddrMode(graphEdgeIROutgoing(dst[0]));
			struct X86AddressingMode *iMode2 CLEANUP(X86AddrModeDestroy) = X86AddrModeIndirReg(regAddr, oMode->valueType);
			asmTypecastAssign(oMode, iMode2,0);
		}

		popReg(regAddr);
		return nextNodesToCompile(start);
	}
	case IR_MEMBERS: {
		struct IRNodeMembers *mems = (void *)graphNodeIRValuePtr(start);

		strGraphEdgeIRP in CLEANUP(strGraphEdgeIRPDestroy) = graphNodeIRIncoming(start);
		strGraphEdgeIRP inAssn CLEANUP(strGraphEdgeIRPDestroy) = IRGetConnsOfType(in, IR_CONN_DEST);
		strGraphEdgeIRP out CLEANUP(strGraphEdgeIRPDestroy) = graphNodeIROutgoing(start);
		strGraphEdgeIRP outAssn CLEANUP(strGraphEdgeIRPDestroy) = IRGetConnsOfType(out, IR_CONN_DEST);

		struct X86AddressingMode *inAssnMode CLEANUP(X86AddrModeDestroy) = NULL;
		if (strGraphEdgeIRPSize(inAssn) == 1) {
			inAssnMode = IRNode2AddrMode(graphEdgeIRIncoming(inAssn[0]));
			consumeRegFromMode(inAssnMode);
		}
		struct X86AddressingMode *outAssnMode CLEANUP(X86AddrModeDestroy) = NULL;
		if (strGraphEdgeIRPSize(outAssn) == 1) {
			outAssnMode = IRNode2AddrMode(graphEdgeIROutgoing(outAssn[0]));
			consumeRegFromMode(outAssnMode);
		}

		// Assign the memReg with pointer to source
		struct reg *memReg = regForTypeExcludingConsumed(IRNodeType(start));
		pushReg(memReg);

		// Load the address of the source arg into register
		struct X86AddressingMode *memRegMode CLEANUP(X86AddrModeDestroy) = X86AddrModeReg(memReg);
		memRegMode->valueType = IRNodeType(start);
		strGraphEdgeIRP source CLEANUP(strGraphEdgeIRPDestroy) = IRGetConnsOfType(in, IR_CONN_SOURCE_A);
		storeMemberPtrInReg(memReg, graphEdgeIRIncoming(source[0]), mems->members);

		X86AddrModeDestroy(&memRegMode);
		if(objectBaseType(IRNodeType(start))->type!=TYPE_ARRAY) 
				memRegMode = X86AddrModeIndirReg(memReg, IRNodeType(start));
		else {
				memRegMode = X86AddrModeReg(memReg);
				memRegMode->valueType=IRNodeType(start);
		}
		// Reserve a register for the member ptr
		if (strGraphEdgeIRPSize(inAssn) == 1) {
			struct X86AddressingMode *asnMode = IRNode2AddrMode(graphEdgeIRIncoming(inAssn[0]));
			asmTypecastAssign(memRegMode, asnMode,0);
		}

		strGraphEdgeIRP inAsnFromPtr CLEANUP(strGraphEdgeIRPDestroy) = IRGetConnsOfType(in, IR_CONN_ASSIGN_FROM_PTR);
		if (strGraphEdgeIRPSize(inAsnFromPtr) == 1) {
			struct X86AddressingMode *iMode2 CLEANUP(X86AddrModeDestroy) = IRNode2AddrMode(graphEdgeIRIncoming(inAsnFromPtr[0]));
			asmAssignFromPtr(memRegMode, iMode2,objectSize(memRegMode->valueType, NULL),0);
		}

		if (strGraphEdgeIRPSize(outAssn) == 1) {
			struct X86AddressingMode *toMode = IRNode2AddrMode(graphEdgeIROutgoing(outAssn[0]));
			asmTypecastAssign(toMode, memRegMode,0);
		}

		if (inAssnMode)
			unconsumeRegFromMode(inAssnMode);
		if (outAssnMode)
			unconsumeRegFromMode(outAssnMode);
		popReg(memReg);

		return nextNodesToCompile(start);
	}
	case IR_SUB_SWITCH_START_LABEL:
	case IR_SPILL_LOAD:
		assert(0);
	}
}
static int isNotArgEdge(const void *data, const graphEdgeIR *edge) {
	__auto_type type = *graphEdgeIRValuePtr(*edge);
	return !IRIsExprEdge(type);
}
static int isUnvisited(const void *data, const graphNodeIR *node) {
	return NULL != ptrMapCompiledNodesGet(compiledNodes, *node);
}
void __IR2AsmExpr(graphNodeIR start) {
computeArgs:;
	strGraphEdgeIRP incoming CLEANUP(strGraphEdgeIRPDestroy) = IREdgesByPrec(start);
	for (long a = 0; a != strGraphEdgeIRPSize(incoming); a++) {
		__IR2AsmExpr(graphEdgeIRIncoming(incoming[a]));
	}
	__IR2Asm(start);
}
static struct X86AddressingMode *includeHCRTFunc(const char *name) {
		__auto_type sym= parserGetGlobalSym(name);
		if(!sym) {
				fprintf(stderr,"Include HCRT.HC for \"%s\"\n", name);
				abort();
		}
		__auto_type name2=parserGetGlobalSymLinkageName(name);
		__auto_type retVal=X86AddrModeLabel(name2);
		retVal->valueType=parserGlobalSymType(name);
		return retVal;
}
void IR2Asm(graphNodeIR start) {
	strGraphNodeIRP next CLEANUP(strGraphNodeIRPDestroy) = NULL;
	if (IREndOfExpr(start) != start) {
		// Start of statement can be label,so compile label first(__IR2Asm only compiles node once)
		__auto_type exprStart = IRStmtStart(start);
		if (graphNodeIRValuePtr(exprStart)->type == IR_LABEL)
			__IR2Asm(exprStart);

		__IR2AsmExpr(IREndOfExpr(start));
		next = graphNodeIROutgoingNodes(IREndOfExpr(start));
	} else {
		next = __IR2Asm(start);
	}
	next = strGraphNodeIRPUnique(next, (gnCmpType)ptrPtrCmp, NULL);

	for (long n = 0; n != strGraphNodeIRPSize(next); n++)
		IR2Asm(next[n]);
}
