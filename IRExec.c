#include <IR.h>
#include <object.h>
#include <assert.h>
#include <base64.h>
#include <math.h>
#include <hashTable.h>
#include <stdio.h>
#include <stdint.h>
#include <exprParser.h>
#include <intExponet.h>
#include <IRExec.h>
static char *ptr2Str(const void *a) {
		return base64Enc((void*)&a, sizeof(a));
}
MAP_TYPE_DEF(graphNodeIR,Func);
MAP_TYPE_FUNCS(graphNodeIR,Func);
static __thread mapFunc funcs=NULL;
MAP_TYPE_DEF(struct IREvalVal,VarVal);
MAP_TYPE_FUNCS(struct IREvalVal,VarVal);
static __thread mapVarVal varVals=NULL;
static void initEvalIRNode(graphNodeIR node) {
		if(varVals)
				mapVarValDestroy(varVals, NULL);

		varVals=mapVarValCreate();
}
static struct IREvalVal dftValueType(enum IREvalValType type) {
		struct IREvalVal retVal;
		retVal.type=type;
		switch(type) {
		case IREVAL_VAL_DFT:
				//?
				break;
		case IREVAL_VAL_FLT:
				retVal.value.flt=0;
				break;
		case IREVAL_VAL_INT:
				retVal.value.i=0;
				break;
		case IREVAL_VAL_PTR:
				retVal.value.ptr.type=IREVAL_VAL_INT;
				retVal.value.ptr.value=0;
				}
		
		return retVal;
}
static struct IREvalVal  *arrayAccessHash(struct IREvalVal *array,struct IREvalVal *index,enum IREvalValType type) {
		__auto_type aHash=ptr2Str(array);
		__auto_type iHash=ptr2Str(index);
		long len=snprintf(NULL,0,"%s[%s]",aHash,iHash);
		char buffer[len+1];
		sprintf(buffer, "%s[%s]",aHash,iHash);

	loop:;
		__auto_type find=mapVarValGet(varVals, buffer);
		if(!find) {
				mapVarValInsert(varVals, buffer, dftValueType(type));
				goto loop;
		}

		return find;
}
static struct IREvalVal *valueHash(struct IRValue *value,enum IREvalValType type) {
		if(value->type==IR_VAL_VAR_REF) {
		if(value-> value.var.var.type==IR_VAR_VAR) {
				__auto_type ptrStr=ptr2Str(value->value.var.var.value.var);
				__auto_type find=mapVarValGet(varVals, ptrStr);
				free(ptrStr);
				
				assert(find);
				return find;
		} else if(value->value.var.var.type==IR_VAR_MEMBER) {
				__auto_type ptrStr=ptr2Str(value->value.var.var.value.member);
				__auto_type find=mapVarValGet(varVals, ptrStr);
				free(ptrStr);
				
				assert(find);
				return find;
		}
		} else if(value->type==IR_VAL_REG) {
				//TODO implement me
		} else if(value->type==__IR_VAL_MEM_FRAME) {
				long len=snprintf(NULL, 0, "FRM:[%li,%i]", value->value.__frame.offset,type);
				char buffer[len+1];
				sprintf(buffer, "FRM:[%li,%i]",value->value.__frame.offset,type);

		loopLoc:;
				__auto_type find=mapVarValGet(varVals, buffer);
				//If doesnt exist,make a value for the frame spot
				if(!find) {
						//Assert that value exists if refernceing dft type
						assert(type!=IREVAL_VAL_DFT);
						mapVarValInsert(varVals, buffer, dftValueType(type));
						goto loopLoc;
				} else {
						return mapVarValGet(varVals, buffer);
				}
		} else if(value->type==__IR_VAL_MEM_GLOBAL) {
				__auto_type sym=ptr2Str(value->value.__global.symbol);
				long len=snprintf(NULL, 0, "GLB:[%s,%i]", sym,type);
				char buffer[len+1];
				sprintf(buffer, "GLB:[%s,%i]",sym,type);
				free(sym);
				
		loopGlob:;
				__auto_type find=mapVarValGet(varVals, buffer);
				//If doesnt exist,make a value for the frame spot
				if(!find) {
						//Assert that value exists if refernceing dft type
						assert(type!=IREVAL_VAL_DFT);
						
						mapVarValInsert(varVals, buffer, dftValueType(type));
						goto loopGlob;
				} else {
						return mapVarValGet(varVals, buffer);
				}
		}

