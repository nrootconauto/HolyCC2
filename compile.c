#include "IR2asm.h"
#include "asmEmitter.h"
#include "cleanup.h"
#include "diagMsg.h"
#include "parse2IR.h"
#include "parserA.h"
#include "parserB.h"
#include "preprocessor.h"
#include "cacheDir.h"
#include "filePath.h"
#include "hcrtLocation.h"
#include "sourceHash.h"
#include "escaper.h"
#include "filePath.h"
#include "compile.h"
__thread int HCC_Debug_Enable=1;
__thread  strLinkFns HCC_Link_To=NULL;
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
static void deleteFile(char **fn) {
		remove(*fn);
}
static void free2(char **str) {
		free(*str);
}
static strParserNode parseFile(const char *fn,strFileMappings *fMappings2,llLexerItem *items2,int silent) {
		setArch(ARCH_X86_SYSV);

		char *tmpName CLEANUP(deleteFile)=__builtin_alloca(TMP_MAX);
		tmpnam(tmpName);

		char *es=escapeString((char*)fn, strlen(fn));
		FILE *f=fopen(tmpName, "w");
		char *dbgInfoLoader CLEANUP(free2)=HCRTFile("LoadDbgInfo.HC");
		char * cwd=getcwd(NULL, 0);
		if(HCC_Debug_Enable)
				fprintf(f, "#include \"%s\"\n", dbgInfoLoader);
		
		if(!pathIsAbsolute(fn))
				fprintf(f, "#include \"%s/%s\"\n",cwd, es);
		else
				fprintf(f, "#include \"%s\"\n", es);
		free(es);
		free(cwd);
		fclose(f);
		
		{
				int err;
				strFileMappings fMappings  = NULL;
				strTextModify tMods = NULL;
				FILE *resultFile CLEANUP(fclose2) = createPreprocessedFile(tmpName, &tMods, &fMappings, &err);
				if (err)
						goto fail;

				fseek(resultFile, 0, SEEK_END);
				long end = ftell(resultFile);
				fseek(resultFile, 0, SEEK_SET);
				long start = ftell(resultFile);
				strChar fText CLEANUP(strCharDestroy) = strCharResize(NULL, end - start + 1);
				fread(fText, end - start, 1, resultFile);
				fText[end - start] = '\0';

				long atPos;
				llLexerItem items = lexText((struct __vec *)fText, &atPos,&err);
				
				diagInstCreate(DIAG_ANSI_TERM, fMappings, tMods, fn, stderr);
				if (err) {
						diagErrorStart(atPos, atPos+1);
						diagPushText("Lexical error.");
						diagEndMsg();
						goto fail;
				}
				
				strParserNode stmts  = NULL;
				while (items) {
						__auto_type s=parseStatement(items, &items);
						if(!s) continue;
						stmts = strParserNodeAppendItem(stmts, s);
				}
				if (diagErrorCount())
						goto fail;

				if(fMappings2)
						*fMappings2=strFileMappingsClone(fMappings);

				if(items2)
						*items2=items;
				return stmts;
		}
	fail:
		abort();
		return NULL;
}
void compileFile(const char *fn, const char *dumpTo) {
		HCC_Link_To=NULL;
		//Move symbols from previous compile to extern
		parserMoveGlobals2Extern();
		X86EmitAsmInit();
			strFileMappings fMappings = NULL;
			llLexerItem items CLEANUP(llLexerItemDestroy2);
			strParserNode stmts CLEANUP(strParserNodeDestroy2)=parseFile(fn,&fMappings,&items,0);
			sourceCacheInitAfterParse(fn);

			//
			// Look for cached functions
			//
			for(long n=0;n!=strParserNodeSize(stmts);n++) {
					if(stmts[n]->type!=NODE_FUNC_DEF) {
							continue;
					}
					struct parserNodeFuncDef *def=(void*)stmts[n];
					struct parserNodeName *nm=(void*)def->name;
					if(!nm)
							continue;
					long existsInCache=0;
					long hashedFileSize=0;
					char *fnBuffer CLEANUP(free2)=hashSource(def->func->__cacheStartToken, def->func->__cacheEndToken, nm->text, &existsInCache);
					if(existsInCache) {
							int existsInCache;
							X86EmitAsmAddCachedFuncIfExists(nm->text, &existsInCache);

							if(existsInCache) {
									//Remove current node  from statements
									parserNodeDestroy(&stmts[n]);
									long len=strParserNodeSize(stmts);
									memmove(&stmts[n], &stmts[n+1], sizeof(*stmts)*(len-n-1));
									stmts=strParserNodePop(stmts, NULL);
									n--;
							}
							continue;
					}
			}
			
		if(dumpTo) {
				IRGenInit(fMappings);
				initIR();
				struct enterExit ee = parserNodes2IR(stmts);
				
				IR2AsmInit();
				IRCompile(ee.enter, 0);
				
				X86EmitAsm2File(dumpTo,NULL);
				
		return;
	}
fail:
	abort();
}
