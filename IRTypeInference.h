#pragma once
#include "IR.h"
struct object *IRNodeType(graphNodeIR node);
void IRInsertImplicitTypecasts(graphNodeIR start);
void IRNodeTypeAssign(graphNodeIR node,struct object *type);
