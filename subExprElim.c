#include <IR.h>
#include <str.h>
#include <stdio.h>
#include <assert.h>
#include <stdarg.h>
#include <base64.h>
#include <topoSort.h>
#include <subExprElim.h>
STR_TYPE_DEF(char ,Char);
STR_TYPE_FUNCS(char ,Char);
static strChar ptr2Str(const void* a) {
		__auto_type txt=base64Enc((void*)&a, sizeof(a));
		__auto_type retVal=strCharAppendData(NULL, txt,strlen(txt)+1);
		free(txt);

		return retVal;
}
static strChar strClone(const char *str) {
		char *retVal=malloc(strlen(str)+1);
		strcpy(retVal, str);

		return retVal;
}
static const char *IR_ATTR_SUB_EXPR_HASH="subExprHash";
static const char *IR_ATTR_TOPO_SORT="topolgoicalSort";
struct IRAttrHash {
		struct IRAttr base;
		const char *mapKeyPtr;
};
static graphEdgeIR getNodeByLab(strGraphEdgeP edges,enum IRConnType type) {
		for(long i=0;strGraphEdgePSize(edges);i++)
				if(type==*graphEdgeIRValuePtr(edges[i]))
						return edges[i];

		return NULL;
}
#define STR_FROM_FORMAT(fmt,...) ({ \
						long count=sprintf(NULL,fmt,__VA_ARGS__);								\
						char buffer[count+1]; \
						sprintf(buffer,fmt,__VA_ARGS__);																						\
						strCharAppendData(NULL,buffer,strlen(buffer)+1);						\
})
static strChar intLit2Str(const struct lexerInt *i) {
		strChar retVal=strCharReserve(NULL, 8);
		retVal=strCharAppendData(retVal, "INT:", strlen("INT:"));
		
		__auto_type clone=*i;
		//Dump hex
		while(clone.value.uLong!=0) {
				int i=clone.value.uLong&0x0fl;
				clone.value.uLong>>=4;

				const char *digits="0123456789ABCDEF";;
				retVal=strCharAppendItem(retVal, digits[i]);
		}

		retVal=strCharAppendItem(retVal, '\0');
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
static __thread mapSubExprs subExprRegistry=NULL;

static int ptrPtrCmp(const void *a,const void *b) {
		if(*(void**)a>*(void**)b)
				return 1;
		else if(*(void**)a<*(void**)b)
				return -1;
		else
				return 0;
}
static int subExprCmp(const void * a,const void * b) {
		const struct subExpr *A=a,*B=b;
		return ptrPtrCmp(&A->node, &B->node);
}
static const char *registerItemHash(graphNodeIR node,const char *text,struct subExpr *expr) {
		__auto_type attrsPtr=&graphNodeIRValuePtr(node)->attrs;

		//Check if item already has hash attr(hence was registered already,we dont want to rehash elems)
		int find=0;
		__auto_type find2=mapSubExprsGet(subExprRegistry, text);
		if(find2) {
				for(long i=0;i!=strSubExprSize(*find2);i++) {
						if(find2[0][i].node==node){
								find=1;
								break;
						} else if(find2[0][i].node>node)
								break;
				}
		}
		
		if(!find) {
				

				//Register find
				if(expr) { 
						if(find2) {
								*find2=strSubExprSortedInsert(*find2, *expr, subExprCmp);
						}else {
								mapSubExprsInsert(subExprRegistry, text,  strSubExprAppendItem(NULL, *expr));
						}
				} else {
						//Insert a null if no expression provided,this can be used to insert dummy expressions
						if(!find2)
								mapSubExprsInsert(subExprRegistry, text,NULL);
				}

				//Assign hash to node
				struct IRAttrHash attr;
				attr.base.name=(void*)IR_ATTR_SUB_EXPR_HASH;
				//Use map hash key pointer to save memory by not copying sting
				attr.mapKeyPtr=mapSubExprsValueKey(mapSubExprsGet(subExprRegistry, text));

				__auto_type newNode= __llCreate(&attr, sizeof(attr));
				llIRAttrInsert(*attrsPtr,newNode,IRAttrInsertPred);
				*attrsPtr=newNode;
		}

		return mapSubExprsValueKey(mapSubExprsGet(subExprRegistry, text));
}

static const char *hashNode(graphNodeIR node) {
		__auto_type val=graphNodeIRValuePtr(node);

		const char *op=NULL;
		switch(val->type) {
		case IR_ADD:
				op="+";goto binopHash;
		case IR_SUB:
				op="-";goto binopHash;
		case IR_POS:
				op="+";goto unopHash;
		case IR_NEG:
				op="-";goto unopHash;
		case IR_MULT:
				op="*";goto binopHash;
		case IR_DIV:
				op="/";goto binopHash;
		case IR_MOD:
				op="%";goto binopHash;
		case IR_POW:
				op="`";goto binopHash;
		case IR_LAND:
				op="&&";goto binopHash;
		case IR_LXOR:
				op="^^";goto binopHash;
		case IR_LOR:
				op="||";goto binopHash;
		case IR_LNOT:
				op="!";goto unopHash;
		case IR_BAND:
				op="&";goto binopHash;
		case IR_BXOR:
				op="^";goto binopHash;
		case IR_BOR:
				op="|";goto binopHash;
		case IR_BNOT:
				op="~";goto unopHash;
		case IR_LSHIFT:
				op="<<";goto binopHash;
		case IR_RSHIFT:
				op=">>";goto binopHash;
		case IR_ARRAY_ACCESS:
				op="[]";goto binopHash;
		case IR_SIMD:
		case IR_GT:
				op=">";goto binopHash;
		case IR_LT:
				op="<";goto binopHash;
		case IR_GE:
				op=">=";goto binopHash;
		case IR_LE:
				op="<=";goto binopHash;
		case IR_NE:
				op="!=";goto binopHash;
		case IR_EQ:
				op="==";goto binopHash;
		case IR_VALUE: {
				struct IRNodeValue *value=(void*)val;
				switch(value->val.type) {
				case IR_VAL_FUNC: {
						__auto_type hash=ptr2Str(value->val.value.func);
						__auto_type retVal=registerItemHash(node, hash, NULL);
						strCharDestroy(&hash);
						return retVal;
				}
				case IR_VAL_REG: {
						//Im not implemented
						assert(0);
				}
				case IR_VAL_VAR_REF: {
						strChar ptrStr=NULL;
						
						if(value->val.value.var.var.type==IR_VAR_VAR) {
								__auto_type hash=ptr2Str(value->val.value.var.var.value.var);
								__auto_type retVal=registerItemHash(node, hash, NULL);
								strCharDestroy(&hash);
								return retVal;
						}
						else if (value->val.value.var.var.type==IR_VAR_MEMBER) {
								__auto_type hash=ptr2Str(value->val.value.var.var.value.member);
								__auto_type retVal=registerItemHash(node, hash, NULL);
								strCharDestroy(&hash);
								return retVal;
						}

						assert(ptrStr!=NULL);
				}
				case IR_VAL_INT_LIT: {
						__auto_type hash=intLit2Str(&value->val.value.intLit);
						__auto_type retVal=registerItemHash(node, hash, NULL);
						strCharDestroy(&hash);
						return retVal;
				}
				case IR_VAL_STR_LIT: {
						__auto_type hash=STR_FROM_FORMAT("STR[%s]",value->val.value.strLit);
						__auto_type retVal=registerItemHash(node, hash, NULL);
						strCharDestroy(&hash);
						return retVal;
				}
				case __IR_VAL_LABEL:{
						__auto_type hash=ptr2Str(value->val.value.__label);
						__auto_type retVal=registerItemHash(node, hash, NULL);
						strCharDestroy(&hash);
						return retVal;
				}
				case __IR_VAL_MEM_GLOBAL: {
						__auto_type typePtr=ptr2Str(value->val.value.__frame.type);
						__auto_type hash=STR_FROM_FORMAT("GM[%li:%s]", value->val.value.__frame.offset,typePtr);
						__auto_type retVal=registerItemHash(node, hash, NULL);
						strCharDestroy(&hash);
						free(typePtr);
						return retVal; 
				}
				case __IR_VAL_MEM_FRAME: {
						__auto_type hash=ptr2Str(value->val.value.__global.symbol);
						__auto_type retVal=registerItemHash(node, hash, NULL);
						strCharDestroy(&hash);
						return retVal;
				}
				}
		}
		default:
				return NULL;
		}
	unopHash: {
				__auto_type incoming=graphNodeIRIncoming(node);
				assert(strGraphEdgeIRPSize(incoming)==1);
				__auto_type a= getNodeByLab(incoming,IR_CONN_SOURCE_A);
				assert(a);

				__auto_type aHash=hashNode(graphEdgeIRIncoming(a));

				strChar retVal=NULL;
				if(aHash) {
						long len=sprintf(NULL, "%s [%s]", op,aHash);
						char buffer[len];
						sprintf(buffer, "%s [%s]", op,aHash);

						retVal=strCharAppendItem(NULL, strlen(buffer)+1);
				}
				
				strGraphEdgeIRPDestroy(&incoming);

				const char *retVal2=NULL; 
				if(retVal) {
						//Create sub-expression
						struct subExpr sub;
						sub.node=node;
						sub.type=SUB_EXPR_UNOP;
						sub.subItems.unop=graphEdgeIRIncoming(a);

						//Assign hash to node
						retVal2=registerItemHash(node, retVal,&sub);
				}
						
				return retVal2;
  }
  binopHash: {
				__auto_type incoming=graphNodeIRIncoming(node);
				assert(strGraphEdgeIRPSize(incoming)==2);
				__auto_type a= getNodeByLab(incoming,IR_CONN_SOURCE_A);
				__auto_type b= getNodeByLab(incoming,IR_CONN_SOURCE_B);
				assert(a&&b);

				__auto_type aHash=hashNode(graphEdgeIRIncoming(a));
				__auto_type bHash=hashNode(graphEdgeIRIncoming(b));
				
				strChar retVal=NULL;
				if(aHash &&bHash) {
						long len=snprintf(NULL,0, "%s [%s][%s]", op,aHash, bHash);
						char buffer[len];
						sprintf(buffer, "%s [%s][%s]", op,aHash, bHash);

						retVal=strCharAppendData(NULL, buffer,strlen(buffer)+1);
				}
				strGraphEdgeIRPDestroy(&incoming);

				const char *retVal2=NULL;
				if(retVal) {
						//Create sub-expression
						struct subExpr sub;
						sub.node=node;
						sub.type=SUB_EXPR_BINOP;
						sub.subItems.binop.a=graphEdgeIRIncoming(a);
						sub.subItems.binop.b=graphEdgeIRIncoming(b);

						retVal2=registerItemHash(node, retVal,&sub);
				}
				
				return retVal2;
		}
}
STR_TYPE_DEF(char *,Str);
STR_TYPE_FUNCS(char *,Str);
GRAPH_TYPE_DEF(const char *, void*, Dummy);
GRAPH_TYPE_FUNCS(const char *, void*, Dummy);
MAP_TYPE_DEF(graphNodeDummy,GNDummy);
MAP_TYPE_FUNCS(graphNodeDummy,GNDummy);

STR_TYPE_DEF(strGraphNodeDummyP, DummyBlob);
STR_TYPE_FUNCS(strGraphNodeDummyP, DummyBlob);
static int visitUntillStartStmt(const struct __graphNode *node,const struct __graphEdge *edge,const void * data) {
		struct IRNode *item= graphNodeIRValuePtr((void*)node);
		return item->type!=IR_STATEMENT_START;
}
static void visitNodeAppendItem(struct __graphNode *node,void * data) {
		strGraphNodeIRP *nodes=data;
		if(NULL==strGraphNodeIRPSortedFind(*nodes, data, ptrPtrCmp))
				*nodes=strGraphNodeIRPSortedInsert(*nodes, node, ptrPtrCmp);
}
void removeSubExprs() {
		long count;
		mapSubExprsKeys(subExprRegistry, NULL, &count);
		const char *keys[count];
		mapSubExprsKeys(subExprRegistry, keys, NULL);

		strStr repeatedKeys=strStrReserve(NULL, count/2);
		
		//Filter out keys that only appear once
		for(long i=0;i!=count;i++) {
				__auto_type find= *mapSubExprsGet(subExprRegistry, keys[i]);
				if(strSubExprSize(find)==1)
						continue;

				repeatedKeys=strStrAppendItem(repeatedKeys, (char*)keys[i]);
		}

		mapGNDummy hashToNode=mapGNDummyCreate();
		//Make dummy nodes for repeated keys
		for(long i=0;i!=strStrSize(repeatedKeys);i++) {
				mapGNDummyInsert(hashToNode, repeatedKeys[i],graphNodeDummyCreate(repeatedKeys[i], 0));
		}
		//Connect nodes
		for(long i=0;i!=strStrSize(repeatedKeys);i++) {
				__auto_type find= *mapSubExprsGet(subExprRegistry, repeatedKeys[i]);
				__auto_type fromNode=mapGNDummyGet(hashToNode, repeatedKeys[i]);
				
				//Create connenctions(if said connection exists as a repeated item)
				for(long i2=0;i2!=strSubExprSize(find);i2++) {
						__auto_type out=graphNodeDummyOutgoingNodes(find[i2].node);
						for(long i3=0;i3!=strGraphNodeIRPSize(out);i3++) {
								//Hash of said node
								__auto_type hashAttr=llIRAttrFind(graphNodeIRValuePtr(out[i3])->attrs , IR_ATTR_SUB_EXPR_HASH ,  IRAttrGetPred);
								if(!hashAttr)
										continue;
								
								const char *hash=((struct IRAttrHash *)__llValuePtr(hashAttr))->mapKeyPtr;

								//Ensure is connected to repeated item
								if(NULL==mapGNDummyGet(hashToNode, hash))
										continue;

								//Connect(if said connection doesnt already exist)
								__auto_type toNode=mapGNDummyGet(hashToNode, hash);								
								if(graphNodeDummyConnectedTo(*fromNode, *toNode))
										continue;
								graphNodeDummyConnect(*fromNode, *toNode, NULL);
						}
						
						strGraphNodeIRPDestroy(&out);
				}
		}

		//
		// Nodes may occor in sepratre "blobs"(graphs)
		// We need to find all the blobs and topologically sort all of them
		//

		//Find all nodes
		strGraphNodeP nodes=strGraphNodePResize(NULL, strStrSize(repeatedKeys));
		for(long i=0;i!=strStrSize(repeatedKeys);i++) {
				nodes[i]=*mapGNDummyGet(hashToNode, repeatedKeys[i]);
		}
		qsort(nodes, strStrSize(repeatedKeys), sizeof(*nodes), ptrPtrCmp);

		//Locate the blobs within the nodes,first locate a blob,remove its nodes from nodes,then repeat until no more nodes
		strDummyBlob blobs=NULL;
		while(strGraphNodePSize(nodes)) {
				__auto_type blob=graphNodeDummyAllNodes(nodes[0]);
				nodes=strGraphNodeDummyPSetDifference(nodes, blob, ptrPtrCmp);
				blobs=strDummyBlobAppendItem(blobs, blob);
		} 

		//
		// Now we topologically sort the blob to find the "tops" and "bottom"
		// So we
		//
		for(long i=0;i!=strDummyBlobSize(blobs);i++) {
				// Topological sort
		
				__auto_type sorted=topoSort(blobs[i]);
				if(!sorted) {
						// I hope you never reach here lol
						goto end;
				}
				//
				// Now that we have a topological sort,we can start from the "tops" and work our way
				// to the bottom while replacing the instances of the represented nodes with references
				// to the first computation to avoid recomputation
				//
				for(long i2=0;i2!=strGraphNodeIRPSize(sorted);i2++) {
						__auto_type hashAttr=*graphNodeDummyValuePtr(sorted[i2]);
						assert(hashAttr);

						__auto_type refs=*mapSubExprsGet(subExprRegistry, hashAttr);

						graphNodeIR firstRef=NULL;
						for(long i3=0;i3!=strSubExprSize(refs);i3++) {
								if(i3==0) {
										//Only compute once,so the first element will be the only computation
										firstRef=refs[i3].node;
										continue;
								}

								__auto_type outgoing=graphNodeIROutgoing(refs[i3].node);
								//"Copy" connections from outgoing to refs[i3].node outgoing neigbors 
								for(long e=0;e!=strGraphEdgeIRPSize(outgoing);e++) {
										__auto_type n=graphEdgeIROutgoing(outgoing[e]);
										graphNodeIRConnect(firstRef, n, *graphEdgeIRValuePtr(outgoing[e]));

										//Remove the outgoing connection from the "old" node so we can kill the redundant expr graph later
										graphEdgeIRKill(refs[i3].node, graphEdgeIROutgoing(outgoing[e]), NULL, NULL, NULL);
								}				
								strGraphEdgeIRPDestroy(&outgoing);
								
								//Disconnect node from start stmt(including current node)
								strGraphNodeIRP untilStart=strGraphNodeIRPAppendItem(NULL, refs[i3].node);
								graphNodeIRVisitBackward(refs[i3].node, &untilStart, visitUntillStartStmt,visitNodeAppendItem);
								for(long i=0;i!=strGraphNodeIRPSize(untilStart);i++)
										graphNodeIRKill(&untilStart[i], IRNodeDestroy, NULL);
								
								strGraphNodeIRPDestroy(&untilStart);
						}
				}
		}
	end:
		strGraphNodePDestroy(&nodes);
		mapGNDummyDestroy(hashToNode, NULL);
		strStrDestroy(&repeatedKeys);
}
static void strSubExprDestroy2(void *item) {
		strSubExprDestroy(item);
}
void clearSubExprs() {
		if(subExprRegistry!=NULL)
				mapSubExprsDestroy(subExprRegistry, strSubExprDestroy2);

		subExprRegistry=mapSubExprsCreate();
}
void findSubExprs(const graphNodeIR node) {
		const struct IRNodeStatementStart *start=(void*)graphNodeIRValuePtr(node);
		__auto_type end=start->end;
		__auto_type nodes=getStatementNodes(node, end);
		for(long i=0;i!=strGraphNodeIRPSize(nodes);i++) {
				hashNode(nodes[i]);
		}
} 
