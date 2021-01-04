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
				OPC_TEMPLATE_ARG_MOFFS8,
				OPC_TEMPLATE_ARG_MOFFS16,
				OPC_TEMPLATE_ARG_MOFFS32,
				OPC_TEMPLATE_ARG_REL8,
				OPC_TEMPLATE_ARG_REL16,
				OPC_TEMPLATE_ARG_REL32,
				OPC_TEMPLATE_ARG_M8,
				OPC_TEMPLATE_ARG_M16,
				OPC_TEMPLATE_ARG_M32,
				OPC_TEMPLATE_ARG_M64,
				OPC_TEMPLATE_ARG_STI,
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
STR_TYPE_DEF(struct opcodeTemplate,OpcodeTemplate);
STR_TYPE_FUNCS(struct opcodeTemplate,OpcodeTemplate);
MAP_TYPE_DEF(strOpcodeTemplate,OpcodeTemplates);
MAP_TYPE_FUNCS(strOpcodeTemplate,OpcodeTemplates);
#define OPCODES_FILE "/home/tc/projects/holycc2/OpCodes.txt"
STR_TYPE_DEF(char, Char);
STR_TYPE_FUNCS(char, Char);
static int lexInt(strChar text,long *Pos,long *retVal) {
		long pos=*Pos;
		long hex=0==strncmp(text+pos, "0x",2);
		if(hex) {
				long originalPos=pos;
				pos+=2;
				while(isxdigit(text[pos]))
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
				while(isdigit(text[pos]))
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
		if(!isalpha(text[pos])&&text[pos]!='_')
				return 0;
		*retVal=NULL;
		while(isalnum(text[pos])||text[pos]=='_')
				*retVal=strCharAppendItem(*retVal, text[pos++]);
		*retVal=strCharAppendItem(*retVal, '\0');
		*Pos=pos;
		return 1;
}
static long skipWhitespace(strChar text,long pos) {
	start:
		while(isblank(text[pos])||text[pos]=='\n'||text[pos]=='\0') {
				if(text[pos]=='\0')
						return pos;
				pos++;
		}
		if(0==strncmp(text+pos, "/*", 2)) {
				__auto_type end=strstr(text+pos, "*/");
				pos= end-text+2;
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
		for(long i=kwCount-1;i>=0;i--) {
				long res=strncmp(keywords[i], text+pos,strlen(keywords[i]));
				if(res==0) {
						//Check if all alnums,if so,ensure there is no alnum charactor after find
						int isAllAlnum=1;
						for(long i2=0;i2!=strlen(keywords[i]);i2++) {
								if(!isalnum(keywords[i][i2])) {
										isAllAlnum=0;
										break;
								}
						}
						if(isAllAlnum)
								if(isalnum(text[pos+strlen(keywords[i])]))
										continue;
						*result=keywords[i];
						*Pos=pos+strlen(keywords[i]);
						return 1;
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
		*pos=skipWhitespace(text, *pos);
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
MAP_TYPE_DEF(struct reg*, Reg);
MAP_TYPE_FUNCS(struct reg*, Reg);
static mapOpcodeTemplates opcodes;
static struct opcodeTemplate opcodeTemplateClone(struct opcodeTemplate from) {
		struct opcodeTemplate retVal=from;
		retVal.bytes=strOpcodeBytesClone(from.bytes);
		retVal.args=strOpcodeTemplateArgClone(from.args);
		retVal.name=malloc(strlen(from.name)+1);
		strcpy(retVal.name, from.name);
		return retVal;
}
static strOpcodeTemplate strOpcodeTemplateClone2(strOpcodeTemplate from) {
		__auto_type retVal=strOpcodeTemplateClone(from);
		for(long i=0;i!=strOpcodeTemplateSize(from);i++)
				retVal[i]=opcodeTemplateClone(from[i]);
		return retVal;
}
static void opcodeTemplateDestroy(struct opcodeTemplate *template) {
		strOpcodeBytesDestroy(&template->bytes);
		free(template->name);
		strOpcodeTemplateArgDestroy(&template->args);
}
static void strOpcodeTemplateDestroy2(strOpcodeTemplate *str) {
		for(long i=0;i!=strOpcodeTemplateSize(*str);i++)
				opcodeTemplateDestroy(&str[0][i]);
}
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
				"16",
				"32",
				"64",
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
				"MOFFS8",
				"MOFFS16",
				"MOFFS32",
				"CB",
				"CW",
				"CD",
				"REL8",
				"REL16",
				"REL32",
				"M8",
				"M16",
				"M32",
				"M64",
				"STI",
		};
		qsort(keywords, sizeof(keywords)/sizeof(*keywords), sizeof(*keywords), strcmp2);
		
		FILE *file=fopen(OPCODES_FILE, "r");
		fseek(file, 0, SEEK_END);
		long end=ftell(file);
		fseek(file, 0, SEEK_SET);
		long start=ftell(file);
		strChar text=strCharResize(NULL,end-start+1);
		fread(text, end, end-start,file);

		opcodes=mapOpcodeTemplatesCreate();
		long pos=0;
		long kwCount=sizeof(keywords)/sizeof(*keywords);
		setArch(ARCH_X64_SYSV);
		strRegP allRegs CLEANUP(strRegPDestroy)=regsForArch();
		mapReg regsByName=mapRegCreate();
		for(long i=0;i!=strRegPSize(allRegs);i++)
				mapRegInsert(regsByName, allRegs[i]->name, allRegs[i]);
		for(;;) {
		findTemplate:
				pos=skipWhitespace(text, pos);
				if(text[pos]=='\0')
						break;
				struct lexerItem *item CLEANUP(lexerItemDestroy)=lexItem(text, &pos, keywords, kwCount);
				assert(item);
				if(item->type==LEXER_KW) {
						if(0==strcmp(item->value.kw,"OPCODE")) {
								struct lexerItem *name CLEANUP(lexerItemDestroy)=malloc(sizeof(struct lexerItem));
								name->type=LEXER_WORD;
								pos=skipWhitespace(text, pos);
								lexName(text, &pos, &name->value.name);

								strOpcodeTemplate templates=NULL;
								for(;;) {
										if(item->type==LEXER_KW) {
												struct opcodeTemplate template;
												memset(&template, 0,sizeof(template));
												template.name=malloc(strlen(name->value.name)+1);
												strcpy(template.name,name->value.name);
												template.bytes=NULL;
												template.args=NULL;
												for(;;) {
														long oldPos=pos;
														struct lexerItem *item CLEANUP(lexerItemDestroy)=lexItem(text, &pos, keywords, kwCount);
														if(item->type==LEXER_INT)
																template.bytes=strOpcodeBytesAppendItem(template.bytes, item->value.Int);
														else if(item->type==LEXER_KW) {
																if(0==strcmp(item->value.kw, ";")) {
																		templates=strOpcodeTemplateAppendItem(templates, template);
																		goto registerTemplates;
																} else if(0==strcmp(item->value.kw,",")) {
																		goto commaPassed;
																} else assert(0);
														} else assert(0);
														continue;
												commaPassed:;
														break;
												}
												//Look for arg keywords or flags
												for(;;) {
														long oldPos=pos;
														struct lexerItem *item CLEANUP(lexerItemDestroy)=lexItem(text, &pos, keywords, kwCount);
#define ARG_BY_NAME(arg,name) else if(0==strcmp(item->value.kw, name)) { \
																		struct opcodeTemplateArg __arg; \
																		__arg.type=arg; \
																		template.args=strOpcodeTemplateArgAppendItem(template.args, __arg); \
														} 
														if(item->type==LEXER_INT) {
																pos=oldPos;
																goto opcodeFinish;
														} else if(item->type==LEXER_KW) {
																if(0==strcmp(item->value.kw, ";")) {
																		goto registerTemplates;
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
																}else if(0==strcmp(item->value.kw, "CB")
																								||0==strcmp(item->value.kw, "CW")
																								||0==strcmp(item->value.kw, "CD")) {
																}
																ARG_BY_NAME(OPC_TEMPLATE_ARG_RM16, "RM8")
																		ARG_BY_NAME(OPC_TEMPLATE_ARG_RM16, "RM16")
																		ARG_BY_NAME(OPC_TEMPLATE_ARG_RM32, "RM32")
																		ARG_BY_NAME(OPC_TEMPLATE_ARG_RM64, "RM64")
																		ARG_BY_NAME(OPC_TEMPLATE_ARG_REL8, "REL8")
																		ARG_BY_NAME(OPC_TEMPLATE_ARG_REL16, "REL32")
																		ARG_BY_NAME(OPC_TEMPLATE_ARG_REL32, "REL16")
																		ARG_BY_NAME(OPC_TEMPLATE_ARG_R8, "R8")
																		ARG_BY_NAME(OPC_TEMPLATE_ARG_R16, "R16")
																		ARG_BY_NAME(OPC_TEMPLATE_ARG_R32, "R32")
																		ARG_BY_NAME(OPC_TEMPLATE_ARG_R64, "R64")
																		ARG_BY_NAME(OPC_TEMPLATE_ARG_SINT8, "IMM8")
																		ARG_BY_NAME(OPC_TEMPLATE_ARG_SINT16, "IMM16")
																		ARG_BY_NAME(OPC_TEMPLATE_ARG_SINT32, "IMM32")
																		ARG_BY_NAME(OPC_TEMPLATE_ARG_SINT64, "IMM64")
																		ARG_BY_NAME(OPC_TEMPLATE_ARG_UINT8, "UIMM8")
																		ARG_BY_NAME(OPC_TEMPLATE_ARG_UINT16, "UIMM16")
																		ARG_BY_NAME(OPC_TEMPLATE_ARG_UINT32, "UIMM32")
																		ARG_BY_NAME(OPC_TEMPLATE_ARG_UINT64, "UIMM64")
																		ARG_BY_NAME(OPC_TEMPLATE_ARG_M8, "M8")
																		ARG_BY_NAME(OPC_TEMPLATE_ARG_M16, "M16")
																		ARG_BY_NAME(OPC_TEMPLATE_ARG_M32, "M32")
																		ARG_BY_NAME(OPC_TEMPLATE_ARG_M64, "M64")
																		ARG_BY_NAME(OPC_TEMPLATE_ARG_MOFFS32, "MOFFS32")
																		ARG_BY_NAME(OPC_TEMPLATE_ARG_MOFFS16, "MOFFS16")
																		ARG_BY_NAME(OPC_TEMPLATE_ARG_MOFFS8, "MOFFS8")
																		ARG_BY_NAME(OPC_TEMPLATE_ARG_SREG, "SREG")
																		ARG_BY_NAME(OPC_TEMPLATE_ARG_STI, "STI")
																else if(0==strcmp(item->value.kw, "!")) {
																} else if(0==strcmp(item->value.kw, "&")) {
																} else if(0==strcmp(item->value.kw, "%")) {
																} else if(0==strcmp(item->value.kw, "=")) {
																		assert(!template.needsREX);
																		template.needsREX=1;
																} else if(0==strcmp(item->value.kw, "^")) {
																	
																} else if(0==strcmp(item->value.kw, "*")) {
																		//+I is used to detirmine if we need to add ST(i)
																} else if(0==strcmp(item->value.kw, "$")) {
																} else 	if(0==strcmp(item->value.kw,":")) {
																		//Aliases
																		for(;;) {
																				struct lexerItem *next CLEANUP(lexerItemDestroy)=lexItem(text, &pos, keywords, kwCount);
																				if(next->type==LEXER_KW)
																						if(0==strcmp(next->value.kw,";"))
																								goto registerTemplates;
																				//Insert clone into map
																				assert(next->type==LEXER_WORD);
																				mapOpcodeTemplatesInsert(opcodes, next->value.name, strOpcodeTemplateClone2(templates));
																		}
																}
														} else if(item->type==LEXER_WORD) {
																// Check for register
																__auto_type find=mapRegGet(regsByName, item->value.name);
																assert(find);
																struct opcodeTemplateArg arg;
																arg.type=OPC_TEMPLATE_ARG_REG;
																arg.value.reg=*find;
																template.args=strOpcodeTemplateArgAppendItem(template.args, arg);
														} else {
																assert(0);
														}
												}
										opcodeFinish:;
												templates=strOpcodeTemplateAppendItem(templates, template);
										}
								}
						registerTemplates:
										mapOpcodeTemplatesInsert(opcodes, item->value.name, templates);
						}
				}
		}
		mapRegDestroy(regsByName, NULL);
}
