#include <IR2asm.h>
#include <asmEmitter.h>
#include <cleanup.h>
#include <diagMsg.h>
#include <parse2IR.h>
#include <parserA.h>
#include <parserB.h>
#include <preprocessor.h>
static void fclose2(FILE **f) {
	fclose(*f);
}
STR_TYPE_DEF(char, Char);
STR_TYPE_FUNCS(char, Char);
static void strParserNodeDestroy2(strParserNode *str) {
	for (long s = 0; s != strParserNodeSize(*str); s++)
		parserNodeDestroy(&str[0][s]);
	strParserNodeDestroy(str);
}

static strParserNode parseFile(const char *fn,strFileMappings *fMappings2) {
		setArch(ARCH_X86_SYSV);
		{
				int err;
				strFileMappings fMappings CLEANUP(strFileMappingsDestroy) = NULL;
				strTextModify tMods CLEANUP(strTextModifyDestroy) = NULL;
				FILE *resultFile CLEANUP(fclose2) = createPreprocessedFile(fn, &tMods, &fMappings, &err);
				if (err)
						goto fail;

				fseek(resultFile, 0, SEEK_END);
				long end = ftell(resultFile);
				fseek(resultFile, 0, SEEK_SET);
				long start = ftell(resultFile);
				strChar fText CLEANUP(strCharDestroy) = strCharResize(NULL, end - start + 1);
				fread(fText, end - start, 1, resultFile);
				fText[end - start] = '\0';

				llLexerItem items CLEANUP(llLexerItemDestroy2) = lexText((struct __vec *)fText, &err);
				if (err)
						goto fail;

				diagInstCreate(DIAG_ANSI_TERM, fMappings, tMods, fn, stderr);

				strParserNode stmts  = NULL;
				while (items)
						stmts = strParserNodeAppendItem(stmts, parseStatement(items, &items));
				if (diagErrorCount())
						goto fail;

				if(fMappings2)
						*fMappings2=strFileMappingsClone(fMappings);

				return stmts;
		}
	fail:
		abort();
		return NULL;
}
void compileFile(const char *fn, const char *dumpTo) {
			initParserData();
			strFileMappings fMappings0 CLEANUP(strFileMappingsDestroy) = NULL;
			strParserNode dummy CLEANUP(strParserNodeDestroy2)=parseFile("/home/tc/projects/holycc2/HolyCRT/HCRT.HC",&fMappings0);
			
			strFileMappings fMappings CLEANUP(strFileMappingsDestroy) = NULL;
			strParserNode stmts CLEANUP(strParserNodeDestroy2)=parseFile(fn,&fMappings);
		if(dumpTo) {
				IRGenInit(fMappings);
				initIR();
				struct enterExit ee = parserNodes2IR(stmts);
				
				IR2AsmInit();
				X86EmitAsmInit();
				X86EmitAsmLabel("_start");
				IRCompile(ee.enter, 0);
				X86EmitAsm2File(dumpTo);
		return;
	}
fail:
	abort();
}
