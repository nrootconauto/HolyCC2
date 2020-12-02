#include <IRLiveness.h>
#include <SSA.h>
#include <assert.h>
#include <graphColoring.h>
#include <subExprElim.h>
#define DEBUG_PRINT_ENABLE 1
#include <debugPrint.h>
#include <graphDominance.h>
#include <base64.h>
static const char *IR_ATTR_VAR_ALIVE_AT_NODE="ALIVE_AT_NODE";
typedef int (*gnCmpType)(const graphNodeIR *, const graphNodeIR *);
MAP_TYPE_DEF(struct regSlice,RegSlice);
MAP_TYPE_FUNCS(struct regSlice,RegSlice);
static int ptrPtrCmp(const void *a, const void *b) {
	if (*(void **)a > *(void **)b)
		return 1;
	else if (*(void **)a < *(void **)b)
		return -1;
	else
		return 0;
}
static char *ptr2Str(const void *a) {
		return base64Enc((void*)&a, sizeof(a));
}
static void transparentKill(graphNodeIR node) {
	__auto_type incoming = graphNodeIRIncoming(node);
	__auto_type outgoing = graphNodeIROutgoing(node);
	for (long i1 = 0; i1 != strGraphEdgeIRPSize(incoming); i1++)
		for (long i2 = 0; i2 != strGraphEdgeIRPSize(outgoing); i2++)
			graphNodeIRConnect(graphEdgeIRIncoming(incoming[i1]),
			                   graphEdgeIROutgoing(outgoing[i2]),
			                   *graphEdgeIRValuePtr(incoming[i1]));

	graphNodeIRKill(&node, IRNodeDestroy, NULL);
}
static int noIncomingPred(const void *data, const graphNodeIR *node) {
	strGraphEdgeIRP incoming __attribute__((cleanup(strGraphEdgePDestroy))) =
	    graphNodeIRIncoming(*node);
	return strGraphEdgeIRPSize(incoming) != 0;
};
static void replaceNodeWithExpr(graphNodeIR node, graphNodeIR valueSink) {
	__auto_type incoming = graphNodeIRIncoming(node);
	__auto_type outgoing = graphNodeIROutgoing(node);

	// Find input nodes(with no incoming)
	__auto_type sources = graphNodeIRAllNodes(valueSink);
	sources = strGraphNodeIRPRemoveIf(graphNodeIRAllNodes(valueSink), NULL,
	                                  noIncomingPred);

	// Connnect incoming to sources
	for (long i1 = 0; i1 != strGraphEdgeIRPSize(incoming); i1++)
		for (long i2 = 0; i2 != strGraphNodeIRPSize(sources); i2++)
			graphNodeIRConnect(graphEdgeIRIncoming(incoming[i1]), sources[i2],
			                   *graphEdgeIRValuePtr(incoming[i1]));

	// Connect sink to outgoing
	for (long i1 = 0; i1 != strGraphEdgeIRPSize(outgoing); i1++)
		graphNodeIRConnect(valueSink, graphEdgeIROutgoing(outgoing[i1]),
		                   *graphEdgeIRValuePtr(outgoing[i1]));

	strGraphEdgeIRPDestroy(&incoming), strGraphEdgeIRPDestroy(&outgoing);
	graphNodeIRKill(&node, IRNodeDestroy, NULL);
}
static int gnIRVarCmp(const graphNodeIR *a, const graphNodeIR *b) {
	__auto_type A = (struct IRNodeValue *)graphNodeIRValuePtr(*a);
	__auto_type B = (struct IRNodeValue *)graphNodeIRValuePtr(*b);
	assert(A->base.type == IR_VALUE);
	assert(B->base.type == IR_VALUE);

	return IRVarCmp(&A->val.value.var, &B->val.value.var);
}
static void removeChooseNode(graphNodeIR chooseNode) {
	// Kill node,remove outgoing nodes too if they variable they point to is
	// unused
	__auto_type outgoingNodes = graphNodeIROutgoingNodes(chooseNode);

	// Remove unused result vars
	for (long i2 = 0; i2 != strGraphNodeIRPSize(outgoingNodes); i2++) {
		__auto_type outgoingEdges = graphNodeIROutgoing(outgoingNodes[i2]);
		__auto_type filtered = IRGetConnsOfType(outgoingEdges, IR_CONN_FLOW);

		// If only flow out,then no need for variable
		int useless =
		    strGraphEdgeIRPSize(filtered) == strGraphEdgeIRPSize(outgoingEdges);

		strGraphEdgeIRPDestroy(&outgoingEdges);
		strGraphEdgeIRPDestroy(&filtered);

		// Remove var if useless
		if (useless)
			transparentKill(outgoingNodes[i2]);
	}

	strGraphNodeIRPDestroy(&outgoingNodes);
	transparentKill(chooseNode);
}
static void removeChooseNodes(strGraphNodeIRP nodes, graphNodeIR start) {
	for (long i = 0; i != strGraphNodeIRPSize(nodes); i++) {
		__auto_type val = graphNodeIRValuePtr(nodes[i]);
		// No value?
		if (!val)
			continue;

		// Ignore non-chooses
		if (val->type != IR_CHOOSE)
			continue;

		removeChooseNode(nodes[i]);
	}
}
LL_TYPE_DEF(strGraphNodeIRP, VarRefs);
LL_TYPE_FUNCS(strGraphNodeIRP, VarRefs);
struct aliasPair {
	graphNodeIR a, b;
};
static int aliasPairCmp(const struct aliasPair *a, const struct aliasPair *b) {
	struct IRNodeValue *Aa = (void *)graphNodeIRValuePtr(a->a);
	struct IRNodeValue *Ba = (void *)graphNodeIRValuePtr(b->a);
	struct IRNodeValue *Ab = (void *)graphNodeIRValuePtr(a->b);
	struct IRNodeValue *Bb = (void *)graphNodeIRValuePtr(b->b);
	int cmpA = IRVarCmp(&Aa->val.value.var, &Ba->val.value.var);
	if (cmpA != 0)
		return cmpA;

	int cmpB = IRVarCmp(&Ab->val.value.var, &Bb->val.value.var);
	if (cmpB != 0)
		return cmpB;

	return 0;
}
static int varRefsInsertCmp(const strGraphNodeIRP *a,
                            const strGraphNodeIRP *b) {
	struct IRNodeValue *valA = (void *)graphNodeIRValuePtr(a[0][0]),
	                   *valB = (void *)graphNodeIRValuePtr(b[0][0]);
	return IRVarCmp((const struct IRVar *)&valA->val.value.var,
	                (const struct IRVar *)&valB->val.value.var);
}
static int varRefsGetCmp(const void *a, const strGraphNodeP *b) {
	struct IRNodeValue *valB = (void *)graphNodeIRValuePtr(b[0][0]);
	struct IRNodeValue *valA = (void *)graphNodeIRValuePtr((graphNodeIR)a);
	return IRVarCmp((const struct IRVar *)&valA->val.value.var,
	                (const struct IRVar *)&valB->val.value.var);
}
STR_TYPE_DEF(struct aliasPair, AliasPair);
STR_TYPE_FUNCS(struct aliasPair, AliasPair);
STR_TYPE_DEF(strGraphNodeIRP, AliasBlob);
STR_TYPE_FUNCS(strGraphNodeIRP, AliasBlob);
static int AliasBlobCmp(const strGraphNodeIRP *A, const strGraphNodeIRP *B) {
	long aSize = strGraphNodeIRPSize(*A);
	long bSize = strGraphNodeIRPSize(*B);
	long max = (aSize > bSize) ? aSize : bSize;

	// Check for difference in shared portion
	for (long i = 0; i != max; i++) {
		if (ptrPtrCmp(&A[i], &B[i]))
			return ptrPtrCmp(&A[i], &B[i]);
	}

	// Now compare by sizes
	if (aSize == bSize)
		return 0;
	else if (aSize > bSize)
		return 1;
	else if (aSize < bSize)
		return -1;

	return 0;
}
void IRCoalesce(strGraphNodeIRP nodes, graphNodeIR start) {
	llVarRefs refs = NULL;
	strAliasPair aliases = NULL;

	for (long i = 0; i != strGraphNodeIRPSize(nodes); i++) {
		__auto_type val = graphNodeIRValuePtr(nodes[i]);
		// No value?
		if (!val)
			continue;

		// Ignore non-chooses
		if (val->type == IR_CHOOSE) {
			struct IRNodeChoose *choose = (void *)val;
			//
			// Remove choose nodes whoose incoming vars are all the same ,ALSO,be sure
			// to alias incoming var to choose with outgoing to choose
			//
			strGraphNodeIRP clone __attribute__((cleanup(strGraphNodeIRPDestroy))) =
			    strGraphNodeIRPUnique(strGraphNodeIRPClone(choose->canidates),
			                          (gnCmpType)ptrPtrCmp, NULL);
			if (strGraphNodeIRPSize(clone) == 1) {
				struct IRNodeValue *value = (void *)graphNodeIRValuePtr(clone[i]);
				assert(value->base.type == IR_VALUE);
				assert(value->val.type == IR_VAL_VAR_REF);

				// Check for signle outgoing vars to alias to
				strGraphEdgeIRP outgoing
				    __attribute__((cleanup(strGraphEdgeIRPDestroy))) =
				        graphNodeIROutgoing(clone[i]);
				strGraphEdgeIRP outgoingAssigns
				    __attribute__((cleanup(strGraphEdgeIRPDestroy))) =
				        IRGetConnsOfType(outgoing, IR_CONN_DEST);

				for (long i = 0; i != strGraphEdgeIRPSize(outgoingAssigns); i++) {
					__auto_type to = graphEdgeIROutgoing(outgoingAssigns[i]);
					struct IRNodeValue *val = (void *)graphNodeIRValuePtr(to);
					assert(val->base.type == IR_VALUE);
					// Only alias to vars
					if (val->val.type != IR_VAL_VAR_REF)
						continue;

					// Alias to previous new version of choose var
					struct aliasPair apForward = {clone[i], nodes[i]};
					if (NULL ==
					    strAliasPairSortedFind(aliases, apForward, aliasPairCmp)) {
						aliases =
						    strAliasPairSortedInsert(aliases, apForward, aliasPairCmp);
					}
				}
			}

		} else if (val->type == IR_VALUE) {
			// Is a var,check for direct assign
			if (((struct IRNodeValue *)val)->val.type == IR_VAL_VAR_REF) {
			findVarLoop:;
				__auto_type find = llVarRefsFind(refs, nodes[i], varRefsGetCmp);
				__auto_type find2 =
				    llVarRefsFindRight(llVarRefsFirst(refs), nodes[3], varRefsGetCmp);

				if (!find) {
					// Add a vec of references(only reference is nodes[i])
					DEBUG_PRINT("Adding var node %s\n", debugGetPtrNameConst(nodes[i]));
					__auto_type newRef =
					    llVarRefsCreate(strGraphNodeIRPAppendItem(NULL, nodes[i]));
					llVarRefsInsert(refs, newRef, varRefsInsertCmp);
					refs = newRef;
				} else {
					DEBUG_PRINT("Adding var ref %s\n", debugGetPtrNameConst(nodes[i]));

					// Add nodes[i] to references
					__auto_type vec = llVarRefsValuePtr(find);
					*vec =
					    strGraphNodeIRPSortedInsert(*vec, nodes[i], (gnCmpType)ptrPtrCmp);
				}

				// Check if written into by another variable.
				__auto_type incoming = graphNodeIRIncoming(nodes[i]);
				__auto_type filtered = IRGetConnsOfType(incoming, IR_CONN_DEST);

				// aliasNode is NULL if an alias node isnt found
				graphNodeIR aliasNode = NULL;

				// If one filtered that is a value and a variable,set that as the alias
				// node
				if (strGraphEdgeIRPSize(filtered) == 1) {
					__auto_type inNode =
					    graphNodeIRValuePtr(graphEdgeIRIncoming(filtered[0]));
					if (inNode->type == IR_VALUE) {
						struct IRNodeValue *inValueNode = (void *)inNode;
						if (inValueNode->val.type == IR_VAL_VAR_REF) {
							aliasNode = graphEdgeIRIncoming(filtered[0]);
							DEBUG_PRINT("%s aliased to node %s\n",
							            debugGetPtrNameConst(aliasNode),
							            debugGetPtrNameConst(nodes[i]));
						}
					}
				}

				strGraphEdgeIRPDestroy(&incoming), strGraphEdgeIRPDestroy(&filtered);

				// If alais,add entry(if doesnt already exist)
				if (aliasNode) {
					struct aliasPair apForward = {aliasNode, nodes[i]};
					if (NULL ==
					    strAliasPairSortedFind(aliases, apForward, aliasPairCmp)) {
						aliases =
						    strAliasPairSortedInsert(aliases, apForward, aliasPairCmp);
					}
				}
			}
		}
	}

	//
	// Merge aliases that share same head/tail untill you cant do it anymore. This
	// allows for multiple vars to be merges For Example: a=b=c would be(before)
	// b<->c
	// a<->b
	// which would be turned into(after)
	// a<->b<->c

	// Turn pairs into blobs.
	strAliasBlob blobs = NULL;
	for (long i = 0; i != strAliasPairSize(aliases); i++) {
		strGraphNodeIRP blob = NULL;
		blob =
		    strGraphNodeIRPSortedInsert(blob, aliases[i].a, (gnCmpType)gnIRVarCmp);
		blob =
		    strGraphNodeIRPSortedInsert(blob, aliases[i].b, (gnCmpType)gnIRVarCmp);

		blobs = strAliasBlobSortedInsert(blobs, blob, AliasBlobCmp);
	}

	for (;;) {
		int changed = 0;

	loop:;
		// First Check heads(i1) with tails(i2)
		for (long i1 = 0; i1 < strAliasBlobSize(blobs); i1++) {
			for (long i2 = 0; i2 < strAliasBlobSize(blobs); i2++) {
				if (i1 == i2)
					continue;

				// Check for a set intersection,this tells if there is overlap between
				// the aliased vars
				strGraphNodeIRP intersect
				    __attribute__((cleanup(strGraphNodeIRPDestroy))) =
				        strGraphNodeIRPSetIntersection(strGraphNodeIRPClone(blobs[i1]),
				                                       blobs[i2], (gnCmpType)gnIRVarCmp,
				                                       NULL);

				if (0 != strGraphNodeIRPSize(intersect)) {
					DEBUG_PRINT("Intersect between %s and %s\n",
					            debugGetPtrNameConst(blobs[i1][0]),
					            debugGetPtrNameConst(blobs[i2][0]));
#if DEBUG_PRINT_ENABLE
					printf("i1:\n");
					for (long i = 0; i != strGraphNodeIRPSize(blobs[i1]); i++)
						printf("    %s\n", debugGetPtrNameConst(blobs[i1][i]));

					printf("i2:\n");
					for (long i = 0; i != strGraphNodeIRPSize(blobs[i2]); i++)
						printf("    %s\n", debugGetPtrNameConst(blobs[i2][i]));
#endif

					// Merge tail node with head node
					blobs[i2] = strGraphNodeIRPSetUnion(blobs[i2], blobs[i1],
					                                    (gnCmpType)gnIRVarCmp);

#if DEBUG_PRINT_ENABLE
					printf("result:\n");
					for (long i = 0; i != strGraphNodeIRPSize(blobs[i2]); i++)
						printf("    %s\n", debugGetPtrNameConst(blobs[i2][i]));
#endif
					// Remove blobs[i1] as it is merged in with blobs[i2]
					memmove(&blobs[i1], &blobs[i1] + 1,
					        (strAliasBlobSize(blobs) - 1) * sizeof(blobs[i1]));
					// Decrese size by 1
					blobs = strAliasBlobPop(blobs, NULL);

					changed = 1;
					goto loop;
				}
			}
		}

		// Didn't loop back
		if (!changed)
			break;
	}

	//
	// Replace vars with aliases
	//
	for (long i = 0; i != strAliasBlobSize(blobs); i++) {
		DEBUG_PRINT("Finding var %s\n", debugGetPtrNameConst(blobs[i][0]));
		__auto_type find = llVarRefsFind(refs, blobs[i][0], varRefsGetCmp);

		// Find first ref.
		strGraphNodeIRP refs2 = *llVarRefsValuePtr(find);
		__auto_type master = refs2[0];

		// Replace rest of blobs
		for (long i2 = 0; i2 != strGraphNodeIRPSize(blobs[i]); i2++) {
			__auto_type find2 =
			    *llVarRefsValuePtr(llVarRefsFind(refs, blobs[i][i2], varRefsGetCmp));
			for (long i3 = 0; i3 != strGraphNodeIRPSize(find2); i3++) {
				// Dont replace self
				if (find2[i3] == master)
					continue;

				DEBUG_PRINT("Replacing %s with %s\n", debugGetPtrNameConst(find2[i3]),
				            debugGetPtrNameConst(master));
				// Replace with cloned value
				replaceNodeWithExpr(find2[i3], cloneNode(master, IR_CLONE_NODE, NULL));
			}
		}
	}
}
static int isVar(graphNodeIR node) {
	if (graphNodeIRValuePtr(node)->type == IR_VALUE) {
		// Is a variable
		struct IRNodeValue *valOut = (void *)graphNodeIRValuePtr(node);
		if (valOut->val.type == IR_VAL_VAR_REF) {
			return 1;
		}
	}
	return 0;
}
void IRRemoveRepeatAssigns(graphNodeIR enter) {
	__auto_type allNodes = graphNodeIRAllNodes(enter);

	strGraphNodeIRP toRemove = NULL;
	for (long i = 0; i != strGraphNodeIRPSize(allNodes); i++) {
		// If not a var,continue
		if (!isVar(allNodes[i]))
			continue;

		// Check for assign
		__auto_type outgoing = graphNodeIROutgoing(allNodes[i]);
		__auto_type outgoingAssign = IRGetConnsOfType(outgoing, IR_CONN_DEST);
		if (strGraphEdgeIRPSize(outgoingAssign) == 1) {
			// Check if assigned to value
			__auto_type out = graphEdgeIROutgoing(outgoingAssign[0]);
			if (isVar(out)) {
				// Compare if vars are equal
				struct IRNodeValue *valIn, *valOut;
				valIn = (void *)graphNodeIRValuePtr(allNodes[i]);
				valOut = (void *)graphNodeIRValuePtr(out);

				if (0 == IRVarCmp(&valIn->val.value.var, &valOut->val.value.var)) {
					// Add first reference to variable(valIn) for removal
					toRemove = strGraphNodeIRPAppendItem(toRemove, allNodes[i]);
				}
			}

			strGraphEdgeIRPDestroy(&outgoingAssign);
			strGraphEdgeIRPDestroy(&outgoing);
		}
	}

	//(Transparently) remove all items marked for removal
	for (long i = 0; i != strGraphNodeIRPSize(toRemove); i++)
		transparentKill(toRemove[i]);

	// Remove removed
	allNodes =
	    strGraphNodeIRPSetDifference(allNodes, toRemove, (gnCmpType)ptrPtrCmp);

	// Find duds(nodes that arent connected to conditionals or expressions)
	strGraphNodeIRP duds = NULL;
	for (long i = 0; i != strGraphNodeIRPSize(allNodes); i++) {
		if (!isVar(allNodes[i]))
			continue;

		__auto_type incoming = graphNodeIRIncoming(allNodes[i]);
		__auto_type outgoing = graphNodeIRIncoming(allNodes[i]);

		// Check if exprssion incoming
		int isUsedIn = 0;
		for (long i = 0; i != strGraphEdgeIRPSize(incoming); i++) {
			if (*graphEdgeIRValuePtr(incoming[i]) != IR_CONN_FLOW) {
				isUsedIn = 1;
				break;
			}
		}

		// Check if expression outgoing,OR IF CONNECTED TO CONDTIONAL
		int isUsedOut = 0;
		for (long i = 0; i != strGraphEdgeIRPSize(outgoing); i++) {
			if (*graphEdgeIRValuePtr(outgoing[i]) != IR_CONN_FLOW) {
				isUsedOut = 1;
				break;
			}
		}

		// Free
		strGraphEdgeIRPDestroy(&incoming), strGraphEdgeIRPDestroy(&outgoing);

		// IF only flows incoimg/outgoing,node is useless to replace
		if (!isUsedOut && !isUsedOut)
			duds = strGraphNodeIRPAppendItem(duds, allNodes[i]);
	}

	// Kill duds
	for (long i = 0; i != strGraphNodeIRPSize(duds); i++)
		transparentKill(duds[i]);

	strGraphNodeIRPDestroy(&duds);
	strGraphNodeIRPDestroy(&allNodes);
	strGraphNodeIRPDestroy(&toRemove);
}
static int IsInteger(struct object *obj) {
	// Check if ptr
	if (obj->type == TYPE_PTR||obj->type==TYPE_ARRAY)
		return 1;

	// Check if integer type
	struct object *valids[] = {
	    &typeI8i,  &typeI16i, &typeI32i, &typeI64i, &typeU8i,
	    &typeU16i, &typeU32i, &typeU64i, &typeBool,
	};
	for (long i = 0; i != sizeof(valids) / sizeof(*valids); i++)
		if (valids[i] == obj)
			return 1;

	return 0;
}
static int isFloating(struct object *obj) {
		return obj==&typeF64;
}
static double interfereMetric(double cost,graphNodeIRLive node) {
		//
		// graphNodeIRLive is undirected node!!!
		//
		__auto_type connections=graphNodeIRLiveOutgoing(node);
		double retVal=cost/strGraphEdgeIRLivePSize(connections);

		strGraphEdgeIRLivePDestroy(&connections);
		return retVal;
}
struct conflictPair {
		graphNodeIRLive a;
		graphNodeIRLive b;
		double aWeight;
		double bWeight;
};
static int conflictPairWeightCmp(const void *a,const void *b) {
		const struct conflictPair *A=a,*B=b;
		
		double minWeightA=(A->aWeight<A->bWeight)?A->aWeight:A->bWeight;
		double minWeightB=(B->aWeight<B->bWeight)?B->aWeight:B->bWeight;

		if(minWeightA>minWeightB)
				return 1;
		else if(minWeightA<minWeightB)
				return -1;
		return 0;
}
static int conflictPairCmp(const struct conflictPair *a,const struct conflictPair *b) {
		int cmp=ptrPtrCmp(&a->a, &b->a);
		if(cmp!=0)
				return cmp;

		return ptrPtrCmp(&a->b, &b->b);
}
STR_TYPE_DEF(struct conflictPair,ConflictPair);
STR_TYPE_FUNCS(struct conflictPair,ConflictPair);
static strConflictPair recolorAdjacentNodes(mapRegSlice node2RegSlice,graphNodeIRLive node) {
		__auto_type allNodes=graphNodeIRLiveAllNodes(node);
		
		strConflictPair conflicts=NULL;
		for(long i=0;i!=strGraphNodeIRLivePSize(allNodes);i++) {
				__auto_type outgoingNodes=graphNodeIRLiveOutgoingNodes(allNodes[i]);

				//Get current register
				char *key=ptr2Str(allNodes[i]);
				__auto_type find=mapRegSliceGet(node2RegSlice, key);
				free(key);
				if(!find) {
				whineNotColored:
						printf("Dear programmer,try assigning registers to nodes before calling %s", __FUNCTION__);
						abort();
				}

				__auto_type masterSlice=*find;

				//Check for conflicting adjacent items
				for(long i2=0;i2!=strGraphNodeIRLivePSize(outgoingNodes);i2++) {
						char *key=ptr2Str(outgoingNodes[i2]);
						__auto_type find=mapRegSliceGet(node2RegSlice, key);
						if(!find)
								goto whineNotColored;

						//If conflict
						if(regSliceConflict(&masterSlice, find)) {
								struct conflictPair pair={allNodes[i],outgoingNodes[i2]};

								//Check if exists if the pair is reverses,there is no need for duplicates
								struct conflictPair backwards={outgoingNodes[i2],allNodes[i]};
								if(NULL==strConflictPairSortedFind(conflicts, backwards, conflictPairCmp))
										continue;
								
								//Check if exists,if so dont insert
								if(NULL==strConflictPairSortedFind(conflicts, pair, conflictPairCmp)) {
										double aWeight=interfereMetric(1.1, allNodes[i]); //TODO implememnt cost
										double bWeight=interfereMetric(1.1, outgoingNodes[i2]);
										pair.aWeight=aWeight;
										pair.bWeight=bWeight;
										conflicts=strConflictPairSortedInsert(conflicts, pair,conflictPairCmp);
								}
						}
				}

				strGraphNodeIRLivePDestroy(&outgoingNodes);
		}
		
		strGraphNodeIRLivePDestroy(&allNodes);

		return conflicts;
}
static int filterIntVars(graphNodeIR node,const void *data) {
		struct IRNodeValue *value=(void*)graphNodeIRValuePtr(node);
		if(value->base.type!=IR_VALUE)
				return 0;
		
		return IsInteger(IRValueGetType(&value->val));
}
static int filterFloatVars(graphNodeIR node,const void *data) {
		struct IRNodeValue *value=(void*)graphNodeIRValuePtr(node);
		if(value->base.type!=IR_VALUE)
				return 0;

		return isFloating(IRValueGetType(&value->val));
}
static int isVarNode(const struct IRNode *irNode) {
	if (irNode->type == IR_VALUE) {
		struct IRNodeValue *val = (void *)irNode;
		if (val->val.type == IR_VAL_VAR_REF)
			return 1;
	}

	return 0;
}
STR_TYPE_DEF(int,Int);
STR_TYPE_FUNCS(int,Int);
static int intCmp(const int *a,const int *b) {
		return *a-*b;
}
strInt getColorList(llVertexColor vertexColors) {
		strInt retVal=NULL;
		for(__auto_type node=llVertexColorFirst(vertexColors);node!=NULL;node=llVertexColorNext(node)) {
				//Insert into retVal if color isnt already in retVal
				if(NULL==strIntSortedFind(retVal, llVertexColorValuePtr(node)->color, intCmp))
						retVal=strIntSortedInsert(retVal, llVertexColorValuePtr(node)->color, intCmp);
		}

		return retVal;
}
struct metricPair {
		graphNodeIRLive node;
		double metricValue;
};
static int metricPairCmp(const void *a,const void *b) {
		const struct metricPair *A=a,*B=b;
		if(A->metricValue>B->metricValue)
				return 1;
		else if(A->metricValue<B->metricValue)
				return -1;
		else
				return 0;
}
STR_TYPE_DEF(struct IRVar*,IRVar);
STR_TYPE_FUNCS(struct IRVar*,IRVar);
typedef int(*IRVarCmpType)(const struct IRVar**,const struct IRVar**);

