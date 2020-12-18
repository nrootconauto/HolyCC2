#include <IR.h>
#include <lexer.h>
#include <parserA.h>
#include <string.h>
#include <assert.h>
#include <parse2IR.h>
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
void parse2IRTests() {
		{
				const char *text="if(1+2) {I64i x=10;}";
				int err;
				__auto_type lexed=lexText(str2Vec(text),&err);
				assert(err);
				__auto_type item=parseStatement(llLexerItemFirst(lexed), NULL);
				assert(item);

				__auto_type node= parserNode2IR(item);
				debugShowGraph(node);
		}
}
