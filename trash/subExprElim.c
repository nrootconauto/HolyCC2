#include <IR.h>
#include <assert.h>
#include <base64.h>
#include <stdarg.h>
#include <stdio.h>
#include <str.h>
#include <subExprElim.h>
#include <topoSort.h>
#include <cleanup.h>
#include <lambda.h>
STR_TYPE_DEF(char, Char);
STR_TYPE_FUNCS(char, Char);
STR_TYPE_DEF(char *, Str);
STR_TYPE_FUNCS(char *, Str);
STR_TYPE_DEF(struct IRVar, IRVar);
STR_TYPE_FUNCS(struct IRVar, IRVar);
GRAPH_TYPE_DEF(const char *, void *, Dummy);
GRAPH_TYPE_FUNCS(const char *, void *, Dummy);
MAP_TYPE_DEF(graphNodeDummy, GNDummy);
MAP_TYPE_FUNCS(graphNodeDummy, GNDummy);
struct subExpr;
typedef int (*gnIRCmpType)(const graphNodeIR *, const graphNodeIR *);
typedef int (*gnDummyCmpType)(const graphNodeDummy *, const graphNodeDummy *);
typedef int (*subExprCmpType)(const struct subExpr *, const struct subExpr *);

static strChar ptr2Str(const void *a) {
	__auto_type txt = base64Enc((void *)&a, sizeof(a));
	__auto_type retVal = strCharAppendData(NULL, txt, strlen(txt) + 1);
	
	return retVal;
}
static strChar strClone(const char *str) {
	char *retVal = malloc(strlen(str) + 1);
	strcpy(retVal, str);

	return retVal;
}
static const char *IR_ATTR_SUB_EXPR_HASH = "subExprHash";
static const char *IR_ATTR_TOPO_SORT = "topolgoicalSort";
struct IRAttrHash {
	struct IRAttr base;
	const char *mapKeyPtr;
};
static graphEdgeIR getNodeByLab(strGraphEdgeP edges, enum IRConnType type) {
	for (long i = 0; strGraphEdgePSize(edges); i++)
		if (type == *graphEdgeIRValuePtr(edges[i]))
			return edges[i];

	return NULL;
}
#define STR_FROM_FORMAT(fmt, ...)                                              \
	({                                                                           \
		long count = sprintf(NULL, fmt, __VA_ARGS__);                              \
		char buffer[count + 1];                                                    \
		sprintf(buffer, fmt, __VA_ARGS__);                                         \
		strCharAppendData(NULL, buffer, strlen(buffer) + 1);                       \
	})
static strChar intLit2Str(const struct lexerInt *i) {
	strChar retVal = strCharReserve(NULL, 8);
	retVal = strCharAppendData(retVal, "INT:", strlen("INT:"));

	struct lexerInt clone = *i;
	// Dump hex
	while (clone.value.uLong != 0) {
		int i = clone.value.uLong & 0x0fl;
		clone.value.uLong >>= 4;

		const char *digits = "0123456789ABCDEF";
		;
		retVal = strCharAppendItem(retVal, digits[i]);
	}

	retVal = strCharAppendItem(retVal, '\0');
	return retVal;
}
enum subExprType {
	SUB_EXPR_BINOP,
	SUB_EXPR_UNOP,
};
struct subExpr {
	enum subExprType type;
	graphNodeIR node;
	union {
		struct {
			graphNodeIR a;
			graphNodeIR b;
		} binop;
		graphNodeIR unop;
	} subItems;
};
STR_TYPE_DEF(struct subExpr, SubExpr);
STR_TYPE_FUNCS(struct subExpr, SubExpr);
MAP_TYPE_DEF(strSubExpr, SubExprs);
MAP_TYPE_FUNCS(strSubExpr, SubExprs);
static  mapSubExprs subExprRegistry  = NULL;

