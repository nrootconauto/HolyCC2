#include <IR.h>
#include <assert.h>
#include <cleanup.h>
#include <ptrMap.h>
#include <registers.h>
static void *IR_ATTR_NODE_TYPE = "NODE_TYPE";
struct IRAttrNodeType {
	struct IRAttr base;
	struct object *type;
};
typedef int(*gnCmpType)(const graphNodeIR *,const graphNodeIR *);
static int ptrPtrCmp(const void *a, const void *b) {
	if (*(void **)a > *(void **)b)
		return 1;
	else if (*(void **)a < *(void **)b)
		return -1;
	else
		return 0;
}
static struct object **getType(graphNodeIR node) {
loop:;
	__auto_type find = llIRAttrFind(graphNodeIRValuePtr(node)->attrs, IR_ATTR_NODE_TYPE, IRAttrGetPred);
	if (!find) {
		struct IRAttrNodeType dummy;
		dummy.base.name = IR_ATTR_NODE_TYPE;
		dummy.type = NULL;
		dummy.base.destroy = NULL;
		graphNodeIRValuePtr(node)->attrs = llIRAttrInsert(graphNodeIRValuePtr(node)->attrs, __llCreate(&dummy, sizeof(dummy)), IRAttrInsertPred);
		goto loop;
	}
	return &((struct IRAttrNodeType *)llIRAttrValuePtr(find))->type;
}
static int isUndetirminedType(const struct __graphNode *node, const struct __graphEdge *edge, const void *data) {
	if (!IRIsExprEdge(*graphEdgeIRValuePtr((graphEdgeIR)edge)))
		return 0;
	if (NULL == IRNodeType((graphNodeIR)node))
		return 1;
	return 0;
}
static void assignType(struct __graphNode *node, void *data) {
	*getType((graphNodeIR)node) = data;
}
static void unifiyUpwards(graphNodeIR node, struct object *type) {
	graphNodeIRVisitBackward(node, type, isUndetirminedType, assignType);
}
static int objIndex(const struct object **objs, long count, const struct object *obj) {
	for (long i = 0; i != count; i++)
		if (obj == objs[i])
			return i;

	assert(0);
	return -1;
}

static struct object *getHigherType(struct object *a, struct object *b) {
	if (a->type == TYPE_PTR||a->type == TYPE_ARRAY)
		return a;
	else if (b->type == TYPE_PTR||a->type == TYPE_ARRAY)
		return b;

	a = objectBaseType(a);
	b = objectBaseType(b);
	;

	const struct object *ranks[] = {
	    &typeU0, &typeI8i, &typeU8i, &typeI16i, &typeU16i, &typeI32i, &typeU32i, &typeI64i, &typeU64i, &typeF64,
	};
	long count = sizeof(ranks) / sizeof(*ranks);
	__auto_type aIndex = objIndex(ranks, count, a);
	__auto_type bIndex = objIndex(ranks, count, b);
	return (aIndex > bIndex) ? a : b;
}
static int intSignedExceeds(struct IRValue *value, int64_t low, int64_t high) {
	if (value->value.intLit.type == INT_SLONG) {
		if (value->value.intLit.value.sLong > high)
			return 1;
		else if (value->value.intLit.value.sLong < low)
			return 1;
	} else if (value->value.intLit.type == INT_ULONG)
		if (value->value.intLit.value.uLong > high)
			return 1;
	return 0;
}
static int intUnsignedExceeds(struct IRValue *value, uint64_t high) {
	if (value->value.intLit.type == INT_SLONG) {
		if (value->value.intLit.value.sLong > high)
			return 1;
	} else if (value->value.intLit.type == INT_ULONG)
		if (value->value.intLit.value.uLong > high)
			return 1;
	return 0;
}
struct object *IRNodeType(graphNodeIR node);
struct object *__IRNodeType(graphNodeIR node) {
	if (NULL != *getType(node))
		return *getType(node);

