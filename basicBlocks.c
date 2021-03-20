#include "basicBlocks.h"
const void *IR_ATTR_BASIC_BLOCK="BASIC_BLOCK";
static void strVarDestroy2(strVar *vars) {
		for(long v=0;v!=strVarSize(*vars);v++)
				variableDestroy(vars[0][v].var);
		strVarDestroy(vars);
}
void IRAttrBasicBlockDestroy(struct IRAttr *bb) {
		struct basicBlockAttr *BB=(void*)bb;
		if(0>=--BB->bb->refCount) {
				strVarDestroy2(&BB->bb->in);
				strVarDestroy2(&BB->bb->out);
				free(BB->bb);
		}
}
