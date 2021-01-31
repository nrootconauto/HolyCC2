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
#include <ptrMap.h>
#include <registers.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
static char *ptr2Str(const void *a) {
	return base64Enc((void *)&a, sizeof(a));
}
static int isNoFlow(const void *data, const graphEdgeIR *edge) {
	return *graphEdgeIRValuePtr(*edge) == IR_CONN_NEVER_FLOW;
}
static strGraphEdgeIRP filterNoFlows(strGraphEdgeIRP edges) {
	return strGraphEdgeIRPRemoveIf(edges, NULL, isNoFlow);
}
PTR_MAP_FUNCS(struct parserFunction *, graphNodeIR, Func);
static __thread ptrMapFunc funcs = NULL;
static __thread struct IREvalVal returnValue;
MAP_TYPE_DEF(struct IREvalVal, VarVal);
MAP_TYPE_FUNCS(struct IREvalVal, VarVal);
static __thread mapVarVal varVals = NULL;
struct frameItem {
	struct IRVar *var;
	struct IREvalVal value;
};
LL_TYPE_DEF(struct frameItem, FrameItem);
LL_TYPE_FUNCS(struct frameItem, FrameItem);
MAP_TYPE_DEF(struct IREvalVal, RegVal);
MAP_TYPE_FUNCS(struct IREvalVal, RegVal);
MAP_TYPE_DEF(struct IREvalVal, IREvalVal);
MAP_TYPE_FUNCS(struct IREvalVal, IREvalVal);
static __thread mapIREvalVal pointers;
static __thread strIREvalVal currentFuncArgs = NULL;
static __thread mapRegVal registerValues = NULL;
static __thread llFrameItem frame = NULL;
void IREvalInit() {
	if (varVals)
		mapVarValDestroy(varVals, NULL);
	if (funcs)
		ptrMapFuncDestroy(funcs, NULL);
	if (registerValues)
		mapRegValDestroy(registerValues, NULL);

	registerValues = mapRegValCreate();
	varVals = mapVarValCreate();
	funcs = ptrMapFuncCreate();
}
static struct IREvalVal dftValueType(enum IREvalValType type) {
	struct IREvalVal retVal;
	retVal.valueStoredAt = NULL;
	retVal.type = type;
	switch (type) {
	case IREVAL_VAL_ARRAY: {
		retVal.value.array = NULL;
		break;
	}
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
	case IREVAL_VAL_PTR:;
		break;
	case IREVAL_VAL_CLASS:
		retVal.value.class = NULL;
	}

