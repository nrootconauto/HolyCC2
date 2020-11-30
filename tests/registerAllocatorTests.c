#include <assert.h>
#include <regAllocator.h>
#include <SSA.h>
#include <debugPrint.h>
static void debugShowGraph(graphNodeIR enter) {
		const char *name=tmpnam(NULL);
		__auto_type map=graphNodeCreateMapping(enter, 1);
		IRGraphMap2GraphViz(map, "viz", name, NULL,NULL,NULL,NULL);
		char buffer[1024];
		sprintf(buffer, "dot -Tsvg %s > /tmp/dot.svg && firefox /tmp/dot.svg", name);

		system(buffer);
}
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

		debugAddPtrName(one, "One");
		debugAddPtrName(two, "two");
		debugAddPtrName(aRef1, "aRef1");
		debugAddPtrName(aRef2, "aRef2");
		debugAddPtrName(bRef1, "bRef1");
		debugAddPtrName(bRef2, "bRef2");
		debugAddPtrName(bRef3, "bRef3");
		debugAddPtrName(cRef, "cRef");
		
		
		createAssign( one,aRef1);
		
		createAssign(aRef2,bRef1);
		createAssign( two,bRef2);
		__auto_type cond=createCondJmp(aRef1, aRef2, two);
		debugAddPtrName(cond, "cond");
		
		
		createAssign(bRef3,cRef);
		graphNodeIRConnect(bRef1, bRef3, IR_CONN_FLOW);
		graphNodeIRConnect(bRef2, bRef3, IR_CONN_FLOW);

		//debugShowGraph(aRef1);
		
		//Do SSA on strucure
		IRToSSA(one);
		//debugShowGraph(aRef1);

		__auto_type beforeBref3=graphNodeIRIncomingNodes(IRGetStmtStart(bRef3))[0];
		__auto_type type=graphNodeIRValuePtr(IRGetStmtStart(beforeBref3))->type;
		assert(type==IR_CHOOSE);
		
		//Merge
		__auto_type 	allNodes= graphNodeIRAllNodes(one);
		IRCoalesce(allNodes, one);

		debugShowGraph(aRef1);		

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