	struct IRNodeValue *nodeVal = (void *)graphNodeIRValuePtr(node);
	if(nodeVal->base.type==IR_FUNC_VAARG_ARGC) {
			return dftValType();
	} if(nodeVal->base.type==IR_FUNC_VAARG_ARGV) {
			return objectPtrCreate(dftValType());
	}  else if(nodeVal->base.type==IR_SPILL_LOAD) {
			struct IRNodeSpill *spill=(void*)nodeVal;
			return spill->item.value.var.var->type;
	} else if(nodeVal->base.type==IR_FUNC_ARG) {
			struct IRNodeFuncArg *arg=(void*)nodeVal;
			return arg->type;
	} else if(nodeVal->base.type == IR_TYPECAST) {
					struct IRNodeTypeCast *cast=(void*)nodeVal;
					return cast->out;
	} else	if(nodeVal->base.type==IR_ARRAY_DECL) {
			struct IRNodeArrayDecl *decl=(void*)nodeVal;
			return decl->itemType;
	} else if (nodeVal->base.type == IR_VALUE) {
			if(nodeVal->val.type==IR_VAL_STR_LIT) {
					return objectPtrCreate(&typeU8i);
			} else if(nodeVal->val.type==IR_VAL_FLT_LIT) {
					return &typeF64;
			}else if (nodeVal->val.type == __IR_VAL_MEM_GLOBAL) {
					return nodeVal->val.value.__global.symbol->type;
			} else if (nodeVal->val.type == IR_VAL_REG) {
					return nodeVal->val.value.reg.type;
			} else if (nodeVal->val.type == IR_VAL_VAR_REF) {
					return nodeVal->val.value.var.var->type;
			} else if (nodeVal->val.type == __IR_VAL_MEM_FRAME) {
					return nodeVal->val.value.__frame.type;
			} else if (nodeVal->val.type == IR_VAL_FUNC) {
					return nodeVal->val.value.func->type;
			}  else if (nodeVal->val.type == IR_VAL_INT_LIT) {
					int dataSize2 = dataSize();
		dataSizeLoop:;
			switch (dataSize2) {
			case 1:
				if (intUnsignedExceeds(&nodeVal->val, UINT8_MAX) && intSignedExceeds(&nodeVal->val, INT8_MIN, INT8_MAX)) {
					dataSize2 = 2;
					goto dataSizeLoop;
				}
				return intSignedExceeds(&nodeVal->val, INT8_MIN, INT8_MAX) ? &typeU8i : &typeI8i;
			case 2:
				if (intUnsignedExceeds(&nodeVal->val, UINT16_MAX) && intSignedExceeds(&nodeVal->val, INT16_MIN, INT16_MAX)) {
					dataSize2 = 4;
					goto dataSizeLoop;
				}
				return intSignedExceeds(&nodeVal->val, INT16_MIN, INT16_MAX) ? &typeU16i : &typeI16i;
			case 4:

				if (intUnsignedExceeds(&nodeVal->val, UINT32_MAX) && intSignedExceeds(&nodeVal->val, INT32_MIN, INT32_MAX)) {
					dataSize2 = 8;
					goto dataSizeLoop;
				}
				return intSignedExceeds(&nodeVal->val, INT32_MIN, INT32_MAX) ? &typeU32i : &typeI32i;
			case 8:
				return intSignedExceeds(&nodeVal->val, INT64_MIN, INT64_MAX) ? &typeU64i : &typeI64i;
			default:
				fprintf(stderr, "That's a weird data size.\n");
				abort();
			}
		}
		return NULL;
	}

	// Cant find type directly so intfer
	strGraphEdgeIRP in CLEANUP(strGraphEdgeIRPDestroy) = graphNodeIRIncoming(node);
	strGraphEdgeIRP sourceA CLEANUP(strGraphEdgeIRPDestroy) = IRGetConnsOfType(in, IR_CONN_SOURCE_A);
	strGraphEdgeIRP sourceB CLEANUP(strGraphEdgeIRPDestroy) = IRGetConnsOfType(in, IR_CONN_SOURCE_B);

