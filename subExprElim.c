#include <IR.h>
#include <str.h>
#include <stdio.h>
#include <assert.h>
#include <stdarg.h>
#include <base64.h>
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
		 strChar hash;
};
llIRAttr createHashAttr(const char *text) {
		struct IRAttrHash attr;
		attr.base.name=IR_ATTR_SUB_EXPR_HASH;
		attr.hash=strClone(text);

		return __llCreate(&attr, sizeof(attr));
}
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
		strChar retVal=strCharResize(NULL, 8);
		retVal=strCharAppendData(retVal, "INT:", strlen("INT:"));
		
		__auto_type clone=*i;
		//Dump hex
		while(clone.value.uLong!=0) {
				int i=clone.value.uLong&0x0fl;
				clone.value.uLong>>=4;

				const char *digits="0123456789ABCDEF";;
				retVal=strCharAppendItem(retVal, digits[i]);
		}

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
void subExprFinderDeinit() {
		if(subExprRegistry!=NULL)
				mapSubExprsDestroy(subExprRegistry, (void(*)(void*))strSubExprDestroy);
				
}
void subExprFinderInit() {
		subExprFinderDeinit();
		subExprRegistry=mapSubExprsCreate();
}
static strChar hashNode(graphNodeIR node) {
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
						return ptr2Str(value->val.value.func);
				}
				case IR_VAL_REG: {
						//Im not implemented
						assert(0);
				}
				case IR_VAL_VAR_REF: {
						strChar ptrStr=NULL;
						
						if(value->val.value.var.var.type==IR_VAR_VAR)
								return ptr2Str(value->val.value.var.var.value.var);
						else if (value->val.value.var.var.type==IR_VAR_MEMBER)
								return ptr2Str(value->val.value.var.var.value.member);

						assert(ptrStr!=NULL);

						__auto_type retVal=STR_FROM_FORMAT("VAR[%s]:%li", ptrStr,value->val.value.var.SSANum); 
						strCharDestroy(&ptrStr);

						return retVal;
				}
				case IR_VAL_INT_LIT:
						return intLit2Str(&value->val.value.intLit);
				case IR_VAL_STR_LIT: {
						return STR_FROM_FORMAT("STR[%s]",value->val.value.strLit); 
				}
				case __IR_VAL_LABEL:
						return ptr2Str(value->val.value.__label);
				case __IR_VAL_MEM_GLOBAL:
						return STR_FROM_FORMAT("GM[%li:%i]", value->val.value.__frame.offset,value->val.value.__frame.width);
				case __IR_VAL_MEM_FRAME:;
						return ptr2Str(value->val.value.__global.symbol);
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
				
				strCharDestroy(&aHash);
				strGraphEdgeIRPDestroy(&incoming);

				if(retVal) {
						//Create sub-expression
						struct subExpr sub;
						sub.node=node;
						sub.type=SUB_EXPR_UNOP;
						sub.subItems.unop=graphEdgeIRIncoming(a);

						//Register expression
						__auto_type find=mapSubExprsGet(subExprRegistry, retVal);
						if(find==NULL){
								mapSubExprsInsert(subExprRegistry, retVal, strSubExprAppendItem(NULL, sub));
						} else {
								*find=strSubExprAppendItem(*find, sub);
						}

						//Assign hash to node
						__auto_type newNode=createHashAttr(retVal); 
						IRAttrInsertPred(newNode, graphNodeIRValuePtr(node)->attrs);
						graphNodeIRValuePtr(node)->attrs=newNode;
				}
						
				return retVal;
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
						long len=sprintf(NULL, "%s [%s][%s]", op,aHash, bHash);
						char buffer[len];
						sprintf(buffer, "%s [%s][%s]", op,aHash, bHash);

						retVal=strCharAppendItem(NULL, strlen(buffer)+1);
				}
				strGraphEdgeIRPDestroy(&incoming);

				if(retVal) {
						//Create sub-expression
						struct subExpr sub;
						sub.node=node;
						sub.type=SUB_EXPR_BINOP;
						sub.subItems.binop.a=graphEdgeIRIncoming(a);
						sub.subItems.binop.b=graphEdgeIRIncoming(b);

						//Register expression
						__auto_type find=mapSubExprsGet(subExprRegistry, retVal);
						if(find==NULL){
								mapSubExprsInsert(subExprRegistry, retVal, strSubExprAppendItem(NULL, sub));
						} else {
								*find=strSubExprAppendItem(*find, sub);
						}
						
						//Assign hash to node
						__auto_type newNode=createHashAttr(retVal); 
						IRAttrInsertPred(newNode, graphNodeIRValuePtr(node)->attrs);
						graphNodeIRValuePtr(node)->attrs=newNode;
				}
				
				strCharDestroy(&aHash);
				strCharDestroy(&bHash);
				return retVal;
		}
}
static int ptrPtrCmp(const void *a,const void *b) {
		if(*(void**)a>*(void**)b)
				return 1;
		else if(*(void**)a<*(void**)b)
				return -1;
		else
				return 0;
}
static void subExprRemoveFromStmt(graphNodeIR stmtStart,graphNodeIR stmtEnd) {
		//
		//Visit all nodes from start->end node of statement
		//
		strGraphNodeIRP heads=graphNodeIROutgoingNodes(stmtStart);
		strGraphNodeIRP allNodes=NULL;
		while(strGraphNodeIRPSize(heads)) {
				strGraphNodeIRP unvisitedHeads=NULL;
				//Add unvisted to visited,
				for(size_t i=0;i!=strGraphNodeIRPSize(heads);i++) {
						if(NULL==strGraphNodeIRPSortedFind(allNodes, heads[i], ptrPtrCmp)) {
								allNodes=strGraphNodeIRPSortedInsert(allNodes, heads[i], ptrPtrCmp);
								unvisitedHeads=strGraphNodeIRPSortedInsert(unvisitedHeads, heads[i], ptrPtrCmp);
						}
				}

				strGraphNodeIRPDestroy(&heads);
				heads=NULL;
				//Add outgoing heads to heads
				for(size_t i=0;i!=strGraphNodeIRPSize(unvisitedHeads);i++) {
						__auto_type newHeads=graphNodeIROutgoingNodes(unvisitedHeads[i]);
						for(size_t i=0;i!=strGraphNodeIRPSize(newHeads);i++) {
								//Dont add end node,we want to stop at end node
								if(graphNodeIRValuePtr(newHeads[i])->type==IR_STATEMENT_END)
										continue;
								
								//Dont re-insert same head
								if(NULL==strGraphNodeIRPSortedFind(heads, newHeads[i], ptrPtrCmp))
											heads=strGraphNodeIRPSortedInsert(heads, newHeads[i], ptrPtrCmp);
						}

						strGraphNodeIRPDestroy(&newHeads);
				}
		}
}
