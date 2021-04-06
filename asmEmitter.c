#include "X86AsmSharedVars.h"
#include "asmEmitter.h"
#include <assert.h>
#include "cleanup.h"
#include "diagMsg.h"
#include <limits.h>
#include <ctype.h>
#include "frameLayout.h"
#include "opcodesParser.h"
#include "parserA.h"
#include "parserB.h"
#include "ptrMap.h"
#include "registers.h"
#include "escaper.h"
#include <stdio.h>
#include "cacheDir.h"
#include <unistd.h>
#include "dumpDebugInfo.h"
#include "sourceHash.h"
#include "compile.h"
STR_TYPE_DEF(char, Char);
STR_TYPE_FUNCS(char, Char);
PTR_MAP_FUNCS(struct parserNode *, strChar, LabelNames);
PTR_MAP_FUNCS(struct reg *, strChar, RegName);
MAP_TYPE_DEF(char *, SymAsmName);
MAP_TYPE_FUNCS(char *, SymAsmName);
STR_TYPE_DEF(strChar, StartCodeName);
STR_TYPE_FUNCS(strChar, StartCodeName);
STR_TYPE_DEF(strChar, StrChar);
STR_TYPE_FUNCS(strChar, StrChar);
MAP_TYPE_DEF(struct X86AddressingMode *,AddrMode);
MAP_TYPE_FUNCS(struct X86AddressingMode *,AddrMode);
MAP_TYPE_DEF(strChar, FnLabel);
MAP_TYPE_FUNCS(strChar, FnLabel);
STR_TYPE_DEF(long, Long);
STR_TYPE_FUNCS(long, Long);
static strChar strClone(const char *text) {
	__auto_type retVal = strCharAppendData(NULL, text, strlen(text) + 1);
	strcpy(retVal, text);
	return retVal;
}
static void strStrCharDestroy2(strStrChar *str) {
		for(long s=0;s!=strStrCharSize(*str);s++)
				strCharDestroy(&str[0][s]);
		strStrCharDestroy(str);
}
#define FUNC_BREAKPOINTS_LAB_FMT_LN "DBG_BP@%s_$ln%li"
#define FUNC_BREAKPOINTS_LAB_FMT_FN "DBG_BP@%s_$fn%li"
#define FUNC_BREAKPOINTS_LAB_INFO "DBG_INFO@%s"
#define FUNC_CODE_END_LAB_FMT "%s@$end"
#define FILE_BREAKPOINTS_MAC_FMT_BPOFF "DBG_BP@%s_$bp%li"

