#include <assert.h>
#include <IRExec.h>
#include <constantPropigation.h>
void constantPropigationTests() {
		{
				__auto_type current=IRCreateLabel();
				__auto_type start=current;
				__auto_type a=IRCreateVirtVar(&typeI64i);
				a->name="A";
				__auto_type b=IRCreateVirtVar(&typeI64i);
				b->name="B";
				__auto_type c=IRCreateVirtVar(&typeI64i);
				c->name="C";
				__auto_type d=IRCreateVirtVar(&typeI64i);
				d->name="D";
				{
						__auto_type add= IRCreateBinop(IRCreateIntLit(1), IRCreateIntLit(2), IR_ADD);
						__auto_type assign=IRCreateAssign(add, IRCreateVarRef(a));
						graphNodeIRConnect(current, IRStmtStart(assign), IR_CONN_FLOW);
						current=assign;
				}
				{
						__auto_type add= IRCreateBinop(IRCreateVarRef(a), IRCreateIntLit(3), IR_ADD);
						__auto_type assign=IRCreateAssign(add, IRCreateVarRef(b));
						graphNodeIRConnect(current, IRStmtStart(assign), IR_CONN_FLOW);
						current=assign;
				}
				{
						__auto_type add= IRCreateBinop(IRCreateVarRef(b), IRCreateIntLit(4), IR_ADD);
						__auto_type assign=IRCreateAssign(add, IRCreateVarRef(c));
						graphNodeIRConnect(current, IRStmtStart(assign), IR_CONN_FLOW);
						current=assign;
				}
				{
						__auto_type add= IRCreateBinop(IRCreateVarRef(c), IRCreateIntLit(5), IR_ADD);
						__auto_type assign=IRCreateAssign(add, IRCreateVarRef(d));
						graphNodeIRConnect(current,IRStmtStart(assign), IR_CONN_FLOW);
						current=assign;
				}
				int success;
				__auto_type value=IREvalPath(start, &success);
				assert(success);
				assert(value.type==IREVAL_VAL_INT);
				assert(value.value.i==15);

				IRConstPropigation(start);
				value=IREvalPath(start, &success);
				assert(success);
				assert(value.type==IREVAL_VAL_INT);
				assert(value.value.i==15);
		}
		{
				__auto_type current=IRCreateLabel();
				__auto_type start=current;
				__auto_type a=IRCreateVirtVar(&typeI64i);
				a->name="A";
				__auto_type b=IRCreateVirtVar(&typeI64i);
				b->name="B";
				__auto_type c=IRCreateVirtVar(&typeI64i);
				c->name="C";
				__auto_type d=IRCreateVirtVar(&typeI64i);
				d->name="D";
				{
						__auto_type expr=IRCreateAssign(IRCreateIntLit(2), IRCreateVarRef(a));
						graphNodeIRConnect(current,IRStmtStart(expr), IR_CONN_FLOW);
						current=expr;
				}
				{
						__auto_type expr=IRCreateAssign(IRCreateIntLit(3), IRCreateVarRef(b));
						graphNodeIRConnect(current,IRStmtStart(expr), IR_CONN_FLOW);
						current=expr;
				}
				{
						__auto_type cond=IRCreateBinop(IRCreateVarRef(a), IRCreateVarRef(b), IR_LT);
						__auto_type assign1=IRCreateAssign(IRCreateIntLit(4), IRCreateVarRef(c));
						__auto_type assign2=IRCreateAssign(IRCreateIntLit(5), IRCreateVarRef(c));
						IRCreateCondJmp(cond, IRStmtStart(assign1), IRStmtStart(assign2));
						graphNodeIRConnect(current, IRStmtStart(cond), IR_CONN_FLOW);
						__auto_type join=IRCreateLabel();
						graphNodeIRConnect(assign1, join, IR_CONN_FLOW);
						graphNodeIRConnect(assign2, join, IR_CONN_FLOW);
						current=join;
				}
				{
						__auto_type assign= IRCreateAssign(IRCreateVarRef(c), IRCreateVarRef(d));
						graphNodeIRConnect(current, IRStmtStart(assign), IR_CONN_FLOW);
				}
				int success;
				__auto_type value= IREvalPath(start, &success);
				assert(value.type==IREVAL_VAL_INT);
				assert(value.value.i==4);
				IRConstPropigation(start);
				__auto_type   map=graphNodeCreateMapping(start, 1);
				IRPrintMappedGraph(map);
				
				value=IREvalPath(start, &success);
				assert(value.type==IREVAL_VAL_INT);
				assert(value.value.i==4);
		}
}
