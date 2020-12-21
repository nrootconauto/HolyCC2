#include <debugPrint.h>
#include <execinfo.h>
#include <exprParser.h>
#include <lexer.h>
#include <object.h>
#include <parserB.h>
#include <registers.h>
#include <signal.h>
#include <stdio.h>
#include <parse2IR.h>
static void printBT(int sig) {
	void *array[50];
	int len = backtrace(array, 50);
	__auto_type syms = backtrace_symbols(array, len);
	for (long i = 0; i != len; i++) {
		printf("%s\n", syms[i]);
	}
	getchar();
	abort();
}
void init() {
	initAssignOps();
	initDebugPrint();
	initTemplates();
	initObjectRegistry();
	initRegisters();
	initParserData();
	initParse2IR();
	//	signal(SIGSEGV,printBT);
}
