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
void graphDominanceTests();
int main() {
	strTests();
	graphDominanceTests();
	/**
	  
	  llTests();
	  mapTests();
	  strTests();
	  graphTests();
	  diffTests();
	  subGraphTests();

	  preprocessorTests();
	  eventPoolTests();
	  */
	//cachingLexerTests();
	//parserTests();
	return 0;
}
