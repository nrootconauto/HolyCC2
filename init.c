#include <debugPrint.h>
#include <exprParser.h>
#include <lexer.h>
#include <object.h>
#include <registers.h>
#include <parserB.h>
#include <garbageCollector.h>
#include <signal.h>
#include <execinfo.h>
#include <stdio.h>
static void printBT(int sig) {
		void *array[50];
		int len=backtrace(array, 50);
		__auto_type syms=backtrace_symbols(array, len);
		for(long i=0;i!=len;i++) {
				printf("%s\n", syms[i]);
		}
		getchar();
		abort();
}
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

		signal(SIGSEGV,printBT);
}
