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
void ptrMapTests();
void parse2IRTests();
void IRTypeInferenceTests();
void constantPropigationTests();
void frameLayoutTests();
void X86OpcodesTests();
void basicBlockTests();
void IEEE754Tests();
void asmEmitterTests();
int main() {
		init();
		SSATests();
		registerAllocatorTests();
		asmEmitterTests();
		/*
				X86OpcodesTests();
				parserTests();
				basicBlockTests();
				IEEE754Tests();
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
				parserDiagTests();
				topoSortTests();
				LivenessTests();
				//subExprElimTests();
				//graphVizTests();
				graphColoringTests();
				ptrMapTests();
				SSATests();
				//IRFilterTests();
				IRTypeInferenceTests();
				constantPropigationTests();
				frameLayoutTests();
				parse2IRTests();
		*/
	return 0;
}
