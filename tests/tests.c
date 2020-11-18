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
void base64Tests();
void topoSortTests();
void subExprElimTests();
int main() {
		strTests();
		preprocessorTests();
		graphDominanceTests();
	graphColoringTests();
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
	//parserDiagTests();
	//base64Tests();
	//	topoSortTests();
	subExprElimTests();
	// typeParserTests();
	return 0;
}
