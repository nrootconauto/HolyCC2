#include <IR.h>
#include <parserA.h>
#include <preprocessor.h>
struct enterExit {
	graphNodeIR enter, exit;
};
struct enterExit parserNodes2IR(strParserNode node);
void IRGenInit(strFileMappings mappings);
extern const void *IR_ATTR_LABEL_NAME;
struct IRAttrLabelName {
	struct IRAttr base;
	char *name;
};
void initParse2IR();
