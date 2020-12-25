#include <IR.h>
#include <lexer.h>
#include <parserA.h>
#include <string.h>
#include <assert.h>
#include <parse2IR.h>
#include <parserB.h>
#include <IRExec.h>
static void debugShowGraph(graphNodeIR enter) {
		const char *name=tmpnam(NULL);
		__auto_type map=graphNodeCreateMapping(enter, 1);
		IRGraphMap2GraphViz(map, "viz", name, NULL,NULL,NULL,NULL);
		char buffer[1024];
		sprintf(buffer, "sleep 0.1 &&dot -Tsvg %s > /tmp/dot.svg && firefox /tmp/dot.svg &", name);

		system(buffer);
}
static struct __vec *str2Vec(const char *text) {
		return __vecAppendItem(NULL, text, strlen(text)+1);
}
strParserNode parseText(const char *text) {
		int err;
		__auto_type lexed=lexText(str2Vec(text),&err);
		assert(!err);

		strParserNode retVal=NULL;
		llLexerItem at=llLexerItemFirst(lexed);
		while(at) {
				__auto_type oldAt=at;
				__auto_type item=parseStatement(at, &at);
				assert(oldAt!=at);
				//item may be NULL if empty statement.
				if(!item)
						continue;
				retVal=strParserNodeAppendItem(retVal, item);
		}

		return retVal;
}
void parse2IRTests() {
		{
				initParserData();
				__auto_type nodes=parseText("1+2;");
				IRGenInit();
				__auto_type res=parserNodes2IR(nodes);
				//debugShowGraph(res.enter);
				
		}
		{
				initParserData();
				__auto_type nodes=parseText("if(1+2) {3+4;} else {5+6;}");
				IRGenInit();
				__auto_type res=parserNodes2IR(nodes);
				IRRemoveNeedlessLabels(res.enter);
				//debugShowGraph(res.enter);
		}
		{
				initParserData();
				__auto_type nodes=parseText("I64i foo() {I64i a=0; for(I64i x=0;x!=2;x++) {a=a+1;} return a;}; foo();");
				IRGenInit();
				__auto_type res=parserNodes2IR(nodes);
				IREvalInit();
				int success;
				//				debugShowGraph(res.enter);
				__auto_type retVal=IREvalPath(res.enter, &success);
				assert(success);
				assert(retVal.type==IREVAL_VAL_INT);
				assert(retVal.value.i==2);
				//IRRemoveNeedlessLabels(res.enter);
				//debugShowGraph(res.enter);
		}
		{
				initParserData();
				__auto_type nodes=parseText("I64i foo() {I64i a=0;while(3>a) {a=a+1;} return a;}; foo();");
				IRGenInit();
				__auto_type res=parserNodes2IR(nodes);
				//IRRemoveNeedlessLabels(res.enter);
				//debugShowGraph(res.enter);
				int success;
				IREvalInit();
				__auto_type retVal=IREvalPath(res.enter, &success);
				assert(success);
				assert(retVal.type==IREVAL_VAL_INT);
				assert(retVal.value.i==3);
		}
		{
				initParserData();
				__auto_type nodes=parseText("I64i foo() {I64i x=10; do {break;x=20;} while(1); return x;} foo();");
				IRGenInit();
				__auto_type res=parserNodes2IR(nodes);
				//IRRemoveNeedlessLabels(res.enter);
				//debugShowGraph(res.enter);
				IREvalInit();
				int success;
				__auto_type retVal=IREvalPath(res.enter, &success);
				assert(success);
				assert(retVal.type==IREVAL_VAL_INT);
				assert(retVal.value.i==10);
		}{
				initParserData();
				__auto_type nodes=parseText("I64i foo() {switch(1) {case 0: break;case 1:return 1;default:break;} return 0;} foo();");
				IRGenInit();
				__auto_type res=parserNodes2IR(nodes);
				//IRRemoveNeedlessLabels(res.enter);
				//debugShowGraph(res.enter);
				IREvalInit();
				int success;
				__auto_type retVal=IREvalPath(res.enter, &success);
				assert(success);
				assert(retVal.type==IREVAL_VAL_INT);
				assert(retVal.value.i==1);
		}
		{
				initParserData();
				__auto_type nodes=parseText("I64i foo() {I64i a=0; switch(1) {start: a=1; case 0: break;case 1: a=a+1;break;default:break;end:;} return a;} foo();");
				IRGenInit();
				__auto_type res=parserNodes2IR(nodes);
				//IRRemoveNeedlessLabels(res.enter);
				//debugShowGraph(res.enter);
				IREvalInit();
				int success;
				__auto_type retVal=IREvalPath(res.enter, &success);
				assert(success);
				assert(retVal.type==IREVAL_VAL_INT);
				assert(retVal.value.i==2);
		}
			{
				initParserData();
				__auto_type nodes=parseText("U8i foo(U64i a) {return a+4;}");
				IRGenInit();
				__auto_type res=parserNodes2IR(nodes);
				//				debugShowGraph(res.enter);
				
		}
} 
