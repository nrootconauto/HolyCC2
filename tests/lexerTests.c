#include <assert.h>
#include <lexer.h>
#include <string.h>
static llLexerItem expectKw(llLexerItem node, const char *text) {
	__auto_type item = llLexerItemValuePtr(node);
	assert(item->template == &kwTemplate);
	__auto_type kw = *(const char **)lexerItemValuePtr(item);
	assert(0 == strcmp(text, kw));

	return llLexerItemNext(node);
}
static llLexerItem expectOp(llLexerItem node, const char *text) {
	__auto_type item = llLexerItemValuePtr(node);
	assert(item->template == &opTemplate);
	__auto_type op = *(const char **)lexerItemValuePtr(item);
	assert(0 == strcmp(text, op));

	return llLexerItemNext(node);
}
static llLexerItem expectName(llLexerItem node, const char *text) {
	__auto_type item = llLexerItemValuePtr(node);
	assert(item->template == &nameTemplate);
	__auto_type name = (const char *)lexerItemValuePtr(item);
	assert(0 == strcmp(text, name));

	return llLexerItemNext(node);
}
static llLexerItem expectInt(llLexerItem node, int value) {
	__auto_type item = llLexerItemValuePtr(node);
	assert(item->template == &intTemplate);
	__auto_type value2 = (struct lexerInt *)lexerItemValuePtr(item);
	assert(value2->value.sInt == value);

	return llLexerItemNext(node);
}
static llLexerItem expectStr(llLexerItem node, const char *text) {
	__auto_type item = llLexerItemValuePtr(node);
	assert(item->template == &strTemplate);
	__auto_type value2 = (struct parsedString *)lexerItemValuePtr(item);
	assert(0 == strcmp((char *)value2->text, text));

	return llLexerItemNext(node);
}
static llLexerItem expectFloating(llLexerItem node, int whole, int zeros,
                                  int frac, int exp) {
	__auto_type item = llLexerItemValuePtr(node);
	assert(item->template == &floatTemplate);
	__auto_type value2 = (struct lexerFloating *)lexerItemValuePtr(item);
	assert(value2->base == whole);
	assert(value2->exponet == exp);
	assert(value2->zerosBeforeBase == zeros);
	assert(value2->frac == frac);
	return llLexerItemNext(node);
}
void lexerTests() {
	const char *text = "if (a==1.04) 'text',1+1";
	__auto_type str = __vecAppendItem(NULL, text, strlen(text));
	int err;
	__auto_type items = lexText(str, &err);
	assert(!err);
	items = expectKw(items, "if");
	items = expectOp(items, "(");
	items = expectName(items, "a");
	items = expectOp(items, "==");
	items = expectFloating(items, 1, 1, 4, 0);
	items = expectOp(items, ")");
	items = expectStr(items, "text");
	items = expectOp(items, ",");
	items = expectInt(items, 1);
	items = expectOp(items, "+");
	items = expectInt(items, 1);
}
