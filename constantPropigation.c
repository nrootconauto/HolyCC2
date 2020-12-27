#include <SSA.h>
#include <IR.h>
#include <basicBlocks.h>
#include <IRFilter.h>
#include <cleanup.h>
#include <linkedList.h>
#include <IRExec.h>
typedef int (*gnCmpType)(const graphNodeMapping *, const graphNodeMapping *);
typedef int (*varRefCmpType)(const struct IRVar **, const struct IRVar **);
enum IRIsConst {
		CONST,
		NOT_CONST,
};
static int ptrPtrCmp(const void *a, const void *b) {
	if (*(void **)a > *(void **)b)
		return 1;
	else if (*(void **)a < *(void **)b)
		return -1;
	else
		return 0;
}

PTR_MAP_FUNCS(graphNodeMapping, enum IRIsConst, IRIsConst);
static int isNotMappedChoose(const void *data,const graphNodeMapping *node) {
		__auto_type irNode=graphNodeMappingValuePtr(*node);
		if(irNode) {
				if(graphNodeIRValuePtr(*irNode)->type==IR_CHOOSE)
						return 0;
		}
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
LL_TYPE_DEF(graphNodeIR,GNIR);
LL_TYPE_FUNCS(graphNodeIR,GNIR);
STR_TYPE_DEF(struct IRVar *, IRVar);
STR_TYPE_FUNCS(struct IRVar *, IRVar);
struct var2Nodes {
		struct IRVar var;
		strGraphNodeIRP users;
		graphNodeIR assign;
};
STR_TYPE_DEF(struct var2Nodes,Var2Node);
STR_TYPE_FUNCS(struct var2Nodes,Var2Node);
PTR_MAP_FUNCS(graphNodeIR, enum IRIsConst, GNIsConst);
static int var2NodesCmp(const struct var2Nodes *a,const struct var2Nodes *b) {
		return IRVarCmp(&a->var, &b->var);
}
static int isEdgePred(const struct __graphNode *node,const struct __graphEdge *edge,const void *data) {
		return IRIsExprEdge(*graphEdgeIRValuePtr((graphEdgeIR)edge));
}
struct isConstPair {
		strIRVar consts;
		int isConst;
};
static int IRVarCmp2(const struct IRVar **a,const struct IRVar **b) {
		return IRVarCmp(*a, *b);
}
static void isConstVisit(struct __graphNode *node,struct isConstPair *data) {
		if(!data->isConst)
				return;
		
		struct IRNode *irNodeVal=graphNodeIRValuePtr(node);
		if(irNodeVal->type==IR_VALUE) {
				struct IRNodeValue *value=(void*)irNodeVal;
				if(value->val.type==IR_VAL_VAR_REF) {
						if(NULL==strIRVarSortedFind(data->consts, &value->val.value.var, IRVarCmp2))
								data->isConst=0;
				}
		} else {
				//A non-value in expression!?!?
		}
}
static int nodeIsConst(graphNodeIR node,strIRVar consts) {
		graphNodeIR irNode=node;
		struct isConstPair pair;
		pair.isConst=1;
		pair.consts=consts;
		graphNodeIRVisitBackward(irNode, &pair, isEdgePred, (void(*)(struct __graphNode*,void*))isConstVisit);
		return pair.isConst;
}
PTR_MAP_FUNCS(graphNodeIR ,struct IREvalVal, IREvalValByGN);
static void appendToNodes(struct __graphNode *node,void *nodes) {
		strGraphNodeIRP *nodes2=nodes;
		*nodes2=strGraphNodeIRPSortedInsert(*nodes2, node, (gnCmpType)ptrPtrCmp);
}
static void replaceExprWithConstant(graphNodeIR node, struct IREvalVal value,strGraphNodeIRP *replaced) {
		graphNodeIR replaceWith;
		if(value.type==IREVAL_VAL_FLT)
				replaceWith=IRCreateFloat(value.value.flt);
		else if(value.type==IREVAL_VAL_INT)
				replaceWith=IRCreateIntLit(value.value.i);
		else
				return;
		
		strGraphNodeIRP nodes CLEANUP(strGraphNodeIRPDestroy)=NULL;
		nodes=strGraphNodeIRPSetDifference(nodes, *replaced, (gnCmpType)ptrPtrCmp);
		//Get list of expression nodes
		graphNodeIRVisitBackward(node, &nodes, isEdgePred, appendToNodes);
		graphReplaceWithNode(nodes, replaceWith, NULL, (void(*)(void *))IRNodeDestroy, sizeof(*graphEdgeIRValuePtr(NULL)));
		*replaced=strGraphNodeIRPSetUnion(*replaced, nodes, (gnCmpType)ptrPtrCmp);
}
PTR_MAP_FUNCS(graphNodeMapping, graphNodeIR , Mapping2IR);
void IRConstPropigation(graphNodeIR start) {
		IRToSSA(start);
		graphNodeMapping map=graphNodeCreateMapping(start, 1);
		strGraphNodeMappingP allMappedNodes CLEANUP(strGraphNodeMappingPDestroy) = graphNodeMappingAllNodes(map);
		strGraphNodeMappingP allMappedChooses CLEANUP(strGraphNodeMappingPDestroy)=strGraphNodeMappingPClone(allMappedNodes);
		allMappedChooses=strGraphNodeMappingPRemoveIf(allMappedChooses, NULL, isNotMappedChoose);
		//Mapped nodes will be replaced with metaNodes,so create a map of the mapping node's(pointer) to the graphNodeIR it represents
		ptrMapMapping2IR mappingPtr2IR=ptrMapMapping2IRCreate();
		for(long i=0;i!=strGraphNodeMappingPSize(allMappedNodes);i++) {
				ptrMapMapping2IRAdd(mappingPtr2IR,allMappedNodes[i],*graphNodeMappingValuePtr(allMappedNodes[i]));
		}
		//Find basic blocks
		ptrMapBlockMetaNode metaNodes=ptrMapBlockMetaNodeCreate();
		strBasicBlock blocks=NULL;
		strVar2Node useAssoc CLEANUP(strVar2NodeDestroy)=NULL;
		for(;strGraphNodeMappingPSize(allMappedNodes);) {
				__auto_type node= *graphNodeMappingValuePtr(allMappedNodes[0]);
				__auto_type stmtStart=IRStmtStart(node);
				//Wont move upwards if not part of expression
				if(stmtStart==node) {
						strGraphNodeMappingP dummy CLEANUP(strGraphNodeMappingPDestroy)=strGraphNodeMappingPAppendItem(NULL, allMappedNodes[0]);
						allMappedNodes=strGraphNodeMappingPSetDifference(allMappedNodes, dummy, (gnCmpType)ptrPtrCmp);
						continue;
				}
				
				__auto_type newBlocks=IRGetBasicBlocksFromExpr(start, metaNodes, allMappedNodes[0], NULL,NULL);
				blocks=strBasicBlockConcat(blocks, newBlocks);
				for(long i=0;i!=strBasicBlockSize(newBlocks);i++) {
						allMappedNodes=strGraphNodeMappingPSetDifference(allMappedNodes, newBlocks[i]->nodes, (gnCmpType)ptrPtrCmp);
				}
				strGraphNodeMappingP dummy CLEANUP(strGraphNodeMappingPDestroy)=strGraphNodeMappingPAppendItem(NULL, allMappedNodes[0]);
				allMappedNodes=strGraphNodeMappingPSetDifference(allMappedNodes, dummy, (gnCmpType)ptrPtrCmp);
					
				for(long i=0;i!=strBasicBlockSize(newBlocks);i++) {
						for(long assign=0;assign!=strVarSize(newBlocks[i]->define);assign++) {
								//Find assign node
								graphNodeIR assignNode;
								for(long i2=0;i2!=strGraphNodeMappingPSize(newBlocks[i]->nodes);i2++) {
										__auto_type node=*ptrMapMapping2IRGet(mappingPtr2IR,newBlocks[i]->nodes[i2]);
										struct IRNode *ir=graphNodeIRValuePtr(node);
										if(ir->type==IR_VALUE) {
												struct IRNodeValue *value=(void*)ir;
												if(value->val.type==IR_VAL_VAR_REF) {
														if(0==IRVarCmp(newBlocks[i]->define[assign], &value->val.value.var)) {
																assignNode=node;

														assignLoop:;
																struct var2Nodes dummy;
																dummy.var=value->val.value.var;
																dummy.users=NULL;
																dummy.assign=NULL;
																__auto_type find=strVar2NodeSortedFind(useAssoc, dummy, var2NodesCmp);
																if(!find) {
																		useAssoc=strVar2NodeSortedInsert(useAssoc, dummy, var2NodesCmp);
																		goto assignLoop;
																}
																find->assign=assignNode;
																break;
														}
												}
										}
								}
									
								for(long i2=0;i2!=strGraphNodeMappingPSize(newBlocks[i]->nodes);i2++) {
										__auto_type node=*ptrMapMapping2IRGet(mappingPtr2IR,newBlocks[i]->nodes[i2]);
										struct IRNode *ir=graphNodeIRValuePtr(node);
										if(ir->type==IR_VALUE) {
												struct IRNodeValue *value=(void*)ir;
												if(value->val.type==IR_VAL_VAR_REF) {
														//Ignore assign node
														if(0==IRVarCmp(newBlocks[i]->define[assign], &value->val.value.var)) {
																continue;
														}
												useLoop:;
														struct var2Nodes dummy;
														dummy.var=value->val.value.var;
														dummy.users=NULL;
														dummy.assign=NULL;
														__auto_type find=strVar2NodeSortedFind(useAssoc, dummy, var2NodesCmp);
														if(!find) {
																useAssoc=strVar2NodeSortedInsert(useAssoc, dummy, var2NodesCmp);
																goto useLoop;
														}
														if(strGraphNodeIRPSortedFind(find->users,assignNode,(gnCmpType)ptrPtrCmp))
																continue;
														find->users=strGraphNodeIRPSortedInsert(find->users,assignNode,(gnCmpType)ptrPtrCmp);
												}
										}
								}
						}
				}
		}

		strIRVar consts CLEANUP(strIRVarDestroy)=NULL;
		strIRVar worklist CLEANUP(strIRVarDestroy)=NULL;
		for(long i=0;i!=strVar2NodeSize(useAssoc);i++)
				worklist=strIRVarSortedInsert(worklist, &useAssoc[i].var, IRVarCmp2);
		
		IREvalInit();
		ptrMapIREvalValByGN nodeValues=ptrMapIREvalValByGNCreate();;
		for(;strIRVarSize(worklist);) {
				strIRVar worklist2=NULL;
				for(long i=0;i!=strIRVarSize(worklist);i++) {
						struct var2Nodes dummy;
						dummy.var=*worklist[i];
						dummy.users=NULL;
						dummy.assign=NULL;
						__auto_type find=strVar2NodeSortedFind(useAssoc, dummy, var2NodesCmp);
						struct IRNodeChoose *choose=NULL;
						graphNodeIR chooseNode=NULL;
						strGraphNodeIRP in CLEANUP(strGraphNodeIRPDestroy)=graphNodeIRIncomingNodes(find->assign);
						for(long i2=0;i2!=strGraphNodeIRPSize(in);i2++) {
								if(graphNodeIRValuePtr(in[i2])->type==IR_CHOOSE) {
										choose=(void*)graphNodeIRValuePtr(in[i2]);
										chooseNode=in[i2];
										break;
								}
						}

						if(choose) {
								int success=1;
								struct IREvalVal value;
								for(long c=0;c!=strGraphNodeIRPSize(choose->canidates);c++) {
										if(!nodeIsConst(choose->canidates[c], consts)) {
												goto notConst;
										}
								}
								//All are constant and equal
								goto allConst;		
						} else {
								//Check expression at node is constant
								if(nodeIsConst(find->assign, consts))
										goto allConst;
						}
				notConst:;
						continue;
				allConst:;
						int success;
						__auto_type value=IREvalNode(find->assign, &success);
						if(!success)
								goto notConst;
						ptrMapIREvalValByGNAdd(nodeValues, find->assign, value);
						consts=strIRVarSortedInsert(consts, worklist[i], IRVarCmp2);
						for(long u=0;u!=strGraphNodeIRPSize(find->users);u++) {
								struct IRNodeValue *val=(void*)graphNodeIRValuePtr(find->users[u]);
								if(NULL==strIRVarSortedFind(worklist2, &val->val.value.var, IRVarCmp2))
										worklist2=strIRVarSortedInsert(worklist2, &val->val.value.var, IRVarCmp2);
						}
				}
				strIRVarDestroy(&worklist);
				worklist=worklist2;
		}
		long size=ptrMapIREvalValByGNSize(nodeValues);
		graphNodeIR keys[size];
		ptrMapIREvalValByGNKeys(nodeValues, keys);
		strGraphNodeIRP replaced CLEANUP(strGraphNodeIRPDestroy)=NULL;
		for(long k=0;k!=size;k++)
				 replaceExprWithConstant(keys[k], *ptrMapIREvalValByGNGet(nodeValues, keys[k]),&replaced);

		ptrMapMapping2IRDestroy(mappingPtr2IR,NULL);
		ptrMapIREvalValByGNDestroy(nodeValues,NULL);
}