	switch(graphNodeIRValuePtr(node)->type) {
	case IR_LAND:
	case IR_LOR:
	case IR_LXOR:
	case IR_GT:
	case IR_GE:
	case IR_LE:
	case IR_LT:
	case IR_EQ:
	case IR_NE:
			return &typeU8i;
	default:;
	}
	
	if (strGraphEdgeIRPSize(sourceA) && strGraphEdgeIRPSize(sourceB)) {
			//If is a subtraction between 2 pointers
			// Binop
		__auto_type aType = IRNodeType(graphEdgeIRIncoming(sourceA[0]));
		__auto_type bType = IRNodeType(graphEdgeIRIncoming(sourceB[0]));
		int aTypeDefined = NULL != aType;
		int bTypeDefined = NULL != bType;
		if (aTypeDefined ^ bTypeDefined) {
			struct object *type = (aTypeDefined) ? aType : bType;
			unifiyUpwards(node, type);
			return type;
		} else if (aTypeDefined && bTypeDefined) {
				int aPtr=aType->type==TYPE_PTR||aType->type==TYPE_ARRAY;
				int bPtr=bType->type==TYPE_PTR||bType->type==TYPE_ARRAY;
				if(aPtr&&bPtr) {
						if(graphNodeIRValuePtr(node)->type!=IR_SUB) {
								fputs("2 Pointer argumenrs to non \"-\".\n", stderr);
								abort();
						}
						return (dataSize()==4)?&typeI32i:&typeI64i;
				}
				return getHigherType(aType, bType);
		}
		return NULL;
	} else if (strGraphEdgeIRPSize(sourceA)) {
		// Unop
		__auto_type aType = IRNodeType(graphEdgeIRIncoming(sourceA[0]));
		__auto_type op = graphNodeIRValuePtr(node)->type;
		if (op == IR_ADDR_OF) {
			return objectPtrCreate(aType);
		} else if (op == IR_DERREF) {
				if(aType->type==TYPE_PTR) {
						struct objectPtr *aPtr = (void *)aType;
						return aPtr->type;
				} else if(aType->type==TYPE_ARRAY) {
						struct objectArray *aArr=(void *)aType;
						return aArr->type;
				} else {
						fputs("Expected array or pointer to derrefernce\n", stderr);
						abort();
				}
		} else if (op == IR_MEMBERS) {
			struct IRNodeMembers *mems = (void *)graphNodeIRValuePtr(node);
			return mems->members[strObjectMemberSize(mems->members) - 1].type;
		}
		return aType;
	} else if(graphNodeIRValuePtr(node)->type==IR_FUNC_CALL) {
			struct IRNodeFuncCall *call=(void*)graphNodeIRValuePtr(node);
			strGraphEdgeIRP in CLEANUP(strGraphEdgeIRPDestroy)=graphNodeIRIncoming(node);
			strGraphEdgeIRP func CLEANUP(strGraphEdgeIRPDestroy)=IRGetConnsOfType(in,IR_CONN_FUNC);
			__auto_type funcType=objectBaseType(IRNodeType(graphEdgeIRIncoming(func[0])));
			if(funcType->type==TYPE_PTR)
					funcType=((struct objectPtr*)funcType)->type;
			assert(funcType->type==TYPE_FUNCTION);
			return ((struct objectFunction*)funcType)->retType;
	}
	return NULL;
}
void IRNodeTypeAssign(graphNodeIR node,struct object *type) {
		*getType(node) = type;
}
struct object *IRNodeType(graphNodeIR node) {
	__auto_type type = __IRNodeType(node);
	*getType(node) = type;
	return type;
}
static int isF64CmpOp(graphNodeIR node) {
		switch(graphNodeIRValuePtr(node)->type) {
		case IR_GE:	
		case IR_LE:
		case IR_GT:
		case IR_LT:
		case IR_EQ:
		case IR_NE: {
				strGraphEdgeIRP incoming CLEANUP(strGraphEdgeIRPDestroy)=IREdgesByPrec(node);
				if(objectBaseType(IRNodeType(graphEdgeIRIncoming(incoming[0])))->type==TYPE_F64)
						return 1;
				if(objectBaseType(IRNodeType(graphEdgeIRIncoming(incoming[1])))->type==TYPE_F64)
						return 1;
		}
				default:
						return 0;
		}
}
void IRInsertImplicitTypecasts(graphNodeIR start) {
		strGraphNodeIRP allNodes CLEANUP(strGraphNodeIRPDestroy)=graphNodeIRAllNodes(start);
		strGraphNodeIRP visited CLEANUP(strGraphNodeIRPDestroy)=NULL;
	loop:
		for(long n=0;n!=strGraphNodeIRPSize(allNodes);n++) {
				visited=strGraphNodeIRPSortedInsert(visited, allNodes[n], (gnCmpType)ptrPtrCmp);
				__auto_type end=IREndOfExpr(allNodes[n]);
				if(end==NULL)
						continue;
				if(end!=allNodes[n])
						continue;
				
				strGraphNodeIRP exprNodes CLEANUP(strGraphNodeIRPDestroy)=IRStmtNodes(end);
				for(long e=0;e!=strGraphNodeIRPSize(exprNodes);e++) {
						strGraphEdgeIRP in CLEANUP(strGraphEdgeIRPDestroy)=IREdgesByPrec(exprNodes[e]);
						if(graphNodeIRValuePtr(exprNodes[e])->type==IR_FUNC_CALL)
								continue;
						if(graphNodeIRValuePtr(exprNodes[e])->type==IR_TYPECAST)
								continue;
						if(graphNodeIRValuePtr(exprNodes[e])->type==IR_DERREF) {
								__auto_type derrefType=IRNodeType(exprNodes[e]);
								strGraphEdgeIRP inDst CLEANUP(strGraphEdgeIRPDestroy)=IRGetConnsOfType(in, IR_CONN_DEST);
								if(strGraphEdgeIRPSize(inDst)) {
										__auto_type inNode=graphEdgeIRIncoming(inDst[0]);
										__auto_type tc=IRCreateTypecast(inNode, IRNodeType(inNode), derrefType);
										graphNodeIRConnect(tc,exprNodes[e], IR_CONN_DEST);
										graphEdgeIRKill(inNode, exprNodes[e], NULL, NULL, NULL);
								}
								continue;
						}
						
						struct object *toType=IRNodeType(exprNodes[e]);

						__auto_type exprType=graphNodeIRValuePtr(exprNodes[e])->type;
						if(exprType==IR_ADDR_OF||exprType==IR_MEMBERS||exprType==IR_MEMBERS_ADDR_OF)
								continue;
						
						//If an array decl,typecast to defuault value type
						if(graphNodeIRValuePtr(exprNodes[e])->type==IR_ARRAY_DECL) {
								toType=&typeI32i;
						}
						
						//If is a compare type,typecast to first argument
						switch(graphNodeIRValuePtr(exprNodes[e])->type) {
						case IR_GT:
						case IR_GE:
						case IR_LE:
						case IR_LT:
						case IR_EQ:
						case IR_NE: {
								strGraphEdgeIRP inA CLEANUP(strGraphEdgeIRPDestroy)=IRGetConnsOfType(in, IR_CONN_SOURCE_A);
								strGraphEdgeIRP inB CLEANUP(strGraphEdgeIRPDestroy)=IRGetConnsOfType(in, IR_CONN_SOURCE_B);
								toType= getHigherType(IRNodeType(graphEdgeIRIncoming(inA[0])), IRNodeType(graphEdgeIRIncoming(inB[0])));
						}
						default:;
						}
						//If is logical compare type,typecast to Bool
						switch(graphNodeIRValuePtr(exprNodes[e])->type) {
						case IR_LXOR:
						case IR_LOR:
						case IR_LAND:{
								strGraphEdgeIRP inA CLEANUP(strGraphEdgeIRPDestroy)=IRGetConnsOfType(in, IR_CONN_SOURCE_A);
								strGraphEdgeIRP inB CLEANUP(strGraphEdgeIRPDestroy)=IRGetConnsOfType(in, IR_CONN_SOURCE_B);
								toType= getHigherType(IRNodeType(graphEdgeIRIncoming(inA[0])), IRNodeType(graphEdgeIRIncoming(inB[0])));
						}
						default:;
						}

						//If is pointer arithmetic ("+"/"-" only),typecast index to data ptrSize
						switch(graphNodeIRValuePtr(exprNodes[e])->type) {
						case IR_ADD:
						case IR_SUB: {
								strGraphEdgeIRP inA CLEANUP(strGraphEdgeIRPDestroy)=IRGetConnsOfType(in, IR_CONN_SOURCE_A);
								strGraphEdgeIRP inB CLEANUP(strGraphEdgeIRPDestroy)=IRGetConnsOfType(in, IR_CONN_SOURCE_B);
								__auto_type aType=objectBaseType(IRNodeType(graphEdgeIRIncoming(inA[0])));
								__auto_type bType=objectBaseType(IRNodeType(graphEdgeIRIncoming(inB[0])));
								
								int aPtr=aType->type==TYPE_PTR||aType->type==TYPE_ARRAY;
								int bPtr=bType->type==TYPE_PTR||bType->type==TYPE_ARRAY;
								if(aPtr&&bPtr) {
										if(graphNodeIRValuePtr(exprNodes[e])->type!=IR_SUB) {
												fputs("2 Pointer argumenrs to non \"-\".\n", stderr);
												abort();
										}
										strGraphEdgeIRP inA CLEANUP(strGraphEdgeIRPDestroy)=IRGetConnsOfType(in, IR_CONN_SOURCE_A);
										strGraphEdgeIRP inB CLEANUP(strGraphEdgeIRPDestroy)=IRGetConnsOfType(in, IR_CONN_SOURCE_B);
										toType= getHigherType(IRNodeType(graphEdgeIRIncoming(inA[0])), IRNodeType(graphEdgeIRIncoming(inB[0])));
										goto end;
								} else if(aPtr||bPtr) {
										toType=(ptrSize()==4)?&typeI32i:&typeI64i;
										__auto_type indexEdge=(aType->type==TYPE_PTR||aType->type==TYPE_ARRAY)?inB[0]:inA[0];
										__auto_type edgeValue=*graphEdgeIRValuePtr(indexEdge);
										__auto_type inNode=graphEdgeIRIncoming(indexEdge);
										
										__auto_type tc=IRCreateTypecast(inNode, IRNodeType(inNode), toType);
										graphNodeIRConnect(tc,exprNodes[e], edgeValue);
										graphEdgeIRKill(inNode, exprNodes[e], NULL, NULL, NULL);
										goto end;
								}
						}
						default:
										;
						}
						
						for(long i=0;i!=strGraphEdgeIRPSize(in);i++) {
								__auto_type edgeValue=*graphEdgeIRValuePtr(in[i]);
								struct object *toType2=toType;
								if(edgeValue==IR_CONN_ASSIGN_FROM_PTR)
										toType2=objectPtrCreate(toType);
								
								__auto_type inNode=graphEdgeIRIncoming(in[i]);
								if(toType2==IRNodeType(inNode))
										continue;
								
								__auto_type tc=IRCreateTypecast(inNode, IRNodeType(inNode), toType2);
								graphNodeIRConnect(tc,exprNodes[e], edgeValue);
								graphEdgeIRKill(inNode, exprNodes[e], NULL, NULL, NULL);
						}
				}
		end:
				visited=strGraphNodeIRPSetUnion(visited, exprNodes, (gnCmpType)ptrPtrCmp);
				allNodes=strGraphNodeIRPSetDifference(allNodes, visited, (gnCmpType)ptrPtrCmp);
				strGraphNodeIRPDestroy(&visited);
				visited=NULL;
				goto loop;
		}
}
