void llTests();
void strTests();
void graphTests();
void mapTests();
void subGraphTests();
void preprocessorTests();
void eventPoolTests();
void precParserTests();
void graphDominanceTests();
void graphColoringTests();
void parserTests();
void lexerTests();
void parserDiagTests();
int main() {
	strTests();
	preprocessorTests();
	// graphDominanceTests();
	// graphColoringTests();
	/**

	  llTests();
	  mapTests();
	  strTests();
	  graphTests();
	  
	  subGraphTests();

	  
	  eventPoolTests();
	  */
	lexerTests();
	parserTests();
	parserDiagTests();
	// typeParserTests();
	return 0;
}
