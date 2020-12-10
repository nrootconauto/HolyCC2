#include <debugPrint.h>
#include <exprParser.h>
#include <lexer.h>
#include <object.h>
#include <registers.h>
#include <parserB.h>
#include <garbageCollector.h>
void init() {
		initAssignOps();
		gcCollect();
		initDebugPrint();
		gcCollect();
		initTemplates();
		gcCollect();
		initObjectRegistry();
		gcCollect();
		initRegisters();
		gcCollect();
		initParserData();
}
