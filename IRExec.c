#include <IR.h>
#include <IRExec.h>
#include <assert.h>
#include <base64.h>
#include <cleanup.h>
#include <exprParser.h>
#include <hashTable.h>
#include <intExponet.h>
#include <math.h>
#include <object.h>
#include <registers.h>
#include <stdint.h>
#include <stdio.h>
static char *ptr2Str(const void *a) { return base64Enc((void *)&a, sizeof(a)); }
MAP_TYPE_DEF(graphNodeIR, Func);
MAP_TYPE_FUNCS(graphNodeIR, Func);
static mapFunc funcs = NULL;
MAP_TYPE_DEF(struct IREvalVal, VarVal);
MAP_TYPE_FUNCS(struct IREvalVal, VarVal);
static mapVarVal varVals = NULL;
struct frameItem {
	struct IRVar *var;
	struct IREvalVal value;
};
LL_TYPE_DEF(struct frameItem, FrameItem);
LL_TYPE_FUNCS(struct frameItem, FrameItem);
MAP_TYPE_DEF(struct IREvalVal, RegVal);
MAP_TYPE_FUNCS(struct IREvalVal, RegVal);
static mapRegVal registerValues = NULL;
static llFrameItem frame = NULL;
void IREvalInit() {
	if (varVals)
		mapVarValDestroy(varVals, NULL);
	if (registerValues)
		mapRegValDestroy(registerValues, NULL);

	registerValues = mapRegValCreate();
	varVals = mapVarValCreate();
}
static struct IREvalVal dftValueType(enum IREvalValType type) {
	struct IREvalVal retVal;
	retVal.type = type;
	switch (type) {
	case IREVAL_VAL_REG: {
		retVal.value.reg = &regX86EAX;
		break;
	}
	case IREVAL_VAL_DFT:
		//?
		break;
	case IREVAL_VAL_FLT:
		retVal.value.flt = 0;
		break;
	case IREVAL_VAL_INT:
		retVal.value.i = 0;
		break;
	case IREVAL_VAL_VAR:
	case IREVAL_VAL_PTR:
		retVal.value.ptr.type = IREVAL_VAL_INT;
		retVal.value.ptr.value = 0;
	}

	return retVal;
}
static struct IREvalVal *arrayAccessHash(struct IREvalVal *array,
                                         struct IREvalVal *index,
                                         enum IREvalValType type) {
	__auto_type aHash = ptr2Str(array);
	__auto_type iHash = ptr2Str(index);
	long len = snprintf(NULL, 0, "%s[%s]", aHash, iHash);
	char buffer[len + 1];
	sprintf(buffer, "%s[%s]", aHash, iHash);

loop:;
	__auto_type find = mapVarValGet(varVals, buffer);
	if (!find) {
		mapVarValInsert(varVals, buffer, dftValueType(type));
		goto loop;
	}

	return find;
}
static int llFrameItemGetPred(const struct IRVar *a,
                              const struct frameItem *item) {
	return IRVarCmpIgnoreVersion(a, item->var);
}
static int llFrameItemInsertPred(const struct frameItem *a,
                                 const struct frameItem *b) {
	return IRVarCmpIgnoreVersion(a->var, b->var);
}
typedef int (*frameItemGetPredType)(const void *, const struct frameItem *);
static struct IREvalVal *valueHash(struct IRValue *value) {
	if (value->type == IR_VAL_REG) {
	registerValueLoop:;
		__auto_type regValue =
		    mapRegValGet(registerValues, value->value.reg.reg->name);
		if (!regValue) {
			mapRegValInsert(registerValues, value->value.reg.reg->name,
			                dftValueType(IREVAL_VAL_INT));
			goto registerValueLoop;
		}

		return regValue;
	} else if (value->type == IR_VAL_VAR_REF) {
		if (value->value.var.type == IR_VAR_VAR) {
			__auto_type ptrStr = ptr2Str(value->value.var.value.var);
		loopVar:;
			__auto_type find = mapVarValGet(varVals, ptrStr);
			if (!find) {
				mapVarValInsert(varVals, ptrStr, dftValueType(IREVAL_VAL_INT));
				goto loopVar;
			}

			assert(find);
			return find;
		} else if (value->value.var.type == IR_VAR_MEMBER) {
				assert(-0);
		}
	} else if (value->type == IR_VAL_REG) {
		// TODO implement me
	} else if (value->type == __IR_VAL_MEM_FRAME) {
		long len = snprintf(NULL, 0, "FRM:[%li]", value->value.__frame.offset);
		char buffer[len + 1];
		sprintf(buffer, "FRM:[%li]", value->value.__frame.offset);

	loopLoc:;
		__auto_type find = mapVarValGet(varVals, buffer);
		// If doesnt exist,make a value for the frame spot
		if (!find) {
			// Assert that value exists if refernceing dft type
			mapVarValInsert(varVals, buffer, dftValueType(IREVAL_VAL_INT));
			goto loopLoc;
		} else {
			return mapVarValGet(varVals, buffer);
		}
	} else if (value->type == __IR_VAL_MEM_GLOBAL) {
		__auto_type sym = ptr2Str(value->value.__global.symbol);
		long len = snprintf(NULL, 0, "GLB:[%s]", sym);
		char buffer[len + 1];
		sprintf(buffer, "GLB:[%s]", sym);

	loopGlob:;
		__auto_type find = mapVarValGet(varVals, buffer);
		// If doesnt exist,make a value for the frame spot
		if (!find) {
			// Assert that value exists if refernceing dft type
			mapVarValInsert(varVals, buffer, dftValueType(IREVAL_VAL_INT));
			goto loopGlob;
		} else {
			return mapVarValGet(varVals, buffer);
		}
	}

