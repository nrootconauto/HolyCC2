#include "X86AsmSharedVars.h"
#include "asmEmitter.h"
#include <assert.h>
#include "cleanup.h"
#include <ctype.h>
#include "frameLayout.h"
#include "opcodesParser.h"
#include "parserA.h"
#include "parserB.h"
#include "ptrMap.h"
#include "registers.h"
#include <stdio.h>
#include "cacheDir.h"
STR_TYPE_DEF(char, Char);
STR_TYPE_FUNCS(char, Char);
PTR_MAP_FUNCS(struct parserNode *, strChar, LabelNames);
PTR_MAP_FUNCS(struct reg *, strChar, RegName);
MAP_TYPE_DEF(char *, SymAsmName);
MAP_TYPE_FUNCS(char *, SymAsmName);
STR_TYPE_DEF(strChar, StartCodeName);
STR_TYPE_FUNCS(strChar, StartCodeName);
static __thread strStartCodeName initCodeNames=NULL;
static __thread struct asmFileSet {
		FILE *constsTmpFile;
		FILE *symbolsTmpFile;
		FILE *initSymbolsTmpFile;
		FILE *codeTmpFile;
		long labelCount;
		strChar funcName;
		struct asmFileSet *parent;
} *currentFileSet=NULL;
MAP_TYPE_DEF(strChar,FuncFiles);
MAP_TYPE_FUNCS(strChar,FuncFiles);
static __thread mapFuncFiles funcFiles=NULL;
static ptrMapRegName regNames;
static void strCharDestroy2(strChar *str) {
	strCharDestroy(str);
}
static strChar strClone(const char *text) {
	__auto_type retVal = strCharAppendData(NULL, text, strlen(text) + 1);
	strcpy(retVal, text);
	return retVal;
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
void X86EmitAsmInit() {
		funcFiles=mapFuncFilesCreate();
		strStartCodeNameDestroy(&initCodeNames);
		initCodeNames=NULL;
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
	return strCharAppendItem(strCharReverse(retVal), '\0');
}
static strChar uint64ToStr(uint64_t value) {
	strChar retVal = NULL;
	const char *digits = "0123456789";
	do {
		retVal = strCharAppendItem(retVal, digits[value % 10]);
		value /= 10;
	} while (value != 0);
	return strCharAppendItem(strCharReverse(retVal), '\0');
	;
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
		} else if ((link->type & LINKAGE_STATIC)) {
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
	case X86ADDRMODE_STR: {
			struct X86AddressingMode *stred CLEANUP(X86AddrModeDestroy) = X86EmitAsmStrLit((char*)args[i]->value.text,__vecSize(args[i]->value.text));
		return emitMode(&stred, 0);
		break;
	}
	case X86ADDRMODE_FLT: {
		assert(0);
		break;
	}
	case X86ADDRMODE_VAR_ADDR: {
			__auto_type find = ptrMapFrameOffsetGet(localVarFrameOffsets, args[i]->value.varAddr.var);
			if (find) {
				struct X86AddressingMode *offset CLEANUP(X86AddrModeDestroy) =
						X86AddrModeIndirSIB(0, NULL, X86AddrModeReg(basePointer(),objectPtrCreate(&typeU0)), X86AddrModeSint(-*find), args[i]->value.varAddr.var->type);
				return emitMode(&offset, 0);
			} else {
					strChar name CLEANUP(strCharDestroy) = strClone(parserGetGlobalSymLinkageName(args[i]->value.varAddr.var->name));
					if(args[i]->value.itemAddr.offset) {
							long offset=args[i]->value.itemAddr.offset;
							long len = snprintf(NULL, 0, "[$%s+%li] ", name,offset);
							strChar retVal = strCharResize(NULL, len + 1);
							sprintf(retVal, "[$%s+%li] ", name,offset);
							return retVal;
					} else {
							long len = snprintf(NULL, 0, "[$%s] ", name);
							strChar retVal = strCharResize(NULL, len + 1);
							sprintf(retVal, "[$%s] ", name);
							return retVal;
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
		if(args[i]->value.itemAddr.offset) {
				long offset=args[i]->value.itemAddr.offset;
				long len = snprintf(NULL, 0, "[$%s+%li] ", name,offset);
				strChar retVal = strCharResize(NULL, len + 1);
				sprintf(retVal, "[$%s+%li] ", name,offset);
				return retVal;
		} else {
				long len = snprintf(NULL, 0, "[$%s] ", name);
				strChar retVal = strCharResize(NULL, len + 1);
				sprintf(retVal, "[$%s] ", name);
				return retVal;
		}
	}
	case X86ADDRMODE_LABEL: {
		long len = snprintf(NULL, 0, "$%s ", args[i]->value.label);
		strChar retVal = strCharResize(NULL, len + 1);
		sprintf(retVal, "$%s ", args[i]->value.label);
		return retVal;
	}
	case X86ADDRMODE_REG: {
		__auto_type find = ptrMapRegNameGet(regNames, args[i]->value.reg);
		assert(find);
		long len = snprintf(NULL, 0, "%s ", *find);
		strChar retVal = strCharResize(NULL, len + 1);
		sprintf(retVal, "%s ", *find);
		return retVal;
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
			long len = snprintf(NULL, 0, " %s [%s] ", sizeStr, labelStr);
			strChar retVal = strCharResize(NULL, len + 1);
			sprintf(retVal, " %s [%s] ", sizeStr, labelStr);
			return retVal;
		}
		case x86ADDR_INDIR_REG: {
			__auto_type reg = ptrMapRegNameGet(regNames, args[i]->value.m.value.indirReg);
			if (args[i]->valueType) {
				strChar sizeStr CLEANUP(strCharDestroy) = getSizeStr(args[i]->valueType);
				if (!sizeStr) {
					fprintf(stderr, "That's one gaint register(%s)\n", *reg);
					assert(0);
				}
				long len = snprintf(NULL, 0, " %s [%s] ", sizeStr, *reg);
				strChar retVal = strCharResize(NULL, len + 1);
				sprintf(retVal, " %s [%s] ", sizeStr, *reg);
				return retVal;
			} else {
				long len = snprintf(NULL, 0, "[%s] ", *reg);
				strChar retVal = strCharResize(NULL, len + 1);
				sprintf(retVal, "[%s] ", *reg);
				return retVal;
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
					long len=snprintf(NULL, 0, "%+li",  args[i]->value.m.value.sib.offset2);
					char buffer[len+1];
					sprintf(buffer,"%+li",args[i]->value.m.value.sib.offset2);
					if(offsetStr==NULL)
							offsetStr=strCharAppendItem(offsetStr, '\0');
					offsetStr=strCharResize(offsetStr,strCharSize(offsetStr)+len);
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
			if (args[i]->value.m.value.sib.offset||args[i]->value.m.value.sib.offset2) {
				if (offsetStr[0] != '-'&&offsetStr[0] != '+')
					retVal = strCharAppendItem(retVal, '+');
				retVal = strCharConcat(retVal, offsetStr);
				offsetStr = NULL; // Will be free'd,but has been free'd by concat
			}
			retVal = strCharAppendItem(retVal, '\0');
			if (args[i]->valueType) {
				strChar typeStr CLEANUP(strCharDestroy) = getSizeStr(args[i]->valueType);
				long len = snprintf(NULL, 0, "%s [%s] ", typeStr, retVal);
				strChar retVal2 = strCharResize(NULL, len + 1);
				sprintf(retVal2, "%s [%s] ", typeStr, retVal);
				return retVal2;
			} else {
				long len = snprintf(NULL, 0, "[%s] ", retVal);
				strChar retVal2 = strCharResize(NULL, len + 1);
				sprintf(retVal2, "[%s] ", retVal);
				return retVal2;
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
				long len = snprintf(NULL, 0, "%s [%s] ", sizeStr, addrStr);
				strChar retVal2 = strCharResize(NULL, len + 1);
				sprintf(retVal2, "%s [%s] ", sizeStr, addrStr);
				return retVal2;
			} else {
				strChar addrStr CLEANUP(strCharDestroy) = uint64ToStr(args[i]->value.m.value.mem);
				long len = snprintf(NULL, 0, "[%s] ", addrStr);
				strChar retVal2 = strCharResize(NULL, len + 1);
				sprintf(retVal2, "[%s] ", addrStr);
				return retVal2;
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
	if (!name) {
			long count = snprintf(NULL, 0, "%s_LBL_%li$",currentFileSet->funcName, ++currentFileSet->labelCount);
		char buffer[count + 1];
		sprintf(buffer, "%s_LBL_%li$", currentFileSet->funcName,currentFileSet->labelCount);
		fprintf(currentFileSet->codeTmpFile, "%s:\n", buffer);
		char *retVal = calloc(count + 1,1);
		strcpy(retVal, buffer);
		return retVal;
	}
	char *retVal = calloc(strlen(name) + 1,1);
	fprintf(currentFileSet->codeTmpFile, "$%s:\n", name);
	strcpy(retVal, name);
	return retVal;
}
static strChar dumpStrLit(const char *str,long len) {
	char *otherValids = " []{}\\|;:\"\'<>?,./`~!@#$%^&*()-_+=";
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
				long count = snprintf(NULL, 0, "0x%02x", ((uint8_t *)str)[i]);
				char buffer[count + 1];
				sprintf(buffer, "0x%02x", ((uint8_t *)str)[i]);
				retVal = strCharAppendData(retVal, buffer, strlen(buffer));
			}
		}
	}
	retVal = strCharAppendItem(retVal, '\0');
	return retVal;
}
struct X86AddressingMode *X86EmitAsmDU64(strX86AddrMode data, long len) {
		long count = snprintf(NULL, 0, "%s_DU64_%li",currentFileSet->funcName, ++currentFileSet->labelCount);
	char buffer[count + 1];
	sprintf(buffer, "%s_DU64_%li", currentFileSet->funcName,currentFileSet->labelCount);
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
		long count = snprintf(NULL, 0, "%s_DU32_%li",currentFileSet->funcName, ++currentFileSet->labelCount);
	char buffer[count + 1];
	sprintf(buffer, "%s_DU32_%li", currentFileSet->funcName,currentFileSet->labelCount);
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
		long count = snprintf(NULL, 0, "%s_DU16_%li", currentFileSet->funcName,++currentFileSet->labelCount);
	char buffer[count + 1];
	sprintf(buffer, "%s_DU16_%li", currentFileSet->funcName,currentFileSet->labelCount);
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
		long count = snprintf(NULL, 0, "%s_DU8_%li",currentFileSet->funcName, ++currentFileSet->labelCount);
	char buffer[count + 1];
	sprintf(buffer, "%s_DU8_%li", currentFileSet->funcName,currentFileSet->labelCount);
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
		strChar unes CLEANUP(strCharDestroy) = dumpStrLit(text,size);
		long count = snprintf(NULL, 0, "%s_STR_%li",currentFileSet->funcName, ++currentFileSet->labelCount);
	char buffer[count + 1];
	sprintf(buffer, "%s_STR_%li", currentFileSet->funcName,currentFileSet->labelCount);
	fprintf(currentFileSet->constsTmpFile, "%s: DB %s\n", buffer, unes);
	return X86AddrModeLabel(buffer);
}
static strChar file2Str(FILE *f) {
	fseek(f, 0, SEEK_END);
	long end = ftell(f);
	fseek(f, 0, SEEK_SET);
	long start = ftell(f);
	strChar retVal = strCharResize(NULL, end - start);
	fread(retVal, end - start, 1, f);
	return retVal;
}
#include "escaper.h"
void X86EmitAsm2File(const char *name,const char *cacheDir) {
		long fCount;
		mapFuncFilesKeys(funcFiles, NULL, &fCount);
		const char *funcs[fCount];
		mapFuncFilesKeys(funcFiles, funcs, &fCount);
		
		FILE *writeTo=fopen(name, "w");
		X86EmitSymbolTable(writeTo);
		cacheDir=(!cacheDir)?cacheDirLocation:cacheDir;
		for(long f=0;f!=fCount;f++) {
				const char *fmt="%s/%s.s";
				long len=snprintf(NULL, 0, fmt, cacheDir,funcs[f]);
				char buffer[len+1];
				sprintf(buffer,  fmt,  cacheDir,funcs[f]);

				char *escaped=escapeString(buffer);
				fprintf(writeTo, "%%include \"%s\"\n", escaped);
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
		
		fclose(writeTo);
}
void X86EmitAsmEnterFileStartCode() {
		long count=strStartCodeNameSize(initCodeNames);
		long r=rand();
		const char *fmt="__init$%li";
		long len=snprintf(NULL, 0, fmt,count+r);
		char name[len+1];
		snprintf(name, len+1, fmt, count+r);
		X86EmitAsmEnterFunc(name);

		initCodeNames=strStartCodeNameAppendItem(initCodeNames, strClone(name));
}
void X86EmitAsmEnterFunc(const char *funcName) {
		struct asmFileSet *set=calloc(sizeof(struct asmFileSet), 1);
		set->codeTmpFile=tmpfile();
		set->constsTmpFile=tmpfile();
		set->initSymbolsTmpFile=tmpfile();
		set->labelCount=0;
		set->parent=currentFileSet;
		set->symbolsTmpFile=tmpfile();
		set->funcName=strClone(funcName);
		currentFileSet=set;
}
void X86EmitAsmLeaveFunc(const char *cacheDir) {
		cacheDir=(!cacheDir)?cacheDirLocation:cacheDir;
		const char *fmt="%s/%s.s";
		long len=snprintf(NULL, 0,fmt , cacheDir,currentFileSet->funcName); 
		char name[len+1];
		snprintf(name, len+1, fmt, cacheDir,currentFileSet->funcName);
		
		FILE *fn = fopen(name, "w");
		strChar symbols CLEANUP(strCharDestroy) = file2Str(currentFileSet->symbolsTmpFile);
		strChar code CLEANUP(strCharDestroy) = file2Str(currentFileSet->codeTmpFile);
		strChar consts CLEANUP(strCharDestroy) = file2Str(currentFileSet->constsTmpFile);
		strChar initSyms CLEANUP(strCharDestroy) = file2Str(currentFileSet->initSymbolsTmpFile);
		fwrite(symbols, strCharSize(symbols), 1, fn);
		fprintf(fn, "SECTION .text\n");
		fprintf(fn, "%s:", currentFileSet->funcName);
		fwrite(code, strCharSize(code), 1, fn);
		fprintf(fn, "SECTION .data\n");
		fwrite(consts, strCharSize(consts), 1, fn);
		fprintf(fn, "SECTION .bss\n");
		fwrite(initSyms, strCharSize(initSyms), 1, fn);

		fclose(fn);

		mapFuncFilesInsert(funcFiles, currentFileSet->funcName,  strClone(name));
		
		fclose(currentFileSet->codeTmpFile);
		fclose(currentFileSet->constsTmpFile);
		strCharDestroy(&currentFileSet->funcName);
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
		long count = snprintf(NULL, 0, fmt, currentFileSet->funcName, head, ++currentFileSet->labelCount);
	char buffer[count + 1];
	sprintf(buffer, fmt, currentFileSet->funcName,head, currentFileSet->labelCount);
	return strcpy(calloc(count+1, 1), buffer);
}
