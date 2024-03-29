#include <IRExec.h>
#include <assert.h>
#include <lexer.h>
#include <parserA.h>
#include <subExprElim.h>
struct parserVar *a;
static void debugShowGraphIR(graphNodeIR enter) {
		const char *name=tmpnam(NULL);
		__auto_type map=graphNodeCreateMapping(enter, 1);
		IRGraphMap2GraphViz(map, "viz", name, NULL,NULL,NULL,NULL);
		char buffer[1024];
		sprintf(buffer, "sleep 0.1 &&dot -Tsvg %s > /tmp/dot.svg && firefox /tmp/dot.svg & ", name);

		system(buffer);
}

// Assumes "a" is 3,only "+" is implemented
void subExprElimTests() {

	// a+1+a+1+a+1
	{
		IREvalInit();
		initIR();
		a = createVirtVar(&typeI64i);
		IREvalSetVarVal(a, IREValValIntCreate(3));
		__auto_type one1 = createIntLit(1);
		__auto_type one2 = createIntLit(1);
		__auto_type one3 = createIntLit(1);

		__auto_type a1 = createVarRef(a);
		__auto_type a2 = createVarRef(a);
		__auto_type a3 = createVarRef(a);

		__auto_type binop1 = createBinop(a1, one1, IR_ADD);
		__auto_type binop2 = createBinop(a2, one2, IR_ADD);
		__auto_type binop3 = createBinop(a3, one3, IR_ADD);

		__auto_type sum1 = createBinop(binop1, binop2, IR_ADD);
		__auto_type sum2 = createBinop(sum1, binop3, IR_ADD);

		__auto_type start = createStmtStart();
		__auto_type end = createStmtEnd(start);
		graphNodeIRConnect(sum2, end, IR_CONN_FLOW);

		graphNodeIR connectToStart[] = {one1, one2, one3, a1, a2, a3};
		for (long i = 0; i != sizeof(connectToStart) / sizeof(*connectToStart); i++)
			graphNodeIRConnect(start, connectToStart[i], IR_CONN_FLOW);

		__auto_type lastAdd = graphNodeIRIncomingNodes(end);
		assert(strGraphNodeIRPSize(lastAdd) == 1);
		findSubExprs(lastAdd[0]);
		replaceSubExprsWithVars();
		
		__auto_type s2o = graphNodeIROutgoingNodes(sum2);
		__auto_type tail = graphNodeIRIncomingNodes(end);
		assert(strGraphNodeIRPSize(tail) == 1);

		debugShowGraphIR(start);
		
		int success;
		__auto_type computed = IREvalNode(tail[0], &success);
		assert(computed.type == IREVAL_VAL_INT);
		assert(4 + 4 + 4 == computed.value.i);

		__auto_type mapped=graphNodeCreateMapping(tail[0], 1);
		char *fn=tmpnam(NULL);
		IRGraphMap2GraphViz(mapped, "Toads", fn, NULL, NULL, NULL, NULL);
		printf("Look at %s\n", fn);
	}
}