#define OBJECT_OFFSET_FMT "%s$%s_Offset"
#define OBJECT_SIZE_FMT "%s$_Size" 
#define DBG_TOKEN_FILE_SUFFIX ".tokInfo"
static __thread strStartCodeName initCodeNames=NULL;
static __thread long breakPointCount=0;
static __thread mapFnLabel filenameStrLabels=NULL;
struct breakPoint {
		long line;
		long bpOffset;
};
MAP_TYPE_DEF(struct breakPoint,BreakLines);
MAP_TYPE_FUNCS(struct breakPoint,BreakLines);
PTR_MAP_DEF(Token2Line);
struct tokenInfo  {const char *fn;long ln;long tokIndex;};
PTR_MAP_FUNCS(llLexerItem,struct tokenInfo,Token2Line);
static __thread mapBreakLines breakpoints;
static __thread struct asmFileSet {
		FILE *constsTmpFile;
		FILE *symbolsTmpFile;
		FILE *initSymbolsTmpFile;
		FILE *codeTmpFile;
		long labelCount;
		mapAddrMode strings;
		struct parserFunction *func;
		struct asmFileSet *parent;
		ptrMapToken2Line token2Line;
} *currentFileSet=NULL;
long initFileNum;
static char *fromFmt(const char *fmt,...) {
		va_list list,list2;
		va_start(list, fmt);
		va_copy(list2, list);
		long len=vsnprintf(NULL, 0, fmt, list);
		char buffer[len+1];
		vsprintf(buffer,fmt, list2);
		char *retVal=strcpy(calloc( len+1, 1),buffer);
		va_end(list);
		va_end(list2);

		return retVal;
}
static strChar getCurrFuncName() {
		if(currentFileSet->func)
				return strClone(currentFileSet->func->name);
		return strClone(fromFmt("__init%li",initFileNum));
}
MAP_TYPE_DEF(strChar,FuncFiles);
MAP_TYPE_FUNCS(strChar,FuncFiles);
static __thread mapFuncFiles funcFiles=NULL;
static ptrMapRegName regNames;
static void strCharDestroy2(strChar *str) {
	strCharDestroy(str);
}
static void free2(char **str) {
		free(*str);
}
__attribute__((destructor)) static void deinit() {
	ptrMapRegNameDestroy(regNames, (void (*)(void *))strCharDestroy2);
}
__attribute__((constructor)) static void init() {
	regNames = ptrMapRegNameCreate();
	ptrMapRegNameAdd(regNames, &regX86ST0, strClone("ST0"));
	ptrMapRegNameAdd(regNames, &regX86ST1, strClone("ST1"));
	ptrMapRegNameAdd(regNames, &regX86ST2, strClone("ST2"));
	ptrMapRegNameAdd(regNames, &regX86ST3, strClone("ST3"));
	ptrMapRegNameAdd(regNames, &regX86ST4, strClone("ST4"));
	ptrMapRegNameAdd(regNames, &regX86ST5, strClone("ST5"));
	ptrMapRegNameAdd(regNames, &regX86ST6, strClone("ST6"));
	ptrMapRegNameAdd(regNames, &regX86ST7, strClone("ST7"));
	
	ptrMapRegNameAdd(regNames, &regX86AL, strClone("AL"));
	ptrMapRegNameAdd(regNames, &regX86AH, strClone("AH"));
	ptrMapRegNameAdd(regNames, &regX86BL, strClone("BL"));
	ptrMapRegNameAdd(regNames, &regX86BH, strClone("BH"));
	ptrMapRegNameAdd(regNames, &regX86CL, strClone("CL"));
	ptrMapRegNameAdd(regNames, &regX86CH, strClone("CH"));
	ptrMapRegNameAdd(regNames, &regX86DL, strClone("DL"));
	ptrMapRegNameAdd(regNames, &regX86DH, strClone("DH"));

	ptrMapRegNameAdd(regNames, &regX86DIL, strClone("DIL"));
	ptrMapRegNameAdd(regNames, &regX86SIL, strClone("SIL"));
	ptrMapRegNameAdd(regNames, &regX86BPL, strClone("BPL"));
	ptrMapRegNameAdd(regNames, &regX86SPL, strClone("SPL"));

	ptrMapRegNameAdd(regNames, &regAMD64R8u8, strClone("R8L"));
	ptrMapRegNameAdd(regNames, &regAMD64R9u8, strClone("R9L"));
	ptrMapRegNameAdd(regNames, &regAMD64R10u8, strClone("R10L"));
	ptrMapRegNameAdd(regNames, &regAMD64R11u8, strClone("R11L"));
	ptrMapRegNameAdd(regNames, &regAMD64R12u8, strClone("R12L"));
	ptrMapRegNameAdd(regNames, &regAMD64R13u8, strClone("R13L"));
	ptrMapRegNameAdd(regNames, &regAMD64R14u8, strClone("R14L"));
	ptrMapRegNameAdd(regNames, &regAMD64R15u8, strClone("R15L"));

	ptrMapRegNameAdd(regNames, &regX86AX, strClone("AX"));
	ptrMapRegNameAdd(regNames, &regX86BX, strClone("BX"));
	ptrMapRegNameAdd(regNames, &regX86CX, strClone("CX"));
	ptrMapRegNameAdd(regNames, &regX86DX, strClone("DX"));
	ptrMapRegNameAdd(regNames, &regX86SI, strClone("SI"));
	ptrMapRegNameAdd(regNames, &regX86DI, strClone("DI"));
	ptrMapRegNameAdd(regNames, &regX86SP, strClone("SP"));
	ptrMapRegNameAdd(regNames, &regX86BP, strClone("BP"));
	ptrMapRegNameAdd(regNames, &regAMD64R8u16, strClone("R8W"));
	ptrMapRegNameAdd(regNames, &regAMD64R9u16, strClone("R9W"));
	ptrMapRegNameAdd(regNames, &regAMD64R10u16, strClone("R10W"));
	ptrMapRegNameAdd(regNames, &regAMD64R11u16, strClone("R11W"));
	ptrMapRegNameAdd(regNames, &regAMD64R12u16, strClone("R12W"));
	ptrMapRegNameAdd(regNames, &regAMD64R13u16, strClone("R13W"));
	ptrMapRegNameAdd(regNames, &regAMD64R14u16, strClone("R14W"));
	ptrMapRegNameAdd(regNames, &regAMD64R15u16, strClone("R15W"));

	ptrMapRegNameAdd(regNames, &regX86EAX, strClone("EAX"));
	ptrMapRegNameAdd(regNames, &regX86EBX, strClone("EBX"));
	ptrMapRegNameAdd(regNames, &regX86ECX, strClone("ECX"));
	ptrMapRegNameAdd(regNames, &regX86EDX, strClone("EDX"));
	ptrMapRegNameAdd(regNames, &regX86ESI, strClone("ESI"));
	ptrMapRegNameAdd(regNames, &regX86EDI, strClone("EDI"));
	ptrMapRegNameAdd(regNames, &regX86ESP, strClone("ESP"));
	ptrMapRegNameAdd(regNames, &regX86EBP, strClone("EBP"));
	ptrMapRegNameAdd(regNames, &regAMD64R8u32, strClone("R8D"));
	ptrMapRegNameAdd(regNames, &regAMD64R9u32, strClone("R9D"));
	ptrMapRegNameAdd(regNames, &regAMD64R10u32, strClone("R10D"));
	ptrMapRegNameAdd(regNames, &regAMD64R11u32, strClone("R11D"));
	ptrMapRegNameAdd(regNames, &regAMD64R12u32, strClone("R12D"));
	ptrMapRegNameAdd(regNames, &regAMD64R13u32, strClone("R13D"));
	ptrMapRegNameAdd(regNames, &regAMD64R14u32, strClone("R14D"));
	ptrMapRegNameAdd(regNames, &regAMD64R15u32, strClone("R15D"));

	ptrMapRegNameAdd(regNames, &regAMD64RAX, strClone("RAX"));
	ptrMapRegNameAdd(regNames, &regAMD64RBX, strClone("RBX"));
	ptrMapRegNameAdd(regNames, &regAMD64RCX, strClone("RCX"));
	ptrMapRegNameAdd(regNames, &regAMD64RDX, strClone("RDX"));
	ptrMapRegNameAdd(regNames, &regAMD64RSI, strClone("RSI"));
	ptrMapRegNameAdd(regNames, &regAMD64RDI, strClone("RDI"));
	ptrMapRegNameAdd(regNames, &regAMD64RSP, strClone("RSP"));
	ptrMapRegNameAdd(regNames, &regAMD64RBP, strClone("RBP"));
	ptrMapRegNameAdd(regNames, &regAMD64R8u64, strClone("R8"));
	ptrMapRegNameAdd(regNames, &regAMD64R9u64, strClone("R9"));
	ptrMapRegNameAdd(regNames, &regAMD64R10u64, strClone("R10"));
	ptrMapRegNameAdd(regNames, &regAMD64R11u64, strClone("R11"));
	ptrMapRegNameAdd(regNames, &regAMD64R12u64, strClone("R12"));
	ptrMapRegNameAdd(regNames, &regAMD64R13u64, strClone("R13"));
	ptrMapRegNameAdd(regNames, &regAMD64R14u64, strClone("R14"));
	ptrMapRegNameAdd(regNames, &regAMD64R15u64, strClone("R15"));

	ptrMapRegNameAdd(regNames, &regX86MM0, strClone("MM0"));
	ptrMapRegNameAdd(regNames, &regX86MM1, strClone("MM1"));
	ptrMapRegNameAdd(regNames, &regX86MM2, strClone("MM2"));
	ptrMapRegNameAdd(regNames, &regX86MM3, strClone("MM3"));
	ptrMapRegNameAdd(regNames, &regX86MM4, strClone("MM4"));
	ptrMapRegNameAdd(regNames, &regX86MM5, strClone("MM5"));
	ptrMapRegNameAdd(regNames, &regX86MM6, strClone("MM6"));
	ptrMapRegNameAdd(regNames, &regX86MM7, strClone("MM7"));

	ptrMapRegNameAdd(regNames, &regX86XMM0, strClone("XMM0"));
	ptrMapRegNameAdd(regNames, &regX86XMM1, strClone("XMM1"));
	ptrMapRegNameAdd(regNames, &regX86XMM2, strClone("XMM2"));
	ptrMapRegNameAdd(regNames, &regX86XMM3, strClone("XMM3"));
	ptrMapRegNameAdd(regNames, &regX86XMM4, strClone("XMM4"));
	ptrMapRegNameAdd(regNames, &regX86XMM5, strClone("XMM5"));
	ptrMapRegNameAdd(regNames, &regX86XMM6, strClone("XMM6"));
	ptrMapRegNameAdd(regNames, &regX86XMM7, strClone("XMM7"));

	ptrMapRegNameAdd(regNames, &regX86ES, strClone("ES"));
	ptrMapRegNameAdd(regNames, &regX86CS, strClone("CS"));
	ptrMapRegNameAdd(regNames, &regX86SS, strClone("SS"));
	ptrMapRegNameAdd(regNames, &regX86DS, strClone("DS"));
	ptrMapRegNameAdd(regNames, &regX86FS, strClone("FS"));
	ptrMapRegNameAdd(regNames, &regX86GS, strClone("GS"));
}
static strChar __getFilenameLabel(char *name) {
	loop:;
		__auto_type find=mapFnLabelGet(filenameStrLabels, name);
		if(!find) {
				long size;
				mapFnLabelKeys(filenameStrLabels, NULL, &size);
				char *fmted CLEANUP(free2)=fromFmt("fn$%li", size);
				mapFnLabelInsert(filenameStrLabels, name, strClone(fmted));
				goto loop;
		}
		return strClone(*find);
}
void X86EmitAsmInit() {
		breakPointCount=0;
		breakpoints=mapBreakLinesCreate();
		funcFiles=mapFuncFilesCreate();
		strStartCodeNameDestroy(&initCodeNames);
		initCodeNames=NULL;
		filenameStrLabels=mapFnLabelCreate();
}
static strChar int64ToStr(int64_t value) {
	strChar retVal = NULL;
	const char *digits = "0123456789";
	int wasNegative = 0;
	if (value < 0) {
		value *= -1;
		wasNegative = 1;
	}
	do {
		retVal = strCharAppendItem(retVal, digits[value % 10]);
		value /= 10;
	} while (value != 0);
	if (wasNegative)
		retVal = strCharConcat(retVal, strCharAppendItem(NULL, '-'));
	else
			retVal = strCharConcat(retVal, strCharAppendItem(NULL, '+'));
	return strCharAppendItem(strCharReverse(retVal), '\0');
}
static strChar uint64ToStr(uint64_t value) {
	strChar retVal = NULL;
	const char *digits = "0123456789";
	do {
		retVal = strCharAppendItem(retVal, digits[value % 10]);
		value /= 10;
	} while (value != 0);
	retVal = strCharConcat(retVal, strCharAppendItem(NULL, '+'));
	return strCharAppendItem(strCharReverse(retVal), '\0');
}
static strChar getSizeStr(struct object *obj) {
	if (!obj)
		return strClone("");
	__auto_type base = objectBaseType(obj);
	switch (objectSize(base, NULL)) {
	case 1:
		return strClone("BYTE");
	case 2:
		return strClone("WORD");
	case 4:
		return strClone("DWORD");
	case 8:
		return strClone("QWORD");
	default:
		return NULL;
	}
}
static char *offsetName(struct objectMember *mem) {
		char *name CLEANUP(free2)=object2Str(mem->belongsTo);
		__auto_type retVal=fromFmt(OBJECT_OFFSET_FMT,name,mem->name);
		return retVal;
}
static char *sizeofName(struct object *type) {
		if(type->type==TYPE_PTR) {
				return fromFmt("%+li",ptrSize());
		} else if(type->type==TYPE_ARRAY) {
				if(!objectArrayIsConstSz(type))
						return fromFmt("%+li",ptrSize());

				long dimCount;
				objectArrayDimValues(type, &dimCount, NULL);

				struct object *baseType=type;
				for(;baseType->type==TYPE_ARRAY;baseType=((struct objectArray*)baseType)->type);
				
				char *name CLEANUP(free2)=sizeofName(baseType);
				long dimVals[dimCount];
				objectArrayDimValues(type, NULL,dimVals);

				strChar times CLEANUP(strCharDestroy)=strClone(name);
				for(long d=0;d!=dimCount;d++) {
						char *mult=fromFmt("*%+li", dimVals[d]);
						times=strCharAppendData(times, mult, strlen(mult));
						free(mult);
				}
				times=strCharAppendItem(times, '\0');

				return strcpy(calloc(strCharSize(times), 1), times);
		}
		char *name CLEANUP(free2)=object2Str(type);
		return  fromFmt(OBJECT_SIZE_FMT,name);
}
static void __emitFuncTokenLines(FILE *dumpTo,struct parserFunction *func,long *tokenIndexes,long len) {
		const char *funcNam=func->name;
		long offset=0;
		long count=0;
		for(__auto_type item=func->__cacheStartToken;item!=func->__cacheEndToken;item=llLexerItemNext(item),offset++) {
				if(count==len) break;
				long i;
						for( i=0;i!=len;i++) {
								if(offset<tokenIndexes[i]) continue;
								else if(offset==tokenIndexes[i]) goto found;
						}
						goto next;
		found:;
						long line;
						const char *fn;
						diagLineCol(&fn, llLexerItemValuePtr(item)->start, &line, NULL);
						
						char *macro1 =fromFmt("%%define " FUNC_BREAKPOINTS_LAB_FMT_LN " %li\n", funcNam, tokenIndexes[i], line);
						fprintf(dumpTo, "%s", macro1);free(macro1);
						strChar fnLab  =__getFilenameLabel((char*)fn);
						char *macro2=fromFmt("%%define " FUNC_BREAKPOINTS_LAB_FMT_FN " %s\n", funcNam, tokenIndexes[i], fnLab);
						fprintf(dumpTo, "%s", macro2);free(macro2);strCharDestroy(&fnLab);

						{
								char *fmted CLEANUP(free2)=fromFmt("%s:%li", fn,line);
						bpLoop:;
								__auto_type find=mapBreakLinesGet(breakpoints,  fmted);
								if(!find) {
										struct breakPoint bp={line,breakPointCount++};
										mapBreakLinesInsert(breakpoints, fmted, bp);
										goto bpLoop;
								}
								char *fmted2 CLEANUP(free2)=fromFmt(FILE_BREAKPOINTS_MAC_FMT_BPOFF, funcNam,tokenIndexes[i]);
								fprintf(dumpTo, "%%define %s %li\n", fmted2,find->bpOffset);
						}
						++count;
				next:;
				}
}
static char *__getUpdatedTokenLinesFromCache(struct parserFunction *func) {
		char tmp[TMP_MAX];
		tmpnam(tmp);
		
		FILE *dumpTo=fopen(tmp, "w");

		char *fn CLEANUP(free2)=hashSource(func->__cacheStartToken,func->__cacheEndToken,func->name,NULL);
		const char *suffix=DBG_TOKEN_FILE_SUFFIX;
		fn=realloc(fn, strlen(fn)+strlen(suffix)+1);
		fn=strcat(fn, suffix);
		
		FILE *file =fopen(fn, "r");
		strLong tokenOffs CLEANUP(strLongDestroy)=NULL;
		if(!file) {
				fprintf(stderr, "File \"%s%s\" not found,dumping all token lines,clear your cache and recompile to generate the missing file.\n", fn,suffix);
				long count=0;
				for(__auto_type item=func->__cacheStartToken;item!=func->__cacheEndToken;item=llLexerItemNext(item)) count++;
				tokenOffs=strLongResize(tokenOffs, count);
				for(long l=0;l!=count;l++)  tokenOffs[l]=l;
				__emitFuncTokenLines(dumpTo, func, tokenOffs ,  count);
				goto ret;
		}
		
		char *line=__builtin_alloca(256);
		size_t count=256;
		while(-1!=getline(&line, &count, file)) {
				count=256;
				
				char *find=strrchr(line, '$');
				if(!find) continue;
				if(0!=strncmp(find+1, "ln", 2)) continue;
				
				long tokOff;
				sscanf(find+1+2, "%li", &tokOff);
				
				tokenOffs=strLongAppendItem(tokenOffs, tokOff);
		}
 		__emitFuncTokenLines(dumpTo, func, tokenOffs ,  strLongSize(tokenOffs));
		fclose(file);

	ret: {
				fclose(dumpTo);
				rename(tmp, fn);
		}
		return strcpy(calloc(strlen(fn)+1,1), fn);
}
static int longCmp(const void *a,const void *b) {
		long A=*(long*)a,B=*(long*)b;
		if(A>B) return 1;
		else if(A<B) return -1;
		return 0;
}
static long tokOffset(llLexerItem start,llLexerItem item) {
		long offset=0;
		for(;start!=item;start=llLexerItemNext(start)) offset++;
		return offset;
}
STR_TYPE_DEF(struct tokenInfo,TokenInfo);
STR_TYPE_FUNCS(struct tokenInfo,TokenInfo);
static int tokenInfoCmp(struct tokenInfo *a,struct tokenInfo *b) {
		return longCmp(&a->tokIndex, &b->tokIndex);
}
/*
	* Emits member offsets/size macros
	*/
