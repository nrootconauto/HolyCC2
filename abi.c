#include <abi.h>
#include <IRLiveness.h>
#include <cleanup.h>
#include <assert.h>
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
