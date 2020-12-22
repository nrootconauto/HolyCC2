#include <IR.h>
#include <lexer.h>
#include <parserA.h>
#include <string.h>
#include <assert.h>
#include <parse2IR.h>
#include <parserB.h>
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
				__auto_type item=parseStatement(at, &at);
				assert(item);
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
				__auto_type nodes=parseText("for(I64i x=0;x!=10;x++) {'foo11'\n;}");
				IRGenInit();
				__auto_type res=parserNodes2IR(nodes);
				//IRRemoveNeedlessLabels(res.enter);
				//debugShowGraph(res.enter);
		}
		{
				initParserData();
				__auto_type nodes=parseText("while(1) {'foo11'\n;}");
				IRGenInit();
				__auto_type res=parserNodes2IR(nodes);
				//IRRemoveNeedlessLabels(res.enter);
				//debugShowGraph(res.enter);
		}
		{
				initParserData();
				__auto_type nodes=parseText(" do {break;} while(1)");
				IRGenInit();
				__auto_type res=parserNodes2IR(nodes);
				//IRRemoveNeedlessLabels(res.enter);
				//debugShowGraph(res.enter);
		}
		{
				initParserData();
				__auto_type nodes=parseText("switch(1) {case 0: break;case 1:break;default:break;};");
				IRGenInit();
				__auto_type res=parserNodes2IR(nodes);
				//IRRemoveNeedlessLabels(res.enter);
				debugShowGraph(res.enter);
		}
} 
