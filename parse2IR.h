#include <IR.h>
#include <parserA.h>
struct enterExit {
	graphNodeIR enter, exit;
};
struct enterExit parserNodes2IR(strParserNode node);
void IRGenInit();
extern const void *IR_ATTR_LABEL_NAME;
struct IRAttrLabelName {
	struct IRAttr base;
	char *name;
};
