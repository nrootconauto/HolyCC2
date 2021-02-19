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

void compileFile(const char *fn, const char *dumpTo) {
	{
		setArch(ARCH_X86_SYSV);

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

		initParserData();
		strParserNode stmts CLEANUP(strParserNodeDestroy) = NULL;
		while (items)
			stmts = strParserNodeAppendItem(stmts, parseStatement(items, &items));
		if (diagErrorCount())
			goto fail;
		
		IRGenInit(fMappings);
		initIR();
		X86EmitAsmInit();
		struct enterExit ee = parserNodes2IR(stmts);

		IR2AsmInit();
		X86EmitAsmLabel("_start");
		IRCompile(ee.enter, 0);
		X86EmitAsm2File(dumpTo);
		return;
	}
fail:
	abort();
}
