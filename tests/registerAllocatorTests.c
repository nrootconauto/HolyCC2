#include <assert.h>
#include <regAllocator.h>
#include <SSA.h>
#include <IRExec.h>
#define DEBUG_PRINT_ENABLE 1
#include <debugPrint.h>
static void debugShowGraph(graphNodeIR enter) {
		const char *name=tmpnam(NULL);
		__auto_type map=graphNodeCreateMapping(enter, 1);
		IRGraphMap2GraphViz(map, "viz", name, NULL,NULL,NULL,NULL);
		char buffer[1024];
		sprintf(buffer, "sleep 0.1 &&dot -Tsvg %s > /tmp/dot.svg && firefox /tmp/dot.svg &", name);

		system(buffer);
}
void registerAllocatorTests() {
		initIR();
		{
				__auto_type a=createVirtVar(&typeI64i);
				__auto_type b=createVirtVar(&typeI64i);
				__auto_type c=createVirtVar(&typeI64i);
				a->name="A";
				b->name="B";
				c->name="C";
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

				debugShowGraph(aRef1);
		
				//Do SSA on strucure
				IRToSSA(one);
				
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

				outgoingFrom=graphNodeIROutgoingNodes(one);
				val=(void*)graphNodeIRValuePtr(outgoingFrom[0]);
				assert(val->base.type==IR_VALUE);
				assert(val->val.value.var.value.var==var);
 
				__auto_type outgoingEdges=graphNodeIROutgoing(outgoingFrom[0]);
				__auto_type assigns=IRGetConnsOfType(outgoingEdges, IR_CONN_DEST);
				assert(strGraphEdgeIRPSize(assigns)==0);
		}
		//
		// Register allocater test
		//
		{
				__auto_type u=createVirtVar(&typeI32i);
				__auto_type v=createVirtVar(&typeI32i);
				__auto_type w=createVirtVar(&typeI32i);
				__auto_type x=createVirtVar(&typeI32i);
				__auto_type y=createVirtVar(&typeI32i);
				__auto_type z=createVirtVar(&typeI32i);
				u->name="U";
				v->name="V";
				w->name="W";
				x->name="X";
				y->name="Y";
				z->name="Z";
				DEBUG_PRINT_REGISTER_VAR(u);
				DEBUG_PRINT_REGISTER_VAR(v);
				DEBUG_PRINT_REGISTER_VAR(w);
				DEBUG_PRINT_REGISTER_VAR(x);
				DEBUG_PRINT_REGISTER_VAR(y);
				DEBUG_PRINT_REGISTER_VAR(z);
				
				__auto_type start=createLabel();
				__auto_type end=start;
				//Assign u-z;
				{
						{
								__auto_type uRef0=createVarRef(u);
								__auto_type zero0=createIntLit(0);
								DEBUG_PRINT_REGISTER_VAR(uRef0);
								DEBUG_PRINT_REGISTER_VAR(zero0);
								graphNodeIRConnect(end,zero0,IR_CONN_FLOW);
								graphNodeIRConnect(zero0, uRef0, IR_CONN_DEST);
								end=uRef0;
						}

						{
								__auto_type vRef0=createVarRef(v);
								__auto_type one0=createIntLit(1);
								DEBUG_PRINT_REGISTER_VAR(vRef0);
								DEBUG_PRINT_REGISTER_VAR(one0);
								graphNodeIRConnect(end,one0,IR_CONN_FLOW);
								graphNodeIRConnect( one0,vRef0, IR_CONN_DEST);
								end=vRef0;
						}
						{
								__auto_type wRef0=createVarRef(w);
								__auto_type two0=createIntLit(2);
								DEBUG_PRINT_REGISTER_VAR(wRef0);
								DEBUG_PRINT_REGISTER_VAR(two0);
								graphNodeIRConnect(end,two0,IR_CONN_FLOW);
								graphNodeIRConnect(two0,wRef0, IR_CONN_DEST);
								end=wRef0;
						}
						{
								__auto_type xRef0=createVarRef(x);
								__auto_type three0=createIntLit(3);
								DEBUG_PRINT_REGISTER_VAR(xRef0);
								DEBUG_PRINT_REGISTER_VAR(three0);
								graphNodeIRConnect(end,three0,IR_CONN_FLOW);
								graphNodeIRConnect(three0,xRef0, IR_CONN_DEST);
								end=xRef0;
						}
						{
								__auto_type yRef0=createVarRef(y);
								__auto_type four0=createIntLit(4);
								DEBUG_PRINT_REGISTER_VAR(yRef0);
								DEBUG_PRINT_REGISTER_VAR(four0);
								graphNodeIRConnect(end,four0,IR_CONN_FLOW);
								graphNodeIRConnect(four0, yRef0, IR_CONN_DEST);
								end=yRef0;
						}
						{
								__auto_type zRef0=createVarRef(z);
								__auto_type five0=createIntLit(5);
								DEBUG_PRINT_REGISTER_VAR(zRef0);
								DEBUG_PRINT_REGISTER_VAR(five0);
								graphNodeIRConnect(end,five0,IR_CONN_FLOW);
								graphNodeIRConnect( five0,zRef0, IR_CONN_DEST);
								end=zRef0;
						}
				}
				DEBUG_PRINT_REGISTER_VAR(start);
				
				{
						__auto_type zRef2=createVarRef(z);
						__auto_type vRef2=createVarRef(v);
						__auto_type one2=createIntLit(1);
						DEBUG_PRINT_REGISTER_VAR(zRef2);
						DEBUG_PRINT_REGISTER_VAR(vRef2);
						DEBUG_PRINT_REGISTER_VAR(one2);
						__auto_type binop2=createBinop(vRef2, one2, IR_ADD);
						DEBUG_PRINT_REGISTER_VAR(binop2);
						graphNodeIRConnect(binop2, zRef2, IR_CONN_DEST);
						graphNodeIRConnect( end,IRGetStmtStart(zRef2),  IR_CONN_FLOW) ;
						
						end=zRef2;
				} {
						__auto_type xRef3=createVarRef(x);
						__auto_type vRef3=createVarRef(v);
						__auto_type zRef3=createVarRef(z);
						__auto_type binop3=createBinop(zRef3, vRef3, IR_MULT);
						DEBUG_PRINT_REGISTER_VAR(xRef3);
						DEBUG_PRINT_REGISTER_VAR(vRef3);
						DEBUG_PRINT_REGISTER_VAR(zRef3);
						DEBUG_PRINT_REGISTER_VAR(binop3);
						
						graphNodeIRConnect(binop3, xRef3, IR_CONN_DEST);
						graphNodeIRConnect( end,IRGetStmtStart(xRef3),  IR_CONN_FLOW);
						end=xRef3;
				}
				{
						__auto_type yRef4=createVarRef(y);
						__auto_type xRef4=createVarRef(x);
						__auto_type two4=createIntLit(2);
						__auto_type binop4=createBinop(xRef4, two4, IR_MULT);
						DEBUG_PRINT_REGISTER_VAR(xRef4);
						DEBUG_PRINT_REGISTER_VAR(yRef4);
						DEBUG_PRINT_REGISTER_VAR(two4);
						DEBUG_PRINT_REGISTER_VAR(binop4);
						graphNodeIRConnect(binop4, yRef4, IR_CONN_DEST);
						graphNodeIRConnect( end,IRGetStmtStart(yRef4),  IR_CONN_FLOW);

						end=yRef4;
				}
					{
							__auto_type wRef5=createVarRef(w);
							__auto_type yRef5=createVarRef(y);
							__auto_type zRef5=createVarRef(z);
							__auto_type xRef5=createVarRef(x);
							DEBUG_PRINT_REGISTER_VAR(wRef5);
							DEBUG_PRINT_REGISTER_VAR(yRef5);
							DEBUG_PRINT_REGISTER_VAR(zRef5);
							DEBUG_PRINT_REGISTER_VAR(xRef5);
							__auto_type binop5_1=createBinop(zRef5, yRef5, IR_MULT);
							__auto_type binop5_2=createBinop(binop5_1,xRef5,IR_ADD);
							graphNodeIRConnect(binop5_2, wRef5, IR_CONN_DEST);
							graphNodeIRConnect( end,IRGetStmtStart(wRef5),  IR_CONN_FLOW);

						end=wRef5;
				}
					{
							__auto_type uRef6=createVarRef(u);
							__auto_type zRef6=createVarRef(z);
							__auto_type two6=createIntLit(2);
							__auto_type binop6=createBinop(two6, zRef6, IR_ADD);
							DEBUG_PRINT_REGISTER_VAR(binop6);
							DEBUG_PRINT_REGISTER_VAR(uRef6);
							DEBUG_PRINT_REGISTER_VAR(zRef6);
							DEBUG_PRINT_REGISTER_VAR(two6);
							
							graphNodeIRConnect(binop6, uRef6, IR_CONN_DEST);
							graphNodeIRConnect( end,IRGetStmtStart(uRef6),  IR_CONN_FLOW);
							
							end=uRef6;
					}
					{
							__auto_type vRef7=createVarRef(v);
							__auto_type uRef7=createVarRef(u);
							__auto_type wRef7=createVarRef(w);
							__auto_type yRef7=createVarRef(y);
							__auto_type binop7_1=createBinop(uRef7, wRef7, IR_ADD);
							__auto_type binop7_2=createBinop(binop7_1,yRef7,IR_ADD);
							DEBUG_PRINT_REGISTER_VAR(vRef7);
							DEBUG_PRINT_REGISTER_VAR(uRef7);
							DEBUG_PRINT_REGISTER_VAR(wRef7);
							DEBUG_PRINT_REGISTER_VAR(yRef7);
							DEBUG_PRINT_REGISTER_VAR(binop7_1);
							DEBUG_PRINT_REGISTER_VAR(binop7_2);
							
							graphNodeIRConnect(binop7_2, vRef7, IR_CONN_DEST);
							graphNodeIRConnect( end,IRGetStmtStart(vRef7),  IR_CONN_FLOW);
							
							end=vRef7;
					}
					{
							__auto_type uRef8=createVarRef(u);
							__auto_type vRef8=createVarRef(v);
							__auto_type binop=createBinop(uRef8, vRef8, IR_MULT);
							DEBUG_PRINT_REGISTER_VAR(uRef8);
							DEBUG_PRINT_REGISTER_VAR(vRef8);
							DEBUG_PRINT_REGISTER_VAR(binop);
							
							graphNodeIRConnect( end,IRGetStmtStart(binop),  IR_CONN_FLOW);
							createReturn(binop, NULL);
							}
				setArch(ARCH_TEST_SYSV);
				int success;
				__auto_type res1= IREvalPath(start, &success);
				assert(success);
				
				debugShowGraph(start);;
				IRRegisterAllocate(start, NULL, NULL);
				debugShowGraph(start);
				
				IREvalInit();
				__auto_type  res2= IREvalPath(start, &success);
				assert(success);

				assert(res1.value.i==res2.value.i);
		}
}
