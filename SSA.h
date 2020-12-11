#pragma once
#include <IR.h>
void IRToSSA(graphNodeIR enter);
MAP_TYPE_DEF(char *, Str);
MAP_TYPE_FUNCS(char *, Str);
mapStr nodeNames;
void IRSSAReplaceChooseWithAssigns(graphNodeIR node,strGraphNodeIRP *replaced);
