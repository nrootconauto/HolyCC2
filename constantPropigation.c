#include <SSA.h>
#include <IR.h>
#include <basicBlocks.h>
#include <IRFilter.h>
#include <cleanup.h>
#include <linkedList.h>
#include <IRExec.h>
#include <assert.h>
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
PTR_MAP_FUNCS(graphNodeIR ,struct IREvalVal, IREvalValByGN);
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
STR_TYPE_DEF(struct IRVar, IRVar);
STR_TYPE_FUNCS(struct IRVar, IRVar);
struct var2Nodes {
		struct IRVar var;
		strVar users;
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
						if(NULL==strIRVarSortedFind(data->consts, value->val.value.var, IRVarCmp))
								data->isConst=0;
				}
		} else {
				//A non-value in expression!?!?
		}
}
static int nodeIsConst(graphNodeIR node,strIRVar consts,ptrMapIREvalValByGN values) {
		graphNodeIR irNode=node;
		struct isConstPair pair;
		pair.isConst=1;
		pair.consts=consts;
		graphNodeIRVisitBackward(irNode, &pair, isEdgePred, (void(*)(struct __graphNode*,void*))isConstVisit);
		if(pair.isConst) {
				if(ptrMapIREvalValByGNGet(values, node))
						return 1;
				
				int success;
				__auto_type value=IREvalNode(node, &success);
				if(!success)
						return 0;
				ptrMapIREvalValByGNAdd(values, node, value);
		}
		return pair.isConst;
}
static void appendToNodes(struct __graphNode *node,void *nodes) {
		strGraphNodeIRP *nodes2=nodes;
		*nodes2=strGraphNodeIRPSortedInsert(*nodes2, node, (gnCmpType)ptrPtrCmp);
}

