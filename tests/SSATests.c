#include <assert.h>
#include <IR.h>
#include <SSA.h>
#include <graphDominance.h>
#include <stdarg.h>
#include <stdio.h>
static int ptrPtrCmp(const void *a,const void *b) {
		if(*(void**)a>*(void**)b)
				return 1;
		else if(*(void**)a<*(void**)b)
				return 1;
		else
				return 0;
}
static void debugShowGraph(graphNodeIR enter) {
		const char *name=tmpnam(NULL);
		__auto_type map=graphNodeCreateMapping(enter, 1);
		IRGraphMap2GraphViz(map, "viz", name, NULL,NULL,NULL,NULL);
		char buffer[1024];
		sprintf(buffer, "dot -Tsvg %s > /tmp/dot.svg && firefox /tmp/dot.svg &", name);

		system(buffer);
}
typedef int(*gnIRCmpType)(const graphNodeIR *,const graphNodeIR *);
static void assertSSANodes(graphNodeIR node,...) {
		strGraphNodeIRP expected=NULL;
		
		va_list args;
		va_start(args, node);
		for(;;) {
				__auto_type expected2=va_arg(args, graphNodeIR);
				if(!expected2)
						break;

				expected=strGraphNodeIRPSortedInsert(expected, expected2, (gnIRCmpType)ptrPtrCmp);
		}
		va_end(args);

		node=IRStmtStart(node);
		//   [Choose]
		//      ||  (IR_CONN_DEST)
		//      \/
		// [New SSA Node]
		//      || (IR_CONN_FLOW)
		//      \/
		//[Expression "as normal"]
		__auto_type in=graphNodeIRIncomingNodes(node);
		assert(strGraphNodeIRPSize(in)==1);
		__auto_type type= graphNodeIRValuePtr(in[0])->type;
		assert(type==IR_VALUE); //Reference to variable
		
		__auto_type in2=graphNodeIRIncomingNodes(in[0]);
		assert(strGraphNodeIRPSize(in2)==1);
		__auto_type type2= graphNodeIRValuePtr(in2[0])->type;
		assert(type2==IR_CHOOSE);
		struct IRNodeChoose *choose=(void*)graphNodeIRValuePtr(in2[0]);

		assert(strGraphNodeIRPSize(choose->canidates)==strGraphNodeIRPSize(expected));
		for(long i=0;i!=strGraphNodeIRPSize(expected);i++ ) {
				assert(strGraphNodeIRPSortedFind(expected, choose->canidates[0], (gnIRCmpType)ptrPtrCmp));
		}
}
#define INSERT_NAME(item ) ({char buffer[128]; sprintf(buffer, "%p", item); mapStrInsert(nodeNames,  buffer,#item); })
void SSATests() {
		// http://pages.cs.wisc.edu/~fischer/cs701.f05/lectures/Lecture22.pdf
		{
				initIR();
				__auto_type var=IRCreateVirtVar(&typeI64i);

				__auto_type enter=IRCreateLabel();
		
				__auto_type a=IRCreateVarRef(var);
				__auto_type b=IRCreateVarRef(var);
				__auto_type c=IRCreateVarRef(var);
				__auto_type d=IRCreateVarRef(var);
				__auto_type e=IRCreateVarRef(var);
				__auto_type f=IRCreateVarRef(var);
				
				graphNodeIRConnect(enter, a, IR_CONN_FLOW);
		
				__auto_type cond1=IRCreateIntLit(101);
				__auto_type cond2=IRCreateIntLit(101);
		
				__auto_type aB_FCJmp=IRCreateCondJmp(cond1, b ,f);
				graphNodeIRConnect(a,cond1,IR_CONN_FLOW);
		
				__auto_type bC_DCJmp=IRCreateCondJmp(cond2, c ,d);
				graphNodeIRConnect(b,cond2,IR_CONN_FLOW);	
	
				graphNodeIRConnect(c, e, IR_CONN_FLOW);
				graphNodeIRConnect(d, e, IR_CONN_FLOW);
				graphNodeIRConnect(e, f, IR_CONN_FLOW);
		
				nodeNames=mapStrCreate();
				INSERT_NAME(enter);
				INSERT_NAME(a);
				INSERT_NAME(b);
				INSERT_NAME(c);
				INSERT_NAME(d);
				INSERT_NAME(e);
				INSERT_NAME(f);
				
				IRToSSA(enter);
				//debugShowGraph(enter);
				
				//
				//Assert for Choose nodes at (select)enter points
				//
				//debugShowGraph(enter);
				assertSSANodes(f, e,a,NULL);
				assertSSANodes(e, c,d,NULL);
				
				__auto_type aSSAVer=((struct IRNodeValue *)graphNodeIRValuePtr(a))->val.value.var.SSANum;
				__auto_type bSSAVer=((struct IRNodeValue *)graphNodeIRValuePtr(b))->val.value.var.SSANum;
				__auto_type cSSAVer=((struct IRNodeValue *)graphNodeIRValuePtr(c))->val.value.var.SSANum;
				__auto_type dSSAVer=((struct IRNodeValue *)graphNodeIRValuePtr(d))->val.value.var.SSANum;
				__auto_type eSSAVer=((struct IRNodeValue *)graphNodeIRValuePtr(e))->val.value.var.SSANum;
				__auto_type fSSAVer=((struct IRNodeValue *)graphNodeIRValuePtr(f))->val.value.var.SSANum;

				assert(aSSAVer==bSSAVer);
				assert(bSSAVer==cSSAVer);
				assert(bSSAVer==dSSAVer);
				assert(cSSAVer!=eSSAVer);
				assert(dSSAVer!=eSSAVer);
				assert(eSSAVer!=fSSAVer);

		loop:;
				__auto_type allNodes=graphNodeIRAllNodes(enter);
				for(long i=0;i!=strGraphNodeIRPSize(allNodes);i++) {
						if(graphNodeIRValuePtr(allNodes[i])->type==IR_CHOOSE) {
								IRSSAReplaceChooseWithAssigns(allNodes[i],NULL);
								goto loop;
								//debugShowGraph(enter);
						}
				}
				
				
		}
		//http://pages.cs.wisc.edu/~fischer/cs701.f05/lectures/Lecture22.pdf
		{
				__auto_type var=IRCreateVirtVar(&typeI64i);

				__auto_type enter=IRCreateLabel();

				__auto_type a=IRCreateVarRef(var);
				__auto_type b=IRCreateVarRef(var);
				__auto_type c=IRCreateVarRef(var);
				
				INSERT_NAME(enter);
				INSERT_NAME(a);
				INSERT_NAME(b);
				INSERT_NAME(c);

				graphNodeIRConnect(enter, a, IR_CONN_FLOW);
				graphNodeIRConnect(a, b, IR_CONN_FLOW);
				__auto_type cond=IRCreateIntLit(101);
				__auto_type bA_CCJmp=IRCreateCondJmp(cond, a ,c);
				graphNodeIRConnect(b,cond, IR_CONN_FLOW);

				
				IRToSSA(enter);
				
				__auto_type aSSAVer=((struct IRNodeValue *)graphNodeIRValuePtr(a))->val.value.var.SSANum;
				__auto_type bSSAVer=((struct IRNodeValue *)graphNodeIRValuePtr(b))->val.value.var.SSANum;
				__auto_type cSSAVer=((struct IRNodeValue *)graphNodeIRValuePtr(c))->val.value.var.SSANum;
				int x=1+2;
		}
}
