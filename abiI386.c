#include <IR.h>
#include <registers.h>
static int ptrPtrCmp(const void *a, const void *b) {
	if (*(void **)a > *(void **)b)
		return 1;
	else if (*(void **)a < *(void **)b)
		return -1;
	return 0;
}
typedef int(*regCmpType)(const struct reg **,const struct reg **);
static strRegP usedRegisters(strGraphNodeIRP nodes) {
		strRegP retVal=NULL;
		for(long i=0;i!=strGraphNodeIRPSize(nodes);i++) {
				struct IRNodeValue *value=(void*)graphNodeIRValuePtr(nodes[i]);
				if(value->base.type==IR_VALUE)
						if(value->val.type==IR_VAL_REG) {
								__auto_type reg=value->val.value.reg.reg;
								if(strRegPSortedFind(retVal, reg, (regCmpType)ptrPtrCmp))
										retVal=strRegPSortedInsert(retVal, reg, (regCmpType)ptrPtrCmp);
						}
		}
		return retVal;
}
void IR2ABI_i386() {
		
}
