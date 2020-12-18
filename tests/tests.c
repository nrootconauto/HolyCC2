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
void LivenessTests();
void graphVizTests();
void registerAllocatorTests();
void IRFilterTests();
void init();
int main() {
		init();
		strTests();
		preprocessorTests();
		base64Tests();
		graphTests();
		graphDominanceTests();
		

	  llTests();
	  mapTests();
	  strTests();


	  subGraphTests();
		lexerTests();
	parserTests();
	parserDiagTests();
		topoSortTests();
		LivenessTests();
		//subExprElimTests();
		//graphVizTests();
		graphColoringTests();
		registerAllocatorTests();;
		SSATests();
		IRFilterTests();
		return 0;
}
