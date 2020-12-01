#include <assert.h>
#include <regAllocator.h>
#include <SSA.h>
#define DEBUG_PRINT_ENABLE 1
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
		{
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
				debugShowGraph(one);

				__auto_type beforeBref3=graphNodeIRIncomingNodes(IRGetStmtStart(bRef3))[0];
				__auto_type type=graphNodeIRValuePtr(IRGetStmtStart(beforeBref3))->type;
				assert(type==IR_CHOOSE);
		
				//Merge
				__auto_type 	allNodes= graphNodeIRAllNodes(one);
				IRCoalesce(allNodes, one);

				//Search for a and bRef3 being merged
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

		//
		//a=b=c=1;d=b Alias all togheter
		//
		{

				__auto_type one=createIntLit(1);
				__auto_type a=createVirtVar(&typeI64i);
				__auto_type b=createVirtVar(&typeI64i);
				__auto_type c=createVirtVar(&typeI64i);
				__auto_type d=createVirtVar(&typeI64i);

				__auto_type aRef1=createVarRef(a);
				__auto_type bRef1=createVarRef(b);
				__auto_type bRef2=createVarRef(b);
				__auto_type cRef=createVarRef(c);
				__auto_type dRef=createVarRef(d);

				DEBUG_PRINT_REGISTER_VAR(aRef1);
				DEBUG_PRINT_REGISTER_VAR(bRef1);
				DEBUG_PRINT_REGISTER_VAR(bRef2);
				DEBUG_PRINT_REGISTER_VAR(cRef);
				DEBUG_PRINT_REGISTER_VAR(dRef);

				graphNodeIRConnect(one, cRef, IR_CONN_DEST);
				graphNodeIRConnect(cRef, bRef1, IR_CONN_DEST);
				graphNodeIRConnect(bRef1,aRef1, IR_CONN_DEST);
				
				graphNodeIRConnect(bRef2,dRef, IR_CONN_DEST);
				//Flow to bRef2 which starts a new startment
				graphNodeIRConnect(aRef1, bRef2, IR_CONN_FLOW);

				__auto_type 	allNodes= graphNodeIRAllNodes(cRef);
				DEBUG_PRINT("TEST %i\n",2);
				IRCoalesce(allNodes, cRef);

				//All vars folowing one will be same

				//c=1
				__auto_type outgoingFrom=graphNodeIROutgoingNodes(one);
				struct IRNodeValue *val=(void*)graphNodeIRValuePtr(outgoingFrom[0]);
				__auto_type var=val->val.value.var.value.var;

				//3 Assigns,so 4 vars to replace(1 per-assign + 1 for last node)
				//b=c,a=b,d=b ... 1+1+1
				for(long i=0;i!=1+1+1+1;i++) {
						outgoingFrom=graphNodeIROutgoingNodes(outgoingFrom[0]);
						struct IRNodeValue *val=(void*)graphNodeIRValuePtr(outgoingFrom[0]);
						assert(val->val.value.var.value.var==var);
				}

				//Replace redundant assigns
				IRRemoveRepeatAssigns(one);

				debugShowGraph(one);

				outgoingFrom=graphNodeIROutgoingNodes(one);
				val=(void*)graphNodeIRValuePtr(outgoingFrom[0]);
				assert(val->base.type==IR_VALUE);
				assert(val->val.value.var.value.var==var);
 
				__auto_type outgoingEdges=graphNodeIROutgoing(outgoingFrom[0]);
				__auto_type assigns=IRGetConnsOfType(outgoingEdges, IR_CONN_DEST);
				assert(strGraphEdgeIRPSize(assigns)==0);
		}
}
