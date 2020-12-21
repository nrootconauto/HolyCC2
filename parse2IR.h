#include <IR.h>
#include <parserA.h>
struct enterExit {
		graphNodeIR enter,exit;
};
void initParse2IR();
struct enterExit parserNodes2IR(strParserNode node);
void IRGenInit();
