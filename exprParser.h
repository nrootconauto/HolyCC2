#pragma once
struct parserNode;
struct object *assignTypeToOp(struct parserNode *node);
void initAssignOps();
void initExprParser();
