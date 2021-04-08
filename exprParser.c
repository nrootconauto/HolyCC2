#include <assert.h>
#include "diagMsg.h"
#include "hashTable.h"
#include "parserA.h"
#include "parserB.h"
#include "registers.h"
#include <stdio.h>
#include <stdlib.h>
struct object *assignTypeToOp(const struct parserNode *node);
static int isArith(const struct object *type) {
		type=objectBaseType(type);
		if (type==&typeBool||type == &typeU8i || type == &typeU16i || type == &typeU32i || type == &typeU64i || type == &typeI8i || type == &typeI16i || type == &typeI32i ||
	    type == &typeI64i || type == &typeF64 || type->type == TYPE_PTR || type->type == TYPE_ARRAY) {
		return 1;
	}
	return 0;
}
MAP_TYPE_DEF(void *, Set);
MAP_TYPE_FUNCS(void *, Set);
static mapSet assignOps = NULL;
static mapSet incOps = NULL;
void initAssignOps();
void initAssignOps() {
	const char *assignOps2[] = {
	    "=",
	    //
	    "-=",
	    "+=",
	    //
	    "*=",
	    "/=",
	    "%=",
	    //
	    "<<=",
	    ">>=",
	    "&=",
	    "^=",
	    "|=",
	};
	long count = sizeof(assignOps2) / sizeof(*assignOps2);
	assignOps = NULL;
	assignOps = mapSetCreate();
	for (long i = 0; i != count; i++)
		mapSetInsert(assignOps, assignOps2[i], NULL);

	const char *incs[] = {
	    "++",
	    "--",
	};
	count = sizeof(incs) / sizeof(*incs);
	incOps = mapSetCreate();
	for (long i = 0; i != count; i++)
		mapSetInsert(incOps, incs[i], NULL);
}
static int isAssignOp(const struct parserNode *op) {
	struct parserNodeOpTerm *op2 = (void *)op;
	assert(op2->base.type == NODE_OP);
	return NULL != mapSetGet(assignOps, op2->text);
}
static int objIndex(const struct object **objs, long count, const struct object *obj) {
	for (long i = 0; i != count; i++)
		if (obj == objs[i])
			return i;

	assert(0);
	return -1;
}
#define inRange(exp, min, max) ((exp) >= min && (exp) <= max)
static struct object *intLitType(struct parserNodeLitInt *i) {
	if (i->value.type == INT_SLONG) {
		__auto_type val = i->value.value.sLong;
		if (inRange(val, INT8_MIN, INT8_MAX))
			return &typeI8i;
		if (inRange(val, INT16_MIN, INT16_MAX))
			return &typeI16i;
		if (inRange(val, INT32_MIN, INT32_MAX))
			return &typeI32i;
		if (inRange(val, INT64_MIN, INT64_MAX))
			return &typeI64i;
	}
	if (i->value.type == INT_ULONG) {
		__auto_type val = i->value.value.uLong;
		if (inRange(val, 0, UINT8_MAX))
			return &typeU8i;
		if (inRange(val, 0, UINT16_MAX))
			return &typeU16i;
		if (inRange(val, 0, UINT32_MAX))
			return &typeU32i;
		if (inRange(val, 0, UINT64_MAX))
			return &typeU64i;
	}
	assert(0);
	return &typeI64i;
}
static struct object *promotionType(const struct object *a, const struct object *b) {
		a=objectBaseType(a);
		b=objectBaseType(b);
		
		if(a->type==TYPE_PTR||a->type==TYPE_ARRAY)
				return (struct object*)a;
		if(b->type==TYPE_PTR||b->type==TYPE_ARRAY)
				return (struct object*)b;
		
		const struct object *ranks[] = {
			&typeU0, &typeBool,&typeI8i, &typeU8i, &typeI16i, &typeU16i, &typeI32i, &typeU32i, &typeI64i, &typeU64i, &typeF64,
	};
	long count = sizeof(ranks) / sizeof(*ranks);
	long I32Rank = objIndex(ranks, count, dftValType());

