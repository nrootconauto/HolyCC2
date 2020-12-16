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
				__auto_type labelNode=IRCreateLabel();
				__auto_type intNode=IRCreateIntLit(10);
				__auto_type strNode=IRCreateStrLit("Hello World");
				graphNodeIRConnect(labelNode, intNode, IR_CONN_FLOW);
				graphNodeIRConnect(labelNode, strNode, IR_CONN_FLOW);
				__auto_type var1=IRCreateVirtVar(&typeI64i);
				__auto_type var1RefNode=IRCreateVarRef(var1);
				graphNodeIRConnect(labelNode, var1RefNode, IR_CONN_FLOW);
				__auto_type binop=IRCreateBinop(intNode, var1RefNode, IR_ADD);
				__auto_type unopNode=IRCreateUnop(binop,IR_NEG);


				__auto_type labelNode2=IRCreateLabel();
				__auto_type condJump=IRCreateCondJmp(unopNode, labelNode, labelNode2);

				__auto_type var2=IRCreateVirtVar(&typeI64i);
				__auto_type var2RefNode=IRCreateVarRef(var2);
				graphNodeIRConnect(labelNode2, var2RefNode, IR_CONN_FLOW);

				struct IRNodeChoose choose;
				choose.base.attrs=NULL;
				choose.base.type=IR_CHOOSE;
				choose.canidates=strGraphNodeIRPAppendItem(NULL, var1RefNode);
				choose.canidates=strGraphNodeIRPAppendItem(choose.canidates, var2RefNode);
				__auto_type chooseNode=GRAPHN_ALLOCATE(choose);
				graphNodeIRConnect(var1RefNode, chooseNode, IR_CONN_DEST);
				graphNodeIRConnect(var2RefNode, chooseNode, IR_CONN_DEST);
		
				__auto_type stmtStartNode=IRCreateStmtStart();
				__auto_type funcVar=IRCreateVirtVar(&typeI64i);
				__auto_type funcVarNode=IRCreateVarRef(funcVar);
				graphNodeIRConnect(stmtStartNode, funcVarNode, IR_CONN_FLOW);
				
				__auto_type funcCallNode= IRCreateFuncCall(funcVarNode,IRCreateTypecast(unopNode,&typeI64i,&typeF64), var2RefNode,NULL);
				__auto_type stmtEndNode=IRCreateStmtEnd(stmtStartNode);
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
