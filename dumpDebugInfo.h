#pragma once
#include "IR.h"
#include "frameLayout.h"
#include <stdio.h>
PTR_MAP_FUNCS(struct reg *, struct IRVar, IRVarByReg);
char *emitDebuggerTypeDefinitions();
