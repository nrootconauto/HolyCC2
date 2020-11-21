#include <assert.h>
#include <IR.h>
#include <SSA.h>
#include <graphDominance.h>
static void createAssignExpr(graphNodeIR lit,struct variable *var,graphNodeIR *enter,graphNodeIR *exit) {
		__auto_type ref=createVarRef(var);
		graphNodeIRConnect(lit, ref, IR_CONN_DEST);
		if(exit)
				*exit=ref;
		if(enter)
				*enter=lit;
}
/**
	* creates prefix##enter prefix##/exit
	*/
#define ASSIGN_EXPR(prefix,lit,var) graphNodeIR prefix##Enter,prefix##Exit; createAssignExpr(lit,var,&prefix##Enter,&prefix##Exit);

void SSATests() {
		// http://pages.cs.wisc.edu/~fischer/cs701.f05/lectures/Lecture22.pdf
		__auto_type start=createLabel();
		__auto_type var=createVirtVar(&typeI64i);

		__auto_type enter=createLabel();
		
		ASSIGN_EXPR(a, createIntLit(0), var);
		ASSIGN_EXPR(b, createIntLit(1), var);
		ASSIGN_EXPR(c, createIntLit(2), var);
		ASSIGN_EXPR(d, createIntLit(3), var);
		ASSIGN_EXPR(e, createIntLit(4), var);
		ASSIGN_EXPR(f, createIntLit(5), var);

		graphNodeIRConnect(enter, aEnter, IR_CONN_FLOW);
		
		__auto_type cond1=createIntLit(101);
		__auto_type cond2=createIntLit(101);
		
		__auto_type aB_FCJmp=createCondJmp(cond1, bEnter ,fEnter);
		graphNodeIRConnect(aExit,cond1,IR_CONN_FLOW);
		
		__auto_type bC_DCJmp=createCondJmp(cond2, cEnter ,dEnter);
		graphNodeIRConnect(bExit,cond2,IR_CONN_FLOW);	
	
		graphNodeIRConnect(cExit, eEnter, IR_CONN_FLOW);
		graphNodeIRConnect(dExit, eEnter, IR_CONN_FLOW);
		graphNodeIRConnect(eExit, fEnter, IR_CONN_FLOW);
		
		__auto_type allNodes=graphNodeIRAllNodes(enter);
		IRToSSA(allNodes,enter);
}
