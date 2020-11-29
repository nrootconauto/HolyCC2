#include <assert.h>
#include <regAllocator.h>
#include <SSA.h>
void registerAllocatorTests() {
		initIR();
		__auto_type a=createVirtVar(&typeI64i);
		__auto_type b=createVirtVar(&typeI64i);
		__auto_type c=createVirtVar(&typeI64i);

		/**
			* a=1
			* if(a) {b=a} else {b=2}
			* c=b
			*/
		__auto_type one=createIntLit(1);
		__auto_type two=createIntLit(2);
		__auto_type aRef1=createVarRef(a);
		__auto_type aRef2=createVarRef(a); // b=a
		__auto_type bRef1=createVarRef(b); // b=a
		__auto_type bRef2=createVarRef(b); // b=2
		__auto_type bRef3=createVarRef(b); // c=b
		__auto_type cRef=createVarRef(c);

		createAssign(aRef1, one);
		
		createAssign(bRef1, aRef2);
		createAssign(bRef2, two);
		__auto_type cond=createCondJmp(aRef1, aRef2, two);
		
		
		createAssign(cRef, bRef3);
		graphNodeIRConnect(bRef1, bRef3, IR_CONN_FLOW);
		graphNodeIRConnect(bRef2, bRef3, IR_CONN_FLOW);

		//Do SSA on strucure
		__auto_type allNodes= graphNodeIRAllNodes(one);
		IRToSSA(allNodes, one);
		assert(graphNodeIRValuePtr(IRGetStmtStart(bRef3))->type==IR_CHOOSE);

		//Merge
		allNodes= graphNodeIRAllNodes(one);
		IRCoalesce(allNodes, one);

		__auto_type outgoing=graphNodeIROutgoingNodes(one);
		__auto_type firstRef=(struct IRNodeValue *)graphNodeIRValuePtr(outgoing[0]);
		assert(firstRef->base.type==IR_VALUE);
		outgoing=graphNodeIROutgoingNodes(one);

		int found=0;
		for(long i=0;i!=strGraphNodeIRPSize(outgoing);i++) {
				struct IRNodeValue *val=(struct IRNodeValue *)graphNodeIRValuePtr(outgoing[i]);
				found|=val->val.value.var.value.var==firstRef->val.value.var.value.var;
		}
		assert(found);
}
