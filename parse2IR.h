#include "IR.h"
#include "parserA.h"
#include "preprocessor.h"
struct enterExit {
	graphNodeIR enter, exit;
};
struct enterExit parserNodes2IR(strParserNode node);
void IRGenInit(strFileMappings mappings);
struct IRAttrLabelName {
	struct IRAttr base;
	char *name;
};
void initParse2IR();
graphNodeIR parserNode2Expr(const struct parserNode *node);
