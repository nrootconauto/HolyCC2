#include <assert.h>
#include <IRFilter.h>
static void debugShowGraph(graphNodeIR enter) {
		const char *name=tmpnam(NULL);
		__auto_type map=graphNodeCreateMapping(enter, 1);
		IRGraphMap2GraphViz(map, "viz", name, NULL,NULL,NULL,NULL);
		char buffer[1024];
		sprintf(buffer, "sleep 0.1 &&dot -Tsvg %s > /tmp/dot.svg && firefox /tmp/dot.svg &", name);

		system(buffer);
}
static int isVarPred(graphNodeIR node,const void *var) {
		struct IRNodeValue *val=(void*)graphNodeIRValuePtr(node);
		if(val->base.type==IR_VALUE) {
				if(val->val.type==IR_VAL_VAR_REF) {
						return 0==IRVarCmp(var, &val->val.value.var);
				}
		}

		return 0;
}
void IRFilterTests() {
		initIR();
		__auto_type a=createVirtVar(&typeI64i);
		__auto_type b=createVirtVar(&typeI64i);
		__auto_type c=createVirtVar(&typeI64i);

		__auto_type current=createLabel();
		__auto_type start=current;
		{
				__auto_type aRef=createVarRef(a);
				__auto_type bRef1=createVarRef(b);
				__auto_type cRef=createVarRef(c);
				__auto_type add=createBinop(aRef, bRef1, IR_ADD);
				__auto_type mult=createBinop(add, cRef, IR_MULT);
	
				graphNodeIRConnect(current, aRef, IR_CONN_FLOW);
				graphNodeIRConnect(current, bRef1, IR_CONN_FLOW);
				graphNodeIRConnect(current, cRef, IR_CONN_FLOW);

				current=mult;
		}

		__auto_type label=createLabel();
		graphNodeIRConnect(current, label, IR_CONN_FLOW);
		__auto_type cRef2=createVarRef(c);
		graphNodeIRConnect(label, cRef2, IR_CONN_FLOW);
		graphNodeIRConnect( cRef2,label, IR_CONN_FLOW);

		//debugShowGraph(start);

		struct IRVar CVar;
		CVar.SSANum=0;
		CVar.type=IR_VAR_VAR;
		CVar.value.var=c;
		filterForVar(start, isVarPred, &CVar);
}
