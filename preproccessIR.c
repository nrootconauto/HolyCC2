#include <IR.h>
#include <cleanup.h>
#include <ptrMap.h>
#include <IRFilter.h>
static int isNotExprEdge(const void *data,const graphEdgeIR *edge) {
		return !IRIsExprEdge(*graphEdgeIRValuePtr(*edge));
}
PTR_MAP_FUNCS(graphNodeIR, graphNodeIR, AffectedNodes);
static void __IRInsertNodesBetweenExprs(graphNodeIR expr,ptrMapAffectedNodes affected) {
		if(ptrMapAffectedNodesGet(affected,expr))
				return;

		strGraphEdgeIRP in CLEANUP(strGraphEdgeIRPDestroy) =graphNodeIRIncoming(expr);
		in=strGraphEdgeIRPRemoveIf(in, NULL, isNotExprEdge);
		for(long i=0;i!=strGraphEdgeIRPSize(in);i++) {
				//Ignore existing assigns
				if(*graphEdgeIRValuePtr(in[i])==IR_CONN_DEST)
						continue;
				
				__auto_type node=graphEdgeIRIncoming(in[i]);
				struct IRNodeValue *nodeValue=(void*)graphNodeIRValuePtr(node);
				if(nodeValue->base.type==IR_VALUE)
						continue;
				//Not a value so insert a variable after the operation(the varaible will be assigned into)
				__auto_type tmp=IRCreateVirtVar(IRNodeType(node));
				__auto_type tmpRef=IRCreateVarRef(tmp);
				IRInsertAfter(node,tmpRef,tmpRef, IR_CONN_DEST);
				//Recursivly do the same for incoming expression
				__IRInsertNodesBetweenExprs(node,affected);
		}
		ptrMapAffectedNodesAdd(affected, expr, NULL);
}
static int isExprNode(graphNodeIR node,const void *data) {
		strGraphEdgeIRP in CLEANUP(strGraphEdgeIRPDestroy)=graphNodeIRIncoming(node);
		strGraphEdgeIRP out CLEANUP(strGraphEdgeIRPDestroy)=graphNodeIROutgoing(node);
		in=strGraphEdgeIRPRemoveIf(in, NULL, isNotExprEdge);
		out=strGraphEdgeIRPRemoveIf(out, NULL, isNotExprEdge);
		return strGraphEdgeIRPSize(in)||strGraphEdgeIRPSize(out);
}
static void IRInsertNodesBetweenExprs(graphNodeIR expr) {
		__auto_type filtered=IRFilter(expr,  isExprNode,  NULL);
		strGraphNodeMappingP all CLEANUP(strGraphNodeMappingPDestroy)= graphNodeMappingAllNodes(filtered);
		__auto_type affected=ptrMapAffectedNodesCreate();
		for(long i=0;i!=strGraphNodeMappingPSize(all);i++) {
				__IRInsertNodesBetweenExprs(*graphNodeMappingValuePtr(all[i]),affected);
		}
		ptrMapAffectedNodesDestroy(affected, NULL);
		graphNodeMappingKillGraph(&filtered, NULL, NULL);
}