struct interferencePair {
		struct IRVar *var;
		strIRVar inteferesWith;
};
//TODO rename
static int spillOrStoreAt(const struct __graphNode *node,const struct interferencePair *pair) {
		//Check if hit a choose node that "consumes" pair->var. Chooses mark the end of one version of a var and a transition to the next version
		struct IRNode *value=graphNodeIRValuePtr((graphNodeIR)node);
		if(value->type==IR_CHOOSE) {
				struct IRNodeChoose *choose=(void*)value;
				for(long i=0;i!=strGraphNodeIRPSize(choose->canidates);i++) {
						assert(isVar(choose->canidates[i]));
						struct IRNodeValue *val=(void*)graphNodeIRValuePtr(choose->canidates[i]);
						if(0==IRVarCmp(&val->val.value.var,pair->var)) {
								return 1;
						}
				}
		}

		//Check if hit a varaible that intereferes with pair->var
		if(isVar((graphNodeIR)node)) {
				strIRVar vars2=pair->inteferesWith;
				for(long i=0;i!=strIRVarSize(vars2);i++) {
						struct IRNodeValue *val=(void*)graphNodeIRValuePtr((graphNodeIR)node);
						return 0==IRVarCmp(&val->val.value.var, vars2[i]);
				}
		}

		return 0;
}
static void findVarInterfereAt(strGraphNodeIRLiveP allLiveNodes,strGraphNodeIRP allIRNodes,struct IRVar *var) {
		graphNodeIRLive liveNode=NULL;
		
		//Find var in interference graph
		for(long i=0;i!=strGraphNodeIRLivePSize(allLiveNodes);i++) {
				if(0==IRVarCmp(graphNodeIRLiveValuePtr(allLiveNodes[i])->ref,var)) {
						liveNode=allLiveNodes[i];
						break;
				}
		}
		
		//Find variables that interfere with var
		__auto_type interfere=graphNodeIRLiveOutgoingNodes(liveNode);
		strIRVar allVars=NULL;
		for(long i=0;i!=strGraphNodeIRLivePSize(interfere);i++) {
				allVars=strIRVarSortedInsert(allVars, graphNodeIRLiveValuePtr(interfere[i])->ref, (IRVarCmpType)IRVarCmp);
		}

		for(long i1=0;i1!=strGraphNodeIRPSize(allIRNodes);i1++) {
				if(isVar(allIRNodes[i1])) {
						//Check if expected var
						if(0==IRVarCmp(var, &((struct IRNodeValue*)graphNodeIRValuePtr(allIRNodes[i1]))->val.value.var)) {
								struct interferencePair pair;
								pair.inteferesWith=allVars;
								pair.var=var;
								//
								// Find all paths to either
								// A) A choose node that signals the end of the current version of var or
								// B) a reference to a var that interferes with var
								//
								__auto_type paths=graphAllPathsToPredicate(allIRNodes[i1], allVars,  (int(*)(const struct __graphNode*,const void*))spillOrStoreAt);
								for(long i2=0;i2!=strGraphPathSize(paths);i2++) {
										if(!paths[i2])
												continue;
										
										//Ignore  choose nodes,we dont need to spill at them( i hope?!?!?)
										__auto_type last=paths[i2][strGraphEdgePSize(paths[i2])-1];
										__auto_type lastNode=graphEdgeIROutgoing(last);
										if(graphNodeIRValuePtr(lastNode)->type==IR_CHOOSE)
												continue;

										//Check if points to variable(which is should)
										else if(graphNodeIRValuePtr(lastNode)->type==IR_VALUE) {
												if(isVar(lastNode)) {
														//We found a conflicting variable,so store var's value before entering new variable,then load new variable's value
														__auto_type spill=createSpill(var);
														IRInsertBefore(lastNode, spill, spill, IR_CONN_FLOW);

														//Insert load
														__auto_type newVar=&((struct IRNodeValue*)graphNodeIRValuePtr(lastNode))->val.value.var;
														__auto_type load=createLoad(newVar);
														IRInsertBefore(lastNode, load,load, IR_CONN_FLOW);
														continue;
												}
										}
								}
						}
				}
		}
}
//
// This a function for turning colors into registers
//
typedef struct regSlice (*color2RegPredicate)(strRegSlice adjacent,strRegP avail,graphNodeIRLive live,int color,const void *data,long colorCount,const int *colors);
static struct regSlice color2Reg(strRegSlice adjacent,strRegP avail,graphNodeIRLive live,int color,const void *data,long colorCount,const int *colors) {
		int *find=bsearch(&color , colors, colorCount, sizeof(int), (int(*)(const void*,const void*))intCmp);
		assert(find);