static int ptrPtrCmp(const void *a, const void *b) {
	if (*(void **)a > *(void **)b)
		return 1;
	else if (*(void **)a < *(void **)b)
		return -1;
	else
		return 0;
}
static int subExprCmp(const void *a, const void *b) {
	const struct subExpr *A = a, *B = b;
	return ptrPtrCmp(&A->node, &B->node);
}
static const char *registerItemHash(graphNodeIR node, const char *text,
                                    struct subExpr *expr) {
	__auto_type attrsPtr = &graphNodeIRValuePtr(node)->attrs;

	// Check if item already has hash attr(hence was registered already,we dont
	// want to rehash elems)
	int find = 0;
	__auto_type find2 = mapSubExprsGet(subExprRegistry, text);
	if (find2) {
		for (long i = 0; i != strSubExprSize(*find2); i++) {
			if (find2[0][i].node == node) {
				find = 1;
				break;
			} else if (find2[0][i].node > node)
				break;
		}
	}

	if (!find) {

		// Register find
		if (expr) {
			if (find2) {
				*find2 =
				    strSubExprSortedInsert(*find2, *expr, (subExprCmpType)subExprCmp);
			} else {
				mapSubExprsInsert(subExprRegistry, text,
				                  strSubExprAppendItem(NULL, *expr));
			}
		} else {
			// Insert a null if no expression provided,this can be used to insert
			// dummy expressions
			if (!find2)
				mapSubExprsInsert(subExprRegistry, text, NULL);
		}

		// Assign hash to node
		struct IRAttrHash attr;
		attr.base.name = (void *)IR_ATTR_SUB_EXPR_HASH;
		// Use map hash key pointer to save memory by not copying sting
		attr.mapKeyPtr = mapSubExprsValueKey(mapSubExprsGet(subExprRegistry, text));

		__auto_type newNode = __llCreate(&attr, sizeof(attr));
		llIRAttrInsert(*attrsPtr, newNode, IRAttrInsertPred);
		*attrsPtr = newNode;
	}

	return mapSubExprsValueKey(mapSubExprsGet(subExprRegistry, text));
}