		assert(0);
		return NULL;
}

struct object *IRValuegetType(struct IRValue *node) {
		switch(node->type) {
		case IR_VAL_VAR_REF: {
				if(node->value.var.var.type==IR_VAR_VAR)
						return node->value.var.var.value.var->type;
				else if(node->value.var.var.type==IR_VAR_MEMBER)
						return  assignTypeToOp((void*)node->value.var.var.value.member);
				return NULL;
		}
		case IR_VAL_STR_LIT:
				return objectPtrCreate(&typeU8i);
		case IR_VAL_INT_LIT:
				return &typeI64i;
		case __IR_VAL_MEM_FRAME:
				return node->value.__frame.type;
		case __IR_VAL_MEM_GLOBAL:
				return node->value.__global.symbol->type;
		case __IR_VAL_LABEL:
		case IR_VAL_REG:
		case IR_VAL_FUNC:
				//?
				return NULL;
		}
}
struct IREvalVal IREValValIntCreate(long i) {
		struct IREvalVal val;
		val.type=IREVAL_VAL_INT;
		val.value.i=i; 
		
		return val;
}
struct IREvalVal IREvalValFltCreate(double f) {
		struct IREvalVal val;
		val.type=IREVAL_VAL_FLT;
		val.value.flt=f;
		
		return val;
}
static struct IREvalVal maskInt2Width(struct IREvalVal input,int width,int *success) {
		if(input.type!=IREVAL_VAL_INT)
				if(success) {*success=0; return IREValValIntCreate(0);};

		//Clear first bits,then xor to clear the remaining "tail" that wasnt cleared
		__auto_type oldVal=input;
		input.value.i>>=8*width;
		input.value.i<<=8*width;
		input.value.i^=oldVal.value.i;

		return input;
}
static int getBinopArgs(graphNodeIR node,struct IREvalVal *arg1,struct IREvalVal *arg2) {
__auto_type incoming =graphNodeIRIncoming(node);
	int success;
	__auto_type a=IREvalNode(graphEdgeIROutgoing(incoming[0]),&success);
	if(!success) goto fail;
	__auto_type b=IREvalNode(graphEdgeIROutgoing(incoming[1]),&success);
	if(!success) goto fail;
	
	if(*graphEdgeIRValuePtr(incoming[0])==IR_CONN_SOURCE_B) {
			__auto_type tmp=a;
			a=b;
			b=tmp;
	}
	strGraphEdgeIRPDestroy(&incoming);

	*arg1=a;
	*arg2=b;
	return 1;
	fail:
	return 0;
}

#define BINOP_BIT(op,successPtr) ({																																					\
						struct IREvalVal a,b;																																													\
		if(!getBinopArgs(node, &a, &b)) if(success) *successPtr=0;																								\
		assert(a.type==b.type);																																															\
		if(a.type==IREVAL_VAL_INT) {																																										\
				if(success) *successPtr=1;																																										\
				return IREValValIntCreate(a.value.i op b.value.i);																					\
		} else {																																																													\
				if(success) *successPtr=0;																		\
				return IREValValIntCreate(0);																			\
		}																																													\
})
#define BINOP_ARITH(op,successPtr) ({																																			\
						struct IREvalVal a,b;																																													\
						if(!getBinopArgs(node, &a, &b)) if(success) *successPtr=0;								\
						assert(a.type==b.type);																																											\
						if(a.type==IREVAL_VAL_INT) {																																						\
								if(success) *successPtr=1;																																						\
								return IREValValIntCreate(a.value.i op b.value.i);}																	\
						else if(a.type==IREVAL_VAL_FLT) {																																	\
								if(success) *successPtr=1;																																						\
								return IREvalValFltCreate(a.value.flt op b.value.flt);														\
						}																																																																	\
						else {																																																												\
								if(success) *successPtr=0; return IREValValIntCreate(0);												\
						}																																																																	\
				})
