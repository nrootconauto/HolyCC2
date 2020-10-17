void llTests();
void strTests();
void graphTests();
void mapTests();
void diffTests();
void subGraphTests();
void cykTests();
void cachingLexerTests();
void preprocessorTests();
void eventPoolTests();
void cykGrammarGeneratorTests();
int main() {
 strTests();
 llTests();
 mapTests();
 /*
	strTests();
	graphTests();
	
	diffTests();
	subGraphTests();
	cykTests();
	cachingLexerTests();
	preprocessorTests();
	eventPoolTests();
	*/
	//cykGrammarGeneratorTests();
	return 0;
}
