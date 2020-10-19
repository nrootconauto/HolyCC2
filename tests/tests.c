void llTests();
void strTests();
void graphTests();
void mapTests();
void diffTests();
void subGraphTests();
void cachingLexerTests();
void preprocessorTests();
void eventPoolTests();
void parserTests();
int main() {
	strTests();
	llTests();
	mapTests();
	strTests();
	graphTests();
	diffTests();
	subGraphTests();
	cachingLexerTests();
	preprocessorTests();
	eventPoolTests();
	parserTests();
	return 0;
}
