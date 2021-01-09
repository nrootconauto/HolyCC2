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
static __thread long labelCount=0;
static __thread ptrMapLabelNames labelsByParserNode=NULL;
static __thread FILE *constsTmpFile=NULL;
static __thread FILE *symbolsTmpFile=NULL;
static __thread FILE *codeTmpFile=NULL;
static void strCharDestroy2(strChar *str) {
		strCharDestroy(str);
}
static strChar strClone(const char *text) {
		__auto_type retVal= strCharAppendData(NULL, text, strlen(text)+1);
		strcpy(retVal, text);
		return retVal;
}
void X86EmitASmInit() {
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
void X86EmitAsmInst(struct opcodeTemplate *template,strX86AddrMode args,int *success) {
		//TODO map TempleOS opcode name to gas opcode name
		fprintf(codeTmpFile, "%s ", opcodeTemplateName(template));
		for(long i=0;i!=strX86AddrModeSize(args);i++) {
				switch(args[i].type) {
				case X86ADDRMODE_FLT: {
						assert(0);
						break;
				}
				case X86ADDRMODE_ITEM_ADDR: {
						 args[i].value.itemAddr;
						break;
				}
				case X86ADDRMODE_LABEL: {
				}
				case X86ADDRMODE_REG: {
						
				}
				case X86ADDRMODE_SINT: {
				}
				case X86ADDRMODE_UINT: {
				}
				case X86ADDRMODE_MEM: {
				}
				}
		}
}
void X86EmitAsmParserInst(struct parserNodeAsmInstX86 *inst,FILE *dumpTo) {
		struct parserNodeName *name=(void*)inst->name;
		strX86AddrMode args CLEANUP(strX86AddrModeDestroy);
		for(long i=0;i!=strParserNodeSize(inst->args);i++)
				args=strX86AddrModeAppendItem(args,parserNode2X86AddrMode(inst->args[i]));
		strOpcodeTemplate templates CLEANUP(strOpcodeTemplateDestroy)=X86OpcodesByArgs(name->text, args, NULL);
		assert(strOpcodeTemplateSize(templates)!=0);
		
}
