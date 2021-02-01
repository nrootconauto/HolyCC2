#include <abi.h>
#include <IRLiveness.h>
#include <cleanup.h>
#include <assert.h>
void *IR_ATTR_ABI_INFO="ABI_INFO";
void IRAttrABIInfoDestroy(struct IRAttr *a) {
		struct IRAttrABIInfo *abi=(void*)a;
		strRegPDestroy(&abi->toPushPop);
}
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
static void findRegisterLiveness(graphNodeIR start) {
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
	for (long n = 0; n != strGraphNodeIRPSize(allNodes); n++) {
		struct IRNodeFuncCall *call = (void *)graphNodeIRValuePtr(allNodes[n]);
		if (call->base.type != IR_FUNC_CALL)
			continue;
		__auto_type attr = llIRAttrFind(call->base.attrs, IR_ATTR_BASIC_BLOCK, IRAttrGetPred);
		assert(attr);
		struct IRAttrBasicBlock *block = (void *)llIRAttrValuePtr(attr);
		struct IRAttrABIInfo info;
		info.toPushPop=NULL;
		info.base.name=IR_ATTR_ABI_INFO;
		info.base.destroy=IRAttrABIInfoDestroy;
		
		for(long i=0;i!=strVarSize(block->block->out);i++) {
				__auto_type var=*ptrMapVar2RegGet(var2Reg, block->block->out[i].value.var);
				info.toPushPop=strRegPSortedInsert(info.toPushPop, var, (regCmpType)ptrPtrCmp);
		}

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
			strGraphNodeIRP dummy CLEANUP(strGraphNodeIRPDestroy)=strGraphNodeIRPAppendItem(NULL, allNodes[n]);
			graphIRReplaceNodes(dummy, IRCreateRegRef(&find->old), NULL,(void(*)(void*))IRNodeDestroy);
	}
	
	ptrMapVar2RegDestroy(var2Reg, NULL);
}
