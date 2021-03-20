#include "IR.h"
#include "IRLiveness.h"
#include <assert.h>
#include "cleanup.h"
//#define DEBUG_PRINT_ENABLE 1
#include "basicBlocks.h"
#include "debugPrint.h"
#include <stdio.h>
typedef int (*gnCmpType)(const graphNodeMapping *, const graphNodeMapping *);
typedef int (*varRefCmpType)(const struct IRVar **, const struct IRVar **);
#define ALLOCATE(x)                                                                                                                                                \
	({                                                                                                                                                               \
			typeof(x) *ptr = calloc(sizeof(x),1);																																	\
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
static int IRVarRefCmp(const struct IRVar **a, const struct IRVar **b) {
	return IRVarCmp(a[0], b[0]);
}
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
static struct IRVar varFromNode(graphNodeIR node) {
		__auto_type val=graphNodeIRLiveValuePtr(node);
		return val->ref;
}
static __thread strGraphNodeIRLiveP lastWritten=NULL;
static graphNodeIRLive  __IRInterferenceGraphFilterExp(graphNodeIR node,strGraphNodeIRLiveP *stack,strVar exclude, const void *data,int (*varFilter)(graphNodeIR node, const void *data)) { 
		strGraphEdgeIRP in CLEANUP(strGraphEdgeIRPDestroy)=IREdgesByPrec(node);
		graphNodeIRLive liveNode=NULL;
		if(varFilter&&isVarNode(graphNodeIRValuePtr(node))) {
				if(varFilter(node,data)) goto valid;
		} else if(isVarNode(graphNodeIRValuePtr(node))) {
		valid:;
				struct IRVarLiveness live;
				__auto_type var=((struct IRNodeValue*)(graphNodeIRValuePtr(node)))->val.value.var;
				if(NULL==strVarSortedFind(exclude, var, IRVarCmp)) {
						live.ref=var;
						liveNode=graphNodeIRLiveCreate(live, 0);
						lastWritten=strGraphNodeIRLivePSortedInsert(lastWritten,liveNode,(gnCmpType)ptrPtrCmp);
				}
		}
		long originalSize=strGraphNodeIRPSize(*stack);

		for(long e=0;e!=strGraphEdgeIRPSize(in);e++) {
				__IRInterferenceGraphFilterExp(graphEdgeIRIncoming(in[e]), stack, exclude,data, varFilter);
		}

		for(long N=0;N!=strGraphNodeIRLivePSize(*stack);N++)  {
				for(long n=0;n!=strGraphNodeIRLivePSize(*stack);n++) {
						if(stack[0][n]==stack[0][N]) continue;
						
						if(!graphNodeIRLiveConnectedTo(stack[0][n], stack[0][N])) {
								graphNodeIRLiveConnect(stack[0][n], stack[0][N], NULL);
								graphNodeIRLiveConnect(stack[0][N],stack[0][n], NULL);
						}
				}
		}
		struct basicBlock inOut;
		inOut.refCount=1;
		inOut.in=NULL;
		inOut.out=NULL;
		for(long s=0;s!=strGraphNodeIRPSize(*stack);s++) {
				inOut.in=strVarSortedInsert(inOut.in,  varFromNode(stack[0][s]), IRVarCmp);
				varFromNode(stack[0][s]).var->refCount++;
		}
		if(liveNode) {
				inOut.out=strVarAppendItem(inOut.out, varFromNode(liveNode));
				varFromNode(liveNode).var->refCount++;
		}
		
		struct IRAttrBasicBlock attr;
		attr.base.name=(void*)IR_ATTR_BASIC_BLOCK;
		attr.base.destroy=IRAttrBasicBlockDestroy;
		attr.block=ALLOCATE(inOut);
		IRAttrReplace(node,__llCreate(&attr, sizeof(attr)));

		if(graphNodeIRValuePtr(node)->type==IR_DERREF) {
				if(strGraphNodeIRLivePSize(*stack)) {
						for(long s=0;s!=strGraphNodeIRLivePSize(*stack);s++) {
								assert(strGraphNodeIRLivePSortedFind(lastWritten, stack[0][s] ,  (gnCmpType)ptrPtrCmp));
								printf("VAR:%p\n", varFromNode(stack[0][s]).var);
						}
						printf("\n");
				}
		}

		
		if(liveNode)
				*stack=strGraphNodeIRPResize(*stack, originalSize);
		
		if(liveNode)
					*stack=strGraphNodeIRLivePAppendItem(*stack, liveNode);

		for(long N=0;N!=strGraphNodeIRLivePSize(*stack);N++)  {
				for(long n=0;n!=strGraphNodeIRLivePSize(*stack);n++) {
						if(stack[0][n]==stack[0][N]) continue;
						
						if(!graphNodeIRLiveConnectedTo(stack[0][n], stack[0][N])) {
								graphNodeIRLiveConnect(stack[0][n], stack[0][N], NULL);
								graphNodeIRLiveConnect(stack[0][N],stack[0][n], NULL);
						}
				}
		}
		
		return NULL;
}
strGraphNodeIRLiveP IRInterferenceGraphFilter(graphNodeIR start, const void *data, int (*varFilter)(graphNodeIR node, const void *data)) {
		strGraphNodeIRP allNodes CLEANUP(strGraphNodeIRPDestroy)=graphNodeIRAllNodes(start);
		//Get a list of variables that appear,if a vairable appears more than once,it is not temporary so do proper(non-stack based reg allocation)
		strVar vars CLEANUP(strVarDestroy)=NULL;
		strVar multiVars CLEANUP(strVarDestroy)=NULL;
		for(long n=0;n!=strGraphNodeIRLivePSize(allNodes);n++) {
				struct IRNodeValue *val=(void*)graphNodeIRValuePtr(allNodes[n]);
				if(val->base.type!=IR_VALUE) continue;
				if(val->val.type!=IR_VAL_VAR_REF) continue;
				__auto_type var=val->val.value.var;
				if(strVarSortedFind(vars, var, IRVarCmp))
						multiVars=strVarSortedInsert(multiVars, var,IRVarCmp);
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
				if(end!=allNodes[n])
						continue;

				strGraphNodeIRLiveP stack CLEANUP(strGraphNodeIRLivePDestroy)=NULL;
				lastWritten=NULL;
				__IRInterferenceGraphFilterExp(end,&stack,multiVars,data,varFilter);

				long old=strGraphNodeIRLivePSize(retVal);
				long lastWrittenS=strGraphNodeIRLivePSize(lastWritten);
				retVal=strGraphNodeIRLivePSetUnion(retVal, lastWritten, (gnCmpType)ptrPtrCmp);
				assert(strGraphNodeIRLivePSize(retVal)==old+lastWrittenS);
				strGraphNodeIRLivePDestroy(&lastWritten);
		}
		
	return retVal;
}