	long aRank = objIndex(ranks, count, a);
	long bRank = objIndex(ranks, count, b);
	if (aRank < I32Rank)
		aRank = I32Rank;
	if (bRank < I32Rank)
		bRank = I32Rank;

	if (aRank == bRank)
		return (struct object *)ranks[aRank];
	else if (aRank > bRank)
		return (struct object *)ranks[aRank];
	else if (aRank < bRank)
		return (struct object *)ranks[bRank];

	return NULL;
}
static struct parserNode *promoteIfNeeded(struct parserNode *node, struct object *toType) {
		/*
				if (assignTypeToOp(node) != toType) {
				struct parserNodeTypeCast *cast = calloc(sizeof(struct parserNodeTypeCast));
				cast->base.type = NODE_TYPE_CAST;
				cast->exp = node;
				cast->type = toType;
				
				return (void *)cast;
				}
		*/

	return node;
}
static void noteItem(struct parserNode *node) {
		long _start,_end;
		parserNodeStartEndPos(node->pos.start, node->pos.end, &_start, &_end);
		if (node->type == NODE_FUNC_REF) {
				struct parserNodeFuncRef *ref = (void *)node;
				const struct parserFunction *func = ref->func;
				
				__auto_type from = func->refs[0];
				diagNoteStart(_start, _end);
				diagPushText("Function declared here:");
				diagHighlight(_start, _end);
				diagEndMsg();
	} else if (node->type == NODE_VAR) {
		struct parserNodeVar *var = (void *)node;
		__auto_type from = var->var->refs[0];

		diagNoteStart(_start, _end);
		diagPushText("Variable declared here:");
		diagHighlight(_start, _end);
		diagEndMsg();
	}
}
static void validatePtrPass(struct object *expected,struct parserNode *node) {
		long _start,_end;
		parserNodeStartEndPos(node->pos.start, node->pos.end, &_start, &_end);
		
		__auto_type type=assignTypeToOp(node);
		if(objectEqual(expected,objectPtrCreate(&typeU0))||objectEqual(type,objectPtrCreate(&typeU0))) {
				if(type->type==TYPE_CLASS||type->type==TYPE_UNION)
						goto invalidPass;
		} else {
				if(!objectEqual(expected,type)) {
						__auto_type t=objectBaseType(type)->type;
						int isNumeric=t==TYPE_I8i||t==TYPE_I16i||t==TYPE_I32i||t==TYPE_I64i||t==TYPE_U8i||t==TYPE_U16i||t==TYPE_U32i||t==TYPE_U64i;
						if(!isNumeric) {
						invalidPass:
								diagNoteStart(_start, _end);

								char *expStr=object2Str(expected);
								char *gotStr=object2Str(objectBaseType(type));
								const char *fmt="Passing type %s to type %s.";
								long len=snprintf(NULL,0, fmt, gotStr,expStr);
								char buffer[len+1];
								snprintf(buffer, len+1, fmt, gotStr,expStr);
						
								diagPushText(buffer);
								free(gotStr),free(expStr);
								diagEndMsg(); 
						}
				}
		}
}
static void incompatTypes(struct parserNode *node, struct object *expected) {
		long _start,_end;
		parserNodeStartEndPos(node->pos.start, node->pos.end, &_start, &_end);

		
	char *haveName = NULL, *expectedName = NULL;
	haveName = object2Str(assignTypeToOp(node));
	expectedName = object2Str(expected);

	char buffer[1024];
	diagErrorStart(_start, _end);
	sprintf(buffer, "Incompatible types '%s' and '%s'.", haveName, expectedName);
	diagPushText(buffer);
	diagHighlight(_start, _end);
	diagEndMsg();
}
static int isPtr(struct object *type) {
		__auto_type base=objectBaseType(type);
		return base->type==TYPE_PTR||base->type==TYPE_ARRAY;
}
struct object *assignTypeToOp(const struct parserNode *node) {
		long _start,_end;
		parserNodeStartEndPos(node->pos.start, node->pos.end, &_start, &_end);
		