static void X86EmitClassMetaData(FILE *dumpTo) {
		long count;
		parserSymTableNames(NULL, &count);
		const char *keys[count];
		parserSymTableNames(keys, NULL);
		for(long k=0;k!=count;k++) {
				__auto_type sym=parserGetGlobalSym(keys[k]);
				//Look for classes/unions,(not varaibles with  class/union types)
				if(sym->var!=NULL)
						continue;
				if(sym->type->type==TYPE_CLASS) {
						__auto_type cls=(struct objectClass*)sym->type;
						char *typeName=object2Str(sym->type);
						for(long m=0;m!=strObjectMemberSize(cls->members);m++) {
								char *ofNam CLEANUP(free2)=offsetName(&cls->members[m]);
								fprintf(dumpTo, "%%define %s %li\n", ofNam,cls->members[m].offset);
						}

						char *szof CLEANUP(free2)=sizeofName(sym->type);
						fprintf(dumpTo,"%%define %s %li\n",szof,objectSize(sym->type, NULL));
				} else if(sym->type->type==TYPE_UNION) {
						__auto_type un=(struct objectUnion*)sym->type;
						char *typeName CLEANUP(free2)=object2Str(sym->type);
						for(long m=0;m!=strObjectMemberSize(un->members);m++) {
								char *ofNam CLEANUP(free2)=offsetName(&un->members[m]);
								fprintf(dumpTo, "%%define %s %li\n", ofNam,un->members[m].offset);
						}
						
						char *szof=sizeofName(sym->type);
						fprintf(dumpTo,"%%define %s %li\n",szof,objectSize(sym->type, NULL));
				} else
						continue;
		}
}
static void X86EmitSymbolTable(FILE *dumpTo) {
	long count;
	parserSymTableNames(NULL, &count);
	const char *keys[count];
	parserSymTableNames(keys, NULL);
	for (long i = 0; i != count; i++) {
		strChar name CLEANUP(strCharDestroy2) = strClone(parserGetGlobalSymLinkageName(keys[i]));
		if(!name)
				continue;
		__auto_type link = parserGlobalSymLinkage(keys[i]); 
		if ((link->type & LINKAGE__EXTERN) || (link->type & LINKAGE__IMPORT)) {
			fprintf(dumpTo, "EXTERN %s ; Appears as %s in src.\n", name, keys[i]);
		} else if ((link->type & LINKAGE_STATIC)||(link->type & LINKAGE_INTERNAL)) {
			// Do nothing
		} else if ((link->type & LINKAGE_EXTERN) || (link->type & LINKAGE_IMPORT)) {
			fprintf(dumpTo, "EXTERN %s\n", name);
		} else if (!(link->type & LINKAGE_LOCAL)) {
			fprintf(dumpTo, "GLOBAL %s\n", name);
		}
	}
}
static strChar emitMode(struct X86AddressingMode **args, long i) {
	switch (args[i]->type) {
	case X86ADDRMODE_MACRO: {
			return strClone(args[i]->value.macroName);
	}
	case X86ADDRMODE_SIZEOF: {
			switch(args[i]->value.objSizeof->type) {
			case TYPE_U0:
			case TYPE_U8i:
			case TYPE_U16i:
			case TYPE_U32i:
			case TYPE_U64i:
			case TYPE_I8i:
			case TYPE_I16i:
			case TYPE_I32i:
			case TYPE_I64i:
			case TYPE_Bool: {
					char *str CLEANUP(free2)= fromFmt("%li", objectSize(args[i]->value.objSizeof, NULL));
					return strClone(str);
			}
			default:;
			}
			char *szofStr CLEANUP(free2)=sizeofName(args[i]->value.objSizeof);
			return  strClone(szofStr);
			break;
	}
	case X86ADDRMODE_STR: {
			struct X86AddressingMode *stred CLEANUP(X86AddrModeDestroy) = X86EmitAsmStrLit((char*)args[i]->value.text,__vecSize(args[i]->value.text));
		return emitMode(&stred, 0);
		break;
	}
	case X86ADDRMODE_FLT: {
		assert(0);
		break;
	}
	case X86ADDRMODE_VAR_VALUE: {
			__auto_type find = ptrMapFrameOffsetGet(localVarFrameOffsets, args[i]->value.varAddr.var);
			if (find) {
					assert(!args[i]->value.varAddr.var->isGlobal);
					struct X86AddressingMode *offset CLEANUP(X86AddrModeDestroy) =
							X86AddrModeIndirSIB(0, NULL, X86AddrModeReg(basePointer(),objectPtrCreate(&typeU0)), X86AddrModeSint(-*find), args[i]->value.varAddr.var->type);

				__auto_type members=args[i]->value.varAddr.memberOffsets;
				for(long m=0;m!=strObjectMemberPSize(members);m++)
						X86AddrModeIndirSIBAddMemberOffset(offset, members[m]);
				
				return emitMode(&offset, 0);
			} else {
					strChar name CLEANUP(strCharDestroy) = strClone(parserGetGlobalSymLinkageName(args[i]->value.varAddr.var->name));

					strChar offsetStr CLEANUP(strCharDestroy)=NULL;
					__auto_type members=args[i]->value.varAddr.memberOffsets;
					for(long m=0;m!=strObjectMemberPSize(members);m++) {
							char *memNam CLEANUP(free2)=offsetName(members[m]);
							char *buffer CLEANUP(free2)=fromFmt("+%s",memNam);
							offsetStr=strCharAppendData(offsetStr, buffer,strlen(buffer));
					}
					offsetStr=strCharAppendItem(offsetStr, '\0');
					
					if(args[i]->value.itemAddr.offset) {
							long offset=args[i]->value.itemAddr.offset;
							char *buffer CLEANUP(free2)=fromFmt("[$%s%s%+li] ", name,offsetStr,offset);
							return strClone(buffer);
					} else {
							char *buffer CLEANUP(free2) = fromFmt( "[$%s%s] ", name,offsetStr);
							return strClone(buffer);
					}
			}
			break;
	}
	case X86ADDRMODE_ITEM_ADDR: {
			strChar name CLEANUP(strCharDestroy) = strClone(parserGetGlobalSymLinkageName(args[i]->value.itemAddr.item));
		if (!name) {
			fprintf(stderr, "Cant find name for symbol\n");
			assert(0);
		}
		strChar offsetStr CLEANUP(strCharDestroy)=NULL;
		if(args[i]->value.itemAddr.offset) {
				long offset=args[i]->value.itemAddr.offset;
				char *buffer CLEANUP(free2)=fromFmt("%+li", offset);
				offsetStr=strClone(buffer);
		} else
				offsetStr=strClone("");
		
		char *buffer CLEANUP(free2)=fromFmt( "$%s%s ", name,offsetStr);
		return strClone(buffer); 
	}
	case X86ADDRMODE_LABEL: {
			char *buffer CLEANUP(free2)=fromFmt("$%s ", args[i]->value.label);
		return strClone(buffer);
	}
	case X86ADDRMODE_REG: {
		__auto_type find = ptrMapRegNameGet(regNames, args[i]->value.reg);
		assert(find);
		char *buffer CLEANUP(free2)=fromFmt( "%s ", *find);
		return strClone(buffer);
	}
	case X86ADDRMODE_SINT: {
		return int64ToStr(args[i]->value.sint);
	}
	case X86ADDRMODE_UINT: {
		return uint64ToStr(args[i]->value.uint);
	}
	case X86ADDRMODE_MEM: {
		switch (args[i]->value.m.type) {
		case x86ADDR_INDIR_LABEL: {
			strChar sizeStr CLEANUP(strCharDestroy) = getSizeStr(args[i]->valueType);
			if (!sizeStr) {
				fprintf(stderr, "Invalid size");
				assert(0);
			}
			strChar labelStr CLEANUP(strCharDestroy) = emitMode(&args[i]->value.m.value.label, 0);
			char *buffer CLEANUP(free2)= fromFmt( " %s [%s] ", sizeStr, labelStr);
			return strClone(buffer);
		}
		case x86ADDR_INDIR_REG: {
			__auto_type reg = ptrMapRegNameGet(regNames, args[i]->value.m.value.indirReg);
			if (args[i]->valueType) {
				strChar sizeStr CLEANUP(strCharDestroy) = getSizeStr(args[i]->valueType);
				if (!sizeStr) {
					fprintf(stderr, "That's one gaint register(%s)\n", *reg);
					assert(0);
				}
				char *buffer CLEANUP(free2)=fromFmt(" %s [%s] ", sizeStr, *reg);
				return strClone(buffer);
			} else {
					char * buffer CLEANUP(free2) = fromFmt("[%s] ", *reg);
				return strClone(buffer);
			}
			break;
		}
		case x86ADDR_INDIR_SIB: {
			strChar retVal CLEANUP(strCharDestroy) = NULL;

			strChar indexStr CLEANUP(strCharDestroy) = NULL;
			if (args[i]->value.m.value.sib.index)
				indexStr = emitMode(&args[i]->value.m.value.sib.index, 0);

			strChar scaleStr CLEANUP(strCharDestroy) = int64ToStr(args[i]->value.m.value.sib.scale);

			strChar baseStr CLEANUP(strCharDestroy) = NULL;
			if (args[i]->value.m.value.sib.base)
				baseStr = emitMode(&args[i]->value.m.value.sib.base, 0);

			// Emit mode takes an array,so pass the pointer
			strChar offsetStr CLEANUP(strCharDestroy) = NULL;
			if (args[i]->value.m.value.sib.offset)
				offsetStr = emitMode(&args[i]->value.m.value.sib.offset, 0);

			//Account for numerical offset2
			if(args[i]->value.m.value.sib.offset2) {
					char *buffer CLEANUP(free2)=fromFmt("%+li",args[i]->value.m.value.sib.offset2);
					offsetStr=strCharResize(offsetStr,strCharSize(offsetStr)+strlen(buffer)+1);
					strcat(offsetStr, buffer);
			}

			__auto_type members=args[i]->value.m.value.sib.memberOffsets;
			for(long m=0;m!=strObjectMemberPSize(members);m++) {
					char *memNam=offsetName(members[m]);
					char *buffer CLEANUP(free2)=fromFmt( "+%s",memNam);
					offsetStr=strCharResize(offsetStr,strCharSize(offsetStr)+strlen(buffer)+1);
					strcat(offsetStr, buffer);
			}
			
			strChar buffer CLEANUP(strCharDestroy) = NULL;
			int insertAdd = 0;
			if (indexStr && args[i]->value.m.value.sib.scale) {
				retVal = strCharAppendData(retVal, indexStr, strlen(indexStr));
				retVal = strCharAppendItem(retVal, '*');
				retVal = strCharAppendData(retVal, scaleStr, strlen(scaleStr));
				insertAdd = 1;
			} else if (indexStr) {
				retVal = strCharAppendData(retVal, indexStr, strlen(indexStr));
				insertAdd = 1;
			}
			if (insertAdd && baseStr)
				retVal = strCharAppendItem(retVal, '+');
			if (baseStr) {
				retVal = strCharAppendData(retVal, baseStr, strlen(baseStr));
			}
			retVal = strCharConcat(retVal, offsetStr);
			retVal = strCharAppendItem(retVal, '\0');
			if (args[i]->valueType) {
				strChar typeStr CLEANUP(strCharDestroy) = getSizeStr(args[i]->valueType);
				char *buffer CLEANUP(free2)= fromFmt( "%s [%s] ", typeStr, retVal);
				return strClone(buffer);
			} else {
					char *buffer CLEANUP(free2)=fromFmt( "[%s] ", retVal);
					return strClone(buffer);
			}
			break;
		}
		case x86ADDR_MEM: {
			if (args[i]->valueType) {
				strChar addrStr CLEANUP(strCharDestroy) = uint64ToStr(args[i]->value.m.value.mem);
				strChar sizeStr CLEANUP(strCharDestroy) = getSizeStr(args[i]->valueType);
				if (!sizeStr) {
					fprintf(stderr, "The size being addressed is weirder than a black rain frog.\n");
					assert(0);
				}
				char *buffer CLEANUP(free2)=fromFmt("%s [%s] ", sizeStr, addrStr);
				return strClone(buffer);
			} else {
				strChar addrStr CLEANUP(strCharDestroy) = uint64ToStr(args[i]->value.m.value.mem);
				char *buffer CLEANUP(free2)=fromFmt( "[%s] ", addrStr);
				return strClone(buffer);
			}
			break;
		}
		default:
			fprintf(stderr, "Invalid/unimplemented addressing mode!!\n");
			assert(0);
		}
	}
	}
}
void X86EmitAsmInst(struct opcodeTemplate *template, strX86AddrMode args, int *err) {
	//
	// Use intelAlias to use intel name/instruction
	//
	fprintf(currentFileSet->codeTmpFile, "%s ", opcodeTemplateIntelAlias(template));
	for (long i = 0; i != strX86AddrModeSize(args); i++) {
		if (i != 0)
			fputc(',', currentFileSet->codeTmpFile);
		strChar mode CLEANUP(strCharDestroy) = emitMode(args, i);
		fputs(mode, currentFileSet->codeTmpFile);
	}
	fputc('\n', currentFileSet->codeTmpFile);
	if (err)
		*err = 0;
	return;
fail:
	if (err)
		*err = 1;
}
void X86EmitAsmParserInst(struct parserNodeAsmInstX86 *inst) {
	struct parserNodeName *name = (void *)inst->name;
	strX86AddrMode args CLEANUP(strX86AddrModeDestroy);
	for (long i = 0; i != strParserNodeSize(inst->args); i++)
		args = strX86AddrModeAppendItem(args, parserNode2X86AddrMode(inst->args[i]));
	struct opcodeTemplate *template = X86OpcodeByArgs(name->text, args);
	assert(template);
	int err;
	X86EmitAsmInst(template, args, &err);
	assert(!err);
}
void X86EmitAsmGlobalVar(struct parserVar *var) {
		__auto_type base=objectBaseType(var->type);
		if(var->isGlobal) {
				__auto_type find=parserGetGlobalSym(var->name);
				switch(find->link.type) {
				case LINKAGE_EXTERN:
				case LINKAGE_IMPORT:
				case LINKAGE__EXTERN:
				case LINKAGE__IMPORT:
						return;
				case LINKAGE_INTERNAL:
						return;
				default:;
				}
		}
		if(base->type==TYPE_CLASS||base->type==TYPE_UNION) {
				char *szStr CLEANUP(free2)=sizeofName(base);
				fprintf(currentFileSet->initSymbolsTmpFile, "$%s: resb %s\n", var->name,szStr);
				return ;
		}
	fprintf(currentFileSet->initSymbolsTmpFile, "$%s: resb %li\n", var->name, objectSize(var->type, NULL));
}
void X86EmitAsmIncludeBinfile(const char *fileName) {
	char *otherValids = " []{}\\|;:\"\'<>?,./`~!@#$%^&*()-_+=";
	fprintf(currentFileSet->codeTmpFile, "incbin \"");
	long len = strlen(fileName);
	for (long c = 0; c != len; c++) { // C++,not a fan
		if (fileName[c] != '\"') {
			fputc(fileName[c], currentFileSet->codeTmpFile);
		} else {
			fputs("\"", currentFileSet->codeTmpFile);
		}
	}
	fprintf(currentFileSet->codeTmpFile, "\"\n");
}
char *X86EmitAsmLabel(const char *name) {
		strChar funcNam CLEANUP(strCharDestroy)=getCurrFuncName();
	if (!name) {
		char *buffer CLEANUP(free2)=fromFmt( "%s_LBL_%li$", funcNam,++currentFileSet->labelCount);
		fprintf(currentFileSet->codeTmpFile, "%s:\n", buffer);
		char *retVal = calloc(strlen(buffer) + 1,1);
		strcpy(retVal, buffer);
		return retVal;
	}
	char *retVal = calloc(strlen(name) + 1,1);
	fprintf(currentFileSet->codeTmpFile, "$%s:\n", name);
	strcpy(retVal, name);
	return retVal;
}
static strChar dumpStrLit(const char *str,long len) {
	char *otherValids = " []{}|;:\"<>?,./`~!@#$%^&*()-_+=";
	strChar retVal = NULL;
	for (long i = 0; i != len; i++) {
		if (i != 0)
			retVal = strCharAppendItem(retVal, ',');
		if (isalnum(str[i])) {
			retVal = strCharAppendItem(retVal, '\'');
			retVal = strCharAppendItem(retVal, str[i]);
			retVal = strCharAppendItem(retVal, '\'');
		} else {
			if (strchr(otherValids, str[i])&&str[i]!='\0') {
				retVal = strCharAppendItem(retVal, '\'');
				retVal = strCharAppendItem(retVal, str[i]);
				retVal = strCharAppendItem(retVal, '\'');
			} else {
				char *buffer CLEANUP(free2)=fromFmt("0x%02x", ((uint8_t *)str)[i]);
				retVal = strCharAppendData(retVal, buffer, strlen(buffer));
			}
		}
	}
	retVal = strCharAppendItem(retVal, '\0');
	return retVal;
}
struct X86AddressingMode *X86EmitAsmDU64(strX86AddrMode data, long len) {
		strChar funcNam CLEANUP(strCharDestroy)=getCurrFuncName();
		