	assert(0);
	return NULL;
}
static void spillToFrame(struct IREvalVal value, struct IRVar *frameVar) {
loop:;
	__auto_type find = llFrameItemFind(frame, frameVar,
	                                   (frameItemGetPredType)llFrameItemGetPred);
	if (!find) {
		// Insert if doesn't exit
		struct frameItem new;
		new.var = frameVar;
		__auto_type newLL = llFrameItemCreate(new);

		llFrameItemInsert(frame, newLL, llFrameItemInsertPred);
		frame = newLL;
		goto loop;
	}

	// Assign value to frame item
	llFrameItemValuePtr(find)->value = value;
}
static struct IREvalVal *loadFromFrame(struct IRVar *frameVar) {
	__auto_type find = llFrameItemFind(frame, frameVar,
	                                   (frameItemGetPredType)llFrameItemGetPred);
	if (!find) {
		return NULL;
	}

	return &llFrameItemValuePtr(find)->value;
}

struct object *IRValuegetType(struct IRValue *node) {
	switch (node->type) {
	case IR_VAL_VAR_REF: {
		if (node->value.var.type == IR_VAR_VAR)
			return node->value.var.value.var->type;
		else if (node->value.var.type == IR_VAR_MEMBER)
			return (void *)node->value.var.value.member.mem->type;
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
	val.type = IREVAL_VAL_INT;
	val.value.i = i;

	return val;
}
struct IREvalVal IREvalValFltCreate(double f) {
	struct IREvalVal val;
	val.type = IREVAL_VAL_FLT;
	val.value.flt = f;

	return val;
}
static struct IREvalVal maskInt2Width(struct IREvalVal input, int width,
                                      int *success) {
	if (input.type != IREVAL_VAL_INT)
		if (success) {
			*success = 0;
			return IREValValIntCreate(0);
		};

	// Clear first bits,then xor to clear the remaining "tail" that wasnt cleared
	__auto_type oldVal = input;
	input.value.i >>= 8 * width;
	input.value.i <<= 8 * width;
	input.value.i ^= oldVal.value.i;

	return input;
}
static int getBinopArgs(graphNodeIR node, struct IREvalVal *arg1,
                        struct IREvalVal *arg2) {
	__auto_type incoming = graphNodeIRIncoming(node);
	int success;
	__auto_type a = IREvalNode(graphEdgeIRIncoming(incoming[0]), &success);
	if (!success)
		goto fail;
	__auto_type b = IREvalNode(graphEdgeIRIncoming(incoming[1]), &success);
	if (!success)
		goto fail;

	if (*graphEdgeIRValuePtr(incoming[0]) == IR_CONN_SOURCE_B) {
		__auto_type tmp = a;
		a = b;
		b = tmp;
	}

	*arg1 = a;
	*arg2 = b;
	return 1;
fail:
	return 0;
}

#define BINOP_BIT(op, successPtr)                                              \
	({                                                                           \
		struct IREvalVal a, b;                                                     \
		if (!getBinopArgs(node, &a, &b))                                           \
			if (success)                                                             \
				*successPtr = 0;                                                       \
		assert(a.type == b.type);                                                  \
		if (a.type == IREVAL_VAL_INT) {                                            \
			if (success)                                                             \
				*successPtr = 1;                                                       \
			return IREValValIntCreate(a.value.i op b.value.i);                       \
		} else {                                                                   \
			if (success)                                                             \
				*successPtr = 0;                                                       \
			return IREValValIntCreate(0);                                            \
		}                                                                          \
	})
#define BINOP_ARITH(op, successPtr)                                            \
	({                                                                           \
		struct IREvalVal a, b;                                                     \
		if (!getBinopArgs(node, &a, &b))                                           \
			if (success)                                                             \
				*successPtr = 0;                                                       \
		assert(a.type == b.type);                                                  \
		if (a.type == IREVAL_VAL_INT) {                                            \
			if (success)                                                             \
				*successPtr = 1;                                                       \
			return IREValValIntCreate(a.value.i op b.value.i);                       \
		} else if (a.type == IREVAL_VAL_FLT) {                                     \
			if (success)                                                             \
				*successPtr = 1;                                                       \
			return IREvalValFltCreate(a.value.flt op b.value.flt);                   \
		} else {                                                                   \
			if (success)                                                             \
				*successPtr = 0;                                                       \
			return IREValValIntCreate(0);                                            \
		}                                                                          \
	})
#define BINOP_LOG(op, successPtr)                                              \
	({                                                                           \
		struct IREvalVal a, b;                                                     \
		if (!getBinopArgs(node, &a, &b))                                           \
			if (success)                                                             \
				*successPtr = 0;                                                       \
		assert(a.type == b.type);                                                  \
		if (a.type == IREVAL_VAL_INT) {                                            \
			if (success)                                                             \
				*successPtr = 1;                                                       \
			return IREValValIntCreate(!!(a.value.i)op !!(a.value.i));                \
		} else if (a.type == IREVAL_VAL_FLT) {                                     \
			if (success)                                                             \
				*successPtr = 1;                                                       \
			return IREvalValFltCreate(!!(a.value.flt)op !!(a.value.flt));            \
		} else {                                                                   \
			if (success)                                                             \
				*successPtr = 0;                                                       \
			return IREValValIntCreate(0);                                            \
		}                                                                          \
	})

// TODO implement me
static struct IREvalVal evalIRCallFunc(struct function *func) {
	return IREValValIntCreate(0);
}
struct IREvalVal IREvalNode(graphNodeIR node, int *success) {
	struct IRNode *ir = graphNodeIRValuePtr(node);
	switch (ir->type) {
	case IR_VALUE: {
		// Check for incoming assign
		int assignedInto = 0;
		strGraphEdgeIRP incoming CLEANUP(strGraphEdgeIRPDestroy) =
		    graphNodeIRIncoming(node);
		strGraphEdgeIRP assign CLEANUP(strGraphEdgeIRPDestroy) =
		    IRGetConnsOfType(incoming, IR_CONN_DEST);
		struct IREvalVal res;
		if (strGraphEdgeIRPSize(assign) != 0) {
			assignedInto = 1;
			int success2;
			res = IREvalNode(graphEdgeIRIncoming(incoming[0]), &success2);
			if (!success2)
				goto fail;
		}

		struct IRNodeValue *__value = (void *)ir;
		struct IRValue *value = &__value->val;
		switch (value->type) {
		case IR_VAL_VAR_REF: {
			int success2 = 1;
			if (assignedInto) {
				*valueHash(value) = res;
			}

			if (!success2)
				goto fail;

			if (success)
				*success = 1;
			return *valueHash(value);
		}
		case IR_VAL_INT_LIT: {
			if (success)
				*success = 1;
			return IREValValIntCreate(value->value.intLit.value.sLong);
		}
		case IR_VAL_REG: {
			if (success)
				*success = 1;
			if (assignedInto)
				*valueHash(value) = res;

			return *valueHash(value);
		}
		case IR_VAL_FUNC:
		default:
			if (success)
				*success = 0;
		}
	}
	case IR_ADD: {
		BINOP_ARITH(+, success);
	}
	case IR_SUB: {
		BINOP_ARITH(-, success);
	}
	case IR_ASSIGN: {
		__auto_type incoming = graphNodeIRIncomingNodes(node);
		__auto_type outgoing = graphNodeIRIncomingNodes(node);
		__auto_type left = graphNodeIRValuePtr(outgoing[0]);
		assert(left->type == IR_VALUE);
		struct IRNodeValue *val = (void *)left;

		__auto_type value = IREvalNode(incoming[0], success);
		__auto_type assignTo = valueHash(&val->val);
		if (assignTo) {
			*assignTo = value;
			if (success)
				*success = 1;
		} else if (success)
			*success = 0;
		return value;
	}
	case IR_BAND: {
		struct IREvalVal a, b;
		if (!getBinopArgs(node, &a, &b))
			goto fail;
		if (a.type != b.type || a.type != IREVAL_VAL_INT)
			goto fail;

		if (success)
			*success = 1;
		return IREValValIntCreate(a.value.i & b.value.i);
	}
	case IR_BNOT: {
		__auto_type incoming = graphNodeIRIncomingNodes(node);
		__auto_type val = IREvalNode(incoming[0], success);
		if (!success)
			goto fail;

		if (val.type == IREVAL_VAL_INT)
			val.value.i = ~val.value.i;
		else
			goto fail;

		if (success)
			*success = 1;
		return val;
	}
	case IR_BOR: {
		BINOP_BIT(|, success);
	}
	case IR_BXOR: {
		BINOP_BIT (^, success);
	}
	case IR_CHOOSE:
		assert(0);
	case IR_DEC: {
		__auto_type incoming = graphNodeIRIncomingNodes(node);
		assert(graphNodeIRValuePtr(incoming[0])->type == IR_VALUE);

		__auto_type valHash = valueHash(
		    &((struct IRNodeValue *)graphNodeIRValuePtr(incoming[0]))->val);
		assert(valHash->type == IREVAL_VAL_FLT || valHash->type == IREVAL_VAL_INT);

		if (valHash->type == IREVAL_VAL_INT) {
			valHash->value.i--;
			if (success)
				*success = 1;
		} else if (valHash->type == IREVAL_VAL_FLT) {
			valHash->value.flt--;
			if (success)
				*success = 1;
		} else if (success)
			*success = 0;

		return *valHash;
	}
	case IR_DIV: {
		BINOP_ARITH(/, success);
	}
	case IR_EQ: {
		BINOP_ARITH(==, success);
	}
	case IR_GE: {
		BINOP_ARITH(>=, success);
	}
	case IR_LE: {
		BINOP_ARITH(<=, success);
	}
	case IR_GT: {
		BINOP_ARITH(>, success);
	}
	case IR_LT: {
		BINOP_ARITH(<, success);
	}
	case IR_INC: {
		__auto_type incoming = graphNodeIRIncomingNodes(node);
		assert(graphNodeIRValuePtr(incoming[0])->type == IR_VALUE);

		__auto_type valHash = valueHash(
		    &((struct IRNodeValue *)graphNodeIRValuePtr(incoming[0]))->val);
		assert(valHash->type == IREVAL_VAL_FLT || valHash->type == IREVAL_VAL_INT);

		if (valHash->type == IREVAL_VAL_INT) {
			valHash->value.i++;
			if (success)
				*success = 1;
		} else if (valHash->type == IREVAL_VAL_FLT) {
			valHash->value.flt++;
			if (success)
				*success = 1;
		} else if (success)
			*success = 0;

		return *valHash;
	}
	case IR_LAND: {
		BINOP_LOG(&, success);
	}
	case IR_LNOT: {
		__auto_type incoming = graphNodeIRIncomingNodes(node);

		__auto_type total = IREvalNode(incoming[0], success);
		if (!success)
			goto fail;

		if (total.type == IREVAL_VAL_INT || total.type == IREVAL_VAL_FLT) {
			if (success)
				*success = 1;
			if (total.type == IREVAL_VAL_INT) {
				total.value.i = !total.value.i;
			} else if (total.type == IREVAL_VAL_FLT) {
				total.value.flt = !total.value.flt;
			}
		} else
			goto fail;
		return total;
	}
	case IR_LOR: {
		BINOP_LOG(|, success);
	}
	case IR_LXOR: {
		BINOP_LOG (^, success);
	}
	case IR_LSHIFT: {
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
		__auto_type incoming = graphNodeIRIncomingNodes(node);
		__auto_type a = IREvalNode(incoming[0], success);
		if (!success)
			goto fail;

		if (a.type == IREVAL_VAL_INT || a.type == IREVAL_VAL_FLT) {
			if (success)
				*success = 1;
		} else
			goto fail;

		if (a.type == IREVAL_VAL_INT)
			a.value.i = -a.value.i;
		if (a.type == IREVAL_VAL_FLT)
			a.value.flt = -a.value.flt;

		return a;
	}
	case IR_POS: {
		__auto_type incoming = graphNodeIRIncomingNodes(node);
		__auto_type a = IREvalNode(incoming[0], success);

		if (a.type == IREVAL_VAL_INT || a.type == IREVAL_VAL_FLT) {
			if (success)
				*success = 1;
		} else
			goto fail;

		return a;
	}
	case IR_TYPECAST: {
		__auto_type incoming = graphNodeIRIncomingNodes(node);

		__auto_type a = IREvalNode(incoming[0], success);
		__auto_type cast = (struct IRNodeTypeCast *)graphNodeIRValuePtr(node);

		// Evaluator only works on ints/floats,it doesnt compute pointer-casts.
		if (a.type != IREVAL_VAL_INT && a.type != IREVAL_VAL_INT) {
			goto fail;
		}

		struct IREvalVal retVal = IREValValIntCreate(0);
		// Int->F64
		if (cast->out == &typeF64 && a.type == IREVAL_VAL_INT) {
			retVal = IREvalValFltCreate(a.value.i);
		} else if (a.type == IREVAL_VAL_INT && cast->in == &typeF64) {
			// Cast is either int or float(can't cast classes/unions),so
			retVal = IREValValIntCreate(a.value.i);
		} else
			goto fail;

		if (success)
			*success = 1;

		return retVal;
	}
	case IR_POW: {
		struct IREvalVal a, b;
		if (!getBinopArgs(node, &a, &b))
			goto fail;

		if (a.type != a.type)
			goto fail;

		struct IREvalVal retVal;
		if (a.type == IREVAL_VAL_INT) {
			if (a.value.i < 0)
				retVal = IREValValIntCreate(intExpS(a.value.i, b.value.i));
			else
				retVal = IREValValIntCreate(intExpU(a.value.i, b.value.i));
		} else if (a.type == IREVAL_VAL_FLT) {
			__auto_type res = pow(a.value.flt, b.value.flt);
			if (success)
				*success = 1;
		}

		if (success)
			*success = 1;
		return retVal;
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
		struct IREvalVal a, b;
		if (!getBinopArgs(node, &a, &b))
			goto fail;

		return *arrayAccessHash(&a, &b, a.type);
	}
	case IR_SIMD:
		goto fail;
	case IR_SPILL: {
		// Expect 1 incoming assign
		strGraphEdgeIRP incoming = graphNodeIRIncoming(node);
		strGraphEdgeIRP assigns = IRGetConnsOfType(incoming, IR_CONN_DEST);
		assert(strGraphEdgeIRPSize(assigns) == 1);

		graphNodeIR incomingNode = graphEdgeIRIncoming(assigns[0]);

		int success2;
		__auto_type val = IREvalNode(incomingNode, &success2);
		if (!success2)
			goto fail;

		struct IRNodeSpill *spill = (void *)graphNodeIRValuePtr(node);
		assert(spill->item.type == IR_VAL_VAR_REF);
		spillToFrame(val, &spill->item.value.var);

		if(success)
				*success=1;
		return val;
	}
	case IR_LOAD: {
		struct IRNodeLoad *load = (void *)graphNodeIRValuePtr(node);
		// Ensure is a variable
		if (load->item.type != IR_VAL_VAR_REF)
			goto fail;

		if (success)
			*success = 1;

		__auto_type find = loadFromFrame(&load->item.value.var);
		if (find)
			return *find;

		return dftValueType(IREVAL_VAL_INT);
	}
	case IR_FUNC_RETURN: {
		strGraphEdgeIRP incoming CLEANUP(strGraphEdgeIRPDestroy) =
		    graphNodeIRIncoming(node);
		if (strGraphEdgeIRPSize(incoming) != 1)
			goto fail;

		if (!IRIsExprEdge(*graphEdgeIRValuePtr(incoming[0])))
			return dftValueType(IREVAL_VAL_INT);

		return IREvalNode(graphEdgeIRIncoming(incoming[0]), success);
	}
	default:
		goto fail;
	}
fail:
	*success = 0;
	return IREValValIntCreate(0);
}
void IREvalSetVarVal(const struct variable *var, struct IREvalVal value) {
	struct IRValue ref;
	ref.type = IR_VAL_VAR_REF;
	ref.value.var.type = IR_VAR_VAR;
	ref.value.var.value.var = (void *)var;
	*valueHash(&ref) = value;
}
static struct IREvalVal
__IREvalPath(graphNodeIR start, struct IREvalVal *currentValue, int *success) {
	if (success)
		*success = 1;

	graphNodeIR endNode = NULL;
	struct IREvalVal retVal;

	graphNodeIR current = start;
	__auto_type nodeValue = graphNodeIRValuePtr(start);
	switch (nodeValue->type) {
	case IR_JUMP_TAB:
	case IR_CHOOSE:
		goto fail;
	case IR_COND_JUMP: {
		if (!currentValue)
			goto fail;

		if (currentValue->type == IREVAL_VAL_PTR) {
			if (currentValue->value.ptr.value != 0)
				goto trueBranch;
			else
				goto falseBranch;
		} else if (currentValue->type == IREVAL_VAL_FLT) {
			if (currentValue->value.flt != .00)
				goto trueBranch;
			else
				goto falseBranch;
		} else if (currentValue->type == IREVAL_VAL_INT) {
			if (currentValue->value.i != 0)
				goto trueBranch;
			else
				goto falseBranch;
		} else {
			goto fail;
		}

	trueBranch : {
		__auto_type outgoing = graphNodeIROutgoing(start);
		__auto_type truePath = IRGetConnsOfType(outgoing, IR_CONN_COND_TRUE);

		int success2;
		__IREvalPath(graphEdgeIROutgoing(truePath[0]), NULL, &success2);

		if (!success2)
			goto fail;

		return dftValueType(IREVAL_VAL_INT);
	}
	falseBranch : {
		__auto_type outgoing = graphNodeIROutgoing(start);
		__auto_type falsePath = IRGetConnsOfType(outgoing, IR_CONN_COND_FALSE);

		int success2;
		__IREvalPath(graphEdgeIROutgoing(falsePath[0]), NULL, &success2);

		if (!success2)
			goto fail;

		return dftValueType(IREVAL_VAL_INT);
	}
	}
	case IR_VALUE: {
		__auto_type end = IREndOfExpr(start);
		int success2;
		retVal = IREvalNode(end, &success2);
		if (!success2)
			goto fail;

		endNode = end;
		goto findNext;
	}
	case IR_LABEL: {
		// If multiple outputs,check if see if they end a shared expression node
		strGraphNodeIRP outgoing = graphNodeIROutgoingNodes(start);

		if (strGraphNodeIRPSize(outgoing) > 1) {
			graphNodeIR commonEnd = IREndOfExpr(outgoing[0]);
			for (long i = 1; i != strGraphNodeIRPSize(outgoing); i++) {
				__auto_type end = IREndOfExpr(outgoing[i]);
				if (commonEnd != end)
					goto fail;
			}

			// All share common end if reached here
			int success2;
			__auto_type value = IREvalNode(commonEnd, &success2);
			retVal = value;
			if (!success2)
				goto fail;

			endNode = commonEnd;
			goto findNext;
		}

		// 1 or less exits so just continue
		endNode = start;
		goto findNext;
	}
	case IR_SPILL: {
		struct IRNodeSpill *value = (void *)nodeValue;
		if (value->item.type != IR_VAL_VAR_REF)
			goto fail;

		spillToFrame(*currentValue, &value->item.value.var);
		retVal = *currentValue;

		endNode = start;
		goto findNext;
	}
	case IR_LOAD: {
		int success2;
		retVal = IREvalNode(start, &success2);
		if (!success2)
			goto fail;

		endNode = IREndOfExpr(start);
		goto findNext;
	}
	case IR_STATEMENT_END:
	case IR_STATEMENT_START:
		endNode = start;
		goto findNext;
	default:;
		// Perhaps is an expression node
		__auto_type end = IREndOfExpr(start);
		int success2;
		retVal = IREvalNode(end, &success2);
		if (!success2)
			goto fail;

		endNode = end;
		goto findNext;
	}
fail : {
	if (success)
		*success = 0;
	return dftValueType(IREVAL_VAL_INT);
}
findNext:;
	strGraphEdgeIRP outgoing = graphNodeIROutgoing(endNode);
	strGraphEdgeIRP flow = IRGetConnsOfType(outgoing, IR_CONN_FLOW);
	long flowCount = strGraphEdgeIRPSize(flow);
	if (flowCount > 1) {
		goto fail;
	} else if (flowCount == 1) {
		return __IREvalPath(graphEdgeIROutgoing(flow[0]), &retVal, success);
	} else {
		return retVal;
	}
}

struct IREvalVal IREvalPath(graphNodeIR start, int *success) {
	return __IREvalPath(start, NULL, success);
}
