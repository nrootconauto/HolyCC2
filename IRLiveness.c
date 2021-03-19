#include "IR.h"
#include "IRLiveness.h"
#include <assert.h>
#include "cleanup.h"
//#define DEBUG_PRINT_ENABLE 1
#include "basicBlocks.h"
#include "debugPrint.h"
#include <stdio.h>
void *IR_ATTR_BASIC_BLOCK = "BASIC_BLOCK";
typedef int (*gnCmpType)(const graphNodeMapping *, const graphNodeMapping *);
typedef int (*varRefCmpType)(const struct IRVar **, const struct IRVar **);
#define ALLOCATE(x)                                                                                                                                                \
	({                                                                                                                                                               \
		typeof(x) *ptr = calloc(sizeof(x));                                                                                                                            \
		*ptr = x;                                                                                                                                                      \
		ptr;                                                                                                                                                           \
	})
static int filterVars(void *data, struct __graphNode *node) {
	graphNodeIR enterNode = data;
	if (node == enterNode)
		return 1;

	if (graphNodeIRValuePtr(node)->type != IR_VALUE)
		return 0;

	struct IRNodeValue *irNode = (void *)graphNodeIRValuePtr(node);
	if (irNode->val.type != IR_VAL_VAR_REF)
		return 0;

	return 1;
}
static void copyConnections(strGraphEdgeP in, strGraphEdgeP out) {
	// Connect in to out(if not already connectected)
	for (long inI = 0; inI != strGraphEdgeMappingPSize(in); inI++) {
		for (long outI = 0; outI != strGraphEdgeMappingPSize(out); outI++) {
			__auto_type inNode = graphEdgeMappingIncoming(in[inI]);
			__auto_type outNode = graphEdgeMappingOutgoing(out[outI]);

			// Check if not connected to
			if (__graphIsConnectedTo(inNode, outNode))
				continue;

			DEBUG_PRINT("Connecting %s to %s\n", var2Str(*graphNodeMappingValuePtr(inNode)), var2Str(*graphNodeMappingValuePtr(outNode)))
			graphNodeMappingConnect(inNode, outNode, NULL);
		}
	}
}
static void __filterTransparentKill(graphNodeMapping node) {
	__auto_type in = graphNodeMappingIncoming(node);
	__auto_type out = graphNodeMappingOutgoing(node);

	copyConnections(in, out);

	graphNodeMappingKill(&node, NULL, NULL);
}
static int IRVarRefCmp(const struct IRVar **a, const struct IRVar **b) {
	return IRVarCmp(a[0], b[0]);
}
static int isExprEdge(graphEdgeIR edge) {
	switch (*graphEdgeIRValuePtr(edge)) {
	case IR_CONN_DEST:
	case IR_CONN_FUNC:
	case IR_CONN_FUNC_ARG_1 ... IR_CONN_FUNC_ARG_128:
	case IR_CONN_SIMD_ARG:
	case IR_CONN_SOURCE_A:
	case IR_CONN_SOURCE_B:
		return 1;
	default:
		return 0;
	}
};
static int ptrPtrCmp(const void *a, const void *b) {
		if(*(void**)a>*(void**)b)
				return 1;
		else if(*(void**)a<*(void**)b)
				return -1;
		return 0;
}
static void appendToNodes(struct __graphNode *node, void *data) {
	strGraphNodeMappingP *nodes = data;
	*nodes = strGraphNodeMappingPSortedInsert(*nodes, node, (gnCmpType)ptrPtrCmp);
}
static int isVarNode(const struct IRNode *irNode) {
	if (irNode->type == IR_VALUE) {
		struct IRNodeValue *val = (void *)irNode;
		if (val->val.type == IR_VAL_VAR_REF)
			return 1;
	}

	return 0;
}
static void killNode(void *ptr) {
		struct blockMetaNode *Ptr=ptr;
	__graphNodeKill(Ptr->node, NULL, NULL);
}
struct varRefNodePair {
	struct IRVar ref;
	graphNodeIRLive node;
};
STR_TYPE_DEF(struct varRefNodePair, VarRefNodePair);
STR_TYPE_FUNCS(struct varRefNodePair, VarRefNodePair);
static int varRefNodePairCmp(const struct varRefNodePair *a, const struct varRefNodePair *b) {
	return IRVarCmp(&a->ref, &b->ref);
}
static char *strDup(const char *text) {
		return strcpy(calloc(strlen(text) + 1,1), text);
}
STR_TYPE_DEF(char, Char);
STR_TYPE_FUNCS(char, Char);
static graphNodeIRLive  __IRInterferenceGraphFilterExp(graphNodeIR node,strGraphNodeIRLiveP *stack,strVar exclude, const void *data,int (*varFilter)(graphNodeIR node, const void *data)) { 
		strGraphEdgeIRP in CLEANUP(strGraphEdgeIRPDestroy)=IREdgesByPrec(node);
		graphNodeIRLive liveNode=NULL;
		if(varFilter) {
				if(varFilter(node,data)) goto valid;
		} else if(isVarNode(graphNodeIRValuePtr(node))) {
		valid:;
				struct IRVarLiveness live;
				live.ref=((struct IRNodeValue*)(graphNodeIRValuePtr(node)))->val.value.var;
				liveNode=graphNodeIRLiveCreate(live, 0);
		}

		if(liveNode)
				for(long n=0;n!=strGraphNodeIRLivePSize(*stack);n++) {
						graphNodeIRLiveConnect(stack[0][n], liveNode, NULL);
						graphNodeIRLiveConnect(liveNode,stack[0][n], NULL);
				}
		
		if(liveNode)
					*stack=strGraphNodeIRLivePAppendItem(*stack, liveNode);

		graphNodeIRLive retVal=NULL;
		for(long e=0;e!=strGraphEdgeIRPSize(in);e++) {
				retVal=__IRInterferenceGraphFilterExp(node, stack, exclude,data, varFilter);
		}
		if(liveNode)
					*stack=strGraphNodeIRLivePPop(*stack, &retVal);

		return retVal;
}
strGraphNodeIRLiveP IRInterferenceGraphFilter(graphNodeIR start, const void *data, int (*varFilter)(graphNodeIR node, const void *data)) {
		strGraphNodeIRP allNodes CLEANUP(strGraphNodeIRPDestroy)=graphNodeIRAllNodes(start);
		//Get a list of variables that appear,if a vairable appears more than once,it is not temporary so do proper(non-stack based reg allocation)
		strVar vars CLEANUP(strVarDestroy)=NULL;
		strVar multiVars CLEANUP(strVarDestroy)=NULL;
		for(long n=0;n!=strGraphNodeIRLivePSize(allNodes);n++) {
				__auto_type var=graphNodeIRLiveValuePtr(allNodes[n])->ref;
				if(strVarSortedFind(vars, var, IRVarCmp))
						multiVars=strVarAppendItem(multiVars, var);
				vars=strVarSortedInsert(vars, var, IRVarCmp);
		}
		multiVars=strVarUnique(multiVars, IRVarCmp , NULL);
		
		strGraphNodeIRLiveP retVal=NULL;
		for(long n=0;n!=strGraphNodeIRPSize(allNodes);n++) {
				__auto_type end=IREndOfExpr(allNodes[n]);
				__auto_type start=IRStmtStart(allNodes[n]);
				if(end==NULL)
						continue;
				if(start==allNodes[n]&&end==allNodes[n])
						continue;

				strGraphNodeIRLiveP stack CLEANUP(strGraphNodeIRLivePDestroy)=NULL;
				__auto_type e=__IRInterferenceGraphFilterExp(end,&stack,multiVars,data,varFilter);
				retVal=strGraphNodeIRLivePSortedInsert(retVal, e, (gnCmpType)ptrPtrCmp);
		}
		
	return retVal;
}
