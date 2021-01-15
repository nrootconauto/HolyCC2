#include <asmEmitter.h>
#include <registers.h>
#include <parserA.h>
#include <opcodesParser.h>
#include <stdio.h>
#include <cleanup.h>
#include <assert.h>
#include <ptrMap.h>
#include <parserB.h>
#include <ctype.h>
STR_TYPE_DEF(char,Char);
STR_TYPE_FUNCS(char,Char);
PTR_MAP_FUNCS(struct parserNode *, strChar, LabelNames);
PTR_MAP_FUNCS(struct reg *, strChar, RegName);
MAP_TYPE_DEF( char *, SymAsmName);
MAP_TYPE_FUNCS(char *, SymAsmName);
static __thread mapSymAsmName asmNames;
static __thread long labelCount=0;
static __thread ptrMapLabelNames labelsByParserNode=NULL;
static __thread FILE *constsTmpFile=NULL;
static __thread FILE *symbolsTmpFile=NULL;
static __thread FILE *codeTmpFile=NULL;
static ptrMapRegName regNames;
static void strCharDestroy2(strChar *str) {
		strCharDestroy(str);
}
static strChar strClone(const char *text) {
		__auto_type retVal= strCharAppendData(NULL, text, strlen(text)+1);
		strcpy(retVal, text);
		return retVal;
}
__attribute__((destructor)) static void  deinit() {
		ptrMapRegNameDestroy(regNames, (void(*)(void*))strCharDestroy2);
}
__attribute__((constructor)) static void init() {
		ptrMapRegName regNames=ptrMapRegNameCreate();
		ptrMapRegNameAdd(regNames, &regX86AL, strClone("AL"));
		ptrMapRegNameAdd(regNames, &regX86AH, strClone("AH"));
		ptrMapRegNameAdd(regNames, &regX86BL, strClone("BL"));
		ptrMapRegNameAdd(regNames, &regX86BH, strClone("BH"));
		ptrMapRegNameAdd(regNames, &regX86CL, strClone("CL"));
		ptrMapRegNameAdd(regNames, &regX86CH, strClone("CH"));
		ptrMapRegNameAdd(regNames, &regX86DL, strClone("DL"));
		ptrMapRegNameAdd(regNames, &regX86DH, strClone("DH"));
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
		labelCount=0;
		ptrMapLabelNamesDestroy(labelsByParserNode, (void(*)(void*))strCharDestroy2);
		mapSymAsmNameDestroy(asmNames, (void(*)(void*))strCharDestroy2);
		asmNames=mapSymAsmNameCreate();
		labelsByParserNode=ptrMapLabelNamesCreate();
		constsTmpFile=tmpfile();
		symbolsTmpFile=tmpfile();
		codeTmpFile=tmpfile();
}
static strChar int64ToStr(int64_t value) {
		strChar retVal=NULL;
		const char *digits="0123456789";
		int wasNegative=0;
		if(value<0) {
				value*=-1;
				wasNegative=1;
		}
		do {
				retVal=strCharAppendItem(retVal, digits[value%10]);
				value/=10;
		} while(value!=0);
		if(wasNegative)
				retVal=strCharConcat(retVal, strCharAppendItem(NULL, '-'));
		return strCharAppendItem(strCharReverse(retVal),'\0');
}
static strChar uint64ToStr(uint64_t value) {
		strChar retVal=NULL;
		const char *digits="0123456789";
		do {
				retVal=strCharAppendItem(retVal, digits[value%10]);
				value/=10;
		} while(value!=0);
		return strCharAppendItem(strCharReverse(retVal),'\0');;
}
static strChar getSizeStr(struct object *obj) {
	__auto_type base=objectBaseType(obj);
	switch(objectSize(base,  NULL)) {
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
static void X86EmitSymbolTable() {
		long count;
		parserSymTableNames(NULL,&count);
		const char *keys[count];
		parserSymTableNames(keys,NULL);
		for(long i=0;i!=count;i++) {
				strChar name CLEANUP(strCharDestroy2)=parserNodeSymbolName(parserGetGlobalSym(keys[i]));
				__auto_type link= parserGlobalSymLinkage(keys[i]);
				if((link->type&LINKAGE__EXTERN)||(link->type&LINKAGE__IMPORT)) {
						fprintf(symbolsTmpFile, "EXTERN %s ; Appears as %s in src.\n", name,keys[i]);
						mapSymAsmNameInsert(asmNames,keys[i],strClone(link->fromSymbol));
				} else if((link->type&LINKAGE_STATIC)) {
						//Do nothing
				} else if((link->type&LINKAGE_EXTERN)
														||(link->type&LINKAGE_IMPORT)) {
						fprintf(symbolsTmpFile, "EXTERN %s\n", name);
				} else if((link->type&LINKAGE_LOCAL)) {
						fprintf(symbolsTmpFile, "GLOBAL %s\n", name);
				}
		}
}
void X86EmitAsmInst(struct opcodeTemplate *template,strX86AddrMode args,int *err) {
		//
		// Use intelAlias to use intel name/instruction
		//
		fprintf(codeTmpFile, "%s ", opcodeTemplateIntelAlias(template));
		for(long i=0;i!=strX86AddrModeSize(args);i++) {
				if(i!=0)
						fputc(',', codeTmpFile);
				switch(args[i].type) {
				case X86ADDRMODE_FLT: {
						assert(0);
						break;
				}
				case X86ADDRMODE_ITEM_ADDR: {
						strChar name CLEANUP(strCharDestroy)=parserNodeSymbolName(args[i].value.itemAddr);
						if(!name)
								goto fail;
						fprintf(codeTmpFile, "%s ", name);
						break;
				}
				case X86ADDRMODE_LABEL: {
						fprintf(codeTmpFile, "%s ", args[i].value.label);
						break;
				}
				case X86ADDRMODE_REG: {
						__auto_type find=ptrMapRegNameGet(regNames, args[i].value.reg);
						assert(find);
						fprintf(codeTmpFile, "%s ", *find);
						break;
				}
				case X86ADDRMODE_SINT: {
						strChar text CLEANUP(strCharDestroy)=int64ToStr(args[i].value.sint);
						fprintf(codeTmpFile, "%s ", text);
						break;
				}
				case X86ADDRMODE_UINT: {
						strChar text CLEANUP(strCharDestroy)=uint64ToStr(args[i].value.uint);
						fprintf(codeTmpFile, "%s ", text);
						break;
				}
				case X86ADDRMODE_MEM: {
					switch(args[i].value.m.type) {
						case x86ADDR_INDIR_REG: {
							__auto_type reg=ptrMapRegNameGet(regNames, args[i].value.m.value.indirReg);
							if(args[i].valueType) {
								strChar sizeStr CLEANUP(strCharDestroy)=getSizeStr(args[i].valueType);
								if(!sizeStr) goto fail;
								fprintf(codeTmpFile, " %s PTR [%s] ",sizeStr, *reg);
							} else {
								fprintf(codeTmpFile, "[%s] ", *reg);
							}
							break;
						}
						case x86ADDR_INDIR_SIB: {
								strChar retVal CLEANUP(strCharDestroy)=NULL;
							__auto_type indexStr=ptrMapRegNameGet(regNames,args[i].value.m.value.sib.index);
							strChar scaleStr CLEANUP(strCharDestroy) =int64ToStr(args[i].value.m.value.sib.scale);
							__auto_type baseStr=ptrMapRegNameGet(regNames,args[i].value.m.value.sib.base);
							strChar offsetStr CLEANUP(strCharDestroy)=int64ToStr(args[i].value.m.value.sib.offset);
							strChar buffer CLEANUP(strCharDestroy)=NULL;
							int insertAdd=0;
							if(indexStr&&args[i].value.m.value.sib.scale) {
								retVal=strCharAppendData(retVal,*indexStr,strlen(*indexStr));
								retVal=strCharAppendItem(retVal,'*');
								retVal=strCharAppendData(retVal,scaleStr,strlen(scaleStr));
								insertAdd=1;
							} else if(indexStr) {
								retVal=strCharAppendData(retVal,*indexStr,strlen(*indexStr));
								insertAdd=1;
							}
							if(insertAdd&&baseStr)
								retVal=strCharAppendItem(retVal,'+');
							if(baseStr) {
								retVal=strCharAppendData(retVal,*baseStr,strlen(*baseStr));
							}
							if(args[i].value.m.value.sib.offset) {
								if(offsetStr[0]!='-')
									retVal=strCharAppendItem(retVal, '+');
								retVal=strCharConcat(retVal, offsetStr);
								offsetStr=NULL; //Will be free'd,but has been free'd by concat
							}
							if(args[i].valueType) {
								strChar typeStr CLEANUP(strCharDestroy)= getSizeStr(args[i].valueType);
								fprintf(codeTmpFile, "%s PTR [%s] ",typeStr,retVal);
							} else {
								fprintf(codeTmpFile, "[%s] ",retVal);
							}
							break;
						}
						case x86ADDR_MEM: {
							if(args[i].valueType) {
								strChar addrStr CLEANUP(strCharDestroy)=uint64ToStr(args[i].value.m.value.mem);
								strChar sizeStr CLEANUP(strCharDestroy)=getSizeStr(args[i].valueType);
								if(!sizeStr) goto fail;
								fprintf(codeTmpFile, "%s PTR [%s] ", sizeStr,addrStr);
							} else {
								strChar addrStr CLEANUP(strCharDestroy)=uint64ToStr(args[i].value.m.value.mem);
								fprintf(codeTmpFile, "[%s] ", addrStr);
							}
							break;
						}
						default:
								assert(0);
					}
				}
				}
		}
		if(err)
				*err=0;
		return;
	fail:
		if(err)
				*err=1;
}
void X86EmitAsmParserInst(struct parserNodeAsmInstX86 *inst) {
		struct parserNodeName *name=(void*)inst->name;
		strX86AddrMode args CLEANUP(strX86AddrModeDestroy);
		for(long i=0;i!=strParserNodeSize(inst->args);i++)
				args=strX86AddrModeAppendItem(args,parserNode2X86AddrMode(inst->args[i]));
		strOpcodeTemplate templates CLEANUP(strOpcodeTemplateDestroy)=X86OpcodesByArgs(name->text, args, NULL);
		assert(strOpcodeTemplateSize(templates)!=0);
		int err;
		X86EmitAsmInst(templates[0],args,&err);
		assert(!err);
}
char *X86EmitAsmLabel(const char *name) {
		if(!name) {
				long count=snprintf(NULL, 0, "$LBL_%li", ++labelCount);
				char buffer[count+1];
				sprintf(buffer,  "$LBL_%li", labelCount);
				fprintf(codeTmpFile, "%s:\n", buffer);
				char *retVal=malloc(count+1);
				strcpy(retVal, buffer);
				return retVal;
		}
		char *retVal=malloc(strlen(name)+1);
		strcpy(retVal, retVal);
		return retVal;
}
static strChar  unescapeString(const char *str) {
		char *otherValids="[]{}\\|;:\"\'<>?,./`~!@#$%^&*()-_+=";
		long len=strlen(str);
		strChar retVal=strCharAppendItem(NULL, '"');
		for(long i=0;i!=len;i++) {
				if(isalnum(str[i])) {
						retVal=strCharAppendItem(retVal, str[i]);
				} else if(str[i]=='\"') {
						retVal=strCharAppendItem(retVal, '\\');
						retVal=strCharAppendItem(retVal, '\"');
				} else {
						if(strchr(otherValids, str[i])) {
								retVal=strCharAppendItem(retVal, str[i]);
						} else {
								long count=snprintf(NULL, 0, "\\%02x",((uint8_t*)str)[i]);
								char buffer[count+1];
								sprintf(buffer,  "\\%02x",((uint8_t*)str)[i]);
								retVal=strCharAppendData(retVal, buffer, strlen(buffer));
						}
				}
		}
		retVal=strCharAppendItem(NULL, '"');
		return retVal;
}
struct X86AddressingMode X86EmitAsmDU64(uint64_t *data,long len) {
		long count=snprintf(NULL, 0, "$DU64_%li", ++labelCount);
		char buffer[count+1];
		sprintf(buffer,  "$DU64_%li", labelCount);
		fprintf(constsTmpFile, "%s: DQ ", buffer);
		for(long i=0;i!=len;i++) {
				if(i!=0)
						fputc(',',constsTmpFile);
				strChar text CLEANUP(strCharDestroy)=uint64ToStr(data[i]);
				fprintf(constsTmpFile, "%s", text);
		}
		struct X86AddressingMode mode;
		mode.valueType=NULL;
		mode.type=X86ADDRMODE_LABEL;
		mode.value.label=strcpy(malloc(count+1),buffer);
		return mode;
}
struct X86AddressingMode X86EmitAsmDU32(uint32_t *data,long len) {
		long count=snprintf(NULL, 0, "$DU32_%li", ++labelCount);
		char buffer[count+1];
		sprintf(buffer,  "$DU32_%li", labelCount);
		fprintf(constsTmpFile, "%s: DD ", buffer);
		for(long i=0;i!=len;i++) {
				if(i!=0)
						fputc(',',constsTmpFile);
				strChar text CLEANUP(strCharDestroy)=int64ToStr(data[i]);
				fprintf(constsTmpFile, "%s", text);
		}
		struct X86AddressingMode mode;
		mode.valueType=NULL;
		mode.type=X86ADDRMODE_LABEL;
		mode.value.label=strcpy(malloc(count+1),buffer);
		return mode;
}
struct X86AddressingMode X86EmitAsmDU16(uint16_t *data,long len) {
		long count=snprintf(NULL, 0, "$DU16_%li", ++labelCount);
		char buffer[count+1];
		sprintf(buffer,  "$DU16_%li", labelCount);
		fprintf(constsTmpFile, "%s: DW ", buffer);
		for(long i=0;i!=len;i++) {
				if(i!=0)
						fputc(',',constsTmpFile);
				strChar text CLEANUP(strCharDestroy)=int64ToStr(data[i]);
				fprintf(constsTmpFile, "%s", text);
		}
		struct X86AddressingMode mode;
		mode.valueType=NULL;
		mode.type=X86ADDRMODE_LABEL;
		mode.value.label=strcpy(malloc(count+1),buffer);
		return mode;
}
struct X86AddressingMode X86EmitAsmDU8(uint8_t *data,long len) {
		long count=snprintf(NULL, 0, "$DU8_%li", ++labelCount);
		char buffer[count+1];
		sprintf(buffer,  "$DU8_%li", labelCount);
		fprintf(constsTmpFile, "%s: DB ", buffer);
		for(long i=0;i!=len;i++) {
				if(i!=0)
						fputc(',',constsTmpFile);
				strChar text CLEANUP(strCharDestroy)=int64ToStr(data[i]);
				fprintf(constsTmpFile, "%s", text);
		}
		struct X86AddressingMode mode;
		mode.valueType=NULL;
		mode.type=X86ADDRMODE_LABEL;
		mode.value.label=strcpy(malloc(count+1),buffer);
		return mode;
}
struct X86AddressingMode X86EmitAsmStrLit(const char *text) {
		strChar unes CLEANUP(strCharDestroy)=unescapeString(text);
		long count=snprintf(NULL, 0, "$STR_%li", ++labelCount);
		char buffer[count+1];
		sprintf(buffer,  "$STR_%li", labelCount);
		fprintf(constsTmpFile, "%s: DB %s\n", buffer,unes);
		struct X86AddressingMode mode;
		mode.valueType=NULL;
		mode.type=X86ADDRMODE_LABEL;
		mode.value.label=strcpy(malloc(count+1),buffer);
		return mode;
}
void X86AddrMoreDestroy(struct X86AddressingMode *mode) {

} 