#define BINOP_LOG(op,successPtr) ({																																\
						struct IREvalVal a,b;																																								\
						if(!getBinopArgs(node, &a, &b)) if(success) *successPtr=0;			\
						assert(a.type==b.type);																																						\
						if(a.type==IREVAL_VAL_INT) {																																	\
								if(success) *successPtr=1;																																	\
								return IREValValIntCreate(!!(a.value.i) op !!(a.value.i));					\
						}																																																												\
						else if(a.type==IREVAL_VAL_FLT) {																												\
								if(success) *successPtr=1;																																	\
								return IREvalValFltCreate(!!(a.value.flt) op !!(a.value.flt)); \
						}																																																												\
						else {																																																							\
								if(success) *successPtr=0;																																	\
								return IREValValIntCreate(0);}																																	\
				})

//TODO implement me
static struct IREvalVal evalIRCallFunc(struct function *func) {
		return IREValValIntCreate(0);
}
struct IREvalVal IREvalNode(graphNodeIR node,int *success) {
		struct IRNode *ir=graphNodeIRValuePtr(node);
		switch(ir->type) {
		case IR_VALUE: {
				struct IRNodeValue *__value=(void*)ir;
				struct IRValue *value=&__value->val;
				switch(value-> type) {
				case IR_VAL_VAR_REF: {
						if(success) *success=1;
						return *valueHash(value,IREVAL_VAL_DFT);
				}
				case IR_VAL_INT_LIT: {
						if(success) *success=1;
						return IREValValIntCreate(value->value.intLit.value.sLong);
				}
				case IR_VAL_FUNC:
				case IR_VAL_REG: {
						//TODO implement me
				}
						default:
								if(success) *success=0;
				} 
		}
		case IR_ADD: {
				BINOP_ARITH(+,success);
		}
		case IR_SUB: {
				BINOP_ARITH(-,success);
		}
		case IR_ASSIGN: {
				__auto_type incoming =graphNodeIRIncomingNodes(node);
				__auto_type outgoing =graphNodeIRIncomingNodes(node);
				__auto_type left=graphNodeIRValuePtr(outgoing[0]);
				assert(left->type==IR_VALUE);
				struct IRNodeValue *val=(void*)left;

				__auto_type value=IREvalNode(incoming[0],success);
				__auto_type assignTo=valueHash(&val->val,IREVAL_VAL_DFT);
				if(assignTo) {
						*assignTo=value;
						if(success) *success=1;
				} else
						if(success) *success=0;
				return value;
		}
		case IR_BAND: {
				struct IREvalVal a,b;
				if(!getBinopArgs(node, &a,&b))
						goto fail;
				if(a.type!=b.type||a.type!=IREVAL_VAL_INT)
						goto fail;
				
				if(success) *success=1;
				return  IREValValIntCreate(a.value.i&b.value.i);
		}
		case IR_BNOT: {
				__auto_type incoming =graphNodeIRIncomingNodes(node);
				__auto_type val=IREvalNode(incoming[0],success);
				if(!success)
						goto fail;

				if(val.type==IREVAL_VAL_INT)
						val.value.i=~val.value.i;
				else
						goto fail;

				if(success) *success=1;
				return val;
		}
		case IR_BOR:  {
				BINOP_BIT(|,success);
		}
		case IR_BXOR:  {
				BINOP_BIT(^, success);
		}
		case IR_CHOOSE: assert(0);
		case IR_DEC: {
				__auto_type incoming =graphNodeIRIncomingNodes(node);
				assert(graphNodeIRValuePtr(incoming[0])->type==IR_VALUE);

				__auto_type valHash=valueHash(&((struct IRNodeValue*)graphNodeIRValuePtr(incoming[0]))->val,IREVAL_VAL_DFT);
				assert(valHash->type==IREVAL_VAL_FLT||valHash->type==IREVAL_VAL_INT);

				if(valHash->type==IREVAL_VAL_INT) {
						valHash->value.i--;
						if(success) *success=1;
				} else if(valHash->type==IREVAL_VAL_FLT) {
						valHash->value.flt--;
						if(success) *success=1;
				} else if(success)
						*success=0;
				
				strGraphNodeIRPDestroy(&incoming);

				return *valHash;
		}
		case IR_DIV: {
				BINOP_ARITH(/,success);
		}
		case IR_EQ: {
				BINOP_ARITH(==,success);
		}
		case IR_GE: {
				BINOP_ARITH(>=,success);
		}
		case IR_LE:	{
				BINOP_ARITH(<=,success);
		}
		case IR_GT: {
				BINOP_ARITH(>,success);
		}
		case IR_LT:	{
				BINOP_ARITH(<,success);
		}
		case IR_INC: {
				__auto_type incoming =graphNodeIRIncomingNodes(node);
				assert(graphNodeIRValuePtr(incoming[0])->type==IR_VALUE);

				__auto_type valHash=valueHash(&((struct IRNodeValue*)graphNodeIRValuePtr(incoming[0]))->val,IREVAL_VAL_DFT);
				assert(valHash->type==IREVAL_VAL_FLT||valHash->type==IREVAL_VAL_INT);

				if(valHash->type==IREVAL_VAL_INT) {
						valHash->value.i++;
						if(success) *success=1;
				} else if(valHash->type==IREVAL_VAL_FLT) {
						valHash->value.flt++;
						if(success) *success=1;
				} else if(success)
						*success=0;
				
				strGraphNodeIRPDestroy(&incoming);

				return *valHash;
		}
		case IR_LAND: {
				BINOP_LOG(&,  success);
		}
		case IR_LNOT:  {
				__auto_type incoming =graphNodeIRIncomingNodes(node);

				__auto_type total=IREvalNode(incoming[0],success);
				if(!success)
						goto fail;
				
				if(total.type==IREVAL_VAL_INT||total.type==IREVAL_VAL_FLT) {
						if(success) *success=1;
						if(total.type==IREVAL_VAL_INT) {
								total.value.i=!total.value.i;
						} else if(total.type==IREVAL_VAL_FLT) {
								total.value.flt=!total.value.flt;
						}
				} else goto fail;
				return total;
		}
		case IR_LOR: {
				BINOP_LOG(|,  success);
		}
		case IR_LXOR: {
				BINOP_LOG(^,  success);
		}
		case IR_LSHIFT:	{
				BINOP_BIT(<<, success);
		}
		case IR_RSHIFT: {
				BINOP_BIT(>>, success);
		}
		case IR_MOD: {
				BINOP_BIT(%, success);
		}
		case IR_MULT: {
				BINOP_BIT(*, success);
		}
		case IR_NE: {
				BINOP_ARITH(!=, success);
		}
		case IR_NEG: {
				__auto_type incoming =graphNodeIRIncomingNodes(node);
				__auto_type a=IREvalNode(incoming[0], success);
				if(!success)
						goto fail;
				
				if(a.type==IREVAL_VAL_INT||a.type==IREVAL_VAL_FLT){
						if(success) *success=1;
				} else
								goto  fail;
				
				if(a.type==IREVAL_VAL_INT)
						a.value.i=-a.value.i;
				if(a.type==IREVAL_VAL_FLT)
						a.value.flt=-a.value.flt;
				
				strGraphNodeIRPDestroy(&incoming);
				return a;
		}
		case IR_POS: {
				__auto_type incoming =graphNodeIRIncomingNodes(node);
				__auto_type a=IREvalNode(incoming[0],success);

				if(a.type==IREVAL_VAL_INT||a.type==IREVAL_VAL_FLT) {
						if(success) *success=1;
				} else goto fail;

				strGraphNodeIRPDestroy(&incoming);
				return a;
		}
		case IR_TYPECAST: {
				__auto_type incoming =graphNodeIRIncomingNodes(node);

				__auto_type a=IREvalNode(incoming[0], success);
				__auto_type cast=(struct IRNodeTypeCast*)graphNodeIRValuePtr(node);

				//Evaluator only works on ints/floats,it doesnt compute pointer-casts.
				if(a.type!=IREVAL_VAL_INT&&a.type!=IREVAL_VAL_INT) {
						strGraphNodeIRPDestroy(&incoming);
						goto fail;
				}

				struct IREvalVal retVal=IREValValIntCreate(0);
				//Int->F64
				if(cast->out==&typeF64&&a.type==IREVAL_VAL_INT) {
						retVal=IREvalValFltCreate(a.value.i);
				} else if(a.type==IREVAL_VAL_INT&&cast->in==&typeF64) {
						//Cast is either int or float(can't cast classes/unions),so
						retVal=IREValValIntCreate(a.value.i);
				} else goto fail;

				if(success)
						*success=1;
				
				strGraphNodeIRPDestroy(&incoming);
				return retVal;
		}
		case IR_POW: {
				struct IREvalVal a,b; 
				if(!getBinopArgs(node, &a, &b))
						goto fail;

				if(a.type!=a.type)
						goto fail;

				struct IREvalVal retVal;
				if(a.type==IREVAL_VAL_INT) {
						if(a.value.i<0)
								retVal=IREValValIntCreate(intExpS(a.value.i, b.value.i));
						else
								retVal=IREValValIntCreate(intExpU(a.value.i, b.value.i));
				} else if(a.type==IREVAL_VAL_FLT) {
						__auto_type res=pow(a.value.flt,b.value.flt);
						if(success) *success=1;
				}
				
				if(success) *success=1;
				return retVal;
		}
		case IR_LOAD: {
				__auto_type incoming =graphNodeIRIncomingNodes(node);
				__auto_type retVal=valueHash(&((struct IRNodeValue*)graphNodeIRValuePtr(incoming[0]))->val, IREVAL_VAL_DFT);
				if(!retVal)
						goto fail;

				if(success) *success=1;
				return *retVal;
		}
		case IR_FUNC_CALL: {
				/**
				struct IRNodeFuncCall *call=(void*)graphNodeIRValuePtr(node);

				struct IREvalVal vals[strGraphNodeIRPSize(call->incomingArgs)];
				for(long i=0;i!=strGraphNodeIRPSize(call->incomingArgs);i++) {
						int success2;
						vals[i]=IREvalNode(call->incomingArgs[i],&success2);
						if(!success2)
								goto fail;
				}

				__auto_type incoming=graphNodeIRIncoming(node); 
				__auto_type in=IRGetConnsOfType(incoming, IR_CONN_FUNC);
				assert(strGraphEdgeIRPSize(in)==1);

				int success2;
				__auto_type func=IREvalNode(graphEdgeIRIncoming(in[0]), &success2);

				strGraphEdgeIRPDestroy(&incoming);
				strGraphEdgeIRPDestroy(&in);
				if(!success2) goto fail;
				*/
				goto fail;
		}
		case IR_ARRAY_ACCESS: {
				struct IREvalVal a,b;
				if(!getBinopArgs(node, &a, &b))
						goto fail;
				
				return *arrayAccessHash(&a, &b, a.type);
		}
		case IR_SIMD:
		default:
				goto fail;
		}
		fail:
		*success=1;
		return IREValValIntCreate(0);
}
void IREValSetVarVal(const struct variable *var,struct IREvalVal value) {
		struct IRValue ref;
		ref.type=IR_VAL_VAR_REF;
		ref.value.var.var.type=IR_VAR_VAR;
		ref.value.var.var.value.var=(void*)var;
		*valueHash(&ref, value.type)=value;
}