		struct regSlice slice;
		slice.reg=avail[*find%strRegPSize(avail)];
		slice.offset=0;
		slice.widthInBits=slice.reg->size*8;
		return slice;
}
STR_TYPE_DEF(struct metricPair,MetricPair);
STR_TYPE_FUNCS(struct metricPair,MetricPair);
static struct conflictPair *__conflictPairFindAffects(graphNodeIRLive node,const struct conflictPair *start,const struct conflictPair *end) {
		for(;start!=end;start++) {
				if(start->a==node)
						return (void*)start;
				else if(start->a==node)
						return (void*)start;
		}

		return NULL;
}
strConflictPair conflictPairFindAffects(graphNodeIRLive node,strConflictPair pairs) {
		strConflictPair retVal=NULL;
		__auto_type start=pairs;
		__auto_type end=pairs+strConflictPairSize(pairs);
		for(;start!=end;) {
				__auto_type find=__conflictPairFindAffects(node, start, end);
				//Quit if no find
				if(!find)
						break;

				//Insert
				retVal=strConflictPairSortedInsert(retVal, *find, conflictPairCmp);

				//Continue from after find
				start=find+1;
		}

		return retVal;
}
static int conflictPairContains(graphNodeIRLive node,const struct conflictPair *pair) {
		return pair->a==node||pair->b==node;
}
static void IRRegisterAllocate(graphNodeIR start,color2RegPredicate colorFunc,void *colorData) {
		//SSA
		__auto_type allNodes = graphNodeIRAllNodes(start);
	removeChooseNodes(allNodes, start);
	IRToSSA(start);

	//Merge variables that can be merges
	__auto_type allNodes2 = graphNodeIRAllNodes(start);
	IRCoalesce(allNodes, start);
	IRRemoveRepeatAssigns(start);
	
	//Contruct an interference graph and color it
	__auto_type interfere=IRInterferenceGraph(start);

	//Do integer and floating variables seperatley
	__auto_type intRegs=getIntRegs();
	__auto_type floatRegs=getFloatRegs();

	__auto_type intInterfere=IRInterferenceGraphFilter(start, filterIntVars,NULL);
	__auto_type floatInterfere=IRInterferenceGraphFilter(start, filterFloatVars,NULL);
	
	//Compute int and flaoting interfernce seperatly
	strGraphNodeIRLiveP spillNodes=NULL;
	for(long i=0;i!=strGraphNodeIRLivePSize(intInterfere);i++)
	{
			__auto_type interfere=intInterfere[i];
			
			__auto_type vertexColors=graphColor(interfere);
			//	recolorAdjacentNodes(interfere,vertexColors);

			__auto_type allNodes=graphNodeIRAllNodes(start);

			__auto_type colors=getColorList(vertexColors);

			//Choose registers
			mapRegSlice regsByLivenessNode=mapRegSliceCreate(); //TODO rename
			__auto_type allColorNodes=graphNodeIRLiveAllNodes(interfere);
			for(long i=0;i!=strGraphNodeIRLivePSize(allColorNodes);i++) {
					//Get adjacent items
					__auto_type outgoing=graphNodeIRLiveOutgoingNodes(allColorNodes[i]);
					strRegSlice adj=NULL;
					for(long i=0;i!=strGraphNodeIRLivePSize(outgoing);i++) {
							//search for adjacent
							char *key=ptr2Str(allColorNodes[i]);
							__auto_type find=mapRegSliceGet(regsByLivenessNode, key);
							free(key);

							//Insert if exists
							if(find)
									adj=strRegSliceAppendItem(adj, *find);
					}

					//Assign register to 
					__auto_type slice=color2Reg(adj,intRegs, allColorNodes[i], llVertexColorGet(vertexColors, allColorNodes[i])->color, NULL,strIntSize(colors), colors);

					//Insert find
					char *key=ptr2Str(allColorNodes[i]);
					mapRegSliceInsert(regsByLivenessNode, key, slice);
					free(key);

					strRegSliceDestroy(&adj),strGraphNodeIRLivePDestroy(&allColorNodes);
			}

			//Get conflicts
			__auto_type conflicts=recolorAdjacentNodes(regsByLivenessNode,allColorNodes[0]);

			//Sort conflicts by minimum spill metric
			__auto_type conflictsSortedByWeight=strConflictPairClone(conflicts);
			qsort(conflictsSortedByWeight, strConflictPairSize(conflicts), sizeof(*conflicts),  conflictPairWeightCmp);

			//
			// Choose spill Nodes:
			//
			// To do this,we first find a list of unspilled nodes that affect unspilled node(i),we then spill the one with the lowest metric.
			// Then we mark the spilled node.We repeat untill there all conflicts are resolved(all unspilled nodes are not adjacent to another unspilled node).
			//
			for(;0!=strConflictPairSize(conflicts);) {
					//Get a list of items that conflict with conflicts[0]
					__auto_type conflictsWithFirst=conflictPairFindAffects(conflicts[0].a, conflicts);

					//
					//Do a set union of all items that conflict with each other untill there is no change
					//
					for(;;) {
							strConflictPair clone __attribute__((cleanup(strConflictPairDestroy)))=strConflictPairClone(conflictsWithFirst);
							
							long oldSize=strConflictPairSize(conflictsWithFirst);
							//union
							for(long i=0;i!=strConflictPairSize(clone);i++) {
									conflictsWithFirst=strConflictPairSetUnion(conflictPairFindAffects(clone[i].a, conflicts), conflictsWithFirst, conflictPairCmp);
									conflictsWithFirst=strConflictPairSetUnion(conflictPairFindAffects(clone[i].b, conflicts), conflictsWithFirst, conflictPairCmp);
							}

							//Break if no change
							long newSize=strConflictPairSize(conflictsWithFirst);
							if(oldSize!=newSize)
									break;
					}

					//
					// Find the node with the lowest cost according conflictsSortedByWeight
					//
					long lowestConflictI=-1;
					for(long i=0;i!=strConflictPairSize(conflictsWithFirst);i++) {
							for(long i2=0;i2!=strConflictPairSize(conflictsSortedByWeight);i2++)
									if(conflictsWithFirst[i].a==conflictsSortedByWeight[i2].a)
											if(conflictsWithFirst[i].b==conflictsSortedByWeight[i2].b)
													lowestConflictI=i;
					}

					//Remove all references to spilled node in conflicts and conflictsSortedByWeight
					conflicts=strConflictPairRemoveIf(conflicts, &conflicts[lowestConflictI].a,(int(*)(const void*,const struct conflictPair*))conflictPairContains);
					conflictsSortedByWeight=strConflictPairRemoveIf(conflictsSortedByWeight, &conflicts[lowestConflictI].a,(int(*)(const void*,const struct conflictPair*))conflictPairContains);
			}

			strConflictPairDestroy(&conflicts);
	}
}
