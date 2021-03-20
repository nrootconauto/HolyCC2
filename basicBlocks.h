#pragma once
#include "IR.h"
STR_TYPE_DEF(struct IRVar, Var);
STR_TYPE_FUNCS(struct IRVar, Var);
struct basicBlock {
	long refCount;
	strVar in;
	strVar out;
};
struct basicBlockAttr {
		struct IRAttr base;
		struct basicBlock *bb;
};
extern const void *IR_ATTR_BASIC_BLOCK;
void IRAttrBasicBlockDestroy(struct IRAttr *bb);
