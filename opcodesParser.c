#include <stdint.h>
#include <str.h>
#include <hashTable.h>
#include <registers.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <assert.h>
#include <cleanup.h>
STR_TYPE_DEF(uint8_t,OpcodeBytes);
STR_TYPE_FUNCS(uint8_t,OpcodeBytes);
struct opcodeTemplateArg {
		enum {
				OPC_TEMPLATE_ARG_REG,
				OPC_TEMPLATE_ARG_SREG, //Segment register
				OPC_TEMPLATE_ARG_SINT8,
				OPC_TEMPLATE_ARG_UINT8,
				OPC_TEMPLATE_ARG_SINT16,
				OPC_TEMPLATE_ARG_UINT16,
				OPC_TEMPLATE_ARG_SINT32,
				OPC_TEMPLATE_ARG_UINT32,
				OPC_TEMPLATE_ARG_SINT64,
				OPC_TEMPLATE_ARG_UINT64,
				OPC_TEMPLATE_ARG_RM8,
				OPC_TEMPLATE_ARG_RM16,
				OPC_TEMPLATE_ARG_RM32,
				OPC_TEMPLATE_ARG_RM64,
				OPC_TEMPLATE_ARG_R8,
				OPC_TEMPLATE_ARG_R16,
				OPC_TEMPLATE_ARG_R32,
				OPC_TEMPLATE_ARG_R64,
		} type;
		union {
				uint64_t uint;
				int64_t sint;
				struct reg *reg;
		} value;
};
STR_TYPE_DEF(struct opcodeTemplateArg,OpcodeTemplateArg);
STR_TYPE_FUNCS(struct opcodeTemplateArg,OpcodeTemplateArg);
struct opcodeTemplate {
		strOpcodeBytes bytes;
		strOpcodeTemplateArg args;
		char *name;
		unsigned int needsREX:1;
		unsigned int usesSTI:1;
		unsigned int notIn64mode:1;
		unsigned int addRegNum:1;
		unsigned int modRMReg:1;
		unsigned int modRMExt:3;
};
MAP_TYPE_DEF(struct opcodeTemplate,OpcodeTemplate);
MAP_TYPE_FUNCS(struct opcodeTemplate,OpcodeTemplate);
#define OPCODES_FILE "OpCodes.txt"
STR_TYPE_DEF(char, Char);
STR_TYPE_FUNCS(char, Char);
static int lexInt(strChar text,long *Pos,long *retVal) {
		long pos=*Pos;
		long hex=strncmp(text+pos, "0x",2);
		if(hex) {
				long originalPos=pos;
				while(isxdigit(text+2+pos))
						pos++;
				char buffer[pos-originalPos+1];
				buffer[pos-originalPos]='\0';
				strncpy(buffer, text+originalPos,pos-originalPos);
				sscanf(buffer, "%li", retVal);
				*Pos=pos;
				return 1;
		}
		long dec=isdigit(text[pos]);
		if(dec) {
				long originalPos=pos;
				while(isdigit(text+2+pos))
						pos++;
				char buffer[pos-originalPos+1];
				buffer[pos-originalPos]='\0';
				strncpy(buffer, text+originalPos,pos-originalPos);
				sscanf(buffer, "%li", retVal);
				*Pos=pos;
				return 1;
		}
		return 0;
}
static int lexName(strChar text,long *Pos,strChar *retVal) {
		long pos=*Pos;
		if(!isalpha(text[pos]))
				return 0;
		*retVal=NULL;
		while(isalnum(text[pos]))
				*retVal=strCharAppendItem(*retVal, text[pos++]);
		*Pos=pos;
		return 1;
}
static long skipWhitespace(strChar text,long pos) {
	start:
		while(isblank(text[pos])) {
				if(text[pos]=='\0')
						return pos;
				pos++;
		}
		if(0==strncmp(text+pos, "/*", 2)) {
				__auto_type end=strstr(text+pos, "*/");
				pos= end-text;
				goto start;
		}
		if(0==strncmp(text+pos, "//", 2)) {
				__auto_type end=strstr(text+pos,"\n");
				pos= end-text;
				goto start;
		}
		return pos;
}
static int strcmp2(const void *a,const void *b) {
		return strcmp(*(const char**)a, *(const char**)b);
}
static int lexKeyword(strChar text,long *Pos,const char **keywords,long kwCount,const char **result) {
		long pos=*Pos;
		for(long i=0;i!=kwCount;i++) {
				long res=strcmp(keywords[i], text+pos);
				if(res==0) {
						*result=keywords[i];
						*Pos=pos;
						return 1;
				} else if(res>0)  {
						break;
				}
		}
		return 0;
}
struct lexerItem {
		enum {
				LEXER_INT,
				LEXER_KW,
				LEXER_WORD
		} type;
		union {
				long Int;
				strChar name;
				const char *kw;
		} value;
};
static void lexerItemDestroy(struct lexerItem **item) {
		if(item[0]->type==LEXER_WORD)
				strCharDestroy(&item[0]->value.name);
		free(item[0]);
}
static struct lexerItem *lexItem(strChar text,long *pos,const char **keywords,long kwCount) {
		struct lexerItem *item=malloc(sizeof(struct lexerItem));
		if(lexKeyword(text, pos, keywords, kwCount, &item->value.kw)) {
				item->type=LEXER_KW;
				return item;
		} else if(lexName(text, pos, &item->value.name)) {
				item->type=LEXER_WORD;
				return item;
		} else if(lexInt(text, pos,&item->value.Int)) {
				item->type=LEXER_INT;
				return item;
		}

