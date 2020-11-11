#include <assert.h>
#include <parserA.h>
#include <object.h>
#include <lexer.h>
#include <parserB.h>
#include <stdio.h>
#include <preprocessor.h>
#include <diagMsg.h>
static FILE *file;
static void createFile(const char *text) {
		if(file!=NULL)
				fclose(file);

		char buffer[1024];
		strcpy(buffer, tmpnam(NULL));
		
		file=fopen(buffer, "w");
		fwrite(text, 1, strlen(text), file);
		fclose(file);

		file=fopen(buffer, "r");
		strTextModify mods;
		strFileMappings fMappings;
		int err;
		FILE *res=createPreprocessedFile(buffer, &mods, &fMappings, &err);
		assert(!err);
		diagInstCreate(DIAG_ANSI_TERM,  fMappings, mods, buffer, stdout);
		file=res;

		killParserData();
		initParserData();
}
STR_TYPE_DEF(char, Char);
STR_TYPE_FUNCS(char, Char);
static int charCmp(const void *a, const void *b) {
	return *(char *)a == *(char *)b;
}
static void precParserTests() {
	const char *text = "0 + 1 + 2 + 3";
	__auto_type textStr = strCharAppendData(NULL, text, strlen(text));

	createFile(text);
	__auto_type lexItems = lexText((struct __vec *)textStr, NULL);

	struct parserNode *node =
	    parseExpression(llLexerItemFirst(lexItems), NULL, NULL);
	assert(node);
	{
		/**
		 *       +
		 *      / \
		 *     +   3
		 *    / \
		 *   +   2
		 *  / \
		 * 0   1
		 */
		assert(node->type == NODE_BINOP);
		struct parserNodeBinop *binop = (void *)node;

		assert(binop->b->type == NODE_LIT_INT);
		struct parserNodeLitInt *intLit = (void *)binop->b;
		assert(intLit->value.value.sInt == 3);

		assert(binop->a->type == NODE_BINOP);
		binop = (void *)binop->a;

		intLit = (void *)binop->b;
		assert(intLit->value.value.sInt == 2);

		binop = (void *)binop->a;

		intLit = (void *)binop->a;
		assert(intLit->value.value.sInt == 0);
		intLit = (void *)binop->b;
		assert(intLit->value.value.sInt == 1);
	}
	for (int i = 0; i != 2; i++) {
		strCharDestroy(&textStr);
		if (i == 0)
			text = "a=b=c=d";
		else if (i == 1)
			text = "a=(b)=(c)=(d)";
		createFile(text);
		
		textStr = strCharAppendData(NULL, text, strlen(text));
		int err;
		lexItems = lexText((struct __vec *)textStr, &err);
		assert(!err);

		struct parserNode *node =
		    parseExpression(llLexerItemFirst(lexItems), NULL, NULL);
		assert(node);
		{
			assert(node->type == NODE_BINOP);
			struct parserNodeBinop *binop = (void *)node;

			assert(binop->a->type == NODE_NAME);
			struct parserNodeName *token = (void *)binop->a;
			assert(0 == strcmp(token->text, "a"));

			assert(binop->b->type == NODE_BINOP);
			binop = (void *)binop->b;

			assert(binop->a->type == NODE_NAME);
			token = (void *)binop->a;
			assert(0 == strcmp(token->text, "b"));

			assert(binop->b->type == NODE_BINOP);
			binop = (void *)binop->b;

			assert(binop->a->type == NODE_NAME);
			token = (void *)binop->a;
			assert(0 == strcmp(token->text, "c"));
			assert(binop->b->type == NODE_NAME);
			token = (void *)binop->b;
			assert(0 == strcmp(token->text, "d"));
		}
	}
	strCharDestroy(&textStr);
	text = "a,b,c";
	createFile(text);
	textStr = strCharAppendData(NULL, text, strlen(text));
	int err;
	lexItems = lexText((struct __vec *)textStr, &err);
	assert(!err);

	node = parseExpression(llLexerItemFirst(lexItems), NULL, NULL);
	assert(node);
	{
		assert(node->type == NODE_COMMA_SEQ);
		struct parserNodeCommaSeq *seq = (void *)node;
		assert(strParserNodeSize(seq->items) == 3);

		const char *names[] = {"a", "b", "c"};
		for (long i = 0; i != 3; i++) {
			assert(seq->items[i]->type == NODE_NAME);
			struct parserNodeName *name = (void *)seq->items[i];
			assert(0 == strcmp(name->text, names[i]));
		}
	}
	strCharDestroy(&textStr);
	text = "*++a,a++.b.c++";
	createFile(text);
	textStr = strCharAppendData(NULL, text, strlen(text));
	err = 0;
	lexItems = lexText((struct __vec *)textStr, &err);
	assert(!err);

	node = parseExpression(llLexerItemFirst(lexItems), NULL, NULL);
	assert(node);
	{
		assert(node->type == NODE_COMMA_SEQ);
		struct parserNodeCommaSeq *seq = (void *)node;
		assert(strParserNodeSize(seq->items) == 2);

		struct parserNodeUnop *unop = (void *)seq->items[0];
		struct parserNodeOpTerm *op = (void *)unop->op;
		assert(op->base.type == NODE_OP);
		assert(0 == strcmp(op->text, "*"));
		assert(unop->isSuffix == 0);

		unop = (void *)unop->a;
		op = (void *)unop->op;
		assert(op->base.type == NODE_OP);
		assert(0 == strcmp(op->text, "++"));
		assert(unop->isSuffix == 0);

		struct parserNodeName *name = (void *)unop->a;
		assert(name->base.type == NODE_NAME);
		assert(0 == strcmp(name->text, "a"));

		unop = (void *)seq->items[1];
		op = (void *)unop->op;
		assert(op->base.type == NODE_OP);
		assert(0 == strcmp(op->text, "++"));
		assert(unop->isSuffix == 1);

		struct parserNodeBinop *binop = (void *)unop->a;
		name = (void *)binop->b;
		assert(name->base.type == NODE_NAME);
		assert(0 == strcmp(name->text, "c"));

		binop = (void *)binop->a;
		name = (void *)binop->b;
		assert(name->base.type == NODE_NAME);
		assert(0 == strcmp(name->text, "b"));

		unop = (void *)binop->a;
		op = (void *)unop->op;
		assert(op->base.type == NODE_OP);
		assert(0 == strcmp(op->text, "++"));
		assert(unop->isSuffix == 1);

		name = (void *)unop->a;
		assert(name->base.type == NODE_NAME);
		assert(0 == strcmp(name->text, "a"));
	}

	text = "c=!(a+1*3)+ ++b";
	createFile(text);
	textStr = strCharAppendData(NULL, text, strlen(text));
	err = 0;
	lexItems = lexText((struct __vec *)textStr, &err);
	assert(!err);
	node = parseExpression(lexItems, NULL, NULL);
	assert(node);
	{
		// c =
		assert(node->type == NODE_BINOP);
		struct parserNodeBinop *binop = (void *)node;
		struct parserNodeOpTerm *op = (void *)binop->op;
		assert(op->base.type == NODE_OP);
		assert(0 == strcmp(op->text, "="));
		struct parserNodeName *name = (void *)binop->a;
		assert(name->base.type == NODE_NAME);
		assert(0 == strcmp(name->text, "c"));

		//+
		binop = (void *)binop->b;
		assert(binop->base.type == NODE_BINOP);
		op = (void *)binop->op;
		assert(op->base.type == NODE_OP);
		assert(0 == strcmp(op->text, "+"));
		__auto_type plus1 = binop;

		//++
		struct parserNodeUnop *unop1 = (void *)plus1->b;
		assert(unop1->base.type == NODE_UNOP);
		name = (void *)unop1->a;
		assert(name->base.type == NODE_NAME);
		assert(0 == strcmp(name->text, "b"));
		op = (void *)unop1->op;
		assert(op->base.type == NODE_OP);
		assert(0 == strcmp(op->text, "++"));

		//!
		struct parserNodeUnop *unop2 = (void *)plus1->a;
		assert(unop2->base.type == NODE_UNOP);
		op = (void *)unop2->op;
		assert(op->base.type == NODE_OP);
		assert(0 == strcmp(op->text, "!"));

		struct parserNodeBinop *plus2 = (void *)unop2->a;
		name = (void *)plus2->a;
		assert(name->base.type == NODE_NAME);
		assert(0 == strcmp(name->text, "a"));
		op = (void *)plus2->op;
		assert(op->base.type == NODE_OP);
		assert(0 == strcmp(op->text, "+"));

		struct parserNodeBinop *times = (void *)plus2->b;
		struct parserNodeLitInt *intLit = (void *)times->a;
		assert(intLit->base.type == NODE_LIT_INT);
		assert(intLit->value.value.sInt == 1);
		intLit = (void *)times->b;
		assert(intLit->base.type == NODE_LIT_INT);
		assert(intLit->value.value.sInt == 3);
	}
	strCharDestroy(&textStr);
	text = "a(b(),,c)";
	createFile(text);
	textStr = strCharAppendData(NULL, text, strlen(text));
	err = 0;
	lexItems = lexText((struct __vec *)textStr, &err);
	assert(!err);
	node = parseExpression(lexItems, NULL, NULL);
	assert(node);
	{
		assert(node->type == NODE_FUNC_CALL);
		struct parserNodeFuncCall *call = (void *)node;
		struct parserNodeName *name = (void *)call->func;
		assert(name->base.type == NODE_NAME);
		assert(0 == strcmp(name->text, "a"));

		assert(strParserNodeSize(call->args) == 3);
		struct parserNodeFuncCall *call2 = (void *)call->args[0];
		assert(call2->base.type == NODE_FUNC_CALL);
		name = (void *)call2->func;
		assert(name->base.type == NODE_NAME);
		assert(0 == strcmp(name->text, "b"));
		assert(strParserNodeSize(call2->args) == 0);

		assert(call->args[1] == NULL);

		name = (void *)call->args[2];
		assert(name->base.type == NODE_NAME);
		assert(0 == strcmp(name->text, "c"));
	}
}
static void varDeclTests() {
	const char *text = "I64i x=10";
	createFile(text);
	__auto_type textStr = strCharAppendData(NULL, text, strlen(text));

	__auto_type lexItems = lexText((struct __vec *)textStr, NULL);
	__auto_type decl = parseVarDecls(lexItems, NULL);
	assert(decl);
	{
		assert(decl->type == NODE_VAR_DECL);
		struct parserNodeVarDecl *declNode = (void *)decl;
		struct parserNodeName *name = (void *)declNode->name;
		assert(name->base.type == NODE_NAME);
		assert(0 == strcmp(name->text, "x"));

		struct parserNodeLitInt *dftVal = (void *)declNode->dftVal;
		assert(dftVal->base.type == NODE_LIT_INT);
		assert(dftVal->value.value.sInt == 10);

		assert(objectByName("I64i") == declNode->type);
	}
	text = "I64i **x[1][2][3]";
	createFile(text);
	textStr = strCharAppendData(NULL, text, strlen(text));

	int err = 0;
	lexItems = lexText((struct __vec *)textStr, &err);
	assert(!err);

	decl = parseVarDecls(lexItems, NULL);
	assert(decl);
	{
		assert(decl->type == NODE_VAR_DECL);
		struct parserNodeVarDecl *declNode = (void *)decl;
		struct parserNodeName *name = (void *)declNode->name;
		assert(0 == strcmp(name->text, "x"));
		assert(declNode->dftVal == NULL);

		__auto_type type = declNode->type;
		for (int i = 3 - 1; i >= 0; i--) {
			struct objectArray *array = (void *)type;
			assert(array->base.type == TYPE_ARRAY);
			assert(array->dim->type == NODE_LIT_INT);
			struct parserNodeLitInt *lit = (void *)array->dim;
			assert(lit->value.value.sInt == i + 1);

			type = (void *)array->type;
		}

		for (int i = 0; i != 2; i++) {
			struct objectPtr *ptr = (void *)type;
			assert(ptr->base.type == TYPE_PTR);
			type = (void *)ptr->type;
		}

		assert(type == objectByName("I64i"));
	}
	text = "I64i a=1,*b=2,c=3";
	createFile(text);
	textStr = strCharAppendData(NULL, text, strlen(text));

	err = 0;
	lexItems = lexText((struct __vec *)textStr, &err);
	assert(!err);

	decl = parseVarDecls(lexItems, NULL);
	assert(decl);
	{
		assert(decl->type == NODE_VAR_DECLS);
		struct parserNodeVarDecls *decls = (void *)decl;

		assert(strParserNodeSize(decls->decls) == 3);
		const char *names[] = {"a", "b", "c"};
		for (int i = 0; i != 3; i++) {
			struct parserNodeVarDecl *decl = (void *)decls->decls[i];
			assert(decl->base.type == NODE_VAR_DECL);

			struct parserNodeName *name = (void *)decl->name;
			assert(name->base.type == NODE_NAME);
			assert(0 == strcmp(names[i], name->text));

			struct parserNodeLitInt *lit = (void *)decl->dftVal;
			assert(lit->base.type == NODE_LIT_INT);
			assert(lit->value.value.sInt == i + 1);

			struct object *type = decl->type;
			if (i == 1) {
				assert(type->type == TYPE_PTR);
				struct objectPtr *ptr = (void *)type;
				type = ptr->type;
			}
			assert(type->type == TYPE_I64i);
		}
	}
	//Type cast
	text = "10(U8i)";
	createFile(text);
	textStr = strCharAppendData(NULL, text, strlen(text));
		err = 0;
	lexItems = lexText((struct __vec *)textStr, &err);
	assert(!err);
	__auto_type cast= parseExpression(lexItems, NULL, NULL);
	assert(cast);
	{
			assert(cast->type==NODE_TYPE_CAST);
			struct parserNodeTypeCast *cast2=(void*)cast;
			assert(cast2->exp->type==NODE_LIT_INT);
			assert(cast2->type==objectByName("U8i"));
	}
	//
	
	text = "I64i (*func)(I64i(*foo)(),I64i x)";
	createFile(text);
	textStr = strCharAppendData(NULL, text, strlen(text));

	err = 0;
	lexItems = lexText((struct __vec *)textStr, &err);

	int i = 0;

	assert(!err);
	decl = parseVarDecls(lexItems, NULL);
	{
		assert(decl->type == NODE_VAR_DECL);
		struct parserNodeVarDecl *decl2 = (void *)decl;
		struct parserNodeName *name = (void *)decl2->name;
		assert(0 == strcmp(name->text, "func"));

		struct objectPtr *ptr = (void *)decl2->type;
		assert(ptr->base.type == TYPE_PTR);

		struct objectFunction *func = (void *)ptr->type;
		assert(func->base.type == TYPE_FUNCTION);
		assert(strFuncArgSize(func->args) == 2);
		assert(func->retType == objectByName("I64i"));

		ptr = (void *)func->args[0].type;
		assert(ptr->base.type == TYPE_PTR);

		struct objectFunction *func2 = (void *)ptr->type;
		assert(func2->base.type == TYPE_FUNCTION);
		assert(strFuncArgSize(func2->args) == 0);
		assert(func2->retType == objectByName("I64i"));

		name = (void *)func->args[0].name;
		assert(name->base.type == NODE_NAME);
		assert(0 == strcmp(name->text, "foo"));

		name = (void *)func->args[1].name;
		assert(name->base.type == NODE_NAME);
		assert(0 == strcmp(name->text, "x"));

		assert((void *)func->args[1].type == objectByName("I64i"));
	}

	text = "I64i x dft_val 10 format \"%s\"";
	createFile(text);
	textStr = strCharAppendData(NULL, text, strlen(text));

	err = 0;
	lexItems = lexText((struct __vec *)textStr, &err);
	assert(!err);

	decl = parseVarDecls(lexItems, NULL);
	assert(decl);
	{
		assert(decl->type == NODE_VAR_DECL);
		struct parserNodeVarDecl *decl2 = (void *)decl;

		assert(strParserNodeSize(decl2->metaData) == 2);

		struct parserNodeMetaData *m1 = (void *)decl2->metaData[0];
		assert(m1->name->type == NODE_NAME);
		struct parserNodeName *name = (void *)m1->name;
		assert(0 == strcmp(name->text, "dft_val"));
		assert(m1->value->type == NODE_LIT_INT);

		struct parserNodeMetaData *m2 = (void *)decl2->metaData[1];
		assert(m2->name->type == NODE_NAME);
		struct parserNodeName *name2 = (void *)m2->name;
		assert(0 == strcmp(name2->text, "format"));
		assert(m2->value->type == NODE_LIT_STR);
	}
}
void classParserTests() {
	const char *text = "class class_x {\n"
	                   "    I64i a dft_val 10,b,c;\n"
	                   "    U8i d;\n"
	                   "};";
	createFile(text);
	__auto_type textStr = strCharAppendData(NULL, text, strlen(text));

	__auto_type lexItems = lexText((struct __vec *)textStr, NULL);
	__auto_type cls = parseClass(lexItems, NULL);
	assert(cls);
	{
		assert(cls->type == NODE_CLASS_DEF);
		struct parserNodeClassDef *clsDef = (void *)cls;

		assert(clsDef->name->type == NODE_NAME);
		struct parserNodeName *name = (void *)clsDef->name;
		assert(0 == strcmp(name->text, "class_x"));

		struct objectClass *clsType = (void *)clsDef->type;
		assert(clsType->base.type == TYPE_CLASS);

		const char *text[] = {"a", "b", "c", "d"};
		for (long i = 0; i != strObjectMemberSize(clsType->members); i++) {
			assert(0 == strcmp(clsType->members[i].name, text[i]));

			if (i == 0) {
				assert(strObjectMemberAttrSize(clsType->members[i].attrs) == 1);
				assert(0 == strcmp(clsType->members[i].attrs[0].name, "dft_val"));
				assert(clsType->members[i].attrs[0].value->type == NODE_LIT_INT);
			} else {
				assert(strObjectMemberAttrSize(clsType->members[i].attrs) == 0);
			}
		}
	}
	text = "class class_foo {\n"
	       "I64i x;"
	       "} a,b,c";
	createFile(text);
	textStr = strCharAppendData(NULL, text, strlen(text));

	int err;
	lexItems = lexText((struct __vec *)textStr, &err);
	assert(!err);
	__auto_type decls = parseVarDecls(lexItems, NULL);
	assert(decls);

	const char *names[] = {"a", "b", "c"};
	{
		assert(decls->type == NODE_VAR_DECLS);
		struct parserNodeVarDecls *decls2 = (void *)decls;

		assert(strParserNodeSize(decls2->decls) == 3);
		for (long i = 0; i != 3; i++) {
			assert(decls2->decls[i]->type == NODE_VAR_DECL);
			struct parserNodeVarDecl *decl = (void *)decls2->decls[i];
			assert(decl->type == objectByName("class_foo"));

			struct parserNodeName *name = (void *)decl->name;
			assert(name->base.type == NODE_NAME);
			assert(0 == strcmp(name->text, names[i]));
		}
	}
}
void keywordTests() {
	const char *text = "if(1) {a;} else {b;}";
	createFile(text);
	__auto_type textStr = strCharAppendData(NULL, text, strlen(text));

	int err;
	__auto_type lexItems = lexText((struct __vec *)textStr, &err);
	assert(!err);

	__auto_type ifStmt = parseIf(lexItems, NULL);
	assert(ifStmt);
	{
		struct parserNodeIf *node = (void *)ifStmt;
		assert(node->base.type == NODE_IF);
		assert(node->cond->type == NODE_LIT_INT);
		assert(node->body->type == NODE_SCOPE);

		struct parserNodeScope *scope = (void *)node->body;
		assert(1 == strParserNodeSize(scope->stmts));
		assert(scope->stmts[0]->type == NODE_NAME);
		struct parserNodeName *name = (void *)scope->stmts[0];
		assert(0 == strcmp(name->text, "a"));

		scope = (void *)node->el;
		assert(1 == strParserNodeSize(scope->stmts));
		assert(scope->stmts[0]->type == NODE_NAME);
		name = (void *)scope->stmts[0];
		assert(0 == strcmp(name->text, "b"));
	}

	text = "I64i x; for(I64i x=0;x!=10;++x) {x;}";
	createFile(text);
	textStr = strCharAppendData(NULL, text, strlen(text));

	lexItems = lexText((struct __vec *)textStr, &err);
	assert(!err);

	__auto_type x1 = parseStatement(lexItems, &lexItems);
	__auto_type forStmt = parseStatement(lexItems, &lexItems);
	{
		assert(x1->type == NODE_VAR_DECL);
		struct parserNodeVarDecl *var = (void *)x1;
		__auto_type x1V = getVar(var->name);

		assert(forStmt->type == NODE_FOR);
		struct parserNodeFor *forStmt2 = (void *)forStmt;
		assert(forStmt2->init->type == NODE_VAR_DECL);
		assert(forStmt2->cond->type == NODE_BINOP);
		assert(forStmt2->inc->type == NODE_UNOP);
		assert(forStmt2->body->type == NODE_SCOPE);

		struct parserNodeScope *scope = (void *)forStmt2->body;
		assert(1 == strParserNodeSize(scope->stmts));
		assert(scope->stmts[0]->type == NODE_VAR);
		struct parserNodeVar *var2 = (void *)scope->stmts[0];
		assert(var2->var != x1V);
	}
	
	text = "while(1) {1+1;}";
	createFile(text);
	textStr = strCharAppendData(NULL, text, strlen(text)); 
	
	lexItems = lexText((struct __vec *)textStr, &err);
	assert(!err);
	
	__auto_type whileStmt=parseStatement(lexItems,NULL);
	{
	 assert(whileStmt->type==NODE_WHILE);
	 
	 struct parserNodeWhile *node=(void*)whileStmt;
	 assert(node->cond->type==NODE_LIT_INT);
	 assert(node->body->type==NODE_SCOPE);
	}
	
	text = "do ; while(1);";
	createFile(text);
	textStr = strCharAppendData(NULL, text, strlen(text));
	
	lexItems = lexText((struct __vec *)textStr, &err);
	assert(!err);
	
	__auto_type doStmt=parseStatement(lexItems,NULL);
	{
	 assert(doStmt->type==NODE_DO);
	 struct parserNodeDo *doNode=(void*)doStmt;
	 assert(doNode->body==NULL);
	 assert(doNode->cond->type==NODE_LIT_INT);
	}

	text="switch(1) {"
	  "case :\n"
	  "case 3:\n"
	  "case :\n"
	  "default :"
	  "}";
	createFile(text);
	textStr=strCharAppendData(NULL,text,strlen(text));

	lexItems=lexText((struct __vec*)textStr, &err);
	assert(!err);

	__auto_type switStmt= parseSwitch(lexItems, NULL);
	assert(switStmt);
	{
	  assert(switStmt->type==NODE_SWITCH);
	  struct parserNodeSwitch *swit=(void*)switStmt;
	  assert(swit->exp->type==NODE_LIT_INT);

	  assert(3==strParserNodeSize(swit->caseSubcases));
	  for(int i=0;i!=3;i++)
	    assert(swit->caseSubcases[i]->type==NODE_CASE);

	  struct parserNodeCase *cs=(void*)swit->caseSubcases[0];
	  assert(cs->parent==switStmt);
	  assert(cs->valueLower==0);

	  cs=(void*)swit->caseSubcases[1];
	  assert(cs->parent==switStmt);
	  assert(cs->valueLower==3);

	  cs=(void*)swit->caseSubcases[2];
	  assert(cs->parent==switStmt);
	  assert(cs->valueLower==4);

	  assert(swit->dft!=NULL);
	  assert(swit->dft->type==NODE_DEFAULT);
	}

	text="switch(1) {"
			"start:\n"
	  "case :\n"
	  "default :\n"
			"end:\n"
	  "}";
	createFile(text);
	textStr=strCharAppendData(NULL,text,strlen(text));

	lexItems=lexText((struct __vec*)textStr, &err);
	assert(!err);
	
	switStmt=parseSwitch(lexItems, NULL);
	assert(switStmt);
	{
			assert(switStmt->type==NODE_SWITCH);

			struct parserNodeSwitch *swit=(void*)switStmt;
			assert(1==strParserNodeSize(swit->caseSubcases) );

			assert(swit->caseSubcases[0]->type==NODE_SUBSWITCH);
			struct parserNodeSubSwitch *sub=(void*)swit->caseSubcases[0];
			assert(1==strParserNodeSize(sub->caseSubcases)) ;
			assert(sub->dft!=NULL);
	}
}
static void funcTests() {
		const char *text=
				"U0 foo(I64i a,I64i b);\n"
				"U0 foo(I64i a,I64i b) {\"Hi World\";}\n";
		createFile(text);
		__auto_type textStr=strCharAppendData(NULL,text,strlen(text));
		int err;
		__auto_type lexItems=lexText((struct __vec*)textStr, &err);

		__auto_type forward=parseStatement(lexItems, &lexItems);
		assert(forward);
		struct objectFunction *funcType1=NULL;
		{
				assert(forward->type==NODE_FUNC_FORWARD_DECL);
				struct parserNodeFuncForwardDec *forward2=(void*)forward;
				assert(forward2->name->type==NODE_NAME);

				funcType1=(void*)forward2->funcType;
				assert(funcType1->base.type==TYPE_FUNCTION);
				assert(funcType1->retType==&typeU0);

				assert(strFuncArgSize(funcType1->args)==2);
				assert(funcType1->args[0].name->type==NODE_NAME);
				assert(funcType1->args[1].name->type==NODE_NAME);
				assert(funcType1->args[0].type==&typeI64i);
				assert(funcType1->args[1].type==&typeI64i);
		}
		
		__auto_type def=parseStatement(lexItems, NULL);
		assert(def);
		{
				assert(def->type==NODE_FUNC_DEF);
				struct parserNodeFuncDef *def2=(void*)def;

				assert(def2->name->type==NODE_NAME);
				assert(objectEqual(def2->funcType,(struct object*)funcType1));

				assert(def2->bodyScope->type==NODE_SCOPE);
				struct parserNodeScope *scope=(void*)def2->bodyScope;
				assert(1==strParserNodeSize(scope->stmts) );
				assert(scope->stmts[0]->type==NODE_LIT_STR);
		}
}
static void typeTests() {
		const char *text=
				"{\n"
				"F64 x=10;\n"
				"1+x;\n"
				"}";
		createFile(text);
		__auto_type textStr=strCharAppendData(NULL, text, strlen(text));
		int err;
		__auto_type lexItems=lexText((struct __vec*)textStr, &err);
		assert(!err);
		__auto_type scope=parseStatement(lexItems, NULL);
		assert(scope);
		{
				assert(scope->type==NODE_SCOPE);
				struct parserNodeScope *scope2=(void*)scope;

				assert(2==strParserNodeSize(scope2->stmts));
				assert(scope2->stmts[1]->type==NODE_BINOP);
				struct parserNodeBinop *binop=(void*)scope2->stmts[1];
				assert(binop->type==&typeF64);
		}
}
void parserTests() {
	precParserTests();
	varDeclTests();
	classParserTests();
	keywordTests();
	funcTests();
	typeTests();
}
