#include <IR.h>
#include <unistd.h>
#include <stdio.h>
#define GRAPHN_ALLOCATE(node) ({__graphNodeCreate(&node,sizeof(node),0);})
void graphVizTests() {
		initIR();
		{
				//
				// Create a graph with all the node types
				//
				const char *fn=tmpnam(NULL);
				__auto_type labelNode=createLabel();
				__auto_type intNode=createIntLit(10);
				__auto_type strNode=createStrLit("Hello World");
				graphNodeIRConnect(labelNode, intNode, IR_CONN_FLOW);
				graphNodeIRConnect(labelNode, strNode, IR_CONN_FLOW);
				__auto_type var1=createVirtVar(&typeI64i);
				__auto_type var1RefNode=createVarRef(var1);
				graphNodeIRConnect(labelNode, var1RefNode, IR_CONN_FLOW);
				__auto_type binop=createBinop(intNode, var1RefNode, IR_ADD);
				__auto_type unopNode=createUnop(binop,IR_NEG);


				__auto_type labelNode2=createLabel();
				__auto_type condJump=createCondJmp(unopNode, labelNode, labelNode2);

				__auto_type var2=createVirtVar(&typeI64i);
				__auto_type var2RefNode=createVarRef(var2);
				graphNodeIRConnect(labelNode2, var2RefNode, IR_CONN_FLOW);

				struct IRNodeChoose choose;
				choose.base.attrs=NULL;
				choose.base.type=IR_CHOOSE;
				choose.canidates=strGraphNodeIRPAppendItem(NULL, var1RefNode);
				choose.canidates=strGraphNodeIRPAppendItem(choose.canidates, var2RefNode);
				__auto_type chooseNode=GRAPHN_ALLOCATE(choose);
				graphNodeIRConnect(var1RefNode, chooseNode, IR_CONN_DEST);
				graphNodeIRConnect(var2RefNode, chooseNode, IR_CONN_DEST);
		
				__auto_type stmtStartNode=createStmtStart();
				__auto_type funcVar=createVirtVar(&typeI64i);
				__auto_type funcVarNode=createVarRef(funcVar);
				graphNodeIRConnect(stmtStartNode, funcVarNode, IR_CONN_FLOW);
				
				__auto_type funcCallNode= createFuncCall(funcVarNode,createTypecast(unopNode,&typeI64i,&typeF64), var2RefNode,NULL);
				__auto_type stmtEndNode=createStmtEnd(stmtStartNode);
				graphNodeIRConnect(funcCallNode, stmtEndNode, IR_CONN_FLOW);
		
				graphNodeIRConnect(chooseNode, stmtStartNode, IR_CONN_FLOW);

				__auto_type mapping=graphNodeCreateMapping(labelNode, 1);
				printf("Toads\n");
				IRGraphMap2GraphViz(mapping, "Test", fn, NULL , NULL, NULL, NULL);
				printf("Result file is:%s\n",fn);

				const char *format="dot -Tsvg %s >/tmp/dot.svg && firefox /tmp/dot.svg &";
				long len=snprintf(NULL, 0, format, fn);
				char buffer[len+1];
				sprintf(buffer,  format,fn);

				system(buffer);
		}
}
