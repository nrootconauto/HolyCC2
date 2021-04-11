#pragma once
struct parserNode;
struct object *assignTypeToOp(struct parserNode *node);
void initAssignOps();
void initExprParser();
void parserValidateAssign(struct parserNode *a,struct parserNode *b);
