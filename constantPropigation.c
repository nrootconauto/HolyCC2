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
		strVar uses;
		strVar usedBy;
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
		
		strGraphNodeIRP nodes CLEANUP(strGraphNodeIRPDestroy)=strGraphNodeIRPAppendItem(NULL, node);
		nodes=strGraphNodeIRPSetDifference(nodes, *replaced, (gnCmpType)ptrPtrCmp);
		//Get list of expression nodes
		graphNodeIRVisitBackward(node, &nodes, isEdgePred, appendToNodes);
		graphReplaceWithNode(nodes, replaceWith, NULL, (void(*)(void *))IRNodeDestroy, sizeof(*graphEdgeIRValuePtr(NULL)));
		*replaced=strGraphNodeIRPSetUnion(*replaced, nodes, (gnCmpType)ptrPtrCmp);
}
PTR_MAP_FUNCS(graphNodeMapping, graphNodeIR , Mapping2IR);
static int untillMultipleFlowIn(const struct __graphNode *node,const struct __graphEdge *edge,const void *data) {
		strGraphEdgeIRP incoming CLEANUP(strGraphEdgeIRPDestroy)=graphNodeIRIncoming((graphNodeIR)node);
		strGraphEdgeIRP inFlow CLEANUP(strGraphEdgeIRPDestroy)=IRGetConnsOfType(incoming, IR_CONN_FLOW);
		return 2>strGraphEdgeIRPSize(inFlow);
} 
static void killBranchPath(graphEdgeIR path,strGraphNodeIRP *killedNodes) {
		__auto_type inNode=graphEdgeIRIncoming(path);
		__auto_type outNode=graphEdgeIROutgoing(path);
		strGraphNodeIRP toKill CLEANUP(strGraphNodeIRPDestroy)=NULL;
		graphNodeIRVisitForward(outNode,  toKill,untillMultipleFlowIn,  appendToNodes);
		for(long i=0;i!=strGraphNodeIRPSize(toKill);i++)
				graphNodeIRKill(&toKill[i], (void(*)(void*))IRNodeDestroy, NULL);
		if(killedNodes)
				*killedNodes=strGraphNodeIRPSetUnion(*killedNodes, toKill, (gnCmpType)ptrPtrCmp);
}
static void removeConstantCondBranches(graphNodeIR start,strIRVar consts,strGraphNodeIRP *removedNodes) {
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
						if(!nodeIsConst(in[0], consts)) continue;
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
								__auto_type connectTo=graphEdgeIROutgoing(oppositeBranch[0]);
								killBranchPath(killPath,&removedNodes2);
								graphNodeIRKill(&allNodes[i], (void(*)(void*))IRNodeDestroy, NULL);
								graphNodeIRConnect(condNode, connectTo, IR_CONN_FLOW);
						}
						goto removeFromQueue;
				} else if(type==IR_JUMP_TAB) {
						strGraphNodeIRP in CLEANUP(strGraphNodeIRPDestroy)=graphNodeIRIncomingNodes(allNodes[i]);
						assert(strGraphNodeIRPSize(in)==1);
						if(!nodeIsConst(in[0], consts))
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
										killBranchPath(cases[c], &removedNodes2);
										break;
								}
						}
						goto removeFromQueue;
				}
				continue;
		removeFromQueue:
				allNodes=strGraphNodeIRPSetDifference(allNodes, visitedNodes, (gnCmpType)ptrPtrCmp);
				allNodes=strGraphNodeIRPSetDifference(allNodes, removedNodes2, (gnCmpType)ptrPtrCmp);
				goto loop;
		}
		if(removedNodes)
				*removedNodes=strGraphNodeIRPClone(removedNodes2);
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
						struct IRNodeChoose *choose=(void*)graphNodeIRValuePtr(ir);
						__auto_type first=choose->canidates[0];
						struct IRNodeValue *fValue=(void*)graphNodeIRValuePtr(first);
						strGraphNodeIRP dummy CLEANUP(strGraphNodeIRPDestroy)=strGraphNodeIRPAppendItem(NULL, ir);
						graphReplaceWithNode(dummy, IRCreateVarRef(fValue->val.value.var.value.var), NULL,(void(*)(void*))IRNodeDestroy, sizeof(enum IRConnType)) ;
				}
		}
}

