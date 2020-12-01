#pragma once
#include <IR.h>
void IRCoalesce(strGraphNodeIRP nodes, graphNodeIR start);
void IRRemoveRepeatAssigns(graphNodeIR enter);
