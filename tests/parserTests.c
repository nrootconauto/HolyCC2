#include <assert.h>
#include <cacheingLexerItems.h>
#include <parserBase.h>
#include <holyCParser.h>
STR_TYPE_DEF(char, Char);
STR_TYPE_FUNCS(char, Char);
static int charCmp(const void *a, const void *b) {
	return *(char *)a == *(char *)b;
}
static struct __lexerItemTemplate intTemplate;
static struct __lexerItemTemplate opsTemplate;
void parserTests() {
	const char *text = "1 + 2 + 3";
	__auto_type textStr = strCharAppendData(NULL, text, strlen(text));

	__auto_type g = holyCGrammarCreate();

	__auto_type lex = lexerCreate((struct __vec *)textStr, holyCLexerTemplates(), charCmp,
	                              skipWhitespace);

	int success;
	parse(g, lexerGetItems(lex), &success, NULL);
	assert(success);
}