	return retVal;
}
static void IREvalValClone(struct IREvalVal *dst, const struct IREvalVal *src) {
	if (src->type != IREVAL_VAL_CLASS) {
		*dst = *src;
		return;
	}
	mapIREvalMembersClone(src->value.class, (void (*)(void *, const void *))IREvalValClone);
}
static void IREvalValAssign(struct IREvalVal *dst, const struct IREvalVal *src) {
	IREvalValDestroy(dst);
	IREvalValClone(dst, src);
	dst->valueStoredAt = dst;
}
static struct IREvalVal *arrayAccessHash(struct IREvalVal *array, struct IREvalVal *index, enum IREvalValType type) {
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
static int llFrameItemGetPred(const struct IRVar *a, const struct frameItem *item) {
	return IRVarCmpIgnoreVersion(a, item->var);
}
static int llFrameItemInsertPred(const struct frameItem *a, const struct frameItem *b) {
	return IRVarCmpIgnoreVersion(a->var, b->var);
}
typedef int (*frameItemGetPredType)(const void *, const struct frameItem *);
static struct IREvalVal *valueHash(struct IRValue *value) {
	if (value->type == IR_VAL_REG) {
	registerValueLoop:;
		__auto_type regValue = mapRegValGet(registerValues, value->value.reg.reg->name);
		if (!regValue) {
			mapRegValInsert(registerValues, value->value.reg.reg->name, dftValueType(IREVAL_VAL_INT));
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
	__auto_type find = llFrameItemFind(frame, frameVar, (frameItemGetPredType)llFrameItemGetPred);
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
	__auto_type find = llFrameItemFind(frame, frameVar, (frameItemGetPredType)llFrameItemGetPred);
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
	val.valueStoredAt = NULL;

	return val;
}
struct IREvalVal IREvalValFltCreate(double f) {
	struct IREvalVal val;
	val.type = IREVAL_VAL_FLT;
	val.value.flt = f;
	val.valueStoredAt = NULL;

	return val;
}
static struct IREvalVal maskInt2Width(struct IREvalVal input, int width, int *success) {
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
static int getBinopArgs(graphNodeIR node, struct IREvalVal *arg1, struct IREvalVal *arg2) {
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

#define BINOP_BIT(op, successPtr)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              \
	({                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           \
		struct IREvalVal a, b;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                     \
		if (!getBinopArgs(node, &a, &b))                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           \
			if (success)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             \
				*successPtr = 0;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
		assert(a.type == b.type);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
		if (a.type == IREVAL_VAL_INT) {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            \
			if (success)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             \
				*successPtr = 1;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
			return IREValValIntCreate(a.value.i op b.value.i);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
		} else {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
			if (success)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             \
				*successPtr = 0;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
			return IREValValIntCreate(0);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            \
		}                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          \
	})
#define BINOP_ARITH(op, successPtr)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            \
	({                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           \
		struct IREvalVal a, b;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                     \
		if (!getBinopArgs(node, &a, &b))                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           \
			if (success)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             \
				*successPtr = 0;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
		assert(a.type == b.type);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
		if (a.type == IREVAL_VAL_INT) {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            \
			if (success)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             \
				*successPtr = 1;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
			return IREValValIntCreate(a.value.i op b.value.i);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
		} else if (a.type == IREVAL_VAL_FLT) {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                     \
			if (success)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             \
				*successPtr = 1;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
			return IREvalValFltCreate(a.value.flt op b.value.flt);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
		} else {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
			if (success)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             \
				*successPtr = 0;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
			return IREValValIntCreate(0);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            \
		}                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          \
	})
#define BINOP_LOG(op, successPtr)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              \
	({                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           \
		struct IREvalVal a, b;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                     \
		if (!getBinopArgs(node, &a, &b))                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           \
			if (success)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             \
				*successPtr = 0;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
		assert(a.type == b.type);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
		if (a.type == IREVAL_VAL_INT) {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            \
			if (success)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             \
				*successPtr = 1;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
			return IREValValIntCreate(!!(a.value.i)op !!(a.value.i));                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                \
		} else if (a.type == IREVAL_VAL_FLT) {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                     \
			if (success)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             \
				*successPtr = 1;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
			return IREvalValFltCreate(!!(a.value.flt)op !!(a.value.flt));                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            \
		} else {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
			if (success)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             \
				*successPtr = 0;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
			return IREValValIntCreate(0);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            \
		}                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          \
	})
static struct IREvalVal evalIRCallFunc(graphNodeIR funcStart, strIREvalVal args, int *success) {
	if (success)
		*success = 1;

	// Ensure 1 outgoing connection to start of function
	strGraphEdgeIRP out CLEANUP(strGraphEdgeIRPDestroy) = graphNodeIROutgoing(funcStart);
	strGraphEdgeIRP outFlow CLEANUP(strGraphEdgeIRPDestroy) = IRGetConnsOfType(out, IR_CONN_FLOW);
	assert(strGraphEdgeIRPSize(outFlow) == 1);

	// Store old currentFuncArgs for "push/pop"
	__auto_type old = currentFuncArgs;
	currentFuncArgs = args; //"Push"
	IREvalPath(graphEdgeIROutgoing(outFlow[0]), success);
	currentFuncArgs = old; //"Pop"
	return returnValue;
}
struct IREvalVal IREvalNode(graphNodeIR node, int *success) {
	struct IRNode *ir = graphNodeIRValuePtr(node);
	if (success)
		*success = 1;
	switch (ir->type) {
	case IR_FUNC_ARG: {
		struct IRNodeFuncArg *arg = (void *)graphNodeIRValuePtr(node);
		assert(arg->argIndex < strIREvalValSize(currentFuncArgs));
		return currentFuncArgs[arg->argIndex];
	}
	case IR_VALUE: {
		// Check for incoming assign
		int assignedInto = 0;
		strGraphEdgeIRP incoming CLEANUP(strGraphEdgeIRPDestroy) = graphNodeIRIncoming(node);
		strGraphEdgeIRP assign CLEANUP(strGraphEdgeIRPDestroy) = IRGetConnsOfType(incoming, IR_CONN_DEST);
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
				IREvalValAssign(valueHash(value), &res);
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
				IREvalValAssign(valueHash(value), &res);

			return *valueHash(value);
		}
		default:
			goto fail;
		}
	}
	case IR_ADD: {
		BINOP_ARITH(+, success);
	}
	case IR_SUB: {
		BINOP_ARITH(-, success);
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
		BINOP_BIT(^, success);
	}
	case IR_CHOOSE:
		assert(0);
	case IR_DEC: {
		__auto_type incoming = graphNodeIRIncomingNodes(node);
		assert(graphNodeIRValuePtr(incoming[0])->type == IR_VALUE);

		__auto_type valHash = valueHash(&((struct IRNodeValue *)graphNodeIRValuePtr(incoming[0]))->val);
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

		__auto_type valHash = valueHash(&((struct IRNodeValue *)graphNodeIRValuePtr(incoming[0]))->val);
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
		BINOP_LOG(^, success);
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
		struct IRNodeFuncCall *call = (void *)graphNodeIRValuePtr(node);
		strIREvalVal args CLEANUP(strIREvalValDestroy) = strIREvalValResize(NULL, strGraphNodeIRPSize(call->incomingArgs));
		for (long i = 0; i != strGraphNodeIRPSize(call->incomingArgs); i++) {
			int success2;
			args[i] = IREvalNode(call->incomingArgs[i], &success2);
			if (!success2)
				goto fail;
		}
		strGraphEdgeIRP incoming CLEANUP(strGraphEdgeIRPDestroy) = graphNodeIRIncoming(node);
		strGraphEdgeIRP inFuncs CLEANUP(strGraphEdgeIRPDestroy) = IRGetConnsOfType(incoming, IR_CONN_FUNC);
		assert(strGraphEdgeIRPSize(inFuncs) == 1);
		int success2;
		__auto_type func = (struct IRNodeValue *)graphNodeIRValuePtr(graphEdgeIRIncoming(inFuncs[0]));
		assert(func->base.type == IR_VALUE);
		assert(func->val.type == IR_VAL_FUNC);
		__auto_type find = ptrMapFuncGet(funcs, func->val.value.func);
		assert(find);
		__auto_type retVal = evalIRCallFunc(*find, args, &success2);
		if (!success2)
			goto fail;
		return retVal;
	}
	case IR_ARRAY_ACCESS: {
		struct IREvalVal a, b;
		if (!getBinopArgs(node, &a, &b))
			goto fail;
		if (a.type != IREVAL_VAL_ARRAY || b.type == IREVAL_VAL_INT || b.value.i < 0)
			goto fail;
		if (strIREvalValSize(a.value.array) > b.value.i)
			goto fail;
		IREvalValAssign(&a.value.array[b.value.i], &a);
		return a.value.array[b.value.i];
	}
	case IR_SIMD:
		goto fail;
	case IR_SPILL_LOAD: {
		struct IRNodeSpill *spill = (void *)graphNodeIRValuePtr(node);
		// Expect 1 incoming assign
		strGraphEdgeIRP incoming = graphNodeIRIncoming(node);
		strGraphEdgeIRP assigns = IRGetConnsOfType(incoming, IR_CONN_DEST);
		if (strGraphEdgeIRPSize(assigns) == 1) {
			graphNodeIR incomingNode = graphEdgeIRIncoming(assigns[0]);
			int success2;
			__auto_type val = IREvalNode(incomingNode, &success2);
			if (!success2)
				goto fail;
			assert(spill->item.type == IR_VAL_VAR_REF);
			spillToFrame(val, &spill->item.value.var);
		}

		if (success)
			*success = 1;
		return *loadFromFrame(&spill->item.value.var);
	}
	case IR_FUNC_RETURN: {
		strGraphEdgeIRP incoming CLEANUP(strGraphEdgeIRPDestroy) = graphNodeIRIncoming(node);
		if (strGraphEdgeIRPSize(incoming) != 1)
			goto fail;

		if (!IRIsExprEdge(*graphEdgeIRValuePtr(incoming[0])))
			return dftValueType(IREVAL_VAL_INT);

		return returnValue = IREvalNode(graphEdgeIRIncoming(incoming[0]), success);
	}
	case IR_STATEMENT_START:
	case IR_STATEMENT_END:
	case IR_COND_JUMP:
	case IR_JUMP_TAB:
	case IR_LABEL:
	case IR_FUNC_START:
	case IR_FUNC_END:
	case IR_SUB_SWITCH_START_LABEL:
		goto fail;
	case IR_ADDR_OF: {
		strGraphEdgeIRP incoming CLEANUP(strGraphEdgeIRPDestroy) = graphNodeIRIncoming(node);
		__auto_type value = IREvalNode(graphEdgeIRIncoming(incoming[0]), success);
		struct IREvalVal retVal;
		retVal.type = IREVAL_VAL_PTR;
		retVal.value.ptr = value.valueStoredAt;
		retVal.valueStoredAt = NULL;

		return retVal;
	}
	case IR_DERREF: {
		strGraphEdgeIRP incoming CLEANUP(strGraphEdgeIRPDestroy) = graphNodeIRIncoming(node);
		strGraphEdgeIRP inSource CLEANUP(strGraphEdgeIRPDestroy) = IRGetConnsOfType(inSource, IR_CONN_SOURCE_A);
		strGraphEdgeIRP inAssign CLEANUP(strGraphEdgeIRPDestroy) = IRGetConnsOfType(inSource, IR_CONN_DEST);

		__auto_type source = IREvalNode(graphEdgeIRIncoming(inSource[0]), success);
		if (source.valueStoredAt == NULL)
			goto fail;

		if (strGraphEdgeIRPSize(inAssign)) {
			__auto_type value = IREvalNode(graphEdgeIRIncoming(inAssign[0]), success);
			IREvalValAssign(source.valueStoredAt, &value);
		}

		return *source.valueStoredAt;
	}
	case IR_MEMBERS: {
		strGraphEdgeIRP incoming CLEANUP(strGraphEdgeIRPDestroy) = graphNodeIRIncoming(node);
		strGraphEdgeIRP inSource CLEANUP(strGraphEdgeIRPDestroy) = IRGetConnsOfType(inSource, IR_CONN_SOURCE_A);
		strGraphEdgeIRP inAssign CLEANUP(strGraphEdgeIRPDestroy) = IRGetConnsOfType(inSource, IR_CONN_DEST);
		int success2;
		__auto_type source = IREvalNode(graphEdgeIRIncoming(inSource[0]), &success2);
		if (!success2)
			goto fail;
		struct IRNodeMembers *members = (void *)graphNodeIRValuePtr(node);
		struct IREvalVal *member = &source;
		for (long i = 0; i != strObjectMemberSize(members->members); i++) {
			if (member->valueStoredAt == NULL)
				goto fail;
			if (member->type != IREVAL_VAL_CLASS)
				goto fail;
			member = mapIREvalMembersGet(member->value.class, members->members[i].name);
			if (member == NULL)
				goto fail;
		}

		if (strGraphEdgeIRPSize(inAssign)) {
			int success2;
			__auto_type value = IREvalNode(graphEdgeIRIncoming(inAssign[0]), &success2);
			if (!success2)
				goto fail;
			IREvalValAssign(source.valueStoredAt, &value);
		}

		return *member;
	}
	}
fail:
	*success = 0;
	return IREValValIntCreate(0);
}
void IREvalSetVarVal(const struct parserVar *var, struct IREvalVal value) {
	struct IRValue ref;
	ref.type = IR_VAL_VAR_REF;
	ref.value.var.type = IR_VAR_VAR;
	ref.value.var.value.var = (void *)var;
	*valueHash(&ref) = value;
}
static struct IREvalVal __IREvalPath(graphNodeIR start, struct IREvalVal *currentValue, int *success) {
	if (success)
		*success = 1;

	graphNodeIR endNode = NULL;
	struct IREvalVal retVal;

	graphNodeIR current = start;
	__auto_type nodeValue = graphNodeIRValuePtr(start);
	switch (nodeValue->type) {
	case IR_JUMP_TAB: {
		struct IRNodeJumpTable *table = (void *)graphNodeIRValuePtr(start);
		struct IREvalVal inputValue;
		if (!currentValue) {
			strGraphEdgeIRP in CLEANUP(strGraphEdgeIRPDestroy) = graphNodeIRIncoming(start);
			strGraphEdgeIRP inCond CLEANUP(strGraphEdgeIRPDestroy) = IRGetConnsOfType(in, IR_CONN_SOURCE_A);
			assert(strGraphEdgeIRPSize(inCond) == 1);
			int success2;
			inputValue = IREvalNode(graphEdgeIRIncoming(inCond[0]), &success2);
			if (!success2)
				goto fail;
			currentValue = &inputValue;
		}
		assert(currentValue->type == IREVAL_VAL_INT);
		__auto_type have = currentValue->value.i;
		for (long i = 0; i != strIRTableRangeSize(table->labels); i++) {
			if (table->labels[i].start <= have && have < table->labels[i].end) {
				return IREvalPath(table->labels[i].to, success);
			}
		}
		strGraphEdgeIRP out CLEANUP(strGraphEdgeIRPDestroy) = graphNodeIROutgoing(start);
		strGraphEdgeIRP outDft CLEANUP(strGraphEdgeIRPDestroy) = IRGetConnsOfType(out, IR_CONN_DFT);
		assert(strGraphEdgeIRPSize(outDft) == 1);
		return IREvalPath(graphEdgeIROutgoing(outDft[0]), success);
	}
	case IR_CHOOSE:
		goto fail;
	case IR_COND_JUMP: {
		struct IREvalVal inputValue;
		if (!currentValue) {
			strGraphEdgeIRP in CLEANUP(strGraphEdgeIRPDestroy) = graphNodeIRIncoming(start);
			strGraphEdgeIRP inArg CLEANUP(strGraphEdgeIRPDestroy) = IRGetConnsOfType(in, IR_CONN_SOURCE_A);
			assert(strGraphEdgeIRPSize(inArg) == 1);
			int success2;
			inputValue = IREvalNode(graphEdgeIRIncoming(inArg[0]), &success2);
			if (!success2)
				goto fail;
			currentValue = &inputValue;
		}

		if (currentValue->type == IREVAL_VAL_PTR) {
			if (currentValue->value.ptr != NULL)
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
		__auto_type retVal = __IREvalPath(graphEdgeIROutgoing(truePath[0]), NULL, &success2);

		if (!success2)
			goto fail;

		return retVal;
	}
	falseBranch : {
		__auto_type outgoing = graphNodeIROutgoing(start);
		__auto_type falsePath = IRGetConnsOfType(outgoing, IR_CONN_COND_FALSE);

		int success2;
		__auto_type retVal = __IREvalPath(graphEdgeIROutgoing(falsePath[0]), NULL, &success2);

		if (!success2)
			goto fail;

		return retVal;
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
		strGraphEdgeIRP outgoing CLEANUP(strGraphEdgeIRPDestroy) = graphNodeIROutgoing(start);
		outgoing = filterNoFlows(outgoing);

		if (strGraphEdgeIRPSize(outgoing) > 1) {
			graphNodeIR commonEnd = IREndOfExpr(graphEdgeIROutgoing(outgoing[0]));
			for (long i = 1; i != strGraphEdgeIRPSize(outgoing); i++) {
				__auto_type end = IREndOfExpr(graphEdgeIROutgoing(outgoing[i]));
				if (commonEnd != end)
					goto fail;
			}

			// All share common end if reached here
			int success2;
			// Check if points to jump-table or conditional jump
			if (graphNodeIRValuePtr(commonEnd)->type == IR_COND_JUMP) {
				return __IREvalPath(commonEnd, NULL, success);
			} else if (graphNodeIRValuePtr(commonEnd)->type == IR_JUMP_TAB) {
				return __IREvalPath(commonEnd, NULL, success);
			}
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
	case IR_SPILL_LOAD: {
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
	case IR_FUNC_START: {
		struct IRNodeFuncStart *func = (void *)graphNodeIRValuePtr(start);
		ptrMapFuncAdd(funcs, func->func, start);
		endNode = func->end;
		goto findNext;
	}
	case IR_FUNC_RETURN: {
		return returnValue = *currentValue;
	}
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
	strGraphEdgeIRP outgoing CLEANUP(strGraphEdgeIRPDestroy) = graphNodeIROutgoing(endNode);
	outgoing = filterNoFlows(outgoing);
	long flowCount = strGraphEdgeIRPSize(outgoing);
	if (flowCount > 1) {
		goto fail;
	} else if (flowCount == 1) {
		return __IREvalPath(graphEdgeIROutgoing(outgoing[0]), &retVal, success);
	} else {
		return retVal;
	}
}

struct IREvalVal IREvalPath(graphNodeIR start, int *success) {
	return __IREvalPath(start, NULL, success);
}
void IREvalValDestroy(struct IREvalVal *val) {
	if (val->type == IREVAL_VAL_CLASS) {
		mapIREvalMembersDestroy(val->value.class, (void (*)(void *))IREvalValDestroy);
	} else if (val->type == IREVAL_VAL_ARRAY) {
		for (long i = 0; i != strIREvalValSize(val->value.array); i++)
			IREvalValDestroy(&val->value.array[i]);
		strIREvalValDestroy(&val->value.array);
	}
}
int IREvalValEqual(const struct IREvalVal *a, const struct IREvalVal *b) {
	if (a->type != b->type)
		return 0;
	switch (a->type) {
	case IREVAL_VAL_ARRAY: {
		if (strIREvalValSize(a->value.array) != strIREvalValSize(a->value.array))
			return 0;
		for (long i = 0; i != strIREvalValSize(a->value.array); i++)
			if (!IREvalValEqual(&a->value.array[i], &b->value.array[i]))
				return 0;
		return 1;
	}
	case IREVAL_VAL_CLASS: {
		long count;
		mapIREvalMembersKeys(a->value.class, NULL, &count);
		long bCount;
		mapIREvalMembersKeys(a->value.class, NULL, &bCount);
		if (count != bCount)
			return 0;
		const char *keysA[count];
		mapIREvalMembersKeys(a->value.class, keysA, NULL);
		const char *keysB[count];
		mapIREvalMembersKeys(b->value.class, keysB, NULL);
		for (long i = 0; i != count; i++) {
			__auto_type aFind = mapIREvalMembersGet(a->value.class, keysA[i]);
			__auto_type bFind = mapIREvalMembersGet(b->value.class, keysA[i]);
			if (!aFind || !bFind)
				return 0;
			if (!IREvalValEqual(aFind, bFind))
				return 0;
		}

		return 1;
	}
	case IREVAL_VAL_DFT:
		return 0;
	case IREVAL_VAL_FLT:
		return a->value.flt == b->value.flt;
	case IREVAL_VAL_PTR:
		return a->value.ptr == b->value.ptr;
	case IREVAL_VAL_INT:
		return a->value.i == b->value.i;
	case IREVAL_VAL_REG:
		return a->value.reg == b->value.reg;
	case IREVAL_VAL_VAR:
		return 0; // Variables should reduce to values
	}
}
