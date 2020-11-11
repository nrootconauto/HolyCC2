#include <assert.h>
#include <lexer.h>
#include <object.h>
#include <parserA.h>
#define DEBUG_PRINT_ENABLE 1
#include <debugPrint.h>
#include <hashTable.h>
#include <parserB.h>
#include <diagMsg.h>
#include <exprParser.h>
static char *strCopy(const char *text) {
	char *retVal = malloc(strlen(text) + 1);
	strcpy(retVal, text);

	return retVal;
}
#define ALLOCATE(x)                                                            \
	({                                                                           \
		__auto_type len = sizeof(x);                                               \
		void *$retVal = malloc(len);                                               \
		memcpy($retVal, &x, len);                                                  \
		$retVal;                                                                   \
	})
static void assignPosByLexerItems(struct parserNode *node,llLexerItem start,llLexerItem end) {
		node->pos.start=llLexerItemValuePtr(start)->start;
		if(end)
				node->pos.end=llLexerItemValuePtr(end)->end;
		else
				node->pos.start=llLexerItemValuePtr(llLexerItemLast(start))->end;
}
static char *strClone(const char *str) {
	__auto_type len = strlen(str);
	char *retVal = malloc(len + 1);
	strcpy(retVal, str);
	return retVal;
}
//Expected a type that can used as a condition
static void getStartEndPos(llLexerItem start,llLexerItem end,long *startP,long *endP) {
		long endI,startI;
		if(end==NULL) 
				endI=llLexerItemValuePtr(llLexerItemLast(start))->end; 
		else
				endI=llLexerItemValuePtr(end)->end;
		startI=llLexerItemValuePtr(start)->start;

		if(startP)
				*startP=startI;
		if(endP)
				*endP=startI;
}
static void whineExpectedCondExpr(llLexerItem start,llLexerItem end,struct object *type) {
		long startI,endI;
		getStartEndPos(start,end,&startI,&endI);
		
		__auto_type typeText=object2Str(type);

		diagErrorStart(startI, endI);
		char buffer[1024];
		sprintf(buffer,"Type '%s' cannot be used as condition.",typeText);
		diagPushText(buffer);
		diagHighlight(startI, endI);
		diagEndMsg();

		free(typeText);
}
static void whineExpectedExpr(llLexerItem item) {
		if(item==NULL) {
				long end=diagInputSize();
				diagErrorStart(end, end);
				diagPushText("Expected expression but got end of input.");
				diagErrorStart(end, end);
				return ;
		}
		
		__auto_type item2=llLexerItemValuePtr(item);
		diagErrorStart(item2->start, item2->end);

		diagHighlight(item2->start,item2->end);
		diagPushText("Expected an expression,got ");
		
		diagPushQoutedText(item2->start, item2->end);
		diagPushText(".");

		diagHighlight(item2->start,item2->end);
		diagEndMsg();
}
static void whineExpected(llLexerItem item,const char *text) {
		char buffer[256];

		if(item==NULL) {
				long end=diagInputSize();
				diagErrorStart(end, end);
				sprintf(buffer, "Expected '%s',got end of input.",text);
				diagPushText(buffer);
				diagEndMsg();
				return ;
		}
		
		__auto_type item2=llLexerItemValuePtr(item);
		diagErrorStart(item2->start, item2->end);
		sprintf(buffer, "Expected '%s', got " , text);
		diagPushText(buffer);
		diagPushText(",got ");
		diagPushQoutedText(item2->start, item2->end);
		diagPushText(".");

		diagHighlight(item2->start,item2->end);

		diagEndMsg();
}
static struct parserNode *expectOp(llLexerItem _item, const char *text) {
	if (_item == NULL)
		return NULL;

	__auto_type item = llLexerItemValuePtr(_item);

	if (item->template == &opTemplate) {
		__auto_type opText = *(const char **)lexerItemValuePtr(item);
		if (0 == strcmp(opText, text)) {
			struct parserNodeOpTerm term;
			term.base.type = NODE_OP;
			term.base.pos.start = item->start;
			term.base.pos.end = item->end;
			term.text = text;

			return ALLOCATE(term);
		}
	}
	return NULL;
}
void parserNodeDestroy(struct parserNode **node) {
	// TODO implement
}
static struct parserNode *nameParse(llLexerItem start, llLexerItem end,
                                    llLexerItem *result);
static void strParserNodeDestroy2(strParserNode *nodes) {
	for (long i = 0; i != strParserNodeSize(*nodes); i++)
		parserNodeDestroy(&nodes[0][i]);
	strParserNodeDestroy(nodes);
}
static struct parserNode *parenRecur(llLexerItem start, llLexerItem end,
                                     llLexerItem *result);
static struct parserNode *literalRecur(llLexerItem start, llLexerItem end,
                                       llLexerItem *result) {
	if (result != NULL)
		*result = start;

	__auto_type item = llLexerItemValuePtr(start);
	if (item->template == &intTemplate) {
		if (result != NULL)
			*result = llLexerItemNext(start);

		struct parserNodeLitInt lit;
		lit.base.type = NODE_LIT_INT;
		lit.value = *(struct lexerInt *)lexerItemValuePtr(item);
		lit.base.pos.start=item->start;
		lit.base.pos.end=item->end;
		
		return ALLOCATE(lit);
	} else if (item->template == &strTemplate) {
		if (result != NULL)
			*result = llLexerItemNext(start);

		__auto_type str = *(struct parsedString *)lexerItemValuePtr(item);
		struct parserNodeLitStr lit;
		lit.base.type = NODE_LIT_STR;
		lit.text = strClone((char *)str.text);
		lit.isChar = str.isChar;
		lit.base.pos.start=item->start;
		lit.base.pos.end=item->end;

		return ALLOCATE(lit);
	} else if (item->template == &nameTemplate) {
		__auto_type name = nameParse(start, end, result);
		//Look for var
		__auto_type findVar = getVar(name);
		if (findVar) {
			struct parserNodeVar var;
			var.base.type = NODE_VAR;
			var.var = findVar;
			var.base.pos.start=name->pos.start;
			var.base.pos.end=name->pos.end;
			
			return ALLOCATE(var);
		}

		//Look for func
		__auto_type findFunc=getFunc(name);
		if(findFunc) {
				struct parserNodeFuncRef ref;
				ref.base.pos.start=name->pos.start;
				ref.base.pos.end=name->pos.end;
				ref.base.type=NODE_FUNC_REF;
				ref.func=findFunc;
				ref.name=name;

				return ALLOCATE(ref);
		}
		
		return name;
	} else {
		return NULL;
	}

	// TODO add float template.
}
static struct parserNode *prec0Binop(llLexerItem start, llLexerItem end,
                                     llLexerItem *result);
static struct parserNode *prec1Recur(llLexerItem start, llLexerItem end,
                                     llLexerItem *result);
static struct parserNode *prec2Recur(llLexerItem start, llLexerItem end,
                                     llLexerItem *result);
static struct parserNode *prec3Recur(llLexerItem start, llLexerItem end,
                                     llLexerItem *result);
static struct parserNode *prec4Recur(llLexerItem start, llLexerItem end,
                                     llLexerItem *result);
static struct parserNode *prec5Recur(llLexerItem start, llLexerItem end,
                                     llLexerItem *result);
static struct parserNode *prec6Recur(llLexerItem start, llLexerItem end,
                                     llLexerItem *result);
static struct parserNode *prec7Recur(llLexerItem start, llLexerItem end,
                                     llLexerItem *result);
static struct parserNode *prec8Recur(llLexerItem start, llLexerItem end,
                                     llLexerItem *result);
static struct parserNode *prec9Recur(llLexerItem start, llLexerItem end,
                                     llLexerItem *result);
static struct parserNode *prec10Recur(llLexerItem start, llLexerItem end,
                                      llLexerItem *result);
static struct parserNode *prec11Recur(llLexerItem start, llLexerItem end,
                                      llLexerItem *result);
static struct parserNode *prec12Recur(llLexerItem start, llLexerItem end,
                                      llLexerItem *result);
static struct parserNode *prec13Recur(llLexerItem start, llLexerItem end,
                                      llLexerItem *result);
