#include <frameLayout.h>
#include <assert.h>
#include <IRLiveness.h>
#include <IRExec.h>
#include <hashTable.h>
MAP_TYPE_DEF(struct frameEntry, FrameEntry);
MAP_TYPE_FUNCS(struct frameEntry, FrameEntry);
static const char *varNameFromLive(graphNodeIRLive node) {
		return graphNodeIRLiveValuePtr(node)->ref.value.var->name;
}
static long min(long a,long b) {
		return (a<b)?a:b;
}
static long max(long a,long b) {
		return (a>b)?a:b;
}
void frameLayoutTests() {
		__auto_type u=IRCreateVirtVar(&typeI32i);
		__auto_type v=IRCreateVirtVar(&typeI64i);
		__auto_type w=IRCreateVirtVar(&typeI16i);
		__auto_type x=IRCreateVirtVar(&typeF64);
		__auto_type y=IRCreateVirtVar(&typeI8i);
		__auto_type z=IRCreateVirtVar(&typeI64i);
		u->name="U";
		v->name="V";
		w->name="W";
		x->name="X";
		y->name="Y";
		z->name="Z";
				
		__auto_type start=IRCreateLabel();
		__auto_type end=start;
		//Assign u-z;
		{
				__auto_type uRef0=IRCreateVarRef(u);
				__auto_type zero0=IRCreateIntLit(0);
				graphNodeIRConnect(end,zero0,IR_CONN_FLOW);
				graphNodeIRConnect(zero0, uRef0, IR_CONN_DEST);
				end=uRef0;
		}

		{
				__auto_type vRef0=IRCreateVarRef(v);
				__auto_type one0=IRCreateIntLit(1);
				graphNodeIRConnect(end,one0,IR_CONN_FLOW);
				graphNodeIRConnect( one0,vRef0, IR_CONN_DEST);
				end=vRef0;
		}
		{
				__auto_type wRef0=IRCreateVarRef(w);
				__auto_type two0=IRCreateIntLit(2);
				graphNodeIRConnect(end,two0,IR_CONN_FLOW);
				graphNodeIRConnect(two0,wRef0, IR_CONN_DEST);
				end=wRef0;
		}
		{
				__auto_type xRef0=IRCreateVarRef(x);
				__auto_type three0=IRCreateIntLit(3);
				graphNodeIRConnect(end,three0,IR_CONN_FLOW);
				graphNodeIRConnect(three0,xRef0, IR_CONN_DEST);
				end=xRef0;
		}
		{
				__auto_type yRef0=IRCreateVarRef(y);
				__auto_type four0=IRCreateIntLit(4);
				graphNodeIRConnect(end,four0,IR_CONN_FLOW);
				graphNodeIRConnect(four0, yRef0, IR_CONN_DEST);
				end=yRef0;
		}
		{
				__auto_type zRef0=IRCreateVarRef(z);
				__auto_type five0=IRCreateIntLit(5);
				graphNodeIRConnect(end,five0,IR_CONN_FLOW);
				graphNodeIRConnect( five0,zRef0, IR_CONN_DEST);
				end=zRef0;
		}
				
		{
				__auto_type zRef2=IRCreateVarRef(z);
				__auto_type vRef2=IRCreateVarRef(v);
				__auto_type one2=IRCreateIntLit(1);
				__auto_type binop2=IRCreateBinop(vRef2, one2, IR_ADD);
				graphNodeIRConnect(binop2, zRef2, IR_CONN_DEST);
				graphNodeIRConnect( end,IRStmtStart(zRef2),  IR_CONN_FLOW) ;
						
				end=zRef2;
		} {
				__auto_type xRef3=IRCreateVarRef(x);
				__auto_type vRef3=IRCreateVarRef(v);
				__auto_type zRef3=IRCreateVarRef(z);
				__auto_type binop3=IRCreateBinop(zRef3, vRef3, IR_MULT);
				graphNodeIRConnect(binop3, xRef3, IR_CONN_DEST);
				graphNodeIRConnect( end,IRStmtStart(xRef3),  IR_CONN_FLOW);
				end=xRef3;
		}
		{
				__auto_type yRef4=IRCreateVarRef(y);
				__auto_type xRef4=IRCreateVarRef(x);
				__auto_type two4=IRCreateIntLit(2);
				__auto_type binop4=IRCreateBinop(xRef4, two4, IR_MULT);
				graphNodeIRConnect(binop4, yRef4, IR_CONN_DEST);
				graphNodeIRConnect( end,IRStmtStart(yRef4),  IR_CONN_FLOW);

				end=yRef4;
		}
		{
				__auto_type wRef5=IRCreateVarRef(w);
				__auto_type yRef5=IRCreateVarRef(y);
				__auto_type zRef5=IRCreateVarRef(z);
				__auto_type xRef5=IRCreateVarRef(x);
				__auto_type binop5_1=IRCreateBinop(zRef5, yRef5, IR_MULT);
				__auto_type binop5_2=IRCreateBinop(binop5_1,xRef5,IR_ADD);
				graphNodeIRConnect(binop5_2, wRef5, IR_CONN_DEST);
				graphNodeIRConnect( end,IRStmtStart(wRef5),  IR_CONN_FLOW);

				end=wRef5;
		}
		{
				__auto_type uRef6=IRCreateVarRef(u);
				__auto_type zRef6=IRCreateVarRef(z);
				__auto_type two6=IRCreateIntLit(2);
				__auto_type binop6=IRCreateBinop(two6, zRef6, IR_ADD);
							
				graphNodeIRConnect(binop6, uRef6, IR_CONN_DEST);
				graphNodeIRConnect( end,IRStmtStart(uRef6),  IR_CONN_FLOW);
							
				end=uRef6;
		}
		{
				__auto_type vRef7=IRCreateVarRef(v);
				__auto_type uRef7=IRCreateVarRef(u);
				__auto_type wRef7=IRCreateVarRef(w);
				__auto_type yRef7=IRCreateVarRef(y);
				__auto_type binop7_1=IRCreateBinop(uRef7, wRef7, IR_ADD);
				__auto_type binop7_2=IRCreateBinop(binop7_1,yRef7,IR_ADD);
							
				graphNodeIRConnect(binop7_2, vRef7, IR_CONN_DEST);
				graphNodeIRConnect( end,IRStmtStart(vRef7),  IR_CONN_FLOW);
							
				end=vRef7;
		}
		{
				__auto_type uRef8=IRCreateVarRef(u);
				__auto_type vRef8=IRCreateVarRef(v);
				__auto_type binop=IRCreateBinop(uRef8, vRef8, IR_MULT);
							
				graphNodeIRConnect( end,IRStmtStart(binop),  IR_CONN_FLOW);
				IRCreateReturn(binop, NULL);
		}
		__auto_type live=IRInterferenceGraph(start);
		__auto_type layout=IRComputeFrameLayout(start);
		mapFrameEntry byVar=mapFrameEntryCreate();
		assert(strFrameEntrySize(layout)==6); //1 for each variable
		for(long i=0;i!=6;i++)
				mapFrameEntryInsert(byVar, layout[i].var.value.var->name, layout[i]);
		strGraphNodeIRLiveP allNodes=graphNodeIRLiveAllNodes(live);
		assert(strGraphNodeIRLivePSize(allNodes)==6);
		for(long i=0;i!=6;i++) {
				strGraphNodeIRLiveP out=graphNodeIRLiveOutgoingNodes(allNodes[i]);
				for(long o=0;o!=strGraphNodeIRLivePSize(out);o++) {
						//Check for conflict
						__auto_type a=*mapFrameEntryGet(byVar, varNameFromLive(allNodes[i]));
						__auto_type b=*mapFrameEntryGet(byVar, varNameFromLive(out[o]));
						int success;
						__auto_type aSize=8*objectSize(a.var.value.var->type,&success);
						assert(success);
						__auto_type bSize=8*objectSize(b.var.value.var->type,&success);
						assert(success);
						//Check for overlap
						//https://stackoverflow.com/questions/3269434/whats-the-most-efficient-way-to-test-two-integer-ranges-for-overlap
						assert(!(max(a.offset,b.offset)<min(a.offset+aSize,b.offset+bSize)));
				}
		}
}