		char *buffer CLEANUP(free2)=fromFmt("%s_DU64_%li", funcNam,++currentFileSet->labelCount);
	fprintf(currentFileSet->constsTmpFile, "%s: DQ ", buffer);
	for (long i = 0; i != len; i++) {
		if (i != 0)
			fputc(',', currentFileSet->constsTmpFile);
		strChar text CLEANUP(strCharDestroy) = emitMode(data, i);
		fprintf(currentFileSet->constsTmpFile, "%s", text);
	}
	fprintf(currentFileSet->constsTmpFile, "\n");
	return X86AddrModeLabel(buffer);
}
struct X86AddressingMode *X86EmitAsmDU32(strX86AddrMode data, long len) {
		strChar funcNam CLEANUP(strCharDestroy)=getCurrFuncName();
		char *buffer CLEANUP(free2)=fromFmt("%s_DU32_%li", funcNam,++currentFileSet->labelCount);
	fprintf(currentFileSet->constsTmpFile, "%s: DD ", buffer);
	for (long i = 0; i != len; i++) {
		if (i != 0)
			fputc(',', currentFileSet->constsTmpFile);
		strChar text CLEANUP(strCharDestroy) = emitMode(data, i);
		fprintf(currentFileSet->constsTmpFile, "%s", text);
	}
	fprintf(currentFileSet->constsTmpFile, "\n");
	return X86AddrModeLabel(buffer);
}
struct X86AddressingMode *X86EmitAsmDU16(strX86AddrMode data, long len) {
		strChar funcNam CLEANUP(strCharDestroy)=getCurrFuncName();
		char *buffer CLEANUP(free2)=fromFmt("%s_DU16_%li", funcNam,++currentFileSet->labelCount);
	fprintf(currentFileSet->constsTmpFile, "%s: DW ", buffer);
	for (long i = 0; i != len; i++) {
		if (i != 0)
			fputc(',', currentFileSet->constsTmpFile);
		strChar text CLEANUP(strCharDestroy) = emitMode(data, i);
		fprintf(currentFileSet->constsTmpFile, "%s", text);
	}
	fprintf(currentFileSet->constsTmpFile, "\n");
	return X86AddrModeLabel(buffer);
}
struct X86AddressingMode *X86EmitAsmDU8(strX86AddrMode data, long len) {
		strChar funcNam CLEANUP(strCharDestroy)=getCurrFuncName();
		char *buffer CLEANUP(free2)=fromFmt("%s_DU8_%li", funcNam,++currentFileSet->labelCount);
	fprintf(currentFileSet->constsTmpFile, "%s: DB ", buffer);
	for (long i = 0; i != len; i++) {
		if (i != 0)
			fputc(',', currentFileSet->constsTmpFile);
		strChar text CLEANUP(strCharDestroy) = emitMode(data, i);
		fprintf(currentFileSet->constsTmpFile, "%s", text);
	}
	fprintf(currentFileSet->constsTmpFile, "\n");
	return X86AddrModeLabel(buffer);
}
void X86EmitAsmComment(const char *text) {
		fprintf(currentFileSet->codeTmpFile, " ;%s\n", text);
}
struct X86AddressingMode *X86EmitAsmStrLit(const char *text,long size) {
		char *unes CLEANUP(free2)=escapeString((char*)text,size);
		__auto_type find=mapAddrModeGet(currentFileSet->strings,unes);
		if(find) {
				return X86AddrModeClone(*find);
		}
		strChar funcNam CLEANUP(strCharDestroy)=getCurrFuncName();
		strChar unes2 CLEANUP(strCharDestroy) = dumpStrLit(text,size);
		char *buffer CLEANUP(free2)=fromFmt( "%s_STR_%li", funcNam,++currentFileSet->labelCount);
	fprintf(currentFileSet->constsTmpFile, "%s: DB %s\n", buffer, unes2);

