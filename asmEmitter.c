#include <registers.h>
#include <parserA.h>
#include <opcodesParser.h>
#include <stdio.h>
#include <cleanup.h>
#include <assert.h>
#include <ptrMap.h>
#include <parserB.h>
STR_TYPE_DEF(char,Char);
STR_TYPE_FUNCS(char,Char);
PTR_MAP_FUNCS(struct parserNode *, strChar, LabelNames);
PTR_MAP_FUNCS(struct reg *, strChar, RegName);
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
}
void X86EmitAsmInit() {
		labelCount=0;
		ptrMapLabelNamesDestroy(labelsByParserNode, (void(*)(void*))strCharDestroy2);
		labelsByParserNode=ptrMapLabelNamesCreate();
		constsTmpFile=tmpfile();
		symbolsTmpFile=tmpfile();
		codeTmpFile=tmpfile();
}
static strChar parserNodeSymbolName(const struct parserNode *node) {
		if(node->type==NODE_VAR) {
				struct parserNodeVar *var=(void*)node;
				return strClone(var->var->name);
		} else if(node->type==NODE_FUNC_REF) {
				struct parserNodeFuncRef *ref=(void*)node;
				struct parserNodeName *name=(void*)ref->name;
				if(!name) {
						long size=snprintf(NULL, 0, "UNAMED_FUNC_%li", ++labelCount);
						char buffer[size+1];
						sprintf(buffer, "UNAMED_FUNC_%li", labelCount);
						return strClone(buffer);
				} else {
						//Check if function is global(one in global symbols table)
						__auto_type find=getGlobalSymbol(name->text);
						if(find) {
								if(find!=node)
										goto localFunc;
								return strClone(name->text);
						}
				localFunc:;
						long size=snprintf(NULL, 0, "LOC_FUNC_%li", ++labelCount);
						char buffer[size+1];
						sprintf(buffer, "LOC_FUNC_%li", labelCount);
						return strClone(buffer);
				}
		} else if(node->type==NODE_LABEL) {
				struct parserNodeLabel *lab=(void*)node;
				struct parserNodeName *name=(void*)lab->name;
				long size=snprintf(NULL, 0, "lbl_%s", name->text);
				char buffer[size+1];
				sprintf(buffer, "L_%s", name->text);
				return strClone(buffer);
		} else if(node->type==NODE_ASM_LABEL_GLBL) {
				struct parserNodeLabelGlbl *lab=(void*)node;
				struct parserNodeName *name=(void*)lab->name;
				long size=snprintf(NULL, 0, "%s", name->text);
				char buffer[size+1];
				sprintf(buffer, "%s", name->text);
				return strClone(buffer);
		} else if(node->type==NODE_ASM_LABEL_LOCAL) {
				struct parserNodeLabelLocal *lab=(void*)node;
				struct parserNodeName *name=(void*)lab->name;
				long size=snprintf(NULL, 0, "_lbl_%s", name->text);
				char buffer[size+1];
				sprintf(buffer, "_lbl_%s", name->text);
				return strClone(buffer);
		} else {
				//Why are you here,you should have throw an error in the parser if you couldn't resolve a (valid) symbol
				return NULL;
		}
}
static strChar int64ToStr(int64_t value) {
		strChar retVal=NULL;
		const char *digits="0123456789";
		if(value<0) {
				value*=-1;
		}
		do {
				retVal=strCharAppendItem(retVal, digits[value%10]);
				value/=10;
		} while(value!=0);
		return retVal;
}
static strChar uint64ToStr(uint64_t value) {
		strChar retVal=NULL;
		const char *digits="0123456789";
		do {
				retVal=strCharAppendItem(retVal, digits[value%10]);
				value/=10;
		} while(value!=0);
		return retVal;
}
void X86EmitAsmInst(struct opcodeTemplate *template,strX86AddrMode args,int *err) {
		//TODO map TempleOS opcode name to gas opcode name
		fprintf(codeTmpFile, "%s ", opcodeTemplateName(template));
		for(long i=0;i!=strX86AddrModeSize(args);i++) {
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
						args[i].value.sint;
				}
				case X86ADDRMODE_UINT: {
				}
				case X86ADDRMODE_MEM: {
				}
				}
		}
	fail:
		if(success)
				*success=0;
}
void X86EmitAsmParserInst(struct parserNodeAsmInstX86 *inst,FILE *dumpTo) {
		struct parserNodeName *name=(void*)inst->name;
		strX86AddrMode args CLEANUP(strX86AddrModeDestroy);
		for(long i=0;i!=strParserNodeSize(inst->args);i++)
				args=strX86AddrModeAppendItem(args,parserNode2X86AddrMode(inst->args[i]));
		strOpcodeTemplate templates CLEANUP(strOpcodeTemplateDestroy)=X86OpcodesByArgs(name->text, args, NULL);
		assert(strOpcodeTemplateSize(templates)!=0);
		
}