static const char *hashNode(graphNodeIR node) {
	__auto_type val = graphNodeIRValuePtr(node);

	const char *op = NULL;
	switch (val->type) {
	case IR_ADD:
		op = "+";
		goto binopHash;
	case IR_SUB:
		op = "-";
		goto binopHash;
	case IR_POS:
		op = "+";
		goto unopHash;
	case IR_NEG:
		op = "-";
		goto unopHash;
	case IR_MULT:
		op = "*";
		goto binopHash;
	case IR_DIV:
		op = "/";
		goto binopHash;
	case IR_MOD:
		op = "%";
		goto binopHash;
	case IR_POW:
		op = "`";
		goto binopHash;
	case IR_LAND:
		op = "&&";
		goto binopHash;
	case IR_LXOR:
		op = "^^";
		goto binopHash;
	case IR_LOR:
		op = "||";
		goto binopHash;
	case IR_LNOT:
		op = "!";
		goto unopHash;
	case IR_BAND:
		op = "&";
		goto binopHash;
	case IR_BXOR:
		op = "^";
		goto binopHash;
	case IR_BOR:
		op = "|";
		goto binopHash;
	case IR_BNOT:
		op = "~";
		goto unopHash;
	case IR_LSHIFT:
		op = "<<";
		goto binopHash;
	case IR_RSHIFT:
		op = ">>";
		goto binopHash;
	case IR_ARRAY_ACCESS:
		op = "[]";
		goto binopHash;
	case IR_SIMD:
	case IR_GT:
		op = ">";
		goto binopHash;
	case IR_LT:
		op = "<";
		goto binopHash;
	case IR_GE:
		op = ">=";
		goto binopHash;
	case IR_LE:
		op = "<=";
		goto binopHash;
	case IR_NE:
		op = "!=";
		goto binopHash;
	case IR_EQ:
		op = "==";
		goto binopHash;
	case IR_VALUE: {
		struct IRNodeValue *value = (void *)val;
		switch (value->val.type) {
		case IR_VAL_FUNC: {
			__auto_type hash = ptr2Str(value->val.value.func);
			__auto_type retVal = registerItemHash(node, hash, NULL);
			return retVal;
		}
		case IR_VAL_REG: {
			// Im not implemented
			assert(0);
		}
		case IR_VAL_VAR_REF: {
			strChar ptrStr = NULL;

			if (value->val.value.var.type == IR_VAR_VAR) {
				__auto_type hash = ptr2Str(value->val.value.var.value.var);
				__auto_type retVal = registerItemHash(node, hash, NULL);
				return retVal;
			} else if (value->val.value.var.type == IR_VAR_MEMBER) {
				__auto_type hash = ptr2Str(value->val.value.var.value.member);
				__auto_type retVal = registerItemHash(node, hash, NULL);
				return retVal;
			}

			assert(ptrStr != NULL);
		}
		case IR_VAL_INT_LIT: {
			__auto_type hash = intLit2Str(&value->val.value.intLit);
			__auto_type retVal = registerItemHash(node, hash, NULL);
			return retVal;
		}
		case IR_VAL_STR_LIT: {
			__auto_type hash = STR_FROM_FORMAT("STR[%s]", value->val.value.strLit);
			__auto_type retVal = registerItemHash(node, hash, NULL);
			return retVal;
		}
		case __IR_VAL_LABEL: {
			__auto_type hash = ptr2Str(value->val.value.__label);
			__auto_type retVal = registerItemHash(node, hash, NULL);
			return retVal;
		}
		case __IR_VAL_MEM_GLOBAL: {
			__auto_type typePtr = ptr2Str(value->val.value.__frame.type);
			__auto_type hash = STR_FROM_FORMAT(
			    "GM[%li:%s]", value->val.value.__frame.offset, typePtr);
			__auto_type retVal = registerItemHash(node, hash, NULL);
			return retVal;
		}
		case __IR_VAL_MEM_FRAME: {
			__auto_type hash = ptr2Str(value->val.value.__global.symbol);
			__auto_type retVal = registerItemHash(node, hash, NULL);
			return retVal;
		}
		}
	}
	default:
		return NULL;
	}
unopHash : {
	__auto_type incoming = graphNodeIRIncoming(node);
	assert(strGraphEdgeIRPSize(incoming) == 1);
	__auto_type a = getNodeByLab(incoming, IR_CONN_SOURCE_A);
	assert(a);

	__auto_type aHash = hashNode(graphEdgeIRIncoming(a));

	strChar retVal = NULL;
	if (aHash) {
		long len = sprintf(NULL, "%s [%s]", op, aHash);
		char buffer[len];
		sprintf(buffer, "%s [%s]", op, aHash);

		retVal = strCharAppendItem(NULL, strlen(buffer) + 1);
	}

	
	const char *retVal2 = NULL;
	if (retVal) {
		// Create sub-expression
		struct subExpr sub;
		sub.node = node;
		sub.type = SUB_EXPR_UNOP;
		sub.subItems.unop = graphEdgeIRIncoming(a);

		// Assign hash to node
		retVal2 = registerItemHash(node, retVal, &sub);
	}

	return retVal2;
}
binopHash : {
	__auto_type incoming = graphNodeIRIncoming(node);
	assert(strGraphEdgeIRPSize(incoming) == 2);
	__auto_type a = getNodeByLab(incoming, IR_CONN_SOURCE_A);
	__auto_type b = getNodeByLab(incoming, IR_CONN_SOURCE_B);
	assert(a && b);

	__auto_type aHash = hashNode(graphEdgeIRIncoming(a));
	__auto_type bHash = hashNode(graphEdgeIRIncoming(b));

	strChar retVal = NULL;
	if (aHash && bHash) {
		long len = snprintf(NULL, 0, "%s [%s][%s]", op, aHash, bHash);
		char buffer[len+1];
		sprintf(buffer, "%s [%s][%s]", op, aHash, bHash);

		retVal = strCharAppendData(NULL, buffer, strlen(buffer) + 1);
	}
	
	const char *retVal2 = NULL;
	if (retVal) {
		// Create sub-expression
		struct subExpr sub;
		sub.node = node;
		sub.type = SUB_EXPR_BINOP;
		sub.subItems.binop.a = graphEdgeIRIncoming(a);
		sub.subItems.binop.b = graphEdgeIRIncoming(b);

		retVal2 = registerItemHash(node, retVal, &sub);
	}

	return retVal2;
}
}

