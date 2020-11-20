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
void SSATests();
int main() {
	strTests();
	preprocessorTests();
	base64Tests();
	graphTests();
	graphDominanceTests();
	graphColoringTests();
	/**

	  llTests();
	  mapTests();
	  strTests();


	  subGraphTests();


	  eventPoolTests();
	  */
	lexerTests();
	parserTests();
	// parserDiagTests();
	topoSortTests();
	subExprElimTests();
	SSATests();
	return 0;
}