		free(item);
		return NULL;
}
static mapOpcodeTemplate opcodes;
void parseOpcodeFile() {
		const char *keywords[]={
				"!", //?
				"&", //Defualt
				"%", //32bit only
				"=", //Requires REX if 64bit mode,
				"`", //REX if R8 or R15,
				"^", //?
				"*", //ST(i) like
				"$", //?
				",",
				"+R",
				"+I",
				"/R",
				"/0",
				"/1",
				"/2",
				"/3",
				"/4",
				"/5",
				"/6",
				"/7",
				"IB",
				"IW",
				"ID",
				"RM8",
				"RM16",
				"RM32",
				"RM64",
				"R8",
				"R16",
				"R32",
				"R64",
				"IMM8",
				"IMM16",
				"IMM32",
				"IMM64",
				"UIMM8",
				"UIMM16",
				"UIMM32",
				"UIMM64",
				"OPCODE",
				"SREG", //Segment register
				";",
				":",
				"OPCODE",
		};
		qsort(keywords, sizeof(keywords)/sizeof(*keywords), sizeof(*keywords), strcmp2);
		
		FILE *file=fopen(OPCODES_FILE, "r");
		fseek(file, 0, SEEK_END);
		long end=ftell(file);
		fseek(file, 0, SEEK_SET);
		long start=ftell(file);
		strChar text=strCharResize(NULL,end-start+1);
		fread(text, end, end-start,file);

		long pos=0;
		long kwCount=sizeof(keywords)/sizeof(*keywords);
		for(;;) {
		findOpcode:
				pos=skipWhitespace(text, pos);
				if(text[pos]=='\0')
						break;
				struct lexerItem *item CLEANUP(lexerItemDestroy)=lexItem(text, &pos, keywords, kwCount);
				assert(item);
				if(item->type==LEXER_KW) {
						if(0==strcmp(item->value.kw,"OPCODE")) {
								struct lexerItem *name CLEANUP(lexerItemDestroy)=lexItem(text, &pos, keywords, kwCount);
								assert(name->type==LEXER_WORD);
								for(;;) {
											if(item->type==LEXER_KW)
													if(0==strcmp(item->value.kw,";"))
															break;
											struct opcodeTemplate template;
											template.name=malloc(strlen(name->value.name)+1);
											strcpy(template.name,name->value.name);
											template.bytes=NULL;
											template.args=NULL;
											for(;;) {
													long oldPos=pos;
													struct lexerItem *byte CLEANUP(lexerItemDestroy)=lexItem(text, &pos, keywords, kwCount);
													if(byte->type!=LEXER_INT) {
															pos=oldPos;
															break;
													}
													template.bytes=strOpcodeBytesAppendItem(template.bytes, byte->value.Int);
													if(item->type==LEXER_KW) {
															if(0==strcmp(item->value.kw,",")) {
																	goto commaPassed;
															}
													}
													fprintf(stderr, "EXPECTED COMMA IN " OPCODES_FILE "\n");
															commaPassed:;
											}
											//Look for arg keywords or flags
											for(;;) {
													long oldPos=pos;
													struct lexerItem *item CLEANUP(lexerItemDestroy)=lexItem(text, &pos, keywords, kwCount);
													if(item->type==LEXER_INT) {
															pos=oldPos;
															goto findOpcode;
													} else if(item->type==LEXER_KW) {
															if(0==strcmp(item->value.kw, ";")) {
																	break;
															} else if(0==strcmp(item->value.kw, "+R")) {
																	assert(!template.addRegNum);
																	template.addRegNum=1;
															}else if(0==strcmp(item->value.kw, "+I")) {
																	assert(!template.addRegNum);
																	template.addRegNum=1;
															} else if(0==strcmp(item->value.kw, "/R")) {
																	assert(!template.modRMReg);
																	template.modRMReg=1;
															} else if(0==strcmp(item->value.kw, "/0")
																									||0==strcmp(item->value.kw, "/1")
																									||0==strcmp(item->value.kw, "/2")
																									||0==strcmp(item->value.kw, "/3")
																									||0==strcmp(item->value.kw, "/4")
																									||0==strcmp(item->value.kw, "/5")
																									||0==strcmp(item->value.kw, "/6")
																									||0==strcmp(item->value.kw, "/7")) {
																	template.modRMExt=item->value.kw[1]-'0';
															} else if(0==strcmp(item->value.kw, "IB")
																									||0==strcmp(item->value.kw, "IW")
																									||0==strcmp(item->value.kw, "ID")) {
																	//Immediate literals set immediate type
															} else if(0==strcmp(item->value.kw, "RM8")) {
																	struct opcodeTemplateArg arg;
																	arg.type=OPC_TEMPLATE_ARG_RM8;
																	template.args=strOpcodeTemplateArgAppendItem(NULL, arg);
															} else if(0==strcmp(item->value.kw, "RM16")) {
																	struct opcodeTemplateArg arg;
																	arg.type=OPC_TEMPLATE_ARG_RM16;
																	template.args=strOpcodeTemplateArgAppendItem(NULL, arg);
															} else if(0==strcmp(item->value.kw, "RM32")) {
																	struct opcodeTemplateArg arg;
																	arg.type=OPC_TEMPLATE_ARG_RM32;
																	template.args=strOpcodeTemplateArgAppendItem(NULL, arg);
															} else if(0==strcmp(item->value.kw, "RM64")) {
																	struct opcodeTemplateArg arg;
																	arg.type=OPC_TEMPLATE_ARG_RM64;
																	template.args=strOpcodeTemplateArgAppendItem(NULL, arg);
															} else if(0==strcmp(item->value.kw, "R8")) {
																	struct opcodeTemplateArg arg;
																	arg.type=OPC_TEMPLATE_ARG_R8;
																	template.args=strOpcodeTemplateArgAppendItem(NULL, arg);
															}else if(0==strcmp(item->value.kw, "R16")) {
																	struct opcodeTemplateArg arg;
																	arg.type=OPC_TEMPLATE_ARG_R16;
																	template.args=strOpcodeTemplateArgAppendItem(NULL, arg);
															}else if(0==strcmp(item->value.kw, "R32")) {
																	struct opcodeTemplateArg arg;
																	arg.type=OPC_TEMPLATE_ARG_R32;
																	template.args=strOpcodeTemplateArgAppendItem(NULL, arg);
															} else if(0==strcmp(item->value.kw, "R64")) {
																	struct opcodeTemplateArg arg;
																	arg.type=OPC_TEMPLATE_ARG_R64;
																	template.args=strOpcodeTemplateArgAppendItem(NULL, arg);
															} else if(0==strcmp(item->value.kw, "IMM8")) {
																	struct opcodeTemplateArg arg;
																	arg.type=OPC_TEMPLATE_ARG_SINT8;
																	template.args=strOpcodeTemplateArgAppendItem(NULL, arg);
															} else if(0==strcmp(item->value.kw, "IMM16")) {
																	struct opcodeTemplateArg arg;
																	arg.type=OPC_TEMPLATE_ARG_SINT16;
																	template.args=strOpcodeTemplateArgAppendItem(NULL, arg);
															} else if(0==strcmp(item->value.kw, "IMM32")) {
																	struct opcodeTemplateArg arg;
																	arg.type=OPC_TEMPLATE_ARG_SINT32;
																	template.args=strOpcodeTemplateArgAppendItem(NULL, arg);
															} else if(0==strcmp(item->value.kw, "IMM64")) {
																	struct opcodeTemplateArg arg;
																	arg.type=OPC_TEMPLATE_ARG_SINT64;
																	template.args=strOpcodeTemplateArgAppendItem(NULL, arg);
															} else if(0==strcmp(item->value.kw, "UIMM8")) {
																	struct opcodeTemplateArg arg;
																	arg.type=OPC_TEMPLATE_ARG_UINT8;
																	template.args=strOpcodeTemplateArgAppendItem(NULL, arg);
															} else if(0==strcmp(item->value.kw, "UIMM16")) {
																	struct opcodeTemplateArg arg;
																	arg.type=OPC_TEMPLATE_ARG_UINT16;
																	template.args=strOpcodeTemplateArgAppendItem(NULL, arg);
															} else if(0==strcmp(item->value.kw, "UIMM32")) {
																	struct opcodeTemplateArg arg;
																	arg.type=OPC_TEMPLATE_ARG_UINT32;
																	template.args=strOpcodeTemplateArgAppendItem(NULL, arg);
															} else if(0==strcmp(item->value.kw, "UIMM64")) {
																	struct opcodeTemplateArg arg;
																	arg.type=OPC_TEMPLATE_ARG_UINT64;
																	template.args=strOpcodeTemplateArgAppendItem(NULL, arg);
															} else if(0==strcmp(item->value.kw, "SREG")) {
																	struct opcodeTemplateArg arg;
																	arg.type=OPC_TEMPLATE_ARG_SREG;
																	template.args=strOpcodeTemplateArgAppendItem(NULL, arg);
															} else if(0==strcmp(item->value.kw, "!")) {
															} else if(0==strcmp(item->value.kw, "&")) {
															} else if(0==strcmp(item->value.kw, "%")) {
															} else if(0==strcmp(item->value.kw, "=")) {
																	assert(!template.needsREX);
																	template.needsREX=1;
															} else if(0==strcmp(item->value.kw, "^")) {
																	
															} else if(0==strcmp(item->value.kw, "*")) {
																	//+I is used to detirmine if we need to add ST(i)
															} else if(0==strcmp(item->value.kw, "$")) {
															}
													} else if(item->type==LEXER_WORD) {
															// Check for register
													} else {
															assert(0);
													}
											}
								}
						}
				}
		}
}