STR_TYPE_DEF(int, Int);
STR_TYPE_FUNCS(int, Int);
static llLexerItem findOtherSide(llLexerItem start, llLexerItem end) {
	const char *lefts[] = {"(", "[", "{"};
	const char *rights[] = {")", "]", "}"};
	__auto_type count = sizeof(lefts) / sizeof(*lefts);

	strInt stack = NULL;
	int dir = 0;
	do {
		if (start == NULL)
			return NULL;
		if (start == end)
			return NULL;

		if (llLexerItemValuePtr(start)->template == &opTemplate) {
			__auto_type text =
			    *(const char **)lexerItemValuePtr(llLexerItemValuePtr(start));
			int i;
			for (i = 0; i != count; i++) {
				if (0 == strcmp(lefts[i], text))
					goto foundLeft;
				if (0 == strcmp(rights[i], text))
					goto foundRight;
			}
			goto next;
		foundLeft : {
			if (dir == 0)
				dir = 1;

			if (dir == 1)
				stack = strIntAppendItem(stack, i);
			else
				stack = strIntResize(stack, strIntSize(stack) - 1);

			goto next;
		}
		foundRight : {
			if (dir == 0)
				dir = -1;

			if (dir == -1)
				stack = strIntAppendItem(stack, i);
			else
				stack = strIntResize(stack, strIntSize(stack) - 1);
			goto next;
		}
		}
	next:
		if (dir == 0)
			return NULL;

		if (strIntSize(stack) == 0) {
			strIntDestroy(&stack);
			return start;
		}
		if (dir == 0)
			return NULL;
		else if (dir == -1)
			start = llLexerItemPrev(start);
		else if (dir == 1)
			start = llLexerItemNext(start);
	} while (strIntSize(stack));

	// TODO whine about unbalanced
	strIntDestroy(&stack);
	return NULL;
}
static struct parserNode *precCommaRecur(llLexerItem start, llLexerItem end,
                                         llLexerItem *result) {
		__auto_type originalStart=start;
		
		if (start == NULL)
		return NULL;

	struct parserNodeCommaSeq seq;
	seq.base.type = NODE_COMMA_SEQ;
	seq.items = NULL;
	seq.type = NULL;
	seq.base.pos.start=llLexerItemValuePtr(start)->start;
	
	__auto_type node = NULL;
	for (; start != NULL && start != end;) {
		__auto_type comma = expectOp(start, ",");
		if (comma) {
			parserNodeDestroy(&comma);
			start = llLexerItemNext(start);
			seq.items = strParserNodeAppendItem(seq.items, node);
			node = NULL;
		} else if (node == NULL) {
			node = prec13Recur(start, end, &start);
			if (node == NULL)
				break;
		} else {
			break;
		}
	}
	if (result != NULL)
		*result = start;

	if (seq.items == NULL) {
		return node;
	} else {
		// Append last item
		seq.items = strParserNodeAppendItem(seq.items, node);

		if(start)
				seq.base.pos.end=llLexerItemValuePtr(start)->end;
		else
				seq.base.pos.end=llLexerItemValuePtr(llLexerItemLast(originalStart))->end;
		return ALLOCATE(seq);
	}
}
static struct parserNode *parenRecur(llLexerItem start, llLexerItem end,
                                     llLexerItem *result) {
	if (start == NULL)
		return NULL;

	struct parserNode *left = expectOp(start, "(");

	struct parserNode *right = NULL;
	struct parserNode *retVal = NULL;

	if (left != NULL) {
		__auto_type pairEnd = findOtherSide(start, end);
		if (pairEnd == NULL)
			goto fail;

		start = llLexerItemNext(start);
		llLexerItem res;
		retVal = precCommaRecur(start, pairEnd, &res);
		if (!retVal)
			goto fail;

		start = res;
		right = expectOp(res, ")");
		start = llLexerItemNext(start);
		if (!right)
			goto fail;

		retVal->pos.start= left->pos.start;
		retVal->pos.end= right->pos.end;
		if (result != NULL)
			*result = start;

		goto success;
	} else {
		retVal = literalRecur(start, end, result);
		goto success;
	}
fail:
	parserNodeDestroy(&retVal);
	retVal = NULL;
success:
	parserNodeDestroy(&right);
	parserNodeDestroy(&left);

	return retVal;
}
struct parserNode *parseExpression(llLexerItem start, llLexerItem end,
                                   llLexerItem *result) {
		__auto_type res=precCommaRecur(start, end, result);
		if(res)
				assignTypeToOp(res);

		return res;
}
struct pnPair {
	struct parserNode *a, *b;
};
STR_TYPE_DEF(struct pnPair, PNPair);
STR_TYPE_FUNCS(struct pnPair, PNPair);
static void strPNPairDestroy2(strPNPair *list) {
	for (long i = 0; i != strPNPairSize(*list); i++) {
		parserNodeDestroy(&list[0][i].a);
		parserNodeDestroy(&list[0][i].b);
	}
	strPNPairDestroy(list);
}
static strPNPair tailBinop(llLexerItem start, llLexerItem end,
                           llLexerItem *result, const char **ops, long opCount,
                           struct parserNode *(*func)(llLexerItem, llLexerItem,
                                                      llLexerItem *)) {
	strPNPair retVal = NULL;
	for (; start != NULL;) {
		struct parserNode *op __attribute__((cleanup(parserNodeDestroy)));
		for (long i = 0; i != opCount; i++) {
			op = expectOp(start, ops[i]);
			if (op != NULL)
				break;
		}
		if (op == NULL)
			break;

		start = llLexerItemNext(start);
		__auto_type find = func(start, end, &start);
		if (find == NULL)
			goto fail;

		struct pnPair pair = {op, find};
		retVal = strPNPairAppendItem(retVal, pair);
	}

	if (result != NULL)
		*result = start;

	return retVal;
fail:
	strPNPairDestroy2(&retVal);
	return NULL;
}
static struct parserNode *nameParse(llLexerItem start, llLexerItem end,
                                    llLexerItem *result) {
	if (start == NULL)
		return NULL;

	__auto_type item = llLexerItemValuePtr(start);
	if (item->template == &nameTemplate) {
		if (result != NULL)
			*result = llLexerItemNext(start);

		__auto_type ptr = strClone(lexerItemValuePtr(item));
		struct parserNodeName retVal;
		retVal.base.type = NODE_NAME;
		retVal.base.pos.start = item->start;
		retVal.base.pos.end = item->end;
		retVal.text = ptr;

		return ALLOCATE(retVal);
	}
	return NULL;
}
static struct parserNode *pairOperator(const char *left, const char *right,
                                       llLexerItem start, llLexerItem end,
                                       llLexerItem *result, int *success,long *startP,long *endP) {
	if (success != NULL)
		*success = 0;

	if (start == NULL)
		return NULL;

	if (result != NULL)
		*result = start;

	llLexerItem result2;
	struct parserNode *l = NULL, *r = NULL, *exp = NULL;

	l = expectOp(start, left);
	result2 = llLexerItemNext(start);
	if (l == NULL)
		goto end;
	__auto_type pairEnd = findOtherSide(start, end);

	exp = precCommaRecur(result2, pairEnd, &result2);

	r = expectOp(result2, right);
	result2 = llLexerItemNext(result2);
	if (r == NULL)
		goto end;

	if (result != NULL)
		*result = result2;
	if (success != NULL)
		*success = 1;

end:
	if (r == NULL)
		parserNodeDestroy(&exp), exp = NULL;

	if(l&&startP)
			*startP=l->pos.start;
	if(r&&endP)
			*endP=r->pos.end;
	
	parserNodeDestroy(&l);
	parserNodeDestroy(&r);
	
	return exp;
}
static struct object *parseVarDeclTail(llLexerItem start, llLexerItem *end,
                                       struct object *baseType,
                                       struct parserNode **name,
                                       struct parserNode **dftVal,
                                       strParserNode *metaDatas);
