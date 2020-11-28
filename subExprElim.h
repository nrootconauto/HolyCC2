#pragma once
#include <IR.h>
void replaceSubExprsWithVars();
void clearSubExprs();
void findSubExprs(const graphNodeIR node);