	__auto_type retVal=X86AddrModeLabel(buffer);
	retVal->valueType=objectPtrCreate(&typeU8i);
	mapAddrModeInsert(currentFileSet->strings, unes,X86AddrModeClone(retVal));

	return retVal;
}
static strChar file2Str(FILE *f) {
	fseek(f, 0, SEEK_END);
	long end = ftell(f);
	fseek(f, 0, SEEK_SET);
	long start = ftell(f);
	strChar retVal = strCharResize(NULL, end - start+1);
	fread(retVal, end - start, 1, f);
	retVal[end-start]='\0';
	return retVal;
}
void X86EmitAsmAddCachedFuncIfExists(const char *funcName,int *success) {
		//
		__auto_type func=parserGetFuncByName(funcName);
		if(!func) return ;
		char *fn2 CLEANUP(free2)=hashSource(func->__cacheStartToken,func->__cacheEndToken,func->name,NULL);
		char *name CLEANUP(free2)=fromFmt("%s.s", fn2);
		if(0==access(name, F_OK)) {
				mapFuncFilesInsert(funcFiles, funcName, strClone(name));
				if(success) *success=1;
		}  else {
				if(success) *success=0;
		}
}
static void createBreakPointInfo(FILE *writrTo);
void X86EmitAsm2File(const char *name,const char *cacheDir) {
		long fCount;
		mapFuncFilesKeys(funcFiles, NULL, &fCount);
		const char *funcs[fCount];
		mapFuncFilesKeys(funcFiles, funcs, &fCount);
		
		FILE *writeTo=fopen(name, "w");
		X86EmitClassMetaData(writeTo);
		X86EmitSymbolTable(writeTo);

		if(HCC_Debug_Enable) {
				//Emit token line macros
				cacheDir=(!cacheDir)?cacheDirLocation:cacheDir;
				for(long f=0;f!=fCount;f++) {
						char *fn=*mapFuncFilesGet(funcFiles, funcs[f]);
						__auto_type func=parserGetFuncByName(funcs[f]);
						if(!func) continue; //Init function
						if(func->isForwardDecl) continue;
						char *tokensFile CLEANUP(free2)=__getUpdatedTokenLinesFromCache(func);
						char *escaped CLEANUP(free2)=escapeString(tokensFile,strlen(tokensFile));
						fprintf(writeTo, "%%include \"%s\"\n", escaped);
				}
		}
		
		for(long f=0;f!=fCount;f++) {
				char *fn=*mapFuncFilesGet(funcFiles, funcs[f]);
				const char *fmt="%s";
				char *buffer CLEANUP(free2)=fromFmt(fmt, fn);
				
				char *escaped=escapeString(buffer,strlen(buffer));
				fprintf(writeTo, "%%include \"/%s\"\n",escaped);
				free(escaped);
		}

		
		switch(getCurrentArch()) {
		case ARCH_TEST_SYSV:
		case ARCH_X86_SYSV:
				fprintf(writeTo, "SECTION .init_array\n");
				fprintf(writeTo, "DD ");
				for(long i=0;i!=strStartCodeNameSize(initCodeNames);i++) {
						if(i) fputc(',', writeTo);
						fprintf(writeTo, "%s",initCodeNames[i]);
				}
		break;
		case ARCH_X64_SYSV:
				fprintf(writeTo, "SECTION .init_array\n");
				fprintf(writeTo, "DQ ");
				for(long i=0;i!=strStartCodeNameSize(initCodeNames);i++) {
						if(i) fputc(',', writeTo);
						fprintf(writeTo, "%s ",initCodeNames[i]);
				}
				break;
		}
		
		if(HCC_Debug_Enable) {
				char *debugSymbolText CLEANUP(free2)=emitDebuggerTypeDefinitions();
		
				fprintf(writeTo, "\nSECTION .data\n");
				strChar symsText CLEANUP(strCharDestroy)=dumpStrLit(debugSymbolText,strlen(debugSymbolText)+1);
				fprintf(writeTo, "HCC_DEBUG_SYMS: DB %s \n",symsText);
				const char *ddType=(ptrSize()==4)?"DD":"DQ";
				fprintf(writeTo, "HCC_DEBUG_FUNC_DATAS:%s ",ddType);
				for(long f=0;f!=fCount;f++) {
						__auto_type func=parserGetFuncByName(funcs[f]);
						if(!func) continue; //Init function
						if(func->isForwardDecl) continue;
						char *fmted CLEANUP(free2)=fromFmt(FUNC_BREAKPOINTS_LAB_INFO, funcs[f]);
						fprintf(writeTo, " %s," FUNC_CODE_END_LAB_FMT",  %s, ", funcs[f],funcs[f],fmted);
				}
				fprintf(writeTo,"0 \n"); //NULL TERMINATE LIST
		
				createBreakPointInfo(writeTo);
				long len=breakPointCount;
				fprintf(writeTo, "SECTION .bss\nHCC_DEBUG_BREAKPOINTS_ARRAY: resb %li\n ",len);		

				{
						//(GlobalPtr,json),(GlobalPtr,json)...0
						fprintf(writeTo, "SECTION .data\nHCC_DEBUG_GLOBALS_INFO: ");
						long count;
						parserSymTableNames(NULL, &count);
						const char *names[count];
						parserSymTableNames(names,NULL);
						for(long k=0;k!=count;k++) {
								__auto_type find=parserGetGlobalSym(names[k]);
								if(!find->var)continue;
								fprintf(writeTo, "%s %s\n", ddType,parserGetGlobalSymLinkageName(names[k]));
								char *text CLEANUP(free2)=emitDebuggerGlobalVarInfo((char*)names[k]);
								strChar infoStr CLEANUP(strCharDestroy)=dumpStrLit(text, strlen(text)+1);
								fprintf(writeTo,"DB %s\n",infoStr);
						}
						fprintf(writeTo, "%s 0\n",ddType); 
				}

				{
						//
						long count;
						mapFnLabelKeys(filenameStrLabels, NULL, &count);
						const char *keys[count];
						mapFnLabelKeys(filenameStrLabels,keys,NULL);
						for(long f=0;f!=count;f++) {
								strChar fn CLEANUP(strCharDestroy)=dumpStrLit(keys[f], strlen(keys[f])+1);
								fprintf(writeTo, "%s: DB %s\n", *mapFnLabelGet(filenameStrLabels, keys[f]),fn);
						}
				}
		}
		
		fclose(writeTo);
}
void X86EmitAsmEnterFileStartCode(llLexerItem first) {
		long count=strStartCodeNameSize(initCodeNames);
		initFileNum=count;
		
		X86EmitAsmEnterFunc(NULL);
		strChar name CLEANUP(strCharDestroy)=getCurrFuncName();
		initCodeNames=strStartCodeNameAppendItem(initCodeNames, strClone(name));
}
struct minMax {
		long min,max;
};
MAP_TYPE_DEF(struct minMax,MinMax);
MAP_TYPE_FUNCS(struct minMax,MinMax);
static void createBreakPointInfo(FILE *writeTo) {
		long bpCount;
		mapBreakLinesKeys(breakpoints,NULL, &bpCount);
		const char *keys[bpCount];
		mapBreakLinesKeys(breakpoints,keys,NULL);

		mapMinMax funcLineRanges=mapMinMaxCreate();
		for(long k=0;k!=bpCount;k++) {
				//Have format file:line
				char *last=strrchr(keys[k], ':');
				char fn[last-keys[k]+1];
				fn[last-keys[k]]='\0';
				strncpy(fn, keys[k],last-keys[k]);

				long line;
				sscanf(last+1, "%li",&line) ;
		loop:;
				__auto_type mm=mapMinMaxGet(funcLineRanges, fn);
				if(!mm) {
						struct minMax tmp={INT_MAX,INT_MIN};
						mapMinMaxInsert(funcLineRanges, fn, tmp);
						goto loop;
				}
				__auto_type find= mapBreakLinesGet(breakpoints, keys[k]);
				if(line<mm->min) mm->min=line;
				if(line>mm->max) mm->max=line;
		}

		/*
				{"breakpoints":[
				[[filename]]: {
				    "lines":[
             ({firstlinnum:bpOffset})...(last linnum:bpOffset) //Only valid linums
								]
				}
				]}
			*/
		long count;
		strChar total CLEANUP(strCharDestroy)=NULL;
		mapMinMaxKeys(funcLineRanges, NULL, &count);
		const char *keys2[count];
		mapMinMaxKeys(funcLineRanges, keys2, &count);
		
		for(long k=0;k!=count;k++) {
				strChar allLines CLEANUP(strCharDestroy)=NULL;
				__auto_type find=mapMinMaxGet(funcLineRanges, keys2[k]);
				for(long l=find->min;l<=find->max;l++) {
						char *fmt CLEANUP(free2)=fromFmt("%s:%li", keys2[k],l);
						__auto_type find2=mapBreakLinesGet(breakpoints, fmt);
						if(!find2) continue;
						char *ln CLEANUP(free2)=fromFmt("{line:%li,offset:%li},\n",l,find2->bpOffset);
						allLines=strCharAppendData(allLines, ln, strlen(ln));;
				}
				allLines=strCharAppendItem(allLines,'\0');

				char *esc CLEANUP(free2)=escapeString((char*)keys2[k], strlen(keys2[k]));
				char *json CLEANUP(free2)=fromFmt("{filename:\"%s\",\"lines\":[%s]},",esc,allLines);
				total=strCharAppendData(total, json,strlen(json));
		}
		total=strCharAppendItem(total, '\0');
		mapMinMaxDestroy(funcLineRanges, NULL);

		//Create json of function first lines 
		strChar funcFirstLns CLEANUP(strCharDestroy)=NULL;
		{
				long count;
				parserSymTableNames(NULL, &count);
				const char *keys[count];
				parserSymTableNames(keys,NULL);
				
				for(long f=0;f!=count;f++) {
						__auto_type find=parserGetGlobalSym(keys[f]);
						if(find->var) continue;
						if(find->type->type!=TYPE_FUNCTION) continue;
						char *buffer CLEANUP(free2)=fromFmt("\"%s\":{\"filename\":\"%s\",\"line\":%li},\n",keys[f],find->fn,find->ln);
						funcFirstLns=strCharAppendData(funcFirstLns,buffer,strlen(buffer));
				}
		}
		funcFirstLns=strCharAppendItem(funcFirstLns, '\0');
		
		char *json CLEANUP(free2)=fromFmt("{breakpoints:[%s],funcLines:{%s}}", total,funcFirstLns);
		strChar infoStr CLEANUP(strCharDestroy)=dumpStrLit(json, strlen(json)+1);
		fprintf(writeTo, "\nHCC_DEBUG_BREAKPOINTS_INFO: DB %s \n",infoStr);

		
}
struct X86AddressingMode *X86EmitAsmDebuggerLine(llLexerItem item) {
		if(!currentFileSet->func) {
				long line;
				diagLineCol(NULL, llLexerItemValuePtr(item)->start, &line,NULL);
				return X86AddrModeSint(line);
		}
		