STR_TYPE_DEF(strGraphNodeDummyP, DummyBlob);
STR_TYPE_FUNCS(strGraphNodeDummyP, DummyBlob);
static int visitUntillStartStmt(const struct __graphNode *node,
                                const struct __graphEdge *edge,
                                const void *data) {
		__auto_type outgoing=graphNodeIROutgoing((graphNodeIR)node);
		for(long i=0;i!=strGraphEdgeIRPSize(outgoing);i++) {
				if(!IRIsExprEdge(*graphEdgeIRValuePtr(outgoing[i])))
						return 0;
		}

		return 1;
}
static void visitNodeAppendItem(struct __graphNode *node, void *data) {
	strGraphNodeIRP *nodes = data;
	if (NULL == strGraphNodeIRPSortedFind(*nodes, data, (gnIRCmpType)ptrPtrCmp))
		*nodes = strGraphNodeIRPSortedInsert(*nodes, node, (gnIRCmpType)ptrPtrCmp);
}
static void moveConnectionsOutgoing(graphNodeIR from, graphNodeIR to) {
		__auto_type outgoing=graphNodeIROutgoing(from);
		
	for (long e = 0; e != strGraphEdgeIRPSize(outgoing); e++) {
		__auto_type n = graphEdgeIROutgoing(outgoing[e]);
		graphNodeIRConnect(to, n, *graphEdgeIRValuePtr(outgoing[e]));

		// Remove the outgoing connection from the "old" node so we can kill
		// the redundant expr graph later
		graphEdgeIRKill(from, n, NULL, NULL, NULL);
		graphEdgeIROutgoing(outgoing[e]);
	}
}
static void graphNodeIRDestroy2(graphNodeIR *node) {
		graphNodeIRKillGraph(node, (void(*)(void*))IRNodeDestroy, NULL);
}
static struct variable *insertSubExpressions(strSubExpr subExprs,strIRVar vars,graphNodeIR clonedExpression);
void replaceSubExprsWithVars() {
	long count;
	mapSubExprsKeys(subExprRegistry, NULL, &count);
	const char *keys[count];
	mapSubExprsKeys(subExprRegistry, keys, NULL);

	strStr repeatedKeys = strStrReserve(NULL, count / 2);

	// Filter out keys that only appear once
	for (long i = 0; i != count; i++) {
		__auto_type find = *mapSubExprsGet(subExprRegistry, keys[i]);
		if (strSubExprSize(find) == 1)
			continue;

		repeatedKeys = strStrAppendItem(repeatedKeys, (char *)keys[i]);
	}

	mapGNDummy hashToNode = mapGNDummyCreate();
	//
	// Nodes may occor in sepratre "blobs"(interconnected graphs)
	// We need to find all the blobs and topologically sort all of them
	//

	// Find all nodes
	strGraphNodeP nodes = strGraphNodePResize(NULL, strStrSize(repeatedKeys));
	for (long i = 0; i != strStrSize(repeatedKeys); i++) {
		nodes[i] = *mapGNDummyGet(hashToNode, repeatedKeys[i]);
	}
	qsort(nodes, strStrSize(repeatedKeys), sizeof(*nodes), ptrPtrCmp);

	//
	// Now we topologically sort the blob to find the "tops" and "bottom"
	// This way we can have the first "item" come first 
	//
	for (long i = 0; i != strGraphNodeIRPSize(nodes); i++) {
			__auto_type hashAttr = *graphNodeDummyValuePtr(nodes[i]);
			assert(hashAttr);

			__auto_type refs = *mapSubExprsGet(subExprRegistry, hashAttr);

			//Clone the expression
			graphNodeIR clonedExpression CLEANUP(graphNodeIRDestroy2)=cloneNode(nodes[i], IR_CLONE_EXPR, NULL);;

			//Insert copieis of the sub  expression at key points
			__auto_type var=insertSubExpressions(refs, NULL, clonedExpression);
			for (long i3 = 0; i3 != strSubExprSize(refs); i3++) {
				__auto_type outgoing = graphNodeIROutgoing(refs[i3].node);

				//Make a clone of firstRef's node(which points to a variable),copy all outgoing traffic from refs[i3].node to the clone
				__auto_type stmtStart=IRGetStmtStart(refs[i3].node);
				__auto_type varRef=createVarRef(var);
				moveConnectionsOutgoing(refs[i3].node, varRef);
				graphNodeIRConnect(stmtStart, varRef, IR_CONN_FLOW);
				
				// Disconnect node from start stmt(including current node)
				strGraphNodeIRP untilStart =
				    strGraphNodeIRPAppendItem(NULL, refs[i3].node);
				graphNodeIRVisitBackward(refs[i3].node, &untilStart,
				                         visitUntillStartStmt, visitNodeAppendItem);
				for (long i = 0; i != strGraphNodeIRPSize(untilStart); i++)
						graphNodeIRKill(&untilStart[i], NULL, NULL);
			}
		}
}
void clearSubExprs() {
	if (subExprRegistry != NULL)
		mapSubExprsDestroy(subExprRegistry, NULL);

	subExprRegistry = mapSubExprsCreate();
}
void findSubExprs(const graphNodeIR node) {
		hashNode(node);
}
struct graphNodeDepthPair {
		graphNodeIR node;
		struct subExpr *sourceExpression;
		long depth;
		long refCount;
};
LL_TYPE_DEF(struct graphNodeDepthPair, GNIR);
LL_TYPE_FUNCS(struct graphNodeDepthPair, GNIR);
static int llGNIRFindCmp(const void *node,const struct graphNodeDepthPair *have) {
		return ptrPtrCmp(&node,have->node);
}
static int llGNIRInsertCmp(const struct graphNodeDepthPair *a,const struct graphNodeDepthPair *b) {
		return  ptrPtrCmp(&a->node, &b->node);
}
static int expressionContainsVar(graphNodeIR stmtEnd,strIRVar vars) {
		strGraphNodeIRP nodes CLEANUP(strGraphNodeIRPDestroy)=IRStmtNodes(stmtEnd);
		for(long i=0;i!=strGraphNodeIRPSize(nodes);i++) {
				struct IRNodeValue *val=(void*)graphNodeIRValuePtr(nodes[i]);
				if(val->base.type==IR_VALUE) {
						if(val->val.type==IR_VAL_VAR_REF) {
								if(NULL!=strIRVarSortedFind(vars, val->val.value.var, IRVarCmp)) {
										return 1;
								}
						}
				}
		}

		return 0;
}
static void reverseDepthSearch(struct subExpr *sourceExpression,graphNodeIR startFrom,strIRVar vars,llGNIR *visited,strGraphNodeIRP *roots,int depth) {
		//Dont visit already visited
		if(NULL!=llGNIRFind(*visited, &startFrom, llGNIRFindCmp))
				return;
		
		//Add to visited
		struct graphNodeDepthPair pair;
		pair.node=startFrom;
		pair.depth=depth;
		pair.refCount=1;
		pair.sourceExpression=sourceExpression;
		__auto_type inserted=llGNIRCreate(pair);
		llGNIRInsert(*visited, inserted, llGNIRInsertCmp);
		*visited=inserted;
		
		//Check if current node expression has a variable in it that is expected
		__auto_type end=IRGetEndOfExpr(startFrom);
		if(end) {
				if(expressionContainsVar(end,  vars)) {
						*roots=strGraphNodeIRPSortedInsert(*roots, end, (gnIRCmpType)ptrPtrCmp);
						return;
				}
		}

		strGraphEdgeIRP incoming CLEANUP(strGraphEdgeIRPDestroy)=graphNodeIRIncoming(startFrom);
		for(long i=0;i!=strGraphEdgeIRPSize(incoming);i++) {
				__auto_type inNode=graphEdgeIRIncoming(incoming[i]);
				__auto_type stmtStart=IRGetStmtStart(inNode);
				inNode=stmtStart?stmtStart:inNode;
				reverseDepthSearch(sourceExpression,inNode,vars,visited,roots,depth+1);
		}
}
STR_TYPE_DEF(llGNIR,LLGNIR);
STR_TYPE_FUNCS(llGNIR,LLGNIR);
STR_TYPE_DEF(struct graphNodeDepthPair,Pair);
STR_TYPE_FUNCS(struct graphNodeDepthPair,Pair);
static int pairCmpType(const struct graphNodeDepthPair *a,const struct graphNodeDepthPair *b) {
		return ptrPtrCmp(&a->node, &b->node);
}
static strPair removeDominatedNodes(strPair items) {
	loop:;
		for(long i=0;i!=strPairSize(items);i++) {
				strGraphNodeIRP in CLEANUP(strGraphNodeIRPDestroy)= graphNodeIRIncomingNodes(items[i].node);
				//Check if incoming nodes contain an node from items,if so it is dominated so remove it
				for(long i2=0;i2!=strGraphNodeIRPSize(in);i2++) {
						struct graphNodeDepthPair dummy;
						dummy.depth=0;
						dummy.refCount=0;
						dummy.node=in[i2];
						__auto_type find=strPairSortedFind(items, dummy, pairCmpType);
						if(find) {
								//Remove find from items
								memmove(find, find+1, ((items+strPairSize(items))-find-1)*sizeof(*find));
								//Pop to reduce size by 1
								items=strPairPop(items, NULL);
								
								goto loop;
						}
				}
		}

		return items;
}
static int pairCmpRefCount(const struct graphNodeDepthPair *a,const struct graphNodeDepthPair *b) {
		if( a->refCount>b->refCount)
				return 1;
		else if( a->refCount<b->refCount)
				return -1;
		else
				return 0;
}
static strPair llGNIRToStr(llGNIR ll) {
		long size=llGNIRSize(ll);
		strPair retVal=strPairResize(NULL,size);

		long i=0;
		for(__auto_type node=llGNIRFirst(ll);node!=NULL;ll=llGNIRNext(node))
				retVal[i++]=*llGNIRValuePtr(ll);

		qsort(retVal, i, sizeof(*retVal),  (int(*)(const void*,const void*))pairCmpType);

		return retVal;
}
static int pairContainsSubExprs(const struct graphNodeDepthPair *pair,const void *subExprs) {
		strSubExpr exprs=(void*)subExprs;
		for(long i=0;i!=strSubExprSize(exprs);i++) {
				if(exprs[i].node==pair->sourceExpression->node)
						return 1;
		}

		return 0;
}
struct insertAtStruct {
		graphNodeIR insertAt;
		strSubExpr subExprs;
};
STR_TYPE_DEF(struct insertAtStruct,  InsertAt);
STR_TYPE_FUNCS(struct insertAtStruct,  InsertAt);
static struct variable *insertSubExpressions(strSubExpr subExprs,strIRVar vars,graphNodeIR clonedExpression) {
		strGraphNodeIRP varTops CLEANUP(strGraphNodeIRPDestroy) =NULL;
		strLLGNIR pathsPerStart=NULL;
		
