#pragma once
#include <IR.h>
void IRToSSA(strGraphNodeIRP nodes, graphNodeIR enter);
MAP_TYPE_DEF(char *,Str);
MAP_TYPE_FUNCS(char *,Str);
mapStr nodeNames;