static __thread ptrMapIREvalValByGN  validNodeValues=NULL;
static __thread strGraphNodeIRP toKillItems=NULL;
static __thread strGraphNodeIRP replacedItems=NULL;
static void replaceExprWithConstant(graphNodeIR node, struct IREvalVal value,strGraphNodeIRP *replaced) {
		if(strGraphNodeIRPSortedFind(replacedItems, node, (gnCmpType)ptrPtrCmp))
				return;
		graphNodeIR replaceWith;
		if(value.type==IREVAL_VAL_FLT)
				replaceWith=IRCreateFloat(value.value.flt);
		else if(value.type==IREVAL_VAL_INT)
				replaceWith=IRCreateIntLit(value.value.i);
		else
				return;
		
		strGraphNodeIRP nodes CLEANUP(strGraphNodeIRPDestroy)=strGraphNodeIRPAppendItem(NULL, node);
		//Get list of expression nodes
		graphNodeIRVisitBackward(node, &nodes, isEdgePred, appendToNodes);
		graphReplaceWithNode(nodes, replaceWith, NULL, (void(*)(void *))IRNodeDestroy, sizeof(*graphEdgeIRValuePtr(NULL)));
		if(replaced)
				*replaced=strGraphNodeIRPSetUnion(*replaced, nodes, (gnCmpType)ptrPtrCmp);
}
PTR_MAP_FUNCS(graphNodeMapping, graphNodeIR , Mapping2IR);
static int untillMultipleFlowIn(const struct __graphNode *node,const struct __graphEdge *edge,const void *data) {
		strGraphEdgeIRP incoming CLEANUP(strGraphEdgeIRPDestroy)=graphNodeIRIncoming((graphNodeIR)node);
		strGraphEdgeIRP inFlow CLEANUP(strGraphEdgeIRPDestroy)=IRGetConnsOfType(incoming, IR_CONN_FLOW);
		return 2>strGraphEdgeIRPSize(inFlow);
}
static void markPathForDestroy(graphEdgeIR path,int destroy) {
		__auto_type inNode=graphEdgeIRIncoming(path);
		__auto_type outNode=graphEdgeIROutgoing(path);
		strGraphNodeIRP toKill CLEANUP(strGraphNodeIRPDestroy)=strGraphNodeIRPAppendItem(NULL, outNode);
		graphNodeIRVisitForward(outNode,  &toKill,untillMultipleFlowIn,  appendToNodes);
		if(destroy)
				for(long i=0;i!=strGraphNodeIRPSize(toKill);i++)
						graphNodeIRKill(&toKill[i], (void(*)(void*))IRNodeDestroy, NULL);
		toKillItems=strGraphNodeIRPSetUnion(toKillItems, toKill, (gnCmpType)ptrPtrCmp);
}
static void removeConstantCondBranches(graphNodeIR start,strIRVar consts,int destroy) {
		strGraphNodeIRP removedNodes2 CLEANUP(strGraphNodeIRPDestroy)=NULL;
		strGraphNodeIRP allNodes CLEANUP(strGraphNodeIRPDestroy)=graphNodeIRAllNodes(start);
		strGraphNodeIRP visitedNodes CLEANUP(strGraphNodeIRPDestroy)=NULL;
	loop:
		for(long i=0;i!=strGraphNodeIRPSize(allNodes);i++) {
				visitedNodes=strGraphNodeIRPSortedInsert(visitedNodes, allNodes[i], (gnCmpType)ptrPtrCmp);
				__auto_type type=graphNodeIRValuePtr(allNodes[i])->type;
				if(type==IR_COND_JUMP||type==IR_JUMP_TAB) {
						//Ensure Condition is constant
						strGraphNodeIRP in CLEANUP(strGraphNodeIRPDestroy)=graphNodeIRIncomingNodes(allNodes[i]);
						assert(strGraphNodeIRPSize(in)==1);
						if(!nodeIsConst(in[0], consts,validNodeValues)) continue;
				}
				if(type==IR_COND_JUMP) {
						strGraphNodeIRP in CLEANUP(strGraphNodeIRPDestroy)=graphNodeIRIncomingNodes(allNodes[i]);
						assert(strGraphNodeIRPSize(in)==1);

						int success;
						__auto_type value=IREvalNode(in[0], &success);
						if(!success) continue;
						if(value.type==IREVAL_VAL_INT) {
								if(value.value.i) goto condTrue; else goto condFalse;
						} else if(value.type==IREVAL_VAL_FLT) {
								if(value.value.flt) goto condTrue; else goto condFalse;
						}  else if(value.type==IREVAL_VAL_PTR) {
								if(value.value.ptr) goto condTrue; else goto condFalse;
						} else
								continue; //?
						graphEdgeIR killPath;
				condTrue: {
								strGraphEdgeIRP out CLEANUP(strGraphEdgeIRPDestroy)=graphNodeIROutgoing(allNodes[i]);
								strGraphEdgeIRP False CLEANUP(strGraphEdgeIRPDestroy)=IRGetConnsOfType(out, IR_CONN_COND_FALSE);
								//Kill false edge as will never reach it
								killPath=False[0];
								goto killOppositeBranch;
						}
				condFalse: {
								strGraphEdgeIRP out CLEANUP(strGraphEdgeIRPDestroy)=graphNodeIROutgoing(allNodes[i]);
								strGraphEdgeIRP True CLEANUP(strGraphEdgeIRPDestroy)=IRGetConnsOfType(out, IR_CONN_COND_TRUE);
								//Kill true edge as will never reach it
								killPath=True[0];
								goto killOppositeBranch;
						}
				killOppositeBranch: {
								//Replace cond-jump with flow connection if removing a possible branch
								strGraphNodeIRP condNodes CLEANUP(strGraphNodeIRPDestroy)=graphNodeIRIncomingNodes(allNodes[i]);
								assert(strGraphNodeIRPSize(condNodes)==1);
								graphNodeIR condNode=condNodes[0];
								__auto_type opposite=(*graphEdgeIRValuePtr(killPath)==IR_CONN_COND_TRUE)?IR_CONN_COND_FALSE:IR_CONN_COND_TRUE;
								strGraphEdgeIRP outPaths CLEANUP(strGraphEdgeIRPDestroy)=graphNodeIROutgoing(allNodes[i]);
								strGraphEdgeIRP oppositeBranch CLEANUP(strGraphEdgeIRPDestroy)=IRGetConnsOfType(outPaths, opposite);
								assert(strGraphEdgeIRPSize(oppositeBranch)==1);
								markPathForDestroy(killPath,destroy);
								if(destroy) {
										__auto_type connectTo=graphEdgeIROutgoing(oppositeBranch[0]);
										graphNodeIRKill(&allNodes[i], (void(*)(void*))IRNodeDestroy, NULL);
										graphNodeIRConnect(condNode, connectTo, IR_CONN_FLOW);
								}
						}
						goto removeFromQueue;
				} else if(type==IR_JUMP_TAB) {
						strGraphNodeIRP in CLEANUP(strGraphNodeIRPDestroy)=graphNodeIRIncomingNodes(allNodes[i]);
						assert(strGraphNodeIRPSize(in)==1);
						if(!nodeIsConst(in[0], consts,validNodeValues))
								continue;
						int success;
						__auto_type value=IREvalNode(in[0], &success);
						if(!success)
								continue;
						int64_t condValue;
						if(value.type==IREVAL_VAL_INT)
								condValue=value.value.i;
						else if(value.type==IREVAL_VAL_FLT)
								condValue=value.value.flt;
						else
								continue;

						struct IRNodeJumpTable *table=(void*)graphNodeIRValuePtr(allNodes[i]);
						strGraphEdgeIRP out CLEANUP(strGraphEdgeIRPDestroy)=graphNodeIROutgoing(allNodes[i]);
						strGraphEdgeIRP dft CLEANUP(strGraphEdgeIRPDestroy)=IRGetConnsOfType(out, IR_CONN_DFT);
						assert(strGraphEdgeIRPSize(dft)==1);
						graphNodeIR choosenNode=graphEdgeIROutgoing(dft[0]);
						for(long c=0;c!=strIRTableRangeSize(table->labels);c++) {
								if(table->labels[c].start<=condValue&&table->labels[c].end>condValue) {
										choosenNode=table->labels[c].to;
										break;
								}
						}
						strGraphEdgeIRP cases CLEANUP(strGraphEdgeIRPDestroy)=IRGetConnsOfType(out, IR_CONN_CASE);
						for(long c=0;c!=strGraphEdgeIRPSize(cases);c++) {
								if(graphEdgeIROutgoing(cases[c])!=choosenNode) {
										markPathForDestroy(cases[c],destroy);
								}
						}
						if(choosenNode!=graphEdgeIROutgoing(dft[0]))
								markPathForDestroy(dft[0],destroy);

						if(destroy) {
								toKillItems=strGraphNodeIRPSortedInsert(toKillItems, allNodes[i], (gnCmpType)ptrPtrCmp);
								graphNodeIRConnect(in[0], choosenNode, IR_CONN_FLOW);
								graphNodeIRKill(&allNodes[i], (void(*)(void*))IRNodeDestroy, NULL);
						}
						goto removeFromQueue;
				}
				continue;
		removeFromQueue:
				allNodes=strGraphNodeIRPSetDifference(allNodes, visitedNodes, (gnCmpType)ptrPtrCmp);
				allNodes=strGraphNodeIRPSetDifference(allNodes, toKillItems, (gnCmpType)ptrPtrCmp);
				goto loop;
		}
}
static int isChooseNode(graphNodeIR node,const void *data) {
		return graphNodeIRValuePtr(node)->type==IR_CHOOSE;
}
static void removeSSANodes(graphNodeIR start) {
		graphNodeMapping chooses=IRFilter(start,isChooseNode,NULL);
		strGraphNodeMappingP allNodes CLEANUP(strGraphNodeMappingPDestroy) = graphNodeMappingAllNodes(chooses);
		for(long i=0;i!=strGraphNodeMappingPSize(allNodes);i++) {
				__auto_type ir=*graphNodeMappingValuePtr(allNodes[i]);
				if(graphNodeIRValuePtr(ir)->type==IR_CHOOSE) {
						__filterTransparentKill(ir);
				}
		}
}
PTR_MAP_FUNCS(graphNodeIR, strGraphNodeIRP, GN2Chooses);
PTR_MAP_FUNCS(graphNodeIR, struct IRVar *, Choose2Var);
static strIRVar  __IRConstPropigation(graphNodeIR start,strIRVar consts) {
		removeSSANodes(start);
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
				
				__auto_type newBlocks=IRGetBasicBlocksFromExpr(start, NULL, allMappedNodes[0], NULL,NULL);
				strGraphNodeMappingP dummy CLEANUP(strGraphNodeMappingPDestroy)=strGraphNodeMappingPAppendItem(NULL, allMappedNodes[0]);
				blocks=strBasicBlockConcat(blocks, newBlocks);
				for(long i=0;i!=strBasicBlockSize(newBlocks);i++) {
						allMappedNodes=strGraphNodeMappingPSetDifference(allMappedNodes, newBlocks[i]->nodes, (gnCmpType)ptrPtrCmp);
				}
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
														if(0==IRVarCmp(&newBlocks[i]->define[assign], &value->val.value.var)) {
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
														if(0==IRVarCmp(&newBlocks[i]->define[assign], &value->val.value.var)) {
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
														struct IRNodeValue *assignVar=(void*)graphNodeIRValuePtr(assignNode);
														if(strVarSortedFind(find->users,assignVar->val.value.var,IRVarCmp))
																continue;
														find->users=strVarSortedInsert(find->users,assignVar->val.value.var,IRVarCmp);
												}
										}
								}
						}
				}
		}

		strIRVar worklist CLEANUP(strIRVarDestroy)=NULL;
		ptrMapGN2Chooses node2Chooses=ptrMapGN2ChoosesCreate(); //FREE
		ptrMapChoose2Var chooses2Var=ptrMapChoose2VarCreate(); //FREE
		strGraphNodeIRP unresolvedChooses CLEANUP(strGraphNodeIRPDestroy)=NULL;
		for(long i=0;i!=strVar2NodeSize(useAssoc);i++)
				worklist=strIRVarSortedInsert(worklist, useAssoc[i].var, IRVarCmp);
		//
		// We replace the items in the order we find them,a varible a that is used by b needs to be  computed before b
		// ```
		// a=1; //Do variable "a" first 
		// b=a;
		// ```
		//
		strGraphNodeIRP replaceOrder=NULL;

		for(;strIRVarSize(worklist);) {
				strIRVar worklist2=NULL;
				for(long i=0;i!=strIRVarSize(worklist);i++) {
						struct  var2Nodes dummy;
						dummy.var=worklist[i];
						dummy.users=NULL;
						dummy.assign=NULL;
						__auto_type find=strVar2NodeSortedFind(useAssoc, dummy, var2NodesCmp);
						if(NULL!=strGraphNodeIRPSortedFind(toKillItems, find->assign, (gnCmpType)ptrPtrCmp))
								continue;						
						if(NULL!=strGraphNodeIRPSortedFind(replacedItems, find->assign, (gnCmpType)ptrPtrCmp))
								continue;					
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
								if(!ptrMapChoose2VarGet(chooses2Var, chooseNode))
										ptrMapChoose2VarAdd(chooses2Var, chooseNode,&find->var);

								int success=1;
								for(long c=0;c!=strGraphNodeIRPSize(choose->canidates);c++) {
										if(strGraphNodeIRPSortedFind(toKillItems, choose->canidates[c], (gnCmpType)ptrPtrCmp)) {
												continue;
										}
										if(!nodeIsConst(choose->canidates[c], consts,validNodeValues)) {
												unresolvedChooses=strGraphNodeIRPSortedInsert(unresolvedChooses, chooseNode, (gnCmpType)ptrPtrCmp);
												goto notConst;
										}
								}
								//All are constant,check if equal
								struct IREvalVal first;
								int notConst=0;
								int isFirstItem=1;
								for(long c=0;c!=strGraphNodeIRPSize(choose->canidates);c++) {
										if(strGraphNodeIRPSortedFind(toKillItems, choose->canidates[c], (gnCmpType)ptrPtrCmp)) {
												continue;
										}
										if(isFirstItem) {
												isFirstItem=0;
												first=*ptrMapIREvalValByGNGet(validNodeValues, choose->canidates[c]);
										} else if(!IREvalValEqual(&first, ptrMapIREvalValByGNGet(validNodeValues, choose->canidates[c]))) {
												notConst=1;
										}

										__auto_type find=ptrMapGN2ChoosesGet(node2Chooses, choose->canidates[c]);
												if(!find) {
														ptrMapGN2ChoosesAdd(node2Chooses, choose->canidates[c],strGraphNodeIRPAppendItem(NULL, chooseNode));
												} else {
														if(NULL==strGraphNodeIRPSortedFind(*find,chooseNode,(gnCmpType)ptrPtrCmp))
																*find=strGraphNodeIRPSortedInsert(*find,chooseNode,(gnCmpType)ptrPtrCmp);
												}
								}
								if(notConst) {
										unresolvedChooses=strGraphNodeIRPSortedInsert(unresolvedChooses, chooseNode, (gnCmpType)ptrPtrCmp);
										goto notConst;
								}
								IREvalSetVarVal(find->var.value.var,first);
								ptrMapIREvalValByGNAdd(validNodeValues, find->assign, first);
								goto allConst;
						} else {
								//Check expression at node is constant
								if(nodeIsConst(find->assign, consts,validNodeValues)) {
										goto allConst;
								}
						}
				notConst:;
						continue;
				allConst:;
						replaceOrder=strGraphNodeIRPAppendItem(replaceOrder, find->assign);
						consts=strIRVarSortedInsert(consts, find->var, IRVarCmp);
						for(long u=0;u!=strVarSize(find->users);u++) {
								if(NULL==strIRVarSortedFind(worklist2, find->users[u], IRVarCmp))
										worklist2=strIRVarSortedInsert(worklist2, find->users[u], IRVarCmp);
						}

						//Replace all occurances of assigned var with value
						strGraphNodeIRP  refs CLEANUP(strGraphNodeIRPDestroy)=IRVarRefs(find->var.value.var, &find->var.SSANum);
						replaceOrder=strGraphNodeIRPAppendData(replaceOrder, refs, strGraphNodeIRPSize(refs));
						for(long i=0;i!=strGraphNodeIRPSize(refs);i++) {
								ptrMapIREvalValByGNAdd(validNodeValues, refs[i],  *ptrMapIREvalValByGNGet(validNodeValues, find->assign));
						}
				}
				strIRVarDestroy(&worklist);
				worklist=worklist2;
				if(strIRVarSize(worklist)==0) {
						__auto_type oldToKillSize=strGraphNodeIRPSize(toKillItems);
						removeConstantCondBranches(start, consts,0);
						if(strGraphNodeIRPSize(toKillItems)!=oldToKillSize) {
								long count=ptrMapGN2ChoosesSize(node2Chooses);
								graphNodeIR keys[count];
								ptrMapGN2ChoosesKeys(node2Chooses, keys);
								for(long i=0;i!=count;i++) {
										__auto_type find=strGraphNodeIRPSortedFind(toKillItems, keys[i], (gnCmpType)ptrPtrCmp);
										if(find) {
												__auto_type chooses=*ptrMapGN2ChoosesGet(node2Chooses, keys[i]);
												//Add variables of choose nodes
												for(long c=0;c!=strGraphNodeIRPSize(chooses);c++) {
														if(NULL==strGraphNodeIRPSortedFind(unresolvedChooses, chooses[c], (gnCmpType)ptrPtrCmp))
																continue;
														__auto_type var=*ptrMapChoose2VarGet(chooses2Var, chooses[c]);
														if(NULL==strIRVarSortedFind(worklist, *var, IRVarCmp))
																worklist=strIRVarSortedInsert(worklist, *var, IRVarCmp);
												}
										}
								}
						}
				}
		}
		long size=strGraphNodeIRPSize(replaceOrder);
		strGraphNodeIRP replaced CLEANUP(strGraphNodeIRPDestroy) =NULL;
		removeConstantCondBranches(start, consts,1);
		replacedItems=strGraphNodeIRPSetUnion(replacedItems, toKillItems, (gnCmpType)ptrPtrCmp);
		for(long i=0;i!=strGraphNodeIRPSize(replaceOrder);i++) {
				replaceExprWithConstant(replaceOrder[i], *ptrMapIREvalValByGNGet(validNodeValues, replaceOrder[i]), &replacedItems);
		}

		
		
		ptrMapMapping2IRDestroy(mappingPtr2IR,NULL);
		return consts; 
}
void IRConstPropigation(graphNodeIR start) {
		IREvalInit();
		strIRVar consts CLEANUP(strIRVarDestroy)=NULL;
		toKillItems=NULL;
		replacedItems=NULL;
		validNodeValues=ptrMapIREvalValByGNCreate();
		consts=__IRConstPropigation(start,consts);
		removeSSANodes(start);
		strGraphNodeIRPDestroy(&toKillItems);
		strGraphNodeIRPDestroy(&replacedItems);
		ptrMapIREvalValByGNDestroy(validNodeValues,(void(*)(void*))IREvalValDestroy);
}