		for(long i=0;i!=strSubExprSize(subExprs);i++) {
				llGNIR visited=NULL;
				reverseDepthSearch(&subExprs[i],subExprs[i].node, vars, &visited, &varTops,0);

				pathsPerStart=strLLGNIRAppendItem(pathsPerStart, visited);
		}
		strLLGNIR pathsPerStartClone=strLLGNIRClone(pathsPerStart);

		strPair intersections CLEANUP(strPairDestroy)=NULL;
	loop:
		for(long i=0;i<strLLGNIRSize(pathsPerStart)-1;i++) {
				strPair a CLEANUP(strPairDestroy)=llGNIRToStr(pathsPerStart[i]);
				strPair b CLEANUP(strPairDestroy)=llGNIRToStr(pathsPerStart[i+1]);

				strPair intersect CLEANUP(strPairDestroy)=strPairSetIntersection(a, b, pairCmpType, NULL);
				for(long i2=0;i2!=strPairSize(intersect);i2++) {
						//Check if item already exists in intersections
						struct graphNodeDepthPair dummy;
						dummy.depth=0;
						dummy.node=intersect[i].node;
						dummy.refCount=0;
						__auto_type find=strPairSortedFind(intersect, dummy, pairCmpType);
						if(NULL!=find) {
								//Exists so increment reference count 
								find->refCount++;
						} else {
								//Add to intersections
								dummy.refCount=2; //(Two items compared at once)
								intersect=strPairSortedInsert(intersect, dummy, pairCmpType);
						}
				}
		}
		
