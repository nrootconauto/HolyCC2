#include <IR.h>
#include <object.h>
#include <assert.h>
#include <base64.h>
#include <math.h>
#include <hashTable.h>
#include <stdio.h>
static char *ptr2Str(const void *a) {
		return base64Enc((void*)&a, sizeof(a));
}
MAP_TYPE_DEF(long,VarVal);
MAP_TYPE_FUNCS(long,VarVal);
static mapVarVal varVals=NULL;
static void initEvalIRNode(graphNodeIR node) {
		if(varVals)
				mapVarValDestroy(varVals, NULL);

		varVals=mapVarValCreate();
}
static long *valueHash(struct IRValue *value) {
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
				long len=snprintf(NULL, 0, "FRM:[%li]", value->value.__frame.offset);
				char buffer[len+1];
				sprintf(buffer, "FRM:[%li]",value->value.__frame.offset);

		loopLoc:;
				__auto_type find=mapVarValGet(varVals, buffer);
				//If doesnt exist,make a value for the frame spot
				if(!find) {
						mapVarValInsert(varVals, buffer, 0);
						goto loopLoc;
				} else {
						return mapVarValGet(varVals, buffer);
				}
		} else if(value->type==__IR_VAL_MEM_GLOBAL) {
				__auto_type sym=ptr2Str(value->value.__global.symbol);
				long len=snprintf(NULL, 0, "GLB:[%s]", sym);
				char buffer[len+1];
				sprintf(buffer, "GLB:[%s]",sym);
				free(sym);
				
		loopGlob:;
				__auto_type find=mapVarValGet(varVals, buffer);
				//If doesnt exist,make a value for the frame spot
				if(!find) {
						mapVarValInsert(varVals, buffer, 0);
						goto loopGlob;
				} else {
						return mapVarValGet(varVals, buffer);
				}
		}

		assert(0);
		return NULL;
}
static long evalIRNode(graphNodeIR node) {
		struct IRNode *ir=graphNodeIRValuePtr(node);
		switch(ir->type) {
		case IR_VALUE: {
				struct IRNodeValue *__value=(void*)ir;
				struct IRValue *value=&__value->val;
				switch(value-> type) {
				case IR_VAL_VAR_REF: {
						return *valueHash(value);
				}
				case IR_VAL_INT_LIT: {
						return value->value.intLit.value.sLong;
				}
				case IR_VAL_FUNC:
				case IR_VAL_REG: {
						//TODO implement me
				}
						default:
								assert(0);
				} 
		}
		case IR_ADD: {
				__auto_type incoming =graphNodeIRIncomingNodes(node);
				long sum=evalIRNode(incoming[0])+evalIRNode(incoming[1]);
				
				return sum;
		}
		case IR_SUB: {
				__auto_type incoming =graphNodeIRIncomingNodes(node);
				long total=evalIRNode(incoming[0])-evalIRNode(incoming[1]);
				return total;
		}
		case IR_ASSIGN: {
				__auto_type incoming =graphNodeIRIncomingNodes(node);
				__auto_type outgoing =graphNodeIRIncomingNodes(node);
				__auto_type left=graphNodeIRValuePtr(outgoing[0]);
				assert(left->type==IR_VALUE);
				struct IRNodeValue *val=(void*)left;
				assert(val->val.type==IR_VAL_VAR_REF);
				
				*valueHash(&val->val)=evalIRNode(incoming[0]);
		}
		case IR_BAND: {
				__auto_type incoming =graphNodeIRIncomingNodes(node);
				long total=evalIRNode(incoming[0])&evalIRNode(incoming[1]);
				return total;
		}
		case IR_BNOT: {
				__auto_type incoming =graphNodeIRIncomingNodes(node);
				long total=~evalIRNode(incoming[0]);
				return total;
		}
		case IR_BOR:  {
				__auto_type incoming =graphNodeIRIncomingNodes(node);
				long total=evalIRNode(incoming[0])|evalIRNode(incoming[1]);
				return total;
		}
		case IR_BXOR:  {
				__auto_type incoming =graphNodeIRIncomingNodes(node);
				long total=evalIRNode(incoming[0])^evalIRNode(incoming[1]);
				return total;
		}
		case IR_CHOOSE: assert(0);
		case IR_DEC: {
				__auto_type incoming =graphNodeIRIncomingNodes(node);
				assert(graphNodeIRValuePtr(incoming[0])->type==IR_VALUE);
				return --*valueHash(&((struct IRNodeValue*)graphNodeIRValuePtr(incoming[0]))->val);
		}
		case IR_DIV: {
				__auto_type incoming =graphNodeIRIncomingNodes(node);
				long total=evalIRNode(incoming[0])/evalIRNode(incoming[1]);
				return total;
		}
		case IR_EQ: {
				__auto_type incoming =graphNodeIRIncomingNodes(node);
				long total=evalIRNode(incoming[0])==evalIRNode(incoming[1]);
				return total;
		}
		case IR_GE: {
				__auto_type incoming =graphNodeIRIncomingNodes(node);
				long total=evalIRNode(incoming[0])>=evalIRNode(incoming[1]);
				return total;
		}
		case IR_LE:	{
				__auto_type incoming =graphNodeIRIncomingNodes(node);
				long total=evalIRNode(incoming[0])<=evalIRNode(incoming[1]);
				return total;
		}
		case IR_GT: {
				__auto_type incoming =graphNodeIRIncomingNodes(node);
				long total=evalIRNode(incoming[0])>evalIRNode(incoming[1]);
				return total;
		}
		case IR_LT:	{
				__auto_type incoming =graphNodeIRIncomingNodes(node);
				long total=evalIRNode(incoming[0])<evalIRNode(incoming[1]);
				return total;
		}
		case IR_INC: {
					__auto_type incoming =graphNodeIRIncomingNodes(node);
				assert(graphNodeIRValuePtr(incoming[0])->type==IR_VALUE);
				return ++*valueHash(&((struct IRNodeValue*)graphNodeIRValuePtr(incoming[0]))->val);
		}
		case IR_LAND: {
				__auto_type incoming =graphNodeIRIncomingNodes(node);
				long total=evalIRNode(incoming[0])&&evalIRNode(incoming[1]);
				return total;
		}
		case IR_LNOT:  {
				__auto_type incoming =graphNodeIRIncomingNodes(node);
				long total=!evalIRNode(incoming[0]);
				return total;
		}
		case IR_LOR: {
				__auto_type incoming =graphNodeIRIncomingNodes(node);
				long total=evalIRNode(incoming[0])||evalIRNode(incoming[1]);
				return total;
		}
		case IR_LXOR: {
				__auto_type incoming =graphNodeIRIncomingNodes(node);
				int boolA=!!evalIRNode(incoming[0]);
				int boolB=!!evalIRNode(incoming[1]);
				long total=boolA^boolB;
				return total;
		}
		case IR_LSHIFT:	{
				__auto_type incoming =graphNodeIRIncomingNodes(node);
				long total=evalIRNode(incoming[0])<<evalIRNode(incoming[1]);
				return total;
		}
		case IR_RSHIFT: {
				__auto_type incoming =graphNodeIRIncomingNodes(node);
				long total=evalIRNode(incoming[0])>>evalIRNode(incoming[1]);
				return total;
		}
		case IR_MOD: {
				__auto_type incoming =graphNodeIRIncomingNodes(node);
				long total=evalIRNode(incoming[0])%evalIRNode(incoming[1]);
				return total;
		}
		case IR_MULT: {
				__auto_type incoming =graphNodeIRIncomingNodes(node);
				long total=evalIRNode(incoming[0])*evalIRNode(incoming[1]);
				return total;
		}
		case IR_NE: {
				__auto_type incoming =graphNodeIRIncomingNodes(node);
				long int total=evalIRNode(incoming[0])!=evalIRNode(incoming[1]);
				return total;
		}
		case IR_NEG: {
				__auto_type incoming =graphNodeIRIncomingNodes(node);
				long  total=-evalIRNode(incoming[0]);
				return total;
		}
		case IR_POS: {
				__auto_type incoming =graphNodeIRIncomingNodes(node);
				long  total=+evalIRNode(incoming[0]);
				return total;
		}
		case IR_TYPECAST: {
				__auto_type incoming =graphNodeIRIncomingNodes(node);
				long total=evalIRNode(incoming[0]);
				return total;
		}
		case IR_POW: {
				__auto_type incoming =graphNodeIRIncomingNodes(node);
				long total=pow(evalIRNode(incoming[0]), evalIRNode(incoming[1]));
				return total;
		}
		case IR_ARRAY_ACCESS:
				default:
						assert(0);
		}
		return 0;
}
