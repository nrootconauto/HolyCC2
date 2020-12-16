#include <IRLiveness.h>
#include <assert.h>
#include <debugPrint.h>
#include <string.h>
struct nameVarPair {
		const char *name;
		struct variable *var;
		graphNodeIRLive node;
};
void LivenessTests() {
		initIR();
		{
				//https://lambda.uta.edu/cse5317/spring15/l10.pdf
				__auto_type u=IRCreateVirtVar(&typeI64i);
				__auto_type v=IRCreateVirtVar(&typeI64i);
				__auto_type w=IRCreateVirtVar(&typeI64i);
				__auto_type x=IRCreateVirtVar(&typeI64i);
				__auto_type y=IRCreateVirtVar(&typeI64i);
				__auto_type z=IRCreateVirtVar(&typeI64i);

				debugAddPtrName(u, "U");
				debugAddPtrName(v, "V");
				debugAddPtrName(w, "W");
				debugAddPtrName(x, "X");
				debugAddPtrName(y, "Y");
				debugAddPtrName(z, "Z");
		
				graphNodeIR exit=NULL;
				__auto_type entry=IRCreateLabel();
				//v=1
				{
						__auto_type one1=IRCreateIntLit(1);
						__auto_type vRef1=IRCreateVarRef(v);
						graphNodeIRConnect(entry, one1, IR_CONN_FLOW);
						graphNodeIRConnect(one1, vRef1, IR_CONN_DEST);
						debugAddPtrName(one1, "1.1");
						debugAddPtrName(vRef1, "v.1");
				
						exit=vRef1;
				}
		
				//z=v+1
				{
						__auto_type one2=IRCreateIntLit(1);
						__auto_type vRef2=IRCreateVarRef(v);
						__auto_type zRef1=IRCreateVarRef(z);
						__auto_type binop=IRCreateBinop(one2, vRef2, IR_ADD);
						graphNodeIRConnect(binop, zRef1, IR_CONN_DEST);
						graphNodeIRConnect(exit, one2, IR_CONN_FLOW);
						graphNodeIRConnect(exit, vRef2, IR_CONN_FLOW);
						exit=zRef1;

						debugAddPtrName(binop, "binop.2");
						debugAddPtrName(one2, "1.2");
						debugAddPtrName(zRef1, "z.2");
						debugAddPtrName(vRef2, "v.2");
				}
				//x=z*v
				{
						__auto_type vRef=IRCreateVarRef(v);
						__auto_type zRef=IRCreateVarRef(z);
						__auto_type xRef=IRCreateVarRef(x);
						__auto_type binop=IRCreateBinop(vRef, zRef, IR_ADD);
						graphNodeIRConnect(binop, xRef,IR_CONN_DEST);
						graphNodeIRConnect(exit, vRef, IR_CONN_FLOW);
						graphNodeIRConnect(exit, zRef, IR_CONN_FLOW);
						exit=xRef;

						debugAddPtrName(binop, "binop.2.5");
						debugAddPtrName(vRef, "v.2.5");
						debugAddPtrName(zRef, "z.2.5");
						debugAddPtrName(xRef, "x.2.5");
				}
		
				//y=x+2
				{
						__auto_type xRef1=IRCreateVarRef(x);
						__auto_type two1=IRCreateIntLit(2);
						__auto_type yRef1=IRCreateVarRef(y);
						graphNodeIRConnect(exit, two1, IR_CONN_FLOW);
						graphNodeIRConnect(exit, xRef1, IR_CONN_FLOW);
						__auto_type binop=IRCreateBinop(two1, xRef1, IR_ADD);
						graphNodeIRConnect(binop, yRef1, IR_CONN_DEST);
						exit=yRef1;

						debugAddPtrName(binop, "binop.3");
						debugAddPtrName(xRef1, "x.3");
						debugAddPtrName(two1, "2.3");
						debugAddPtrName(yRef1, "y.3");
				}

				//w=x+y*z
				{
						__auto_type xRef2=IRCreateVarRef(x);
						__auto_type yRef2=IRCreateVarRef(y);
						__auto_type zRef2=IRCreateVarRef(z);
						__auto_type wRef1=IRCreateVarRef(w);
						graphNodeIRConnect(exit, xRef2, IR_CONN_FLOW);
						graphNodeIRConnect(exit, yRef2, IR_CONN_FLOW);
						graphNodeIRConnect(exit, zRef2, IR_CONN_FLOW);
						__auto_type binopB=IRCreateBinop(yRef2, zRef2, IR_MULT);
						__auto_type binopA=IRCreateBinop(xRef2,binopB,IR_ADD);
						graphNodeIRConnect(binopA, wRef1, IR_CONN_DEST);
						exit=wRef1;

						debugAddPtrName(binopA, "binopA.4");
						debugAddPtrName(binopB, "binopB.4");
						debugAddPtrName(xRef2, "x.4");
						debugAddPtrName(yRef2, "y.4");
						debugAddPtrName(xRef2, "x.4");
						debugAddPtrName(yRef2, "y.4");
						debugAddPtrName(zRef2, "z.4");
						debugAddPtrName(wRef1, "w.4");
				}
		
				//u=z+2
				{
						__auto_type two2=IRCreateIntLit(2);
						__auto_type zRef3=IRCreateVarRef(z);
						__auto_type uRef1=IRCreateVarRef(u);
						graphNodeIRConnect(exit, two2, IR_CONN_FLOW);
						graphNodeIRConnect(exit, zRef3, IR_CONN_FLOW);
						__auto_type binop=IRCreateBinop(two2, zRef3, IR_ADD);
						graphNodeIRConnect(binop, uRef1, IR_CONN_DEST);
						exit=uRef1;

						debugAddPtrName(binop, "binop.5");
						debugAddPtrName(two2, "2.5");
						debugAddPtrName(zRef3, "z.5");
						debugAddPtrName(uRef1, "u.5");
				}

				//v=u+w+y
				{
						__auto_type uRef=IRCreateVarRef(u);
						__auto_type wRef=IRCreateVarRef(w);
						__auto_type yRef=IRCreateVarRef(y);
						__auto_type vRef=IRCreateVarRef(v);
						graphNodeIRConnect(exit, uRef, IR_CONN_FLOW);
						graphNodeIRConnect(exit, wRef, IR_CONN_FLOW);
						graphNodeIRConnect(exit, yRef, IR_CONN_FLOW);
						__auto_type binopB=IRCreateBinop(yRef, wRef, IR_ADD);
						__auto_type binopA=IRCreateBinop(uRef,binopB,IR_ADD);
						graphNodeIRConnect(binopA,vRef, IR_CONN_DEST);
						exit=vRef;
				
						debugAddPtrName(binopA, "binopA.6");
						debugAddPtrName(binopB, "binopB.6");
						debugAddPtrName(uRef, "u.6");
						debugAddPtrName(wRef, "w.6");
						debugAddPtrName(yRef, "y.6");
						debugAddPtrName(vRef, "v.6");
				}

				//u*v
				{
						__auto_type uRef=IRCreateVarRef(u);
						__auto_type vRef=IRCreateVarRef(v);
						__auto_type  binop=IRCreateBinop(uRef, vRef, IR_MULT);
						graphNodeIRConnect(exit, uRef, IR_CONN_FLOW);
						graphNodeIRConnect(exit, vRef, IR_CONN_FLOW);
						debugAddPtrName(uRef, "u.7");
						debugAddPtrName(vRef, "v.7");
						debugAddPtrName(binop, "binop.7");
				}
				
				__auto_type interfere=IRInterferenceGraph(entry);
				
				mapGraphNode byVar=mapGraphNodeCreate();
				__auto_type allNodes=graphNodeIRLiveAllNodes(interfere);
				assert(strGraphNodeIRLivePSize(allNodes)==6);

				struct nameVarPair expected[]={
						{"u",u,NULL},
						{"v",v,NULL},
						{"w",w,NULL},
						{"x",x,NULL},
						{"y",y,NULL},
						{"z",z,NULL},
				};
				//
				// Find expected items
				//
				long len=sizeof(expected)/sizeof(*expected);
				for(long i=0;i!=len;i++) {
						for(long i2=0;i2!=len;i2++) {
								if(graphNodeIRLiveValuePtr(allNodes[i2])->ref.value.var==expected[i].var) {
										expected[i].node=allNodes[i2];

										//Register in map
										mapGraphNodeInsert(byVar, expected[i].name, expected[i].node);

										goto found;
								}
						}
						assert(0);
				found:;
				}

				__auto_type uNode=*mapGraphNodeGet(byVar, "u");
				__auto_type vNode=*mapGraphNodeGet(byVar, "v");
				__auto_type wNode=*mapGraphNodeGet(byVar, "w");
				__auto_type xNode=*mapGraphNodeGet(byVar, "x");
				__auto_type yNode=*mapGraphNodeGet(byVar, "y");
				__auto_type zNode=*mapGraphNodeGet(byVar, "z");

				//u
				assert(graphNodeIRLiveConnectedTo(uNode, vNode));
				assert(graphNodeIRLiveConnectedTo(uNode, wNode));
				assert(graphNodeIRLiveConnectedTo(uNode, yNode));

				//v
				assert(graphNodeIRLiveConnectedTo(vNode, uNode));
				assert(graphNodeIRLiveConnectedTo(vNode, zNode));
		
				//w
				assert(graphNodeIRLiveConnectedTo(wNode, uNode));
				assert(graphNodeIRLiveConnectedTo(wNode, yNode));
				assert(graphNodeIRLiveConnectedTo(wNode, zNode));

				// x
				assert(graphNodeIRLiveConnectedTo(xNode, yNode));
				assert(graphNodeIRLiveConnectedTo(xNode, zNode));

				// y
				assert(graphNodeIRLiveConnectedTo(yNode, uNode));
				assert(graphNodeIRLiveConnectedTo(yNode, wNode));
				assert(graphNodeIRLiveConnectedTo(yNode, xNode));
				assert(graphNodeIRLiveConnectedTo(yNode, zNode));

				// z
				assert(graphNodeIRLiveConnectedTo(zNode, xNode));
				assert(graphNodeIRLiveConnectedTo(zNode, yNode));
				assert(graphNodeIRLiveConnectedTo(zNode, wNode));
				assert(graphNodeIRLiveConnectedTo(zNode, vNode));
		}
		{
				__auto_type a=IRCreateVirtVar(&typeI64i);
				__auto_type b=IRCreateVirtVar(&typeI64i);
				__auto_type c=IRCreateVirtVar(&typeI64i);
				DEBUG_PRINT_REGISTER_VAR(a);
				DEBUG_PRINT_REGISTER_VAR(b);
				DEBUG_PRINT_REGISTER_VAR(c);
				
				__auto_type enter=IRCreateLabel();
				DEBUG_PRINT_REGISTER_VAR(enter);
				graphNodeIR exit=NULL;
				//1: a=0
				{
						__auto_type aRef1=IRCreateVarRef(a);
						__auto_type zero1=IRCreateIntLit(0);
						DEBUG_PRINT_REGISTER_VAR(aRef1);
						DEBUG_PRINT_REGISTER_VAR(zero1);
						graphNodeIRConnect(enter, zero1, IR_CONN_FLOW);
						graphNodeIRConnect(zero1,aRef1, IR_CONN_DEST);
						exit=aRef1;
				}
				
				//Label (for loop)
				__auto_type label2=IRCreateLabel();
				DEBUG_PRINT_REGISTER_VAR(label2);
				graphNodeIRConnect(exit, label2, IR_CONN_FLOW);
				//2: b=a+1
				{
				__auto_type bRef2=IRCreateVarRef(b);
				__auto_type aRef2=IRCreateVarRef(a); 
				__auto_type one2=IRCreateIntLit(1);
				__auto_type binop2=IRCreateBinop(aRef2, one2, IR_ADD);
				DEBUG_PRINT_REGISTER_VAR(bRef2);
				DEBUG_PRINT_REGISTER_VAR(aRef2);
				DEBUG_PRINT_REGISTER_VAR(one2);
				DEBUG_PRINT_REGISTER_VAR(binop2);
						
				graphNodeIRConnect(binop2, bRef2, IR_CONN_DEST);
				graphNodeIRConnect(label2, aRef2, IR_CONN_FLOW);
				graphNodeIRConnect(label2, one2, IR_CONN_FLOW);
				exit=bRef2;
				}
				
				//3: c=c+b
				{
				__auto_type cRef3_Src=IRCreateVarRef(c);
				__auto_type cRef3_Dst=IRCreateVarRef(c);
				__auto_type bRef3=IRCreateVarRef(b);
				__auto_type binop3=IRCreateBinop(cRef3_Src, bRef3, IR_ADD);
				DEBUG_PRINT_REGISTER_VAR(bRef3);
				DEBUG_PRINT_REGISTER_VAR(cRef3_Src);
				DEBUG_PRINT_REGISTER_VAR(cRef3_Dst);
				DEBUG_PRINT_REGISTER_VAR(binop3);
				
				graphNodeIRConnect(exit, cRef3_Src, IR_CONN_FLOW);
				graphNodeIRConnect(exit, bRef3, IR_CONN_FLOW);
				graphNodeIRConnect(binop3,cRef3_Dst, IR_CONN_DEST);
				exit=cRef3_Dst;
				}
				
				//4: a=b+2
				{
						__auto_type aRef4=IRCreateVarRef(a);
						__auto_type bRef4=IRCreateVarRef(b);
						__auto_type two4=IRCreateIntLit(2);
						graphNodeIRConnect(exit, two4, IR_CONN_FLOW);
						graphNodeIRConnect(exit, bRef4, IR_CONN_FLOW);
						__auto_type binop4=IRCreateBinop(bRef4, two4, IR_ADD);
						graphNodeIRConnect(binop4, aRef4, IR_CONN_DEST);
						exit=aRef4;

						DEBUG_PRINT_REGISTER_VAR(aRef4);
						DEBUG_PRINT_REGISTER_VAR(bRef4);
						DEBUG_PRINT_REGISTER_VAR(two4);
						DEBUG_PRINT_REGISTER_VAR(binop4);
				}

				//6: return c(DOESNT NOT ATTACH TO exit,IT IS USED AHEAD IN JUMP)
				graphNodeIR retLabel=IRCreateLabel();
				DEBUG_PRINT_REGISTER_VAR(retLabel);
				{
						__auto_type cRef6=IRCreateVarRef(c);
						__auto_type ret=IRCreateReturn(cRef6, NULL);
						DEBUG_PRINT_REGISTER_VAR(cRef6);
						DEBUG_PRINT_REGISTER_VAR(ret);
						graphNodeIRConnect(retLabel, cRef6, IR_CONN_FLOW);
				}

				//5: a<10
				{
						__auto_type aRef5=IRCreateVarRef(a);
						__auto_type ten5=IRCreateIntLit(10);
						__auto_type cmp=IRCreateBinop(aRef5, ten5, IR_GT);
						DEBUG_PRINT_REGISTER_VAR(aRef5);
						DEBUG_PRINT_REGISTER_VAR(ten5);
						DEBUG_PRINT_REGISTER_VAR(cmp);
						
						graphNodeIRConnect(exit, aRef5, IR_CONN_FLOW);
						graphNodeIRConnect(exit, ten5, IR_CONN_FLOW);
						__auto_type jump=IRCreateCondJmp(cmp, retLabel, label2);
						DEBUG_PRINT_REGISTER_VAR(jump);
						
						exit=jump;
				}

				__auto_type interfere=IRInterferenceGraph(enter);
		}
}