		//Remove items that are dominated(alive nodes are comming into it)
		intersections=removeDominatedNodes(intersections);

		//Sort interesections by  reference count
		qsort(intersections, strPairSize(intersections), sizeof(*intersections),  (int(*)(const void*,const void*))pairCmpRefCount);

		//Remove top with most references
		strInsertAt insertAt CLEANUP(strInsertAtDestroy)=NULL;
		
		for(;strLLGNIRSize(pathsPerStartClone);) {
				struct graphNodeDepthPair top;
				strPairPop(intersections, &top);

				//
				struct insertAtStruct where;
				where.insertAt=top.node;
				where.subExprs=NULL;
				insertAt=strInsertAtAppendItem(insertAt, where);

				__auto_type subExprsPtr=&insertAt[strInsertAtSize(insertAt)-1].subExprs;
				
		loop2:
				for(long i=0;i!=strLLGNIRSize(pathsPerStartClone);i++) {
						//Find iems from clone that have top.node in their path
						for(__auto_type node=llGNIRFirst(pathsPerStartClone[i]);node!=NULL;node=llGNIRNext(node)) {
								//Sorted by ptr
								int cmp=ptrPtrCmp(&llGNIRValuePtr(node)->node,&top.node);
								if(cmp<0) {
										break;
								} else if(cmp==0) {
										//Add sub-expression to add current insertAt item
										*subExprsPtr=strSubExprAppendItem(*subExprsPtr, *top.sourceExpression);
										
										//Destroy entry
										llGNIRDestroy(&pathsPerStartClone[i],NULL);
										//Remove from clones
										long len=sizeof(*pathsPerStartClone)*(strLLGNIRSize(pathsPerStartClone)-1);
										memmove(&pathsPerStartClone[i],&pathsPerStartClone[i+1],len);
										//Pop to reduce size by 1
										pathsPerStartClone=strLLGNIRPop(pathsPerStartClone, NULL);
										
										goto loop2;
								}
						}
				}
		}

		//
		// Insert sub-expression 
		//
		__auto_type var=createVirtVar(IRNodeType(clonedExpression)); //
		for(long i=0;i!=strInsertAtSize(insertAt);i++) {
				__auto_type stmtEnd=IRGetEndOfExpr(insertAt[i].insertAt);
				if(!stmtEnd)
						stmtEnd=insertAt[i].insertAt;
				
				__auto_type clone=cloneNode(clonedExpression, IR_CLONE_EXPR, NULL);
				//Assign clone to variable
				__auto_type assignTo=createVarRef(var);
				graphNodeIRConnect(IRGetEndOfExpr(clone),assignTo, IR_CONN_DEST);
				
				IRInsertAfter(stmtEnd, IRGetStmtStart(clone), IRGetEndOfExpr(clone), IR_CONN_FLOW);
		}

		return var;
}