		long offset=tokOffset(currentFileSet->func->__cacheStartToken, item);
		strChar funcNam CLEANUP(strCharDestroy)=getCurrFuncName();
		char *macro CLEANUP(free2)=fromFmt(FUNC_BREAKPOINTS_LAB_FMT_LN, funcNam,offset);
		return X86AddrModeMacro(macro, dftValType());
}
void X86EmitAsmEnterFunc(struct parserFunction *func) {
		struct asmFileSet *set=calloc(sizeof(struct asmFileSet), 1);
		set->codeTmpFile=tmpfile();
		set->constsTmpFile=tmpfile();
		set->initSymbolsTmpFile=tmpfile();
		set->labelCount=0;
		set->parent=currentFileSet;
		set->symbolsTmpFile=tmpfile();
		set->token2Line=ptrMapToken2LineCreate();
		set->func=func;
		set->strings=mapAddrModeCreate();
		currentFileSet=set;
}
void X86EmitAsmLeaveFunc(const char *cacheDir) {
		const char *fmt="%s/%s.s";
		strChar funcNam CLEANUP(strCharDestroy)=getCurrFuncName();
		char *name CLEANUP(free2)=NULL;
				if(currentFileSet->func) {
						char *fn2 CLEANUP(free2)=hashSource(currentFileSet->func->__cacheStartToken,currentFileSet->func->__cacheEndToken,currentFileSet->func->name,NULL);
						name=fromFmt("%s.s", fn2);	
		} else name	=fromFmt(fmt, cacheDirLocation,funcNam);
		
		FILE *fn = fopen(name, "w");
		strChar symbols CLEANUP(strCharDestroy) = file2Str(currentFileSet->symbolsTmpFile);
		strChar code CLEANUP(strCharDestroy) = file2Str(currentFileSet->codeTmpFile);
		strChar consts CLEANUP(strCharDestroy) = file2Str(currentFileSet->constsTmpFile);
		strChar initSyms CLEANUP(strCharDestroy) = file2Str(currentFileSet->initSymbolsTmpFile);
		fwrite(symbols, strlen(symbols), 1, fn);
		fprintf(fn, "SECTION .text\n");
		fprintf(fn, "%s:\n", funcNam);
		fwrite(code, strlen(code), 1, fn);
		fprintf(fn, FUNC_CODE_END_LAB_FMT ": \n",funcNam);
		if(strlen(consts)) {
				fprintf(fn, "SECTION .data\n");
				fwrite(consts, strCharSize(consts)-1, 1, fn);
		}
		if(strlen(initSyms)) {
				fprintf(fn, "SECTION .bss\n");
				fwrite(initSyms, strCharSize(initSyms)-1, 1, fn);
		}
		fclose(fn);

		mapFuncFilesInsert(funcFiles, funcNam,  strClone(name));
		
		fclose(currentFileSet->codeTmpFile);
		fclose(currentFileSet->constsTmpFile);
		fclose(currentFileSet->initSymbolsTmpFile);
		fclose(currentFileSet->symbolsTmpFile);
		
		__auto_type old=currentFileSet->parent;
		free(currentFileSet);
		currentFileSet=old;
}
char *X86EmitAsmUniqueLabName(const char *head) {
		const char *fmt="%s_%s_%li$";
		if (!head)
		head = "";
		strChar funcNam CLEANUP(strCharDestroy)=getCurrFuncName();
		return fromFmt(fmt, funcNam,head, ++currentFileSet->labelCount);
}
void X86EmitAsmDebuggerInfo(char *text) {
		if(!HCC_Debug_Enable) return;
		
		strChar funNam CLEANUP(strCharDestroy)=getCurrFuncName();
		strChar unes CLEANUP(strCharDestroy)=dumpStrLit(text, strlen(text)+1); 
		char *lab CLEANUP(free2)= fromFmt(FUNC_BREAKPOINTS_LAB_INFO,funNam);
		fprintf(currentFileSet->constsTmpFile,"%s:DB  %s\n",lab,unes);
		
		if(currentFileSet->token2Line) {
				long tokCount=ptrMapToken2LineSize(currentFileSet->token2Line);
				llLexerItem tokens[tokCount];
				ptrMapToken2LineKeys(currentFileSet->token2Line, tokens);
				struct tokenInfo tInfos[tokCount];
				for(long t=0;t!=tokCount;t++)
						tInfos[t]=*ptrMapToken2LineGet(currentFileSet->token2Line, tokens[t]);

				qsort(tInfos, tokCount, sizeof(*tInfos), (int(*)(const void*,const void*))tokenInfoCmp);
				long indexes[tokCount];
				for(long t=0;t!=tokCount;t++)
						indexes[t]=tInfos[t].tokIndex;

				if(currentFileSet->func) {
						char *fn CLEANUP(free2)=hashSource(currentFileSet->func->__cacheStartToken,currentFileSet->func->__cacheEndToken,currentFileSet->func->name,NULL);
						const char *suffix=DBG_TOKEN_FILE_SUFFIX;
						fn=realloc(fn, strlen(fn)+strlen(suffix)+1);
						fn=strcat(fn, suffix);

						FILE *f=fopen(fn, "w");
						__emitFuncTokenLines(f, currentFileSet->func, indexes, tokCount);
						fclose(f);
				}
		}
}
static void __registerToken(llLexerItem token) {
		__auto_type find=ptrMapToken2LineGet(currentFileSet->token2Line, token);
		llLexerItem startNode=NULL;
		if(!find&&currentFileSet->func) {
				startNode=currentFileSet->func->__cacheStartToken;
		registerTok:;
				const char *fn;
 				long line;
				diagLineCol(&fn, llLexerItemValuePtr(token)->start, &line, NULL);
				struct tokenInfo info;
				info.ln=line;
				info.tokIndex=tokOffset(startNode,  token);
				info.fn=fn;
				ptrMapToken2LineAdd(currentFileSet->token2Line, token, info);

				char *bp CLEANUP(free2)=fromFmt("%s:%li", fn,line);
				__auto_type find=mapBreakLinesGet(breakpoints, bp);
				if(!find) {
						struct breakPoint toIns;
						toIns.bpOffset=breakPointCount++;
						toIns.line=line;
						mapBreakLinesInsert(breakpoints, bp,toIns);
				}
		} else if(!find&&!currentFileSet->func) {
				startNode=llLexerItemFirst(token);
				goto  registerTok;
		}
}
struct X86AddressingMode *X86EmitAsmDebuggerTokenLine(llLexerItem token) {
		__registerToken(token);
		__auto_type find=ptrMapToken2LineGet(currentFileSet->token2Line, token);
		if(currentFileSet->func) {
				strChar funcNm CLEANUP(strCharDestroy)=getCurrFuncName();
				char *fmted CLEANUP(free2)=fromFmt(FUNC_BREAKPOINTS_LAB_FMT_LN, funcNm,find->tokIndex);
				return X86AddrModeMacro(fmted, dftValType());
		} else {
				return X86AddrModeSint(find->ln);
		}
}
struct X86AddressingMode *X86EmitAsmDebuggerTokenFn(llLexerItem token) {
		__registerToken(token);
		__auto_type find=ptrMapToken2LineGet(currentFileSet->token2Line, token);
		if(currentFileSet->func) {
				strChar funcNm CLEANUP(strCharDestroy)=getCurrFuncName();
				char *fmted CLEANUP(free2)=fromFmt(FUNC_BREAKPOINTS_LAB_FMT_FN, funcNm,find->tokIndex);
				return X86AddrModeMacro(fmted, objectPtrCreate(&typeU8i));
		} else {
				return X86AddrModeStr(find->fn,strlen(find->fn)+1);
		}
}
struct X86AddressingMode *X86EmitAsmDebuggerBreakpoint(llLexerItem token) {
		__registerToken(token);
		if(currentFileSet->func) {
				__auto_type find=ptrMapToken2LineGet(currentFileSet->token2Line, token);
				strChar funcNm CLEANUP(strCharDestroy)=getCurrFuncName();
				char *fmted CLEANUP(free2)=fromFmt(FILE_BREAKPOINTS_MAC_FMT_BPOFF, funcNm,find->tokIndex);
				return X86AddrModeMacro(fmted, objectPtrCreate(&typeU8i));
		} else {
				const char *fn;
				long line;
				diagLineCol(&fn, llLexerItemValuePtr(token)->start, &line, NULL);
				char *fmted CLEANUP(free2)=fromFmt("%s:%li", fn,line);
				return X86AddrModeSint(mapBreakLinesGet(breakpoints, fmted)->bpOffset);
		}
}