static strVar2Node getUseDefines(graphNodeIR start) {
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
		strVar2Node useAssoc=NULL;
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
																dummy.uses=NULL,dummy.assign=NULL,dummy.usedBy=NULL;
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
												
														struct IRNodeValue *assignVar=(void*)graphNodeIRValuePtr(assignNode);
														//Adde value to uses
														{
																useLoop:;
																struct var2Nodes dummy;
																dummy.var=assignVar->val.value.var;
																dummy.uses=NULL,dummy.assign=NULL,dummy.usedBy=NULL;
																__auto_type find=strVar2NodeSortedFind(useAssoc, dummy, var2NodesCmp);
																if(!find) {
																		useAssoc=strVar2NodeSortedInsert(useAssoc, dummy, var2NodesCmp);
																		goto useLoop;
																}
																if(!strVarSortedFind(find->uses,&value->val.value.var,IRVarCmp2))
																		find->uses=strVarSortedInsert(find->uses,&value->val.value.var,IRVarCmp2);
														}
														//Add assignVar to usedBy
														{
														useLoop2:;
																struct var2Nodes dummy;
																dummy.var=value->val.value.var;
																dummy.uses=NULL,dummy.assign=NULL,dummy.usedBy=NULL;
																__auto_type find=strVar2NodeSortedFind(useAssoc, dummy, var2NodesCmp);
																if(!find) {
																		useAssoc=strVar2NodeSortedInsert(useAssoc, dummy, var2NodesCmp);
																		goto useLoop2;
																}
																if(!strVarSortedFind(find->usedBy,&assignVar->val.value.var,IRVarCmp2))
																		find->usedBy=strVarSortedInsert(find->usedBy,&assignVar->val.value.var,IRVarCmp2);
														}
												}
										}
								}
						}
				}
		}
		ptrMapMapping2IRDestroy(mappingPtr2IR,NULL);

		return useAssoc;
}
PTR_MAP_FUNCS(graphNodeMapping, int, Executable);
PTR_MAP_FUNCS(graphNodeMapping, int, Const);
 void IRConstPropigation(graphNodeIR start) {
		removeSSANodes(start);
		IRToSSA(start);
		strVar2Node useDefines=getUseDefines(start);

		//Both of these worklists are free'd ahead so dont use CLEANUP
		strGraphNodeIRP flowWorklist=NULL;
		strGraphNodeIRP ssaWorklist=NULL;
		ptrMapIREvalValByGN values=ptrMapIREvalValByGNCreate();
		ptrMapExecutable exec=ptrMapExecutableCreate();
		ptrMapConst consts =ptrMapConstCreate();
		flowWorklist=strGraphNodeIRPAppendItem(NULL, start);

		strVar defined CLEANUP(strVarDestroy)=NULL;
		strGraphNodeIRP flowWorklistClone CLEANUP(strGraphNodeIRPDestroy)=NULL;
		strGraphNodeIRP ssaWorlistClone CLEANUP(strGraphNodeIRPDestroy)=NULL;
	loop:;
		flowWorklistClone=NULL;
		ssaWorlistClone=NULL;
		for(;strGraphNodeIRPSize(flowWorklist)||strGraphNodeIRPSize(ssaWorklist);) {
				if(strGraphNodeIRPSize(ssaWorklist)) {
						graphNodeIR chooseNode;
						ssaWorklist=strGraphNodeIRPPop(ssaWorklist, &chooseNode);
						//Add outgoing flow(if any) to flowWorklistClone
						strGraphEdgeIRP outFlow CLEANUP(strGraphEdgeIRPDestroy) =graphNodeIROutgoing(IREndOfExpr(chooseNode));
						if(strGraphEdgeIRPSize(outFlow)) {
								assert(strGraphEdgeIRPSize(outFlow)==1);
								if(!strGraphNodeIRPSortedFind(flowWorklistClone, graphEdgeIROutgoing(outFlow[0]), (gnCmpType)ptrPtrCmp))
										flowWorklistClone=strGraphNodeIRPSortedInsert(flowWorklistClone, graphEdgeIROutgoing(outFlow[0]), (gnCmpType)ptrPtrCmp);
						}
						
						struct IRNodeChoose *choose=(void*)graphNodeIRValuePtr(chooseNode);
						struct IREvalVal currentValue;
						int firstValue=1;
						int success=1;
						for(long c=0;c!=strGraphNodeIRPSize(choose->canidates);c++) {
								struct IRNodeValue *irValue=(void*)graphNodeIRValuePtr(choose->canidates[c]);
								if(irValue->base.type==IR_VALUE) {
										if(irValue->val.type==IR_VAL_VAR_REF) {
												if(NULL==strIRVarSortedFind(defined, &irValue->val.value.var, IRVarCmp2)) {
														//If it's not defined,thats ok so long as it never gets executed
														if(ptrMapExecutableGet(exec, choose->canidates[c]))
																	goto chooseFail;
												} else {
														//Defined yay!
														if(firstValue) {
																currentValue=*ptrMapIREvalValByGNGet(values, choose->canidates[c]);
																firstValue=0;
														} else if(success) {
																__auto_type value=*ptrMapIREvalValByGNGet(values, choose->canidates[c]);;
																if(!IREvalValEqual(&currentValue, &value))
																		success=0;
														}
												}
										}
								}
						}
						if(success) {
								//SSA nodes assign into variable
								strGraphNodeIRP out CLEANUP(strGraphNodeIRPDestroy)=graphNodeIROutgoingNodes(chooseNode);
								struct IRNodeValue *irValue=(void*)graphNodeIRValuePtr(out[0]);
								if(irValue->base.type==IR_VALUE) {
										if(irValue->val.type==IR_VAL_VAR_REF)  {
												ptrMapIREvalValByGNAdd(values, out[0],currentValue);
												defined=strVarSortedInsert(defined, &irValue->val.value.var, IRVarCmp2);
										}
								}
						}
						if(!ptrMapConstGet(consts, chooseNode))
								ptrMapConstAdd(consts, chooseNode,1);
						continue;
				chooseFail:
								ptrMapConstRemove(consts, chooseNode);
				} else if(strGraphNodeIRPSize(flowWorklist)) {
						graphNodeIR flowNode;
						flowWorklist=strGraphNodeIRPPop(flowWorklist, &flowNode);
						if(IREndOfExpr(flowNode)!=flowNode) {
								strGraphNodeIRP exprNodes CLEANUP(strGraphNodeIRPDestroy)=IRStmtNodes(IREndOfExpr(flowNode));
						basicBlockLoop:
								for(long i=0;i!=strGraphNodeIRPSize(exprNodes);i++) {
										struct IRNodeValue *irValue=(void*)graphNodeIRValuePtr(exprNodes[i]);
										if(irValue->base.type==IR_VALUE) {
												if(irValue->val.type==IR_VAL_VAR_REF) {
														struct var2Nodes dummy;
														dummy.var=irValue->val.value.var;
														dummy.uses=NULL,dummy.assign=NULL,dummy.usedBy=NULL;
														__auto_type find=strVar2NodeSortedFind(useDefines, dummy, var2NodesCmp);
														if(find) {
																//Mark as executed
																if(!ptrMapExecutableGet(exec, exprNodes[i]))
																		ptrMapExecutableAdd(exec, exprNodes[i], 1);
																
																//Ensure all used are defined
																for(long i=0;i!=strVarSize(find->uses);i++) {
																		if(NULL==strVarSortedFind(defined, find->uses[i], IRVarCmp2)) {
																				goto notAllUsed;
																		}
																}
																int success;
																__auto_type value=IREvalNode(exprNodes[i], &success);
																if(!success) goto notAllUsed;
																ptrMapIREvalValByGNAdd(values, exprNodes[i], value);
																defined=strVarSortedInsert(defined,&irValue->val.value.var, IRVarCmp2);
																//Remove and restart loop
																memmove(&exprNodes[i],&exprNodes[i+1],sizeof(*exprNodes)*(strGraphNodeIRPSize(exprNodes)-i-1));
																exprNodes=strGraphNodeIRPResize(exprNodes, strGraphNodeIRPSize(exprNodes)-1);
																goto basicBlockLoop;
																continue;
														notAllUsed:;
														}
												}
										}
								}
						}
						flowNode=IREndOfExpr(flowNode);
						strGraphEdgeIRP outgoing CLEANUP(strGraphEdgeIRPDestroy)=graphNodeIROutgoing(flowNode);
						strGraphEdgeIRP flow CLEANUP(strGraphEdgeIRPDestroy)=IRGetConnsOfType(outgoing, IR_CONN_FLOW);
						if(strGraphEdgeIRPSize(outgoing)) {
								assert(strGraphEdgeIRPSize(outgoing)==1);
								__auto_type out=graphEdgeIROutgoing(outgoing[0]);
								__auto_type type=graphNodeIRValuePtr(out)->type;
								if(type==IR_COND_JUMP) {
										if(nodeIsConst(flowNode, defined)) {
												int success;
												__auto_type value=IREvalNode(IRStmtStart(flowNode), &success);
												if(!success)
														goto ifStmtMarkAllOut;
												int64_t cond;
												if(value.type==IREVAL_VAL_INT)
														cond=value.value.i;
												else if(value.type==IREVAL_VAL_FLT)
														cond=value.value.flt!=0.0;
												else
														goto ifStmtMarkAllOut;

												{
														strGraphEdgeIRP ifOut CLEANUP(strGraphEdgeIRPDestroy)=graphNodeIROutgoing(out);
														if(cond) {
																strGraphEdgeIRP truePath CLEANUP(strGraphEdgeIRPDestroy)=IRGetConnsOfType(ifOut, IR_CONN_COND_TRUE);
																flowWorklistClone=strGraphNodeIRPSortedInsert(flowWorklistClone, graphEdgeIROutgoing(truePath[0]), (gnCmpType)ptrPtrCmp); 
														} else {
																strGraphEdgeIRP falsePath CLEANUP(strGraphEdgeIRPDestroy)=IRGetConnsOfType(ifOut, IR_CONN_COND_FALSE);
																flowWorklistClone=strGraphNodeIRPSortedInsert(flowWorklistClone, graphEdgeIROutgoing(falsePath[0]), (gnCmpType)ptrPtrCmp);
														}
												}
												continue;
										ifStmtMarkAllOut: {
														strGraphNodeIRP ifOut CLEANUP(strGraphNodeIRPDestroy)=graphNodeIROutgoingNodes(out);
														flowWorklistClone=strGraphNodeIRPSetUnion(flowWorklistClone, ifOut, (gnCmpType)ptrPtrCmp);
												}
										}
								} else if(type==IR_JUMP_TAB) {
										if(nodeIsConst(flowNode, defined)) {
												int success;
												__auto_type value=IREvalNode(IRStmtStart(flowNode), &success);
												if(!success)
														goto ifStmtMarkAllOut;
												int64_t cond;
												if(value.type==IREVAL_VAL_INT)
														cond=value.value.i;
												else if(value.type==IREVAL_VAL_FLT)
														cond=value.value.flt!=0.0;
												else
														continue;
												struct IRNodeJumpTable *table=(void*)graphNodeIRValuePtr(flowNode);
												strGraphEdgeIRP out CLEANUP(strGraphEdgeIRPDestroy)=graphNodeIROutgoing(flowNode);
												strGraphEdgeIRP dft CLEANUP(strGraphEdgeIRPDestroy)=IRGetConnsOfType(out, IR_CONN_DFT);
												assert(strGraphEdgeIRPSize(dft)==1);
												graphNodeIR choosenNode=graphEdgeIROutgoing(dft[0]);
												for(long c=0;c!=strIRTableRangeSize(table->labels);c++) {
														if(table->labels[c].start<=cond&&table->labels[c].end>cond) {
																choosenNode=table->labels[c].to;
																break;
														}
												}
												flowWorklistClone=strGraphNodeIRPSortedInsert(flowWorklistClone, choosenNode, (gnCmpType)ptrPtrCmp);
										}
								} else if(type==IR_CHOOSE) {
										ssaWorlistClone=strGraphNodeIRPSortedInsert(ssaWorlistClone, out, (gnCmpType)ptrPtrCmp);
								} else {
										flowWorklistClone=strGraphNodeIRPSortedInsert(flowWorklistClone, out, (gnCmpType)ptrPtrCmp);
								} 
						} 
				}
		}
		strGraphNodeIRPDestroy(&ssaWorklist);
		strGraphNodeIRPDestroy(&flowWorklist);
		flowWorklist=flowWorklistClone;
		ssaWorklist=ssaWorlistClone;
		if(strGraphNodeIRPSize(ssaWorklist)||strGraphNodeIRPSize(flowWorklist))
				goto loop;

		long count=ptrMapIREvalValByGNSize(values);
		graphNodeIR __toReplace[count];
		ptrMapIREvalValByGNKeys(values, __toReplace);
		strGraphNodeIRP toReplace CLEANUP(strGraphNodeIRPDestroy)=strGraphNodeIRPAppendData(NULL, __toReplace, count);
	replaceLoop:
		for(;strGraphNodeIRPSize(toReplace);) {
				__auto_type value=*ptrMapIREvalValByGNGet(values, toReplace[0]);
				strGraphNodeIRP replaced CLEANUP(strGraphNodeIRPDestroy)=NULL;
				replaceExprWithConstant(toReplace[0], value,  &replaced);
				toReplace=strGraphNodeIRPSetDifference(toReplace, replaced, (gnCmpType)ptrPtrCmp);
		}
		
		ptrMapIREvalValByGNDestroy(values, (void(*)(void*))IREvalValDestroy);
}
