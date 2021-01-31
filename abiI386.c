#include <IR.h>
#include <IRLiveness.h>
#include <cleanup.h>
#include <ptrMap.h>
#include <registers.h>
static void *IR_ATTR_OLD_REG_SLICE = "OLD_REG_SLICE";
struct IRATTRoldRegSlice {
	struct IRAttr base;
	struct regSlice old;
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
			return NULL != strVariableSortedFind(*select, val->val.value.var.value.var, (varCmpType)ptrPtrCmp);

	return 0;
}
#define ARRAY_SIZE(array) (sizeof(array) / sizeof(*array))
PTR_MAP_FUNCS(struct parserVar *, struct reg *, Var2Reg);
void IR2ABI_i386(graphNodeIR start) {
	strGraphNodeIRP allNodes CLEANUP(strGraphNodeIRPDestroy) = graphNodeIRAllNodes(start);
	// Replace Registers with variables and do liveness analysis on the variables
	strRegP usedRegs CLEANUP(strRegPDestroy) = usedRegisters(allNodes);
	strVariable regVars CLEANUP(strVariableDestroy) = NULL;
	ptrMapVar2Reg var2Reg = ptrMapVar2RegCreate();
	for (long i = 0; i != strRegPSize(usedRegs); i++) {
		__auto_type newVar = IRCreateVirtVar(&typeI64i);
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
		newAttr.old = value->val.value.reg;
		IRAttrReplace(newNode, __llCreate(&newAttr, sizeof(newAttr)));
		strGraphNodeIRP dummy CLEANUP(strGraphNodeIRPDestroy) = strGraphNodeIRPAppendItem(NULL, allNodes[i]);
		graphReplaceWithNode(dummy, newNode, NULL, (void (*)(void *))IRNodeDestroy, sizeof(enum IRConnType));
		replacedNodes = strGraphNodeIRPSortedInsert(replacedNodes, allNodes[i], (gnCmpType)ptrPtrCmp);
		addedNodes = strGraphNodeIRPSortedInsert(addedNodes, newNode, (gnCmpType)ptrPtrCmp);
	}
	allNodes = strGraphNodeIRPSetDifference(allNodes, replacedNodes, (gnCmpType)ptrPtrCmp);
	allNodes = strGraphNodeIRPSetUnion(allNodes, addedNodes, (gnCmpType)ptrPtrCmp);
	//
	// This has a side effect of attributing basic block attributes to nodes,which tell the in/out variables for each node in an expression
	//
	strGraphNodeIRLiveP livenessGraphs CLEANUP(strGraphNodeIRLivePDestroy) = IRInterferenceGraphFilter(start, regVars, isSelectVariable);
	for (long i = 0; i != strGraphNodeIRPSize(allNodes); i++) {
		struct IRNodeFuncCall *call = (void *)graphNodeIRValuePtr(allNodes[i]);
		if (call->base.type != IR_FUNC_CALL)
			continue;
		strGraphEdgeIRP incoming CLEANUP(strGraphEdgeIRPDestroy) = graphNodeIRIncoming(allNodes[i]);
		strGraphEdgeIRP func CLEANUP(strGraphEdgeIRPDestroy) = IRGetConnsOfType(incoming, IR_CONN_FUNC);
		// ABI fun time
		// http://www.sco.com/developers/devspecs/abi386-4.pdf
		struct reg *toPreserve[] = {&regX86EBX, &regX86ESI, &regX86EDI, &regX86EBP, &regX86ESP};
		qsort(toPreserve, sizeof(toPreserve) / sizeof(*toPreserve), sizeof(toPreserve), ptrPtrCmp);
		//
		// ecx,edx are destroyed,so backup
		//
		struct reg *destroyed[] = {&regX86ECX, &regX86EDX};
		qsort(destroyed, sizeof(destroyed) / sizeof(*destroyed), sizeof(destroyed), ptrPtrCmp);
		// x87 "barrel stack" needs to be emptied,
		struct reg *toEmpty[] = {&regX86ST0, &regX86ST1, &regX86ST2, &regX86ST3, &regX86ST4, &regX86ST5, &regX86ST6, &regX86ST7};
		qsort(toEmpty, sizeof(toEmpty) / sizeof(*toEmpty), sizeof(toEmpty), ptrPtrCmp);

		// Handle spills if we are in a block that consumes  registers
		__auto_type attr = llIRAttrFind(call->base.attrs, IR_ATTR_BASIC_BLOCK, IRAttrGetPred);
		if (attr) {
			struct IRAttrBasicBlock *block = (void *)llIRAttrValuePtr(attr);
			for (long i = 0; i != strVarSize(block->block->in); i++) {
				__auto_type reg = ptrMapVar2RegGet(var2Reg, block->block->in[i]->value.var);
				if (!reg)
					continue;
				struct reg *find = bsearch(reg, destroyed, ARRAY_SIZE(toPreserve), sizeof(*toPreserve), ptrPtrCmp);
				if (find) {
				}
				find = bsearch(reg, destroyed, ARRAY_SIZE(toPreserve), sizeof(*toPreserve), ptrPtrCmp);
			}
		}
	}
}
