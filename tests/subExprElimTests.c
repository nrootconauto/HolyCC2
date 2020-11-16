#include <assert.h>
#include <subExprElim.h>
struct variable* a;
//Assumes "a" is 3,only "+" is implemented
void subExprElimTests() {
		//a+1+a+1+a+1
		{
				initIR();
				a	=createVirtVar(&typeI64i);
		__auto_type one1=createIntLit(1);
		__auto_type one2=createIntLit(1);
		__auto_type one3=createIntLit(1);
		
		__auto_type a1=createVarRef(a);
		__auto_type a2=createVarRef(a);
		__auto_type a3=createVarRef(a);

		__auto_type binop1=createBinop(a1, one1, IR_ADD);
		__auto_type binop2=createBinop(a2, one2, IR_ADD);
		__auto_type binop3=createBinop(a3, one3, IR_ADD);
		
		__auto_type sum1= createBinop(binop1,binop2 ,  IR_ADD);
		__auto_type sum2= createBinop(sum1,binop3,  IR_ADD);

		__auto_type start=createStmtStart();
		__auto_type end=createStmtEnd(start);
		graphNodeIRConnect(sum2, end, IR_CONN_FLOW);

		graphNodeIR connectToStart[]={
				one1,one2,one3,a1,a2,a3
		};
		for(long i=0;i!=sizeof(connectToStart)/sizeof(*connectToStart);i++)
				graphNodeIRConnect(start, connectToStart[i], IR_CONN_FLOW);

		findSubExprs(start);
		removeSubExprs();

		__auto_type s2o= graphNodeIROutgoingNodes(sum2);
		__auto_type tail=graphNodeIRIncomingNodes(end);
		assert(strGraphNodeIRPSize(tail)==1);
		assert(4+4+4==evalIRNode(tail[0]));
		}
}
