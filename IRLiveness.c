#include <IR.h>
static int filterVars(void * data,struct __graphNode* node) {
		graphNodeIR enterNode=data;
		if(node==enterNode)
				return 1;
		
		if(graphNodeIRValuePtr(node)->type!=IR_VALUE)
				return 0;

		struct IRNodeValue *irNode=(void*)graphNodeIRValuePtr(node);
		if(irNode->val.type!=IR_VAL_VAR_REF)
				return 0;

		return 1;
}
struct IRVarLiveness {
		graphNodeIR var;
};
GRAPH_TYPE_DEF(struct IRVarLiveness, void*, IRLive);
GRAPH_TYPE_FUNCS(struct IRVarLiveness, void*, IRLive);
void IRLivenessGraph(graphNodeIR start) {
		__auto_type allNodes=graphNodeIRAllNodes(start);
		__auto_type vars createFilteredGraph(start,allNodes , start, filterVars);
}