static struct parserNode *prec0Binop(llLexerItem start, llLexerItem end,
                                     llLexerItem *result) {
	if (start == NULL)
		return NULL;

	if (result != NULL)
		*result = start;

	llLexerItem result2;
	struct parserNode *head = parenRecur(start, end, &result2);
	if (head == NULL)
		return NULL;
	const char *binops[] = {".", "->"};
	const char *unops[] = {"--", "++"};
	strPNPair tails = NULL;
	for (; result2 != NULL && result2 != end;) {
		for (long i = 0; i != 2; i++) {
			__auto_type ptr = expectOp(result2, unops[i]);
			if (ptr != NULL) {
				result2 = llLexerItemNext(result2);

				struct parserNodeUnop unop;
				unop.a = head;
				unop.base.type = NODE_UNOP;
				unop.isSuffix = 1;
				unop.op = ptr;
				unop.type=NULL;
				unop.base.pos.start=head->pos.start;
				unop.base.pos.end=ptr->pos.end;

				head = ALLOCATE(unop);
				goto loop1;
			}
		}
		for (long i = 0; i != 2; i++) {
			__auto_type ptr = expectOp(result2, binops[i]);
			if (ptr)
				result2 = llLexerItemNext(result2);
			else
				continue;

			__auto_type next = nameParse(result2, end, &result2);
			if (next == NULL)
				goto fail;
			if (ptr != NULL) {
				struct parserNodeBinop binop;
				binop.a = head;
				binop.base.type = NODE_BINOP;
				binop.op = ptr;
				binop.b = next;
				binop.base.pos.start=head->pos.start;
				binop.base.pos.end=head->pos.end;
				binop.type=NULL;
						
				head = ALLOCATE(binop);
				goto loop1;
			}
		}

		// Check for typecast before func call
		struct parserNode *lP=expectOp(result2,"(");
		if(lP) {
				llLexerItem end2=findOtherSide(result2, NULL);
				if(end2){
						__auto_type item=llLexerItemValuePtr(llLexerItemNext(result2));
						if(item->template==&nameTemplate) {
								__auto_type baseType= objectByName((char*)lexerItemValuePtr(item));
								if(baseType!=NULL) {
										strParserNode dims;
										long ptrLevel;
										struct parserNode *name;
										llLexerItem end3;

										struct parserNode *dft;
										strParserNode metas=NULL;
										__auto_type type= parseVarDeclTail(start, &end3, baseType, &name, &dft, &metas);
										result2=end2;
										
										//Create type
										strParserNodeDestroy2(&metas);
										parserNodeDestroy(&name);

										struct parserNodeTypeCast cast;
										cast.base.type=NODE_TYPE_CAST;
										cast.exp=head;
										cast.type=type;
										cast.base.pos.start=lP->pos.start;
										cast.base.pos.end=llLexerItemValuePtr(end2)->end;
										
										head=ALLOCATE(cast);

										//Move past ")"
										result2=llLexerItemNext(end2);
										goto loop1;
								}
						}
				}
		}
		parserNodeDestroy(&lP);
		int success;
		long startP,endP;
		__auto_type funcCallArgs =
				pairOperator("(", ")", result2, end, &result2, &success,&startP,&endP);
		if (success) {
			struct parserNodeFuncCall newNode;
			newNode.base.type = NODE_FUNC_CALL;
			newNode.func = head;
			newNode.args = NULL;
			newNode.base.pos.start=startP;
			newNode.base.pos.end=endP;
			newNode.type=NULL;
			
			if (funcCallArgs != NULL) {
				if (funcCallArgs->type == NODE_COMMA_SEQ) {
					struct parserNodeCommaSeq *seq = (void *)funcCallArgs;
					for (long i = 0; i != strParserNodeSize(seq->items); i++)
						newNode.args = strParserNodeAppendItem(newNode.args, seq->items[i]);
				} else if (funcCallArgs != NULL) {
					newNode.args = strParserNodeAppendItem(newNode.args, funcCallArgs);
				}
			}

			head = ALLOCATE(newNode);
			goto loop1;
		}
		__auto_type oldResult2 = result2;
		__auto_type array =
				pairOperator("[", "]", result2, end, &result2, &success,&startP,&endP);
		if (success) {
			struct parserNodeArrayAccess access;
			access.exp = head;
			access.base.type = NODE_BINOP;
			access.index=array;
			access.base.pos.start=startP;
			access.base.pos.end=endP;
			access.type=NULL;
			
			head = ALLOCATE(access);

			goto loop1;
		}

		// Nothing found
		break;
	loop1:;
	}

	if (result != NULL)
		*result = result2;
	return head;
fail:
	parserNodeDestroy(&head);
	return NULL;
}
static struct parserNode *prec1Recur(llLexerItem start, llLexerItem end,
                                     llLexerItem *result) {
	if (start == NULL)
		return NULL;

	llLexerItem result2 = start;
	const char *unops[] = {"--", "++", "+", "-", "!", "~", "*", "&"};
	__auto_type count = sizeof(unops) / sizeof(*unops);
	strParserNode opStack = NULL;

	for (; result2 != NULL;) {
	loop1:
		for (long i = 0; i != count; i++) {
			__auto_type op = expectOp(result2, unops[i]);
			if (op != NULL) {
				result2 = llLexerItemNext(result2);
				opStack = strParserNodeAppendItem(opStack, op);
				goto loop1;
			}
		}
		break;
	}
	struct parserNode *tail = prec0Binop(result2, end, &result2);
	if (opStack != NULL) {
		if (tail == NULL) {
			strParserNodeDestroy2(&opStack);
			goto fail;
		}

		for (long i = strParserNodeSize(opStack) - 1; i >= 0; i--) {
			struct parserNodeUnop unop;
			unop.base.type = NODE_UNOP;
			unop.a = tail;
			unop.isSuffix = 0;
			unop.op = opStack[i];
			unop.base.pos.start=tail->pos. end;
			unop.base.pos.end=opStack[i]->pos.start;
			unop.type=NULL;
			tail = ALLOCATE(unop);
		} 
	}

	if (result != NULL)
		*result = result2;
	strParserNodeDestroy(&opStack);
	return tail;

fail:
	return tail;
}
static struct parserNode *binopLeftAssoc(
    const char **ops, long count, llLexerItem start, llLexerItem end,
    llLexerItem *result,
    struct parserNode *(*next)(llLexerItem, llLexerItem, llLexerItem *)) {
	if (start == NULL)
		return NULL;

	llLexerItem result2 = NULL;
	__auto_type head = next(start, end, &result2);
	strPNPair tail = NULL;
	if (head == NULL)
		goto end;

	tail = tailBinop(result2, end, &result2, ops, count, next);
	for (long i = 0; i != strPNPairSize(tail); i++) {
		struct parserNodeBinop binop;
		binop.base.type = NODE_BINOP;
		binop.a = head;
		binop.op = tail[i].a;
		binop.b = tail[i].b;
		binop.base.pos.start=head->pos.start;
		binop.base.pos.end=tail[i].b->pos.end;
		binop.type=NULL;
		
		head = ALLOCATE(binop);
	}
end:
	if (result != NULL)
		*result = result2;

	return head;
}
static struct parserNode *binopRightAssoc(
    const char **ops, long count, llLexerItem start, llLexerItem end,
    llLexerItem *result,
    struct parserNode *(*next)(llLexerItem, llLexerItem, llLexerItem *)) {
	if (start == NULL)
		return NULL;

	llLexerItem result2 = start;
	struct parserNode *retVal = NULL;
	__auto_type head = next(result2, end, &result2);

	__auto_type tail = tailBinop(result2, end, &result2, ops, count, next);
	if (tail == NULL) {
		retVal = head;
		goto end;
	}

	struct parserNode *right = NULL;
	for (long i = strPNPairSize(tail) - 1; i >= 0; i--) {
		if (right == NULL)
			right = tail[i].b;

		struct parserNode *left;
		if (i == 0)
			left = head;
		else
			left = tail[i - 1].b;

		struct parserNodeBinop binop;
		binop.a = left;
		binop.op = tail[i].a;
		binop.b = right;
		binop.base.type = NODE_BINOP;
		binop.base.pos.start=left->pos.start;
		binop.base.pos.start=right->pos.end;
		binop.type=NULL;

		right = ALLOCATE(binop);
	}

	retVal = right;
end:;
	if (result != NULL)
		*result = result2;

	return retVal;
}
#define LEFT_ASSOC_OP(name, next, ...)                                         \
	static const char *name##Ops[] = {__VA_ARGS__};                              \
	static struct parserNode *name##Recur(llLexerItem start, llLexerItem end,    \
	                                      llLexerItem *result) {                 \
		__auto_type count = sizeof(name##Ops) / sizeof(*name##Ops);                \
		return binopLeftAssoc(name##Ops, count, start, end, result, next);         \
	}
#define RIGHT_ASSOC_OP(name, next, ...)                                        \
	static const char *name##Ops[] = {__VA_ARGS__};                              \
	static struct parserNode *name##Recur(llLexerItem start, llLexerItem end,    \
	                                      llLexerItem *result) {                 \
		__auto_type count = sizeof(name##Ops) / sizeof(*name##Ops);                \
		return binopRightAssoc(name##Ops, count, start, end, result, next);        \
	}
RIGHT_ASSOC_OP(prec13, prec12Recur, "=",
               "-=", "+=", "*=", "/=", "%=", "<<=", ">>=", "&=", "|=", "^=");
LEFT_ASSOC_OP(prec12, prec11Recur, "||");
LEFT_ASSOC_OP(prec11, prec10Recur, "^^");
LEFT_ASSOC_OP(prec10, prec9Recur, "&&");
LEFT_ASSOC_OP(prec9, prec8Recur, "==", "!=");
LEFT_ASSOC_OP(prec8, prec7Recur, ">", "<", ">=", "<=");
LEFT_ASSOC_OP(prec7, prec6Recur, "+", "-");
LEFT_ASSOC_OP(prec6, prec5Recur, "|");
LEFT_ASSOC_OP(prec5, prec4Recur, "^");
LEFT_ASSOC_OP(prec4, prec3Recur, "&");
LEFT_ASSOC_OP(prec3, prec2Recur, "*", "%", "/");
LEFT_ASSOC_OP(prec2, prec1Recur, "`", "<<", ">>");

static struct parserNode *expectKeyword(llLexerItem __item, const char *text) {
	if (__item == NULL)
		return NULL;

	__auto_type item = llLexerItemValuePtr(__item);

	if (item->template == &kwTemplate) {
		__auto_type itemText = *(const char **)lexerItemValuePtr(item);
		if (0 == strcmp(itemText, text)) {
			struct parserNodeKeyword kw;
			kw.base.type = NODE_KW;
			kw.base.pos.start = item->start;
			kw.base.pos.end = item->end;
			kw.text = text;

			return ALLOCATE(kw);
		}
	}
	return NULL;
}
struct linkagePair {
	enum linkage link;
	const char *text;
};
static enum linkage getLinkage(llLexerItem start, llLexerItem *result) {
	if (result != NULL)
		*result = start;
	if (start == NULL)
		return 0;

	if (llLexerItemValuePtr(start)->template != &kwTemplate)
		return 0;

	struct linkagePair pairs[] = {
	    {LINKAGE_PUBLIC, "public"}, {LINKAGE_STATIC, "static"},
	    {LINKAGE_EXTERN, "extern"}, {LINKAGE__EXTERN, "_extern"},
	    {LINKAGE_IMPORT, "import"}, {LINKAGE__IMPORT, "_import"},
	};
	__auto_type count = sizeof(pairs) / sizeof(*pairs);

	for (long i = 0; i != count; i++) {
		if (expectKeyword(start, pairs[i].text)) {
			if (result != NULL)
				*result = llLexerItemNext(start);

			return pairs[i].link;
		}
	}

	return 0;
}
static void getPtrsAndDims(llLexerItem start, llLexerItem *end,
                           struct parserNode **name, long *ptrLevel,
                           strParserNode *dims) {
	long ptrLevel2 = 0;
	for (; start != NULL; start = llLexerItemNext(start)) {
		__auto_type op = expectOp(start, "*");
		if (op) {
			parserNodeDestroy(&op);
			ptrLevel2++;
		} else
			break;
	}

	__auto_type name2 = nameParse(start, NULL, &start);

	strParserNode dims2 = NULL;
	for (;;) {
		__auto_type left = expectOp(start, "[");
		if (left == NULL)
			break;

		__auto_type dim = pairOperator("[", "]", start, NULL, &start, NULL,NULL,NULL);
		dims2 = strParserNodeAppendItem(dims2, dim);
	}

	if (dims != NULL)
		*dims = dims2;
	else
		strParserNodeDestroy(&dims2);

	if (ptrLevel != NULL)
		*ptrLevel = ptrLevel2;

	if (name != NULL)
		*name = name2;
	else
		parserNodeDestroy(&name2);

	if (end != NULL)
		*end = start;
}
static struct object *parseVarDeclTail(llLexerItem start, llLexerItem *end,
                                       struct object *baseType,
                                       struct parserNode **name,
                                       struct parserNode **dftVal,
                                       strParserNode *metaDatas);
struct parserNode *parseSingleVarDecl(llLexerItem start, llLexerItem *end) {
		__auto_type originalStart=start;
	struct parserNode *base __attribute__((cleanup(parserNodeDestroy)));
	base = nameParse(start, NULL, &start);
	if (base != NULL) {
		struct parserNodeName *baseName = (void *)base;
		__auto_type baseType = objectByName(baseName->text);
		if (baseType == NULL)
			return NULL;

		struct parserNodeVarDecl decl;
		decl.base.type = NODE_VAR_DECL;
		decl.type = parseVarDeclTail(start, end, baseType, &decl.name, &decl.dftVal,
		                             &decl.metaData);

		if(end)
				assignPosByLexerItems((struct parserNode*)&decl,originalStart,*end);
		else
				assignPosByLexerItems((struct parserNode*)&decl,originalStart,NULL);
		return ALLOCATE(decl);
	}
	return NULL;
}
struct parserNode *parseVarDecls(llLexerItem start, llLexerItem *end) {
		__auto_type originalStart=start;
		
	struct parserNode *base __attribute__((cleanup(parserNodeDestroy)));
	struct object *baseType = NULL;
	int foundType = 0;
	base = nameParse(start, NULL, &start);

	if (base) {
		struct parserNodeName *baseName = (void *)base;
		baseType = objectByName(baseName->text);
		foundType = baseType != NULL;
	} else {
		__auto_type cls = parseClass(start, &start);
		if (cls != NULL) {
			foundType = 1;

			if (cls->type == NODE_CLASS_DEF) {
				struct parserNodeClassDef *clsDef = (void *)cls;
				baseType = clsDef->type;
			} else if (cls->type == NODE_UNION_DEF) {
				struct parserNodeUnionDef *unDef = (void *)cls;
				baseType = unDef->type;
			}
		}
	}

	if (foundType) {
		strParserNode decls __attribute__((cleanup(strParserNodeDestroy2)));
		decls = NULL;

		for (int firstRun = 1;; firstRun = 0) {
			if (!firstRun) {
				struct parserNode *op __attribute__((cleanup(parserNodeDestroy)));
				op = expectOp(start, ",");
				if (!op)
					goto end;

				start = llLexerItemNext(start);
			}

			struct parserNodeVarDecl decl;
			decl.base.type = NODE_VAR_DECL;
			decl.type = parseVarDeclTail(start, &start, baseType, &decl.name,
			                             &decl.dftVal, &decl.metaData);

			decls = strParserNodeAppendItem(decls, ALLOCATE(decl));
		}
	end:;
		if (end != NULL)
			*end = start;

		if (strParserNodeSize(decls) == 1) {
			return decls[0];
		} else if (strParserNodeSize(decls) > 1) {
			struct parserNodeVarDecls retVal;
			retVal.base.type = NODE_VAR_DECLS;
			retVal.decls = strParserNodeAppendData(
			    NULL, (const struct parserNode **)decls, strParserNodeSize(decls));
			if(end)
					assignPosByLexerItems((struct parserNode *)&retVal, originalStart, *end);
			else
					assignPosByLexerItems((struct parserNode *)&retVal, originalStart, NULL);
			
			return ALLOCATE(retVal);
		}
	}
fail:
	if (end != NULL)
		*end = start;

	return NULL;
}
static llLexerItem findEndOfExpression(llLexerItem start, int stopAtComma) {
	for (; start != NULL; start = llLexerItemNext(start)) {
		__auto_type item = llLexerItemValuePtr(start);
		__auto_type otherSide = findOtherSide(start, NULL);
		start = (otherSide != NULL) ? otherSide : start;

		if (stopAtComma)
			if (item->template == &opTemplate)
				if (0 == strcmp(",", *(const char **)lexerItemValuePtr(item)))
					return start;

		// TODO add floating template
		if (item->template == &intTemplate)
			continue;
		if (item->template == &strTemplate)
			continue;
		if (item->template == &nameTemplate)
			continue;
		if (item->template == &opTemplate)
			continue;
		return start;
	}

	return NULL;
}
static struct object *parseVarDeclTail(llLexerItem start, llLexerItem *end,
                                       struct object *baseType,
                                       struct parserNode **name,
                                       struct parserNode **dftVal,
                                       strParserNode *metaDatas) {
		int failed=0;
		
		if (metaDatas != NULL)
		*metaDatas = NULL;

	__auto_type funcPtrLeft = expectOp(start, "(");
	long ptrLevel = 0;
	struct parserNode *eq __attribute__((cleanup(parserNodeDestroy)));
	eq = NULL;
	strParserNode dims __attribute__((cleanup(strParserNodeDestroy)));
	dims = NULL;
	struct object *retValType = NULL;

	if (funcPtrLeft != NULL) {
		start = llLexerItemNext(start);
		parserNodeDestroy(&funcPtrLeft);

		getPtrsAndDims(start, &start, name, &ptrLevel, &dims);

		__auto_type r = expectOp(start, ")");
		if (r == NULL) {
			parserNodeDestroy(&r);
			failed=1;
			whineExpected(start, ")");
		} else {
				start = llLexerItemNext(start);
				failed=1;
		}

		struct parserNode *l2 __attribute__((cleanup(parserNodeDestroy)));
		l2 = expectOp(start, "(");
		if(l2) {
		start = llLexerItemNext(start);
		} else {
				whineExpected(start, ")");
				failed=1;
		}	

		strFuncArg args __attribute__((cleanup(strFuncArgDestroy2)));
		args = NULL;
		for (int firstRun = 1;; firstRun = 0) {
			struct parserNode *r2 __attribute__((cleanup(parserNodeDestroy)));
			r2 = expectOp(start, ")");
			if (r2 != NULL) {
				start = llLexerItemNext(start);
				break;
			}

			// Check for comma if not firt run
			if (!firstRun) {
				struct parserNode *comma __attribute__((cleanup(parserNodeDestroy)));
				comma = expectOp(start, ",");
				if (comma == NULL){
						whineExpected(start, ",");
				} else
						start = llLexerItemNext(start);
			}

			struct parserNodeVarDecl *argDecl =
			    (void *)parseSingleVarDecl(start, &start);
			if (argDecl == NULL)
				goto fail;

			struct objectFuncArg arg;
			arg.dftVal = argDecl->dftVal;
			arg.name = argDecl->name;
			arg.type = argDecl->type;
			free(argDecl);

			args = strFuncArgAppendItem(args, arg);
		}
		retValType = objectFuncCreate(baseType, args);
	} else {
		getPtrsAndDims(start, &start, name, &ptrLevel, &dims);
		retValType = baseType;
	}

	// Make type has pointers and dims
	for (long i = 0; i != ptrLevel; i++)
		retValType = objectPtrCreate(retValType);
	for (long i = 0; i != strParserNodeSize(dims); i++)
		retValType = objectArrayCreate(retValType, dims[i]);

	// Look for metaData
	strParserNode metaDatas2 = NULL;
metaDataLoop:;

	__auto_type metaName = nameParse(start, NULL, &start);
	if (metaName != NULL) {
		// Find end of expression,comma is reserved for next declaration.
		__auto_type expEnd = findEndOfExpression(start, 1);
		struct parserNode *metaValue = parseExpression(start, expEnd, &start);
		struct parserNodeMetaData meta;
		meta.base.type = NODE_META_DATA;
		meta.name = metaName;
		meta.value = metaValue;

		metaDatas2 = strParserNodeAppendItem(metaDatas2, ALLOCATE(meta));
		goto metaDataLoop;
	}
	// Look for default value

	eq = expectOp(start, "=");
	if (eq != NULL) {
		start = llLexerItemNext(start);
		__auto_type expEnd = findEndOfExpression(start, 1);
		__auto_type dftVal2 = parseExpression(start, expEnd, &start);

		if (dftVal != NULL)
			*dftVal = dftVal2;
	} else {
		if (dftVal != NULL)
			*dftVal = NULL;
	}

	if (metaDatas != NULL)
		*metaDatas = metaDatas2;
	else
		strParserNodeDestroy2(&metaDatas2);

	if (end != NULL)
		*end = start;

	return retValType;
fail:
	if (end != NULL)
		*end = start;
	return NULL;
}
static strObjectMember parseMembers(llLexerItem start, llLexerItem *end) {
	__auto_type decls = parseVarDecls(start, &start);
	if (decls != NULL) {
		strParserNode decls2 = NULL;
		if (decls->type == NODE_VAR_DECL) {
			decls2 = strParserNodeAppendItem(decls2, decls);
		} else if (decls->type == NODE_VAR_DECLS) {
			struct parserNodeVarDecls *d = (void *)decls;
			decls2 =
			    strParserNodeAppendData(NULL, (const struct parserNode **)d->decls,
			                            strParserNodeSize(d->decls));
		}

		strObjectMember members = NULL;
		for (long i = 0; i != strParserNodeSize(decls2); i++) {
			if (decls2[i]->type != NODE_VAR_DECL)
				continue;

			struct objectMember member;

			struct parserNodeVarDecl *decl = (void *)decls2[i];
			struct parserNodeName *name = (void *)decl->name;
			member.name = NULL;
			if (name != NULL)
				if (name->base.type == NODE_NAME)
					member.name = strCopy(name->text);

			member.type = decl->type;
			member.offset = -1;
			member.attrs = NULL;

			for (long i = 0; i != strParserNodeSize(decl->metaData); i++) {
				struct parserNodeMetaData *meta = (void *)decl->metaData[i];
				assert(meta->base.type == NODE_META_DATA);

				struct objectMemberAttr attr;
				name = (void *)meta->name;
				assert(name->base.type == NODE_NAME);
				attr.name = strCopy(name->text);
				attr.value = meta->value;

				member.attrs = strObjectMemberAttrAppendItem(member.attrs, attr);
			}

			members = strObjectMemberAppendItem(members, member);
		}
		strParserNodeDestroy2(&decls2);

		if (end != NULL)
			*end = start;
		return members;
	}
	if (end != NULL)
		*end = start;
	return NULL;
}
static void referenceType(struct object *type) {
		struct parserNodeName *existingName=NULL;
		if(type->type==TYPE_CLASS) {
				struct objectClass *cls=(void*)type;
				existingName=(void*)cls->name;
		} else if(type->type==TYPE_UNION) {
				struct objectUnion *un=(void*)type;
				existingName=(void*)un->name;
		} else if(type->type==TYPE_FORWARD) {
				struct objectForwardDeclaration *f=(void*)type;
				existingName=(void*)f->name;
		}
								
		if(existingName) {
				assert(existingName->base.type==NODE_NAME);
				diagNoteStart(existingName->base.pos.start, existingName->base.pos.end);
				diagPushText("Previous definition here:");
				diagHighlight(existingName->base.pos.start, existingName->base.pos.end);
				diagEndMsg();
		}
}
struct parserNode *parseClass(llLexerItem start, llLexerItem *end) {
		__auto_type originalStart = start;
		__auto_type baseName = nameParse(start, NULL, &start);
		struct parserNode *cls = NULL, *un = NULL;
		struct object *retValObj = NULL;
		struct parserNode *retVal = NULL;
		
		struct object *baseType = NULL;
		if (baseName != NULL) {
				struct parserNodeName *name2 = (void *)baseName;
				baseType = objectByName(name2->text);
				if (baseType == NULL)
						goto end;
		}
	cls = expectKeyword(start, "class");
	un = expectKeyword(start, "union");

	struct parserNode *name2 = NULL;
	if (cls || un) {
		start = llLexerItemNext(start);

		name2 = nameParse(start, NULL, &start);

		struct parserNode *l __attribute__((cleanup(parserNodeDestroy))) =
		    expectKeyword(start, "{");
		start = llLexerItemNext(start);
		if (l == NULL) {
				//Is a forward declaration(!?!)
				__auto_type semi=expectKeyword(start, ";");
				if(semi) {
						start=llLexerItemNext(start);

						struct parserNodeName *name=(void*)name2;
						__auto_type type=objectByName(name->text);
						if(NULL==type) {
								objectForwardDeclarationCreate(name2,(cls!=NULL)?TYPE_CLASS:TYPE_UNION);
						} else if(cls) {
								if(type->type!=TYPE_CLASS)
										goto incompat ;
						} else if(un) {
								if(type->type!=TYPE_UNION)
										goto incompat ;
						} else if(type->type==TYPE_FORWARD) {
								struct objectForwardDeclaration *f=(void*)type;
								if(f->type != (cls!=NULL)?TYPE_CLASS:TYPE_UNION)
										goto incompat;
						} else {
								//Whine about forward declaration of incompatible existing type
								
						incompat:;
								diagErrorStart(name->base.pos.start, name->base.pos.end);
								diagPushText("Forward declaration ");
								diagPushQoutedText(name->base.pos.start, name->base.pos.end);
								diagPushText(" conflicts with existing type.");
								diagHighlight(name->base.pos.start, name->base.pos.end);
								diagEndMsg();

								referenceType(type);
						}
				}
				goto end;
		}

		strObjectMember members __attribute__((cleanup(strObjectMemberDestroy))) =
		    NULL;
		int findOtherSide=0;
		for (int firstRun=1;;firstRun=0) {
			struct parserNode *r __attribute__((cleanup(parserNodeDestroy))) =
			    expectKeyword(start, "}");
			if (r != NULL) {
					findOtherSide=1;
					start = llLexerItemNext(start);
					break;
			}

			__auto_type newMembers = parseMembers(start, &start);
			members = strObjectMemberConcat(members, newMembers);

			__auto_type kw = expectKeyword(start, ";");
			if (kw != NULL) {
				start = llLexerItemNext(start);
			} else {
					whineExpected(start, ";");
			}
			parserNodeDestroy(&kw);
		}

		struct parserNode *className = NULL;
		if (name2 != NULL) {
			className = name2;
		}

		// Whine is type of same name already exists
		int alreadyExists = 0;
		if (objectByName(((struct parserNodeName *)name2)->text)) {
				__auto_type name=((struct parserNodeName *)name2);

				diagErrorStart(name->base.pos.start, name->base.pos.end);
				diagPushText("Type ");
				diagPushQoutedText(name->base.pos.start, name->base.pos.end);
				diagPushText("already exists!");
				diagHighlight(name->base.pos.start, name->base.pos.end);
				diagEndMsg();

				referenceType(objectByName(name->text));
				
				alreadyExists = 1;
		}

		if (!alreadyExists) {
			if (cls) {
				retValObj =
				    objectClassCreate(className, members, strObjectMemberSize(members));
			} else if (un) {
				retValObj =
				    objectUnionCreate(className, members, strObjectMemberSize(members));
			}
		}
	}
	if (cls) {
		struct parserNodeClassDef def;
		def.base.type = NODE_CLASS_DEF;
		def.name = name2;
		def.type = retValObj;

		retVal = ALLOCATE(def);
	} else if (un) {
		struct parserNodeUnionDef def;
		def.base.type = NODE_UNION_DEF;
		def.name = name2;
		def.type = retValObj;

		retVal = ALLOCATE(def);
	}
end:
	if (end != NULL)
		*end = start;

	parserNodeDestroy(&cls);
	parserNodeDestroy(&un);
	parserNodeDestroy(&baseName);

	if(retVal) {
			if(end)
					assignPosByLexerItems(retVal, originalStart, *end);
			else
					assignPosByLexerItems(retVal, originalStart,NULL);
	}
	return retVal;
}
static void addDeclsToScope(struct parserNode *varDecls) {
	if (varDecls->type == NODE_VAR_DECL) {
		struct parserNodeVarDecl *decl = (void *)varDecls;
		addVar(decl->name, decl->type);
	} else if (varDecls->type == NODE_VAR_DECLS) {
		struct parserNodeVarDecls *decls = (void *)varDecls;
		for (long i = 0; i != strParserNodeSize(decls->decls); i++) {
			struct parserNodeVarDecl *decl = (void *)decls->decls[i];
			addVar(decl->name, decl->type);
		}
	}
}
struct parserNode *parseScope(llLexerItem start, llLexerItem *end,strParserNode vars) {
		__auto_type originalStart=start;

		struct parserNode *lC = NULL, *rC = NULL;
	lC = expectKeyword(start, "{");

	strParserNode nodes = NULL;
	if (lC) {
		enterScope();

		//Add vars to scope
		for(long i=0;i!=strParserNodeSize(vars);i++)
				addDeclsToScope(vars[i]);

		start = llLexerItemNext(start);

		int foundOtherSide=0;
		for (; start != NULL;) {
			rC = expectKeyword(start, "}");

			if (!rC) {
				__auto_type expr = parseStatement(start, &start);
				if (expr)
					nodes = strParserNodeAppendItem(nodes, expr);
			} else {
					foundOtherSide=1;
					leaveScope();
					start = llLexerItemNext(start);
					break;
			}
		}
		
		if(!foundOtherSide) {
				struct parserNodeKeyword *kw=(void*)lC;
				diagErrorStart(kw->base.pos.start,kw->base.pos.end);
				diagPushText("Expecte other '}'.");
				diagHighlight(kw->base.pos.start,kw->base.pos.end);
				diagEndMsg();
		}
	}
	parserNodeDestroy(&lC);
	parserNodeDestroy(&rC);

	if (end != NULL)
		*end = start;

	if (nodes != NULL) {
			struct parserNodeScope scope;
			scope.base.type = NODE_SCOPE;
			scope.stmts = nodes;
			if(end)
					assignPosByLexerItems((struct parserNode*)&scope, originalStart, *end);
			else
					assignPosByLexerItems((struct parserNode*)&scope, originalStart,NULL);
		
			return ALLOCATE(scope);
	}

	return NULL;
}
struct parserNode *parseStatement(llLexerItem start, llLexerItem *end) {
		//If end is NULL,make a dummy version for testing for ";" ahead
		llLexerItem endDummy;
		if(!end)
				end=&endDummy;
		
	__auto_type originalStart = start;
	__auto_type semi = expectKeyword(start, ";");
	if (semi) {
		parserNodeDestroy(&semi);
		if (end != NULL)
			*end = llLexerItemNext(start);

		return NULL;
	}
	struct parserNode *retVal = NULL;

	__auto_type func= parseFunction(originalStart,end);
	if(func) {
			return func;
	}

	__auto_type varDecls = parseVarDecls(start, end);
	if (varDecls) {
		addDeclsToScope(varDecls);

		if(end){
				__auto_type semi=expectKeyword(*end, ";");
				if(!semi)
						whineExpected(*end, ";");
				parserNodeDestroy(&semi);
		}
		
		retVal = varDecls;
		goto end;
	}

	__auto_type labStmt=parseLabel(originalStart,end);
	if(labStmt) {
	  retVal=labStmt;
	  goto end;
	}
	
	start = originalStart;
	__auto_type expr = parseExpression(start, NULL, end);
	if (expr) {
			if(end){
					__auto_type semi=expectKeyword(*end, ";");
					if(!semi)
							whineExpected(*end, ";");
					parserNodeDestroy(&semi);
			}
			
		retVal = expr;
		goto end;
	}

	start = originalStart;
	__auto_type ifStat = parseIf(start, end);
	if (ifStat) {
		retVal = ifStat;
		goto end;
	}

	start = originalStart;
	__auto_type scope = parseScope(start, end,NULL);
	if (scope) {
		retVal = scope;
		goto end;
	}

	__auto_type forStmt = parseFor(originalStart, end);
	if (forStmt) {
		retVal = forStmt;
		goto end;
	}

	__auto_type whileStmt = parseWhile(originalStart, end);
	if (whileStmt) {
		retVal = whileStmt;
		goto end;
	}

	__auto_type doStmt = parseDo(originalStart, end);
	if (doStmt) {
			if(end){
					__auto_type semi=expectKeyword(*end, ";");
					if(!semi)
							whineExpected(*end, ";");
					parserNodeDestroy(&semi);
			}
			
		retVal = doStmt;
		goto end;
	}

	__auto_type caseStmt=parseCase(originalStart,end);
	if(caseStmt) {
	  retVal=caseStmt;
	  goto end;
	}

	__auto_type stStatement=parseSwitch(originalStart,end);
	if(stStatement) {
	  retVal=stStatement;
	  goto end;
	}

	return NULL;
end : {
	if (end) {
		__auto_type semi = expectKeyword(*end, ";");
		if (semi) {
			*end = llLexerItemNext(*end);
			parserNodeDestroy(&semi);
		}
	}
	return retVal;
}
}
struct parserNode *parseWhile(llLexerItem start, llLexerItem *end) {
		__auto_type originalStart=start;
		
	struct parserNode *kwWhile = NULL, *lP = NULL, *rP = NULL, *cond = NULL,
	                  *body = NULL, *retVal = NULL;
	struct parserNode *toFree[] = {
	    lP,
	    rP,
	    kwWhile,
	};
	__auto_type count = sizeof(toFree) / sizeof(*toFree);
	kwWhile = expectKeyword(start, "while");

	int failed=0;
	if (kwWhile) {
		start = llLexerItemNext(start);

		lP = expectOp(start, "(");
		if (!lP) {
				failed=1;
				whineExpected(start, "(");
		} else
		start = llLexerItemNext(start);

		cond = parseExpression(start, NULL, &start);
		if (!cond) {
				failed=1;
				whineExpectedExpr(start);
		}

		rP = expectOp(start, ")");
		if (! rP) {
				failed=1;
				whineExpected(start,")");
		}
		start = llLexerItemNext(start);
		
		body = parseStatement(start, &start);
	} else
		goto fail;
	struct parserNodeWhile node;
	node.base.type = NODE_WHILE;
	node.body = body;
	node.cond = cond;
	if(end)
			assignPosByLexerItems((struct parserNode*)&node, originalStart, *end);
	else
			assignPosByLexerItems((struct parserNode*)&node, originalStart, NULL);
	
	retVal = ALLOCATE(node);
	goto end;
fail:
	parserNodeDestroy(&cond);
	parserNodeDestroy(&body);
end:
	for (int i = 0; i != count; i++)
		parserNodeDestroy(&toFree[i]);

	if (end != NULL)
		*end = start;

	return retVal;
}
struct parserNode *parseFor(llLexerItem start, llLexerItem *end) {
		__auto_type originalStart=start;
		
	struct parserNode *lP = NULL, *rP = NULL, *kwFor = NULL, *semi1 = NULL,
	                  *semi2 = NULL, *cond = NULL, *inc = NULL, *body = NULL,
	                  *init = NULL;
	struct parserNode *toFree[] = {
	    lP, rP, kwFor, semi1, semi2,
	};
	__auto_type count = sizeof(toFree) / sizeof(*toFree);

	struct parserNode *retVal = NULL;

	kwFor = expectKeyword(start, "for");
	int leaveScope2 = 0;
	if (kwFor) {
		start = llLexerItemNext(start);

		lP = expectOp(start, "(");
		enterScope();
		leaveScope2 = 1;
		start = llLexerItemNext(start);

		__auto_type originalStart = start;
		init = parseVarDecls(originalStart, &start);
		if (!init)
			init = parseExpression(originalStart, NULL, &start);
		else
			addDeclsToScope(init);

		semi1 = expectKeyword(start, ";");
		start = llLexerItemNext(start);

		cond = parseExpression(start, NULL, &start);

		semi2 = expectKeyword(start, ";");
		start = llLexerItemNext(start);

		inc = parseExpression(start, NULL, &start);

		rP = expectOp(start, ")");
		start = llLexerItemNext(start);

		body = parseStatement(start, &start);

		struct parserNodeFor forStmt;
		forStmt.base.type = NODE_FOR;
		forStmt.body = body;
		forStmt.cond = cond;
		forStmt.init = init;
		forStmt.inc = inc;

		retVal = ALLOCATE(forStmt);
		if(end)
				assignPosByLexerItems(retVal, originalStart, *end);
		else
				assignPosByLexerItems(retVal, originalStart, NULL);
		goto end;
	}
fail:
	parserNodeDestroy(&body);
	parserNodeDestroy(&inc);
	parserNodeDestroy(&cond);
	parserNodeDestroy(&init);
end:
	if (leaveScope2)
		leaveScope();

	for (int i = 0; i != count; i++)
		parserNodeDestroy(&toFree[i]);

	if (end != NULL)
		*end = start;

	return retVal;
}
struct parserNode *parseDo(llLexerItem start, llLexerItem *end) {
		__auto_type originalStart=start;
		
	__auto_type kwDo = expectKeyword(start, "do");
	if (kwDo == NULL)
		return NULL;

	struct parserNode *body = NULL, *cond = NULL, *kwWhile = NULL, *lP = NULL,
	                  *rP = NULL;
	struct parserNode *retVal = NULL;
	struct parserNode *toFree[] = {lP, rP, kwWhile, kwDo};
	__auto_type count = sizeof(toFree) / sizeof(*toFree);

	start = llLexerItemNext(start);
	body = parseStatement(start, &start);

	int failed=0;
	
	kwWhile = expectKeyword(start, "while");
	if(kwWhile) {
		start = llLexerItemNext(start);
	} else {
			failed=1;
			whineExpected(start, "while");
	}
	
		lP = expectOp(start, "(");
		if (!lP) {
			failed=1;
			whineExpected(start, "(");
		} else
				start = llLexerItemNext(start);

		cond = parseExpression(start, NULL, &start);
		if(cond==NULL) {
				failed=1;
				whineExpectedExpr(start);
		}
		
		rP = expectOp(start, ")");
		if (!rP) {
				failed=1;
				whineExpected(start, ")");
		}
		start = llLexerItemNext(start);
		
	struct parserNodeDo doNode;
	doNode.base.type = NODE_DO;
	doNode.body = body;
	doNode.cond = cond;
	if(end)
				assignPosByLexerItems((struct parserNode*)&doNode,originalStart, *end);
	else
			assignPosByLexerItems((struct parserNode*)&doNode, originalStart,NULL);

	
	retVal = ALLOCATE(doNode);
	goto end;
fail:
	parserNodeDestroy(&cond);
	parserNodeDestroy(&body);
end:
	for (long i = 0; i != count; i++)
		parserNodeDestroy(&toFree[i]);

	if (end != NULL)
		*end = start;

	return retVal;
}
struct parserNode *parseIf(llLexerItem start, llLexerItem *end) {
		__auto_type originalStart=start;
		
		__auto_type kwIf = expectKeyword(start, "if");
	struct parserNode *lP = NULL, *rP = NULL, *cond = NULL, *elKw = NULL,
	                  *elBody = NULL;

	struct parserNode *retVal = NULL;
	int failed=0;
	if (kwIf) {
		start = llLexerItemNext(start);

		lP = expectOp(start, "(");
		if (!lP) {
				failed=1;
				whineExpected(start,"(");
		}
		start = llLexerItemNext(start);

		cond = parseExpression(start, NULL, &start);
		if (!cond) {
				failed=1;
				whineExpectedExpr(start);
		}

		rP = expectOp(start, ")");
		if (!rP) {
				failed=1;
				whineExpected(start,")");
		} else 
		start = llLexerItemNext(start);

		__auto_type body = parseStatement(start, &start);

		elKw = expectKeyword(start, "else");
		start = llLexerItemNext(start);
		if (elKw) {
			elBody = parseStatement(start, NULL);
			failed|=elBody==NULL;
		}
		
		struct parserNodeIf ifNode;
		ifNode.base.type = NODE_IF;
		ifNode.cond = cond;
		ifNode.body = body;
		ifNode.el = elBody;

		if(end)
				assignPosByLexerItems((struct parserNode*)&ifNode, originalStart, *end);
		else
				assignPosByLexerItems((struct parserNode*)&ifNode, originalStart,NULL);
		
		// Dont free cond ahead
		cond = NULL;
		elBody=NULL;

		retVal = ALLOCATE(ifNode);
	}

	goto end;
fail:
	retVal = NULL;

end:;
	struct parserNode *toFree[] = {
	    lP,
	    rP,
	    cond,
	    elBody,
	};
	__auto_type count = sizeof(toFree) / sizeof(*toFree);
	for (int i = 0; i != count; i++)
		parserNodeDestroy(&toFree[i]);

	return retVal;
}
/**
 * Switch section
 */
static strParserNode switchStack = NULL;
static void switchStackDestroy() __attribute__((destructor));
static void switchStackDestroy() { strParserNodeDestroy(&switchStack); }
static long getNextCaseValue(struct parserNode *parent) {
	struct parserNode *entry = NULL;

	if (parent->type == NODE_SWITCH) {
		struct parserNodeSwitch *node = (void *)parent;

		__auto_type len = strParserNodeSize(node->caseSubcases);
		if (len > 0) {
			entry = node->caseSubcases[len - 1];
			goto getValue;
		} else {
			return 0;
		}
	} else if (parent->type == NODE_SUBSWITCH) {
		struct parserNodeSubSwitch *node = (void *)parent;
		__auto_type len = strParserNodeSize(node->caseSubcases);
		if (len > 0) {
			entry = node->caseSubcases[len - 1];
			goto getValue;
		} else {
			return 0;
		}
	} else {
		assert(0);
	}
getValue : {
	if (entry->type == NODE_CASE) {
		struct parserNodeCase *node = (void *)entry;
		return node->valueUpper+1;
	} else if (entry->type == NODE_SUBSWITCH) {
		return getNextCaseValue(entry);
	} else {
		assert(0);
	}
	return -1;
}
}
struct parserNode *parseSwitch(llLexerItem start,llLexerItem *end) {
		__auto_type originalStart=start;
		
  struct parserNode *kw=NULL,*lP=NULL,*rP=NULL,*exp=NULL,*body=NULL,*retVal=NULL;
  struct parserNode *toFree[]={kw,lP,rP};
  __auto_type count=sizeof(toFree)/sizeof(toFree);

  kw=expectKeyword(start,"switch");
  int success=0;
  if(kw) {
    success=1;
    start=llLexerItemNext(start);

    lP=expectOp(start,"(");
    if(!lP) {
						success=0;
						whineExpected(start,"(");
				} else
						start=llLexerItemNext(start);

    exp=parseExpression(start,NULL,&start);
    if(!exp)
      success=0;

    rP=expectOp(start,")");
    if(!rP) {
      success=0;
						whineExpected(start,")");
				} else
						start=llLexerItemNext(start);

    struct parserNodeSwitch swit;
    swit.base.type=NODE_SWITCH;
    swit.body=body;
    swit.caseSubcases=NULL;
    swit.dft=NULL;
    swit.exp=exp;
    retVal=ALLOCATE(swit);

    //Push to stack
				long oldStackSize=strParserNodeSize(switchStack);
				switchStack=strParserNodeAppendItem(switchStack, retVal);

    body=parseStatement(start,&start);
				
				if(!success)
						parserNodeDestroy(&retVal);

				//Whine about unterminated sub switches
				//+1 ignores current switch entry
				for(long i=oldStackSize+1;i!=strParserNodeSize(switchStack);i++) {
						assert(switchStack[i]->type==NODE_SUBSWITCH);

						struct parserNodeSubSwitch *sub=(void*)switchStack[i];
						assert(sub->start->type==NODE_LABEL);
						struct parserNodeLabel *lab=(void*)sub->start;
						struct parserNodeName *name=(void*)lab->name;

						diagErrorStart(  name->base.pos.start,name->base.pos.end);
						diagPushText("Unterminated sub-switch.");
						diagHighlight(  name->base.pos.start,name->base.pos.end);
						diagEndMsg();

						struct parserNodeKeyword *kw2=(void*)kw;
						diagNoteStart(  kw2->base.pos.start,kw2->base.pos.end);
						diagPushText("From here:");
						diagHighlight(  kw2->base.pos.start,kw2->base.pos.end);
						diagEndMsg();
				}
				
				switchStack = strParserNodeResize(switchStack,oldStackSize);
  }
    

  for(int i=0;i!=count;i++)
    parserNodeDestroy(&toFree[i]);

		if(retVal) {
				if(end)
						assignPosByLexerItems(retVal, originalStart, *end);
				else
						assignPosByLexerItems(retVal, originalStart, NULL);
		}
  return retVal;
}
static long searchForNode(const strParserNode nodes,const struct parserNode *node) {
  long retVal=0;
  for(long i=0;i!=strParserNodeSize(nodes);i++)
    if(nodes[i]==node)
      return i;

  return -1;
}
struct parserNode *parseLabel(llLexerItem start, llLexerItem *end) {
	struct parserNode *colon1 = NULL, *retVal = NULL;
	__auto_type originalStart=start;
	
	__auto_type name = nameParse(start, NULL, &start);
	if (name == NULL)
		return NULL;
	colon1 = expectKeyword(start, ":");
	if (!colon1)
		goto end;
	start=llLexerItemNext(start);

	struct parserNodeLabel lab;
	lab.base.type=NODE_LABEL;
	lab.name=name;
	retVal=ALLOCATE(lab);
	
	struct parserNodeName *name2 = (void *)name;
	if (0 == strcmp(name2->text, "end") && 0 != strParserNodeSize(switchStack)) {
	  //Create sub-switch
	  struct parserNodeSubSwitch sub;
	  //Pop
		switchStack =
		    strParserNodeResize(switchStack, strParserNodeSize(switchStack) - 1);
	} else if (0 == strcmp(name2->text, "start") &&
	           0 != strParserNodeSize(switchStack)) {
			//Create sub-switch
			struct parserNodeSubSwitch sub;
			sub.base.type = NODE_SUBSWITCH;
			sub.caseSubcases=NULL;
			sub.dft=NULL;
			sub.end=NULL;
			sub.start=retVal;
struct parserNode *top=switchStack[strParserNodeSize(switchStack)-1];
	struct parserNode *sub2 = ALLOCATE(sub);
			retVal=sub2;
	if(top->type
				==NODE_SWITCH) {
			struct parserNodeSwitch *swit=(void*)top;
			swit->caseSubcases=strParserNodeAppendItem(swit->caseSubcases,sub2);
	} else if(top->type
											==NODE_SUBSWITCH) {
			struct parserNodeSubSwitch *sub=(void*)top;
			sub->caseSubcases=strParserNodeAppendItem(sub->caseSubcases,sub2);
	}
	

			switchStack = strParserNodeAppendItem(switchStack, retVal);
	}
	;
end:
	//Check if success
	if(!retVal)
			start=originalStart;
	if(end!=NULL)
			*end=start;

	if(retVal) {
			if(end)
					assignPosByLexerItems(retVal, originalStart, *end);
			else
					assignPosByLexerItems(retVal, originalStart,NULL);
	}
	
	parserNodeDestroy(&colon1);

	return retVal;
}
static void ensureCaseDoesntExist(long valueLow,long valueHigh,long rangeStart,long rangeEnd) {
		long len=strParserNodeSize(switchStack);
		for(long i=len-1;i>=0;i--) {
				strParserNode cases=NULL;
				if(switchStack[i]->type==NODE_SWITCH) {
						cases=((struct parserNodeSwitch*)switchStack[i])->caseSubcases;
				} else if(switchStack[i]->type==NODE_SUBSWITCH) {
						cases=((struct parserNodeSubSwitch*)switchStack[i])->caseSubcases;
				} else assert(0);

				for(long i=0;i!=strParserNodeSize(cases);i++) {
						if(cases[i]->type==NODE_CASE) {
								struct parserNodeCase *cs=(void*)cases[i];

								int consumed=0;
								//Check if high is in range
								consumed|=valueHigh<cs->valueUpper&&valueHigh<=cs->valueLower;
								//Check if low is in range
								consumed|=valueLow>=cs->valueUpper&&valueLow<=cs->valueLower;
								//Check if range is consumed
								consumed|=valueHigh>=cs->valueUpper&&valueLow<=cs->valueLower;
								if(consumed) {
										struct parserNodeKeyword *kw=(void*)cs->label;
										diagErrorStart(rangeStart,rangeEnd);
										diagPushText("Conflicting case statements.");
										diagHighlight(rangeStart, rangeEnd);
										diagEndMsg();

										diagNoteStart(kw->base.pos.start, kw->base.pos.end);
										diagPushText("Previous case here: ");
										diagHighlight(kw->base.pos.start, kw->base.pos.end);
										diagEndMsg();
										goto end;
								}
						}
				}
		}
	end:;
		
}
static void whineCaseNoSwitch(const struct parserNode * kw,long start,long end) {
		diagErrorStart(start, end);
		const struct parserNodeKeyword *kw2=(void*)kw;
		char buffer[128];
		sprintf(buffer, "Unexpected '%s'. Not in switch statement. ", kw2->text);
		diagPushText(buffer);
		diagHighlight(start,end);
		diagEndMsg();
}
struct parserNode *parseCase(llLexerItem start, llLexerItem *end) {
		__auto_type originalStart=start;
		
		struct parserNode *kwCase = NULL, *colon = NULL,*dotdotdot=NULL;
	struct parserNode *retVal = NULL;
	int failed=0;
	kwCase = expectKeyword(start, "case");
	/**
		 * Get parent switch
		 */
		struct parserNode *parent = NULL;
		if (strParserNodeSize(switchStack) == 0) {
		} else {
			parent = switchStack[strParserNodeSize(switchStack) - 1];
		}
	if (kwCase) {
		start = llLexerItemNext(start);
		
		//Used later
		__auto_type valueStart=start;
		
		int gotInt = 0;
	long caseValue = -1,caseValueUpper=-1;
		if (start != NULL) {
			if (llLexerItemValuePtr(start)->template == &intTemplate) {
				gotInt = 1;
				struct lexerInt *i = lexerItemValuePtr(llLexerItemValuePtr(start));
				caseValue = i->value.sLong;

				start=llLexerItemNext(start);
			} else if (parent) {
				caseValue = getNextCaseValue(parent);
			}
		}

		dotdotdot=expectKeyword(start, "...");
		if(dotdotdot) {
				start=llLexerItemNext(start);
				if(start) {
						if (llLexerItemValuePtr(start)->template == &intTemplate) {
								struct lexerInt *i = lexerItemValuePtr(llLexerItemValuePtr(start));
								caseValueUpper = i->value.sLong;

								start=llLexerItemNext(start);
						} else {
								failed=1;
						}
				}
		}
		
		colon = expectKeyword(start, ":");
		if (colon) { 
			start = llLexerItemNext(start);
		} else {
				failed=1;
				whineExpected(start, ":");
		}

		long startP,endP;
			getStartEndPos(valueStart,start,&startP,&endP);
		if(parent==NULL)
				whineCaseNoSwitch(kwCase,startP,endP);
		
		caseValueUpper=(caseValueUpper==-1)?caseValue:caseValue+1;
	
				ensureCaseDoesntExist(caseValue, caseValueUpper,startP,endP);

		struct parserNodeCase caseNode;
		caseNode.base.type = NODE_CASE;
		caseNode.parent = parent;
		caseNode.valueLower = caseValue;
		caseNode.valueUpper=caseValueUpper;
		retVal = ALLOCATE(caseNode);
		if(end)
				assignPosByLexerItems(retVal, originalStart, *end);
		else
				assignPosByLexerItems(retVal, originalStart, NULL);

		if(parent->type==NODE_SWITCH) {
					struct parserNodeSwitch *swit=(void*)parent;
					swit->caseSubcases=strParserNodeAppendItem(swit->caseSubcases,retVal);
			} else if(parent->type==NODE_SUBSWITCH) {
					struct parserNodeSubSwitch *sub=(void*)parent;
					sub->caseSubcases=strParserNodeAppendItem(sub->caseSubcases,retVal);
			}
	} else {
			__auto_type defStart=start;
	  kwCase=expectKeyword(start, "default");
	  if(kwCase) {
	    start=llLexerItemNext(start);

	    colon=expectKeyword(start, ":");
	    if(colon) {
	      start=llLexerItemNext(start);
					} else {
							whineExpected(start, ":");
					}
	  } else
					goto end;
			
			struct parserNodeDefault dftNode;
			dftNode.base.type=NODE_DEFAULT;
			dftNode.parent=parent;
			retVal=ALLOCATE(dftNode);
			if(end)
					assignPosByLexerItems(retVal, originalStart, *end);
			else
					assignPosByLexerItems(retVal, originalStart, NULL);;

			long startP,endP;
			getStartEndPos(start,start,&startP,&endP);
			if(parent==NULL) {
				whineCaseNoSwitch(kwCase,startP,endP);
			} else  {
					if(parent->type==NODE_SWITCH) {
							struct parserNodeSwitch *swit=(void*)parent;
							swit->dft=retVal;
							} else if(parent->type==NODE_SUBSWITCH) {
							struct parserNodeSubSwitch *sub=(void*)parent;
							sub->dft=retVal;
					}
			}
	}
end:
	if (end)
		*end = start;

	parserNodeDestroy(&dotdotdot);
	parserNodeDestroy(&colon);
	
	return retVal;
}
struct parserNode *parseFunction(llLexerItem start,llLexerItem *end) {
		__auto_type originalStart=start;
		struct parserNode *name=nameParse(start, NULL, &start);
		if(!name)
				return NULL;
		parserNodeDestroy(&name);
		
		struct parserNodeName *nm=(void*)name;
		__auto_type baseType=objectByName(nm->text);
		if(!baseType)
				return NULL;

		long ptrLevel=0;
		for(;;) {
				__auto_type star =expectOp(start, "*");
				if(!star)
						break;
				
				parserNodeDestroy(&star);
				ptrLevel++,start=llLexerItemNext(start);
		}

		name= nameParse(start, NULL, &start);
		if(!name)
				return NULL;

		struct parserNode* lP=NULL,*rP=NULL; 
		struct parserNode *toKill[]={lP,rP};
		strParserNode args=NULL;
		long count=sizeof(toKill)/sizeof(*toKill);
		lP=expectOp(start, "(");
		if(!lP)
				goto fail;
		start=llLexerItemNext(start);
		
		for(int firstRun=1;;firstRun=0) {
				rP=expectOp(start, ")");
				if(rP) {
						start=llLexerItemNext(start);
						break;
				}
				if(!firstRun) {
						__auto_type comma=expectOp(start, ",");
						if(comma==NULL)
								whineExpected(start, ",");
						else
								start=llLexerItemNext(start);
				}
				
				struct parserNode *decl= parseSingleVarDecl(start, &start);
				if(decl)
						args=strParserNodeAppendItem(args, decl);
		}

		struct object *retType=baseType;
		for(long i=0;i!=ptrLevel;i++)
				retType=objectPtrCreate(retType);
		
		strFuncArg fargs=NULL;
		for(long i=0;i!=strParserNodeSize(args);i++) {
				if(args[i]->type!=NODE_VAR_DECL) {
						//TODO whine
						continue;
				}
				struct parserNodeVarDecl *decl=(void*)args[i];
				struct objectFuncArg arg;
				arg.dftVal=decl->dftVal;
				arg.name=decl->name;
				arg.type=decl->type;

				//TODO whine on metadata
				strParserNodeDestroy2(&decl->metaData);

				fargs=strFuncArgAppendItem(fargs, arg);
		}

		struct object *funcType;
		funcType=objectFuncCreate(retType, fargs);
		strFuncArgDestroy(&fargs);
		
		struct parserNode *retVal=NULL;
		__auto_type scope=parseScope(start, end, args);
		if(!scope) {
				//If no scope follows,is a forward declaration
				__auto_type semi=expectKeyword(start, ";"); 
				if(!semi)
						whineExpected(start, ";");
				else
						start=llLexerItemNext(start);

				struct parserNodeFuncForwardDec forward;
				forward.base.type=NODE_FUNC_FORWARD_DECL;
				forward.funcType=funcType;
				forward.name=name;
				
				retVal=ALLOCATE(forward);
		} else {
				//Has a function body
				struct parserNodeFuncDef func;
				func.base.type=NODE_FUNC_DEF;
				func.bodyScope=scope;
				func.funcType=funcType;
				func.name=name;
				
				retVal=ALLOCATE(func);
		}

		if(end)
				*end=start;

		if(end)
				assignPosByLexerItems(retVal, originalStart, *end);
		else
				assignPosByLexerItems(retVal, originalStart, NULL);

		strParserNodeDestroy(&args);

		addFunc(name, funcType, retVal);
		return retVal;
	fail:
		parserNodeDestroy(&name);
		strParserNodeDestroy2(&args);
		for(long i=0;i!=count;i++)
				parserNodeDestroy(&toKill[i]);
		return NULL;
}
