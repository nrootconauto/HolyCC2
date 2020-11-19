#include <IR.h>
#include <assert.h>
#include <lexer.h>
#include <parserA.h>
STR_TYPE_DEF(char, Char);
STR_TYPE_FUNCS(char, Char);
static struct parserNode *parseText(const char *text) {
	int err;
	__auto_type str = strCharAppendData(NULL, text, strlen(text));
	__auto_type items = lexText((struct __vec *)str, &err);
	assert(!err);

	return parseStatement(items, NULL);
}
static int calledFoo, calledBar;

void IRGenTests() {
	// Create vars
	parseText("I64i a,b,c;");
	{
		__auto_type add = parseText("10+b-c");
		__auto_type top = parserNode2IRStmt(add);
		assert(top);
		__auto_type ir = graphNodeIRValuePtr(top);
		assert(ir->type == IR_STATEMENT_START);
		__auto_type end = ((struct IRNodeStatementStart *)ir)->end;
		assert(end);
		__auto_type irEnd = graphNodeIRValuePtr(end);
		assert(irEnd->type == IR_STATEMENT_END);

		__auto_type tail = graphNodeIRIncomingNodes(end);
		assert(10 + 2 - 3 == evalIRNode(tail[0]));
	}
}