		if(node->type == NODE_RANGES) {
				struct parserNodeRanges *ranges=(void*)node;
				for(long e=0;e!=strParserNodeSize(ranges->exprs);e++) {
						if(!isArith(assignTypeToOp(ranges->exprs[e]))) {
								long start,end;
								parserNodeStartEndPos(ranges->exprs[e]->pos.start, ranges->exprs[e]->pos.end, &start, &end);
								diagErrorStart(start, end);
								diagPushText("Expected arithmetic type:");
								diagHighlight(start, end);
								diagEndMsg();
						}
				}
				return dftValType();
		} else if(node->type == NODE_NAME) {
				diagErrorStart(_start, _end);
				diagPushText("Unknown symbol ");
				diagPushQoutedText(_start, _end);
				diagPushText(".");
				diagEndMsg();
		} else if (node->type == NODE_FUNC_REF) {
		struct parserNodeFuncRef *ref = (void *)node;
		return ref->func->type;
	} else if (node->type == NODE_VAR) {
		struct parserNodeVar *var = (void *)node;
		return var->var->type;
	} else if (node->type == NODE_COMMA_SEQ) {
		struct parserNodeCommaSeq *seq = (void *)node;
		if (seq->type)
			return seq->type;

		long len = strParserNodeSize(seq->items);
		for (long i = 0; i != len; i++)
			if (seq->items[i])
				assignTypeToOp(seq->items[i]);

		if (len)
			if (seq->items[len - 1] != NULL) {
				seq->type = assignTypeToOp(seq->items[len - 1]);
				return seq->type;
			}

		// else if no last type,use U0
		seq->type = &typeU0;
		return &typeU0;
	}
	if (node->type == NODE_MEMBER_ACCESS) {
		struct parserNodeMemberAccess *access = (void *)node;
		__auto_type aType = assignTypeToOp(access->exp);
		struct parserNodeOpTerm *op = (void *)access->op;
		if (0 == strcmp(op->text, ".") || 0 == strcmp(op->text, "->")) {
			if (aType->type != TYPE_PTR && 0 == strcmp(op->text, "->")) {
					long _ostart,_oend;
					parserNodeStartEndPos(op->base.pos.start, op->base.pos.end, &_ostart, &_oend);
		
				diagErrorStart(_ostart, _oend);
				diagPushQoutedText(_ostart, _oend);
				diagPushText(" needs a pointer operand.");
				diagEndMsg();
				goto fail;
			}
			if (aType->type == TYPE_PTR)
				aType = ((struct objectPtr *)aType)->type;

			struct parserNodeName *nm = (void *)access->name;
			if(aType->type==TYPE_FORWARD)
					aType=objectBaseType(aType);
			if (aType->type != TYPE_CLASS && aType->type != TYPE_UNION) {
			failMember:;
					long _ostart,_oend;
					parserNodeStartEndPos(op->base.pos.start, op->base.pos.end, &_ostart, &_oend);
		
					diagErrorStart(_ostart,_oend);
				diagPushText("Type ");
				diagPushText(aType->name);
				diagPushText(" doesn't have member ");
					long _nstart,_nend;
					parserNodeStartEndPos(nm->base.pos.start, nm->base.pos.end, &_nstart, &_nend);
				diagPushQoutedText(_nstart, _nend);
				diagPushText(".");
				diagEndMsg();
				goto fail;
			}
			__auto_type member=objectMemberGet(aType,(void*)nm);
			if (member == NULL)
				goto failMember;

			return member->type;
		}
	}
	if (node->type == NODE_BINOP) {
		struct parserNodeBinop *binop = (void *)node;
		struct parserNodeOpTerm *op = (void *)binop->op;
		if (0 == strcmp(op->text, "=")) {
			__auto_type aType = assignTypeToOp(binop->a);
			__auto_type base = objectBaseType(aType);
			if (base->type == TYPE_CLASS || base->type == TYPE_UNION) {
				__auto_type bType = objectBaseType(assignTypeToOp(binop->b));
				if (aType != bType)
					goto binopInvalid;
				return base;
			} else if(base->type==TYPE_PTR) {
					validatePtrPass(base, binop->b);
			}
		}
		if (binop->type != NULL)
			return binop->type;

		__auto_type aType = assignTypeToOp(binop->a);
		__auto_type bType = assignTypeToOp(binop->b);
		
		int aArih = isArith(aType);
		int bArih = isArith(bType);
		if (aArih && bArih) {
				//Whine is ptr arith between incompatible types.
				int aPtr=(objectBaseType(aType)->type==TYPE_PTR);
				int bPtr=(objectBaseType(bType)->type==TYPE_PTR);
				if(aPtr&&bPtr) {
						validatePtrPass(aType, binop->b);
				}
				
			// Dont promote left value on assign
			if (isAssignOp(binop->op)) {
				binop->b = promoteIfNeeded(binop->b, assignTypeToOp(binop->a));

				binop->type = assignTypeToOp(binop->a);
				return binop->type;
			}
			// Not an assign
			__auto_type resType = promotionType(aType, bType);
			binop->b = promoteIfNeeded(binop->b, resType);
			binop->a = promoteIfNeeded(binop->a, resType);

			binop->type = resType;
			return binop->type;
		} else if (aArih || bArih) {
		binopInvalid:;
			// Both are no compatible with arithmetic operators,so if one isnt,whine
			struct parserNodeOpTerm *op = (void *)binop->op;
			assert(op->base.type == NODE_OP);

				long _ostart,_oend;
					parserNodeStartEndPos(op->base.pos.start, op->base.pos.end, &_ostart, &_oend);
			diagErrorStart(_ostart, _oend);
			diagPushText("Invalid operands to operator ");
			diagPushQoutedText(_ostart,_oend);
			char buffer[1024];
			char *aName = object2Str(aType), *bName = object2Str(bType);
			sprintf(buffer, ". Operands are type '%s' and '%s'.", aName, bName);
			diagPushText(buffer);
			diagHighlight(_ostart,_oend);
			diagEndMsg();

			noteItem(binop->a);
			noteItem(binop->b);

			// Dummy value
			binop->type = &typeI64i;
			return &typeI64i;
		} else {
			// Both are non-arithmetic
			goto binopInvalid;
		}
	} else if (node->type == NODE_UNOP) {
		struct parserNodeUnop *unop = (void *)node;
		if (unop->type)
			return unop->type;

		__auto_type aType = assignTypeToOp(unop->a);
		
			struct parserNodeOpTerm *op = (void *)unop->op;
				//
			// Check if "&" on function(which is valid),used for function ptr's
			//
			if (0 == strcmp(op->text, "*")) {
				if (aType->type == TYPE_PTR) {
					struct objectPtr *ptr = (void *)aType;
					return ptr->type;
				} else if (aType->type == TYPE_ARRAY) {
					struct objectArray *arr = (void *)aType;
					return arr->type;
				} else {
					diagErrorStart(_start, _end);
					diagPushText("Attempting to der-refference a non-pointer/array type.");
					diagEndMsg();
					goto fail;
				}
			} else if (0 == strcmp(op->text, "&")) {
				// Make a func-ptr
				__auto_type ptr = objectPtrCreate(aType);
				unop->type = ptr;
				return ptr;
			} else if(!isArith(aType)) {
				// Not a function pointer or arithmetic
			invalidUnop:;

				assert(op->base.type == NODE_OP);

					long _ostart,_oend;
					parserNodeStartEndPos(op->base.pos.start, op->base.pos.end, &_ostart, &_oend);
				diagErrorStart(_ostart, _oend);
				diagPushText("Invalid operands to operator.");
				diagHighlight(_ostart,_oend);

				char buffer[1024];
				char *name = object2Str(aType);
				sprintf(buffer, "Operand is of type '%s'.", name);
				diagPushText(buffer);
				diagEndMsg();

				noteItem(unop->a);

				// Dummy value
				unop->type = &typeI64i;
				return &typeI64i;
			}
			
		// Promote,but...
		// Dont promote if inc/dec
		if (!mapSetGet(incOps, op->text))
			unop->a = promoteIfNeeded(unop->a, dftValType());

		unop->type = assignTypeToOp(unop->a);
		return unop->type;
	} else if (node->type == NODE_FUNC_CALL) {
		struct parserNodeFuncCall *call = (void *)node;
					long _cstart,_cend;
					parserNodeStartEndPos(call->base.pos.start, call->base.pos.end, &_cstart, &_cend);
					
		__auto_type funcType = assignTypeToOp(call->func);
		if(funcType->type==TYPE_PTR)
				funcType=((struct objectPtr*)funcType)->type;
		if (funcType->type != TYPE_FUNCTION) {
		nonCallable:;
			// Isn't callable
					diagErrorStart(_cstart,_cend);
			char buffer[1024];
			char *typeName = object2Str(funcType);
			sprintf(buffer, "Type '%s' isn't callable.", typeName);
			diagHighlight(_cstart,_cend);
			diagPushText(buffer);
			diagEndMsg();
			noteItem(call->func);

			call->type = &typeI64i;
			return &typeI64i;
		} else {
			struct objectFunction *func = (void *)funcType;
			if (!func->hasVarLenArgs&&strFuncArgSize(func->args) < strParserNodeSize(call->args)) {
				// Error excess args
					diagErrorStart(_cstart,_cend);

				// Get start/end of excess arguments
					long excessS;
					long excessE;
					parserNodeStartEndPos(call->args[strFuncArgSize(func->args)]->pos.start, call->args[strParserNodeSize(call->args) - 1]->pos.end, &excessS, &excessE);
				
				char buffer[1024];
				char *name = object2Str((void *)func);
				sprintf(buffer, "Too many args to function of type '%s'.", name);
				diagPushText(buffer);
				diagHighlight(excessS, excessE);
				diagEndMsg();
				noteItem(call->func);

				goto callFail;
			}

			if (!func->hasVarLenArgs&&strFuncArgSize(func->args) > strParserNodeSize(call->args)) {
					int hasDfts=1;
					for(long a=strParserNodeSize(call->args);a!=strFuncArgSize(func->args);a++) {
							if(!func->args[a].dftVal) hasDfts=0;
					}
					if(!hasDfts) {
							// Error excess args
							diagErrorStart(_cstart,_cend);

							char buffer[1024];
							char *name = object2Str((void *)func);
							sprintf(buffer, "Too few args to function of type '%s'.", name);
							diagPushText(buffer);
							diagEndMsg();
							noteItem(call->func);

							goto callFail;
					}
			}
			
			// Check args
			for (long i = 0; i != strParserNodeSize(call->args); i++) {
					__auto_type have = assignTypeToOp(call->args[i]);
				// If past provided args,check for defualt
				if (strParserNodeSize(call->args) <= i&&!func->hasVarLenArgs) {
 						if (func->args[i].dftVal == NULL)
								goto noDft;
						else
								continue;
				}
				struct object *expected = objectPtrCreate(&typeU0);
				if(strFuncArgSize(func->args)>i) expected=func->args[i].type;
				if(strFuncArgSize(func->args) <= i&&!func->hasVarLenArgs) {
						// Whine about no defualt
						diagErrorStart(_cstart,_cend);
						char buffer[1024];
						sprintf(buffer, "Too many aruments for function.");
						diagPushText(buffer);
						diagEndMsg();
						noteItem(call->func);
						goto callFail;
				}

				// If empty arg check for defualt
				if (call->args[i] == NULL) {
					if (func->args[i].dftVal == NULL) {
					noDft:

						// Whine about no defualt
							diagErrorStart(_cstart,_cend);
						char buffer[1024];
						sprintf(buffer, "No defualt value for arguement '%li'.", i);
						diagPushText(buffer);
						diagEndMsg();
						noteItem(call->func);

						goto callFail;
					}
				}
				// If both are arithmetic,can be used
				int expectedArith = isArith(expected);
				int haveArith = isArith(have);
				if (expectedArith && haveArith) {
						//validatePtrPass(expected->type,call->args[i]);
				} else if (expectedArith || haveArith) {
					// One is arithmetic and one isn't
					incompatTypes(call->args[i], expected);
					noteItem(call->func);

					goto callFail;
				} else {
					// Both are non-arithmetic(class/union),so check if types are equal
					if (!objectEqual(expected, assignTypeToOp(call->args[i]))) {
						incompatTypes(call->args[i], expected);
						noteItem(call->func);

						goto callFail;
					}
				}
			}

			// All tests passed
			call->type = func->retType;
			return func->retType;

		callFail:;
			call->type = func->retType;
			return func->retType;
		}
		
		if (call->type != NULL)
			return call->type;
		
	} else if (node->type == NODE_TYPE_CAST) {
		struct parserNodeTypeCast *cast = (void *)node;
		if (!isArith(cast->type)) {
				long _cstart,_cend;
				parserNodeStartEndPos(cast->base.pos.start, cast->base.pos.end, &_cstart, &_cend);
			// Cant convert to non-arithmetic
			diagErrorStart(_cstart,_cend);
			diagPushText("Can't cast to non-arithmetic type!.");
			diagEndMsg();

			goto castEnd;
		}
		if (!isArith(assignTypeToOp(cast->exp))) {
			incompatTypes(cast->exp, cast->type);
			goto castEnd;
		}

	castEnd:
		return cast->type;
	} else if (node->type == NODE_LIT_INT) {
		__auto_type lit = (struct parserNodeLitInt *)node;
		switch (objectSize(dftValType(), NULL)) {
		case 2:
			return promotionType(intLitType(lit), &typeI16i);
		case 4:
			return promotionType(intLitType(lit), &typeI32i);
		case 8:
			return promotionType(intLitType(lit), &typeI64i);
		default:
			fprintf(stderr, "That's an odd defualt value size.");
			abort();
		}
	} else if (node->type == NODE_LIT_FLT) {
		return &typeF64;
	} else if (node->type == NODE_SIZEOF_TYPE) {
		struct parserNodeSizeofType *t = (void *)node;
		int success;
		objectSize(t->type, &success);
		if (!success) {
				diagErrorStart(_start,_end);
			diagPushText("Unable to detirmine size of type.");
			diagEndMsg();
		}
		return dftValType();
	} else if (node->type == NODE_SIZEOF_EXP) {
		struct parserNodeSizeofExp *e = (void *)node;
		int success;
		objectSize(assignTypeToOp(e->exp), &success);
		if (!success) {
			diagErrorStart(_start,_end);
			diagPushText("Unable to detirmine size of type.");
			diagEndMsg();
		}
		return dftValType();
	}  else if(node->type==NODE_LIT_STR) {
			struct parserNodeLitStr *str=(void*)node;
			if(str->str.isChar) {
					struct parserNodeLitInt i;
					i.base.type=NODE_LIT_INT;
					i.value.base=10;
					i.value.type=INT_ULONG;
					i.value.value.uLong=0;
					for(long c=0;c!=__vecSize(str->str.text);c++) {
							i.value.value.uLong<<=8;
							i.value.value.uLong|=((unsigned char*)str->str.text)[c];
					}
					return intLitType(&i);
			} else {
					return objectPtrCreate(&typeU8i);
			}
	} else if (node->type == NODE_ARRAY_LITERAL) {
			return objectPtrCreate(&typeU0);	
	} else if (node->type == NODE_ARRAY_ACCESS) {
		struct parserNodeArrayAccess *arrAcc = (void *)node;
		__auto_type indexType=assignTypeToOp(arrAcc->index);
		if(!isArith(indexType)) {
				diagErrorStart(_start,_end);
				diagPushText("Expected arithmetic type for array index.");
				diagEndMsg();
		}
		__auto_type baseType = objectBaseType(assignTypeToOp(arrAcc->exp));
		struct object *retVal = NULL;
		if (baseType->type == TYPE_PTR) {
			struct objectPtr *ptr = (void *)baseType;
			retVal = ptr->type;
		} else if (baseType->type == TYPE_ARRAY) {
			struct objectArray *array = (void *)baseType;
			retVal = array->type;
		} else {
				diagErrorStart(_start,_end);
			diagPushText("Attempting to der-refference a non-pointer/array type.");
			diagEndMsg();
			goto fail;
		}
		return retVal;
	}
fail:
	// Couldn't detirmine type
	return dftValType();
}
