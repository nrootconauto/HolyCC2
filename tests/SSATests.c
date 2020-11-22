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

		node=IRGetStmtStart(node);
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
				__auto_type var=createVirtVar(&typeI64i);

				__auto_type enter=createLabel();
		
				__auto_type a=createVarRef(var);
				__auto_type b=createVarRef(var);
				__auto_type c=createVarRef(var);
				__auto_type d=createVarRef(var);
				__auto_type e=createVarRef(var);
				__auto_type f=createVarRef(var);
				
				graphNodeIRConnect(enter, a, IR_CONN_FLOW);
		
				__auto_type cond1=createIntLit(101);
				__auto_type cond2=createIntLit(101);
		
				__auto_type aB_FCJmp=createCondJmp(cond1, b ,f);
				graphNodeIRConnect(a,cond1,IR_CONN_FLOW);
		
				__auto_type bC_DCJmp=createCondJmp(cond2, c ,d);
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
				
				__auto_type allNodes=graphNodeIRAllNodes(enter);
				IRToSSA(allNodes,enter);
				
				//
				//Assert for Choose nodes at (select)enter points
				//
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
				assert(cSSAVer<eSSAVer);
				assert(dSSAVer<eSSAVer);
				assert(eSSAVer<fSSAVer);
		}
		/*
		//http://pages.cs.wisc.edu/~fischer/cs701.f05/lectures/Lecture22.pdf
		{
				__auto_type var=createVirtVar(&typeI64i);

				__auto_type enter=createLabel();
		
				ASSIGN_EXPR(a, createIntLit(0), var);
				ASSIGN_EXPR(b, createIntLit(1), var);
				ASSIGN_EXPR(c, createIntLit(2), var);
				INSERT_NAME(enter);
				INSERT_NAME(aExit);
				INSERT_NAME(bExit);
				INSERT_NAME(cExit);

				graphNodeIRConnect(enter, aEnter, IR_CONN_FLOW);
				graphNodeIRConnect(aExit, bEnter, IR_CONN_FLOW);
				__auto_type cond=createIntLit(101);
				__auto_type bA_CCJmp=createCondJmp(cond, aEnter ,cEnter);
				graphNodeIRConnect(bExit,cond, IR_CONN_FLOW);
				
				__auto_type allNodes=graphNodeIRAllNodes(enter);
				IRToSSA(allNodes,enter);
		}
		*/
}
