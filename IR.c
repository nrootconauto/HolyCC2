#include <IR.h>
#include <base64.h>
#include <assert.h>
#define ALLOCATE(x) ({typeof(&x) ptr=malloc(sizeof(x));memcpy(ptr,&x,sizeof(x));ptr;})
#define GRAPHN_ALLOCATE(x) ({__graphNodeCreate(&x,sizeof(x),0);})
static int ptrCmp(const void *a,const void *b) {
		if(a>b)
				return 1;
		else if(a<b)
				return -1;
		else
				return 0;
}
int IRAttrInsertPred(const void* a ,const void* b) {
		const struct IRAttr *A=a,*B=b;
		return ptrCmp(A->name,B->name);
}
int IRAttrGetPred(const void* key ,const void* b) {
		const struct IRAttr *B=b;
		return ptrCmp(key, B->name);
}
STR_TYPE_DEF(char,Char);
STR_TYPE_FUNCS(char,Char);
static strChar ptr2Str(const void *a) {
		__auto_type res= base64Enc((const char*)&a, sizeof(a));
		__auto_type retVal=strCharAppendData(NULL, res, strlen(a)+1);
		free(res);

		return retVal;
}
graphNodeIR createIntLit(int64_t lit) {
		struct IRNodeValue val;
		val.base.attrs=NULL;
		val.base.type=IR_VALUE;
		val.val.type=IR_VAL_INT_LIT;
		val.val.value.intLit.base=10;
		val.val.value.intLit.type=INT_SLONG;
		val.val.value.intLit.value.sLong=lit;

		return GRAPHN_ALLOCATE(val);
}
graphNodeIR createBinop(graphNodeIR a,graphNodeIR b,enum IRNodeType type) {
		struct IRNodeBinop binop2;
		binop2.base.type=type;
		binop2.base.attrs=NULL;
		
		__auto_type retVal=GRAPHN_ALLOCATE(binop2);
		graphNodeIRConnect(a, retVal, IR_CONN_SOURCE_A);
		graphNodeIRConnect(b, retVal, IR_CONN_SOURCE_B);

		return retVal;
}
 graphNodeIR createLabel() {
		struct IRNodeLabel lab;
		lab.base.attrs=NULL;
		lab.base.type=IR_LABEL;

		return GRAPHN_ALLOCATE(lab);
}
struct variable *createVirtVar(struct object *type) {
		struct variable var;
		var.name=NULL;
		var.refs=NULL;
		var.type=type;
		__auto_type alloced=ALLOCATE(var);
		
		struct IRVarRefs refs;
		refs.var.type=IR_VAR_VAR;
		refs.var.value.var=alloced;
		refs.refs=0;
		
		__auto_type ptrStr=ptr2Str(alloced);
		mapIRVarRefsInsert(IRVars, ptrStr, refs); 
		strCharDestroy(&ptrStr);
		
		return alloced;
}
 graphNodeIR createVarRef(struct variable *var) {
		__auto_type ptrStr=ptr2Str(var);
		__auto_type find= mapIRVarRefsGet(IRVars, ptrStr); 
		strCharDestroy(&ptrStr);
		//TODO add on not find
		assert(find);

		struct IRNodeValue val;
		val.base.attrs=NULL;
		val.base.type=IR_VALUE;
		val.val.type=IR_VAL_VAR_REF;
		
		val.val.value.var.SSANum=find->refs;

		return GRAPHN_ALLOCATE(val);
}

 graphNodeIR createJmp(graphNodeIR to) {
		struct IRNodeJump jmp;
		jmp.base.attrs=NULL;
		jmp.forward=-1;
		jmp.base.type=IR_JUMP;

		__auto_type retVal= GRAPHN_ALLOCATE(jmp);
		graphNodeIRConnect(to, retVal, IR_CONN_FLOW);

		return retVal;
}
static graphNodeIR createValueFromLabel(graphNodeIR lab) {
		struct IRNodeValue val;
		val.base.attrs=NULL;
		val.base.type=IR_VALUE;
		val.val.type=__IR_VAL_LABEL;
		val.val.value.memLabel=lab;

		return GRAPHN_ALLOCATE(lab);
} 
extern __thread mapIRVarRefs IRVars;
