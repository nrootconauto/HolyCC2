#include "X86AsmSharedVars.h"
#include <assert.h>
#include "cleanup.h"
#include <ctype.h>
#include "hashTable.h"
#include "opcodesParser.h"
#include "parserA.h"
#include "parserB.h"
#include "registers.h"
#include <stdio.h>
#include <stdlib.h>
#include "str.h"
#include "hcrtLocation.h"
#define ALLOCATE(item)																																																		\
	({                                                                                                                                                               \
			typeof(item) *ptr = calloc(sizeof(item),1);																											\
		*ptr = item;                                                                                                                                                   \
		ptr;                                                                                                                                                           \
	})
const char *opcodeTemplateName(struct opcodeTemplate *template) {
	return template->name;
};
MAP_TYPE_DEF(strOpcodeTemplate, OpcodeTemplates);
MAP_TYPE_FUNCS(strOpcodeTemplate, OpcodeTemplates);
STR_TYPE_DEF(char, Char);
STR_TYPE_FUNCS(char, Char);
static int lexInt(strChar text, long *Pos, long *retVal) {
	long pos = *Pos;
	long hex = 0 == strncmp(text + pos, "0x", 2);
	if (hex) {
		long originalPos = pos;
		pos += 2;
		while (isxdigit(text[pos]))
			pos++;
		char buffer[pos - originalPos + 1];
		buffer[pos - originalPos] = '\0';
		strncpy(buffer, text + originalPos, pos - originalPos);
		sscanf(buffer, "%li", retVal);
		*Pos = pos;
		return 1;
	}
	long dec = isdigit(text[pos]);
	if (dec) {
		long originalPos = pos;
		while (isdigit(text[pos]))
			pos++;
		char buffer[pos - originalPos + 1];
		buffer[pos - originalPos] = '\0';
		strncpy(buffer, text + originalPos, pos - originalPos);
		sscanf(buffer, "%li", retVal);
		*Pos = pos;
		return 1;
	}
	return 0;
}
static int lexStr(strChar text, long *Pos, strChar *retVal) {
	long pos = *Pos;
	if (text[pos] != '\'' && text[pos] != '\"')
		return 0;
	*retVal = NULL;
	char *end = strchr(text + pos + 1, text[pos]);
	*retVal = strCharAppendData(*retVal, text + pos + 1, end - &text[pos + 1]);
	*retVal = strCharAppendItem(*retVal, '\0');
	*Pos = end - text + 1;
	return 1;
}
static int lexName(strChar text, long *Pos, strChar *retVal) {
	long pos = *Pos;
	if (!isalpha(text[pos]) && text[pos] != '_')
		return 0;
	*retVal = NULL;
	while (isalnum(text[pos]) || text[pos] == '_')
		*retVal = strCharAppendItem(*retVal, text[pos++]);
	*retVal = strCharAppendItem(*retVal, '\0');
	*Pos = pos;
	return 1;
}
static long skipWhitespace(strChar text, long pos) {
start:
	while (isblank(text[pos]) || text[pos] == '\n' || text[pos] == '\0') {
		if (text[pos] == '\0')
			return pos;
		pos++;
	}
	if (0 == strncmp(text + pos, "/*", 2)) {
		__auto_type end = strstr(text + pos, "*/");
		pos = end - text + 2;
		goto start;
	}
	if (0 == strncmp(text + pos, "//", 2)) {
		__auto_type end = strstr(text + pos, "\n");
		pos = end - text;
		goto start;
	}
	return pos;
}
static int strcmp2(const void *a, const void *b) {
	return strcmp(*(const char **)a, *(const char **)b);
}
static int lexKeyword(strChar text, long *Pos, const char **keywords, long kwCount, const char **result) {
	long pos = *Pos;
	for (long i = kwCount - 1; i >= 0; i--) {
		long res = strncmp(keywords[i], text + pos, strlen(keywords[i]));
		if (res == 0) {
			// Check if all alnums,if so,ensure there is no alnum charactor after find
			int isAllAlnum = 1;
			for (long i2 = 0; i2 != strlen(keywords[i]); i2++) {
				if (!isalnum(keywords[i][i2])) {
					isAllAlnum = 0;
					break;
				}
			}
			if (isAllAlnum)
				if (isalnum(text[pos + strlen(keywords[i])]))
					continue;
			*result = keywords[i];
			*Pos = pos + strlen(keywords[i]);
			return 1;
		}
	}
	return 0;
}
struct opLexerItem {
	enum {
		OP_LEXER_INT,
		OP_LEXER_KW,
		OP_LEXER_WORD,
		OP_LEXER_STR,
	} type;
	union {
		long Int;
		strChar name;
		strChar str;
		const char *kw;
	} value;
};
static void opLexerItemDestroy(struct opLexerItem **item) {
	if (!*item)
		return;
	if (item[0]->type == OP_LEXER_WORD)
		strCharDestroy(&item[0]->value.name);
	if (item[0]->type == OP_LEXER_STR)
		strCharDestroy(&item[0]->value.str);
	free(item[0]);
}
static struct opLexerItem *lexItem(strChar text, long *pos, const char **keywords, long kwCount) {
	*pos = skipWhitespace(text, *pos);
	struct opLexerItem *item = calloc(sizeof(struct opLexerItem),1);
	if (lexStr(text, pos, &item->value.str)) {
		item->type = OP_LEXER_STR;
		return item;
	} else if (lexKeyword(text, pos, keywords, kwCount, &item->value.kw)) {
		item->type = OP_LEXER_KW;
		return item;
	} else if (lexName(text, pos, &item->value.name)) {
		item->type = OP_LEXER_WORD;
		return item;
	} else if (lexInt(text, pos, &item->value.Int)) {
		item->type = OP_LEXER_INT;
		return item;
	}

	free(item);
	return NULL;
}
MAP_TYPE_DEF(struct reg *, Reg);
MAP_TYPE_FUNCS(struct reg *, Reg);
static mapOpcodeTemplates opcodes;
static struct opcodeTemplate opcodeTemplateClone(struct opcodeTemplate from) {
	struct opcodeTemplate retVal = from;
	retVal.bytes = strOpcodeBytesClone(from.bytes);
	retVal.args = strOpcodeTemplateArgClone(from.args);
	retVal.name = calloc(strlen(from.name) + 1,1);
	strcpy(retVal.name, from.name);
	if (from.intelAlias) {
			retVal.intelAlias = calloc(strlen(from.intelAlias) + 1,1);
		strcpy(retVal.intelAlias, from.intelAlias);
	} else
		retVal.intelAlias = NULL;
	return retVal;
}
static strOpcodeTemplate strOpcodeTemplateClone2(strOpcodeTemplate from) {
	__auto_type retVal = strOpcodeTemplateClone(from);
	for (long i = 0; i != strOpcodeTemplateSize(from); i++)
		retVal[i] = ALLOCATE(opcodeTemplateClone(*from[i]));
	return retVal;
}
static void opcodeTemplateDestroy(struct opcodeTemplate *template) {
	strOpcodeBytesDestroy(&template->bytes);
	free(template->name);
	strOpcodeTemplateArgDestroy(&template->args);
}
static void strOpcodeTemplateDestroy2(strOpcodeTemplate *str) {
	for (long i = 0; i != strOpcodeTemplateSize(*str); i++) {
		opcodeTemplateDestroy(str[0][i]);
		free(str[0]);
	}
}
const char *opcodeTemplateIntelAlias(const struct opcodeTemplate *template) {
	return template->intelAlias;
}
void parseOpcodeFile() {
	const char *keywords[] = {
	    "!",  //?
	    "&",  // Defualt
	    "%",  // 32bit only
	    "=",  // Requires REX if 64bit mode,
	    "`",  // REX if R8 or R15,
	    "^",  //?
	    "*",  // ST(i) like
	    "$",  //?
					"@",  // Modification I made,Makes it so argument is changed by the opcode
	    "#",  // Modification I made,specifies intel name
	    "##", // Modification I made,specifies alias(followed by string)
	    ",",      "+R",   "+I",     "/R",     "/0",      "/1",      "/2", "/3", "/4", "/5",   "/6",    "/7",    "IB",    "IW",    "ID",     "RM8",    "RM16",
	    "RM32",   "RM64", "R8",     "R16",    "R32",     "R64",     "16", "32", "64", "IMM8", "IMM16", "IMM32", "IMM64", "UIMM8", "UIMM16", "UIMM32", "UIMM64",
	    "OPCODE",
	    "SREG", // Segment register
	    ";",      ":",    "OPCODE", "MOFFS8", "MOFFS16", "MOFFS32", "CB", "CW", "CD", "REL8", "REL16", "REL32", "M8",    "M16",   "M32",    "M64",    "STI", "XMM"
	};
	qsort(keywords, sizeof(keywords) / sizeof(*keywords), sizeof(*keywords), strcmp2);

	char *opcodesFile=HCRTFile("OpCodes.txt");
	FILE *file = fopen(opcodesFile, "r");
	free(opcodesFile);
	fseek(file, 0, SEEK_END);
	long end = ftell(file);
	fseek(file, 0, SEEK_SET);
	long start = ftell(file);
	strChar text = strCharResize(NULL, end - start + 1);
	fread(text, end, end - start, file);

	opcodes = mapOpcodeTemplatesCreate();
	long pos = 0;
	long kwCount = sizeof(keywords) / sizeof(*keywords);
	setArch(ARCH_X64_SYSV);
	strRegP allRegs CLEANUP(strRegPDestroy) = regsForArch();
	mapReg regsByName = mapRegCreate();
	for (long i = 0; i != strRegPSize(allRegs); i++)
		mapRegInsert(regsByName, allRegs[i]->name, allRegs[i]);
	for (;;) {
	findTemplate:
		pos = skipWhitespace(text, pos);
		if (text[pos] == '\0')
			break;
		struct opLexerItem *item CLEANUP(opLexerItemDestroy) = lexItem(text, &pos, keywords, kwCount);
		assert(item);
		if (item->type == OP_LEXER_KW) {
			if (0 == strcmp(item->value.kw, "OPCODE")) {
					struct opLexerItem *name CLEANUP(opLexerItemDestroy) = calloc(sizeof(struct opLexerItem),1);
				struct opLexerItem *intelAlias CLEANUP(opLexerItemDestroy) = NULL;
				name->type = OP_LEXER_WORD;
				pos = skipWhitespace(text, pos);
				lexName(text, &pos, &name->value.name);
				// OPCODE TOSname #intelName or
				// OPCODE TOSname ##"alias"
				long oldPos = pos;
				struct opLexerItem *hashTag CLEANUP(opLexerItemDestroy) = lexItem(text, &pos, keywords, kwCount);
				if (hashTag) {
					if (hashTag->type == OP_LEXER_KW) {
						pos = skipWhitespace(text, pos);
						if (0 == strcmp(hashTag->value.kw, "#")) {
							intelAlias = lexItem(text, &pos, keywords, kwCount);
							goto lookForEncodings;
						} else if (0 == strcmp(hashTag->value.kw, "##")) {
							intelAlias = lexItem(text, &pos, keywords, kwCount);
							goto lookForEncodings;
						}
					}
				}
				// Failed to find intel name
				oldPos = pos;
			lookForEncodings:;
				strOpcodeTemplate templates = NULL;
				for (;;) {
					if (item->type == OP_LEXER_KW) {
						struct opcodeTemplate template;
						memset(&template, 0, sizeof(template));
						template.name = calloc(strlen(name->value.name) + 1,1);
						strcpy(template.name, name->value.name);
						template.intelAlias = NULL;
						if (intelAlias) {
							// intelName can be string or name
							if (intelAlias->type == OP_LEXER_WORD) {
									template.intelAlias = calloc(strlen(intelAlias->value.name) + 1,1);
								strcpy(template.intelAlias, intelAlias->value.name);
							} else if (intelAlias->type == OP_LEXER_STR) {
									template.intelAlias = calloc(strlen(intelAlias->value.str) + 1,1);
								strcpy(template.intelAlias, intelAlias->value.str);
							} else {
								assert(intelAlias->type == OP_LEXER_STR || intelAlias->type == OP_LEXER_WORD);
							}
						} else {
							// Use defualt TOS name
								template.intelAlias = calloc(strlen(name->value.name) + 1,1);
							strcpy(template.intelAlias, name->value.name);
						}
						template.bytes = NULL;
						template.args = NULL;
						for (;;) {
							long oldPos = pos;
							struct opLexerItem *item CLEANUP(opLexerItemDestroy) = lexItem(text, &pos, keywords, kwCount);
							if (item->type == OP_LEXER_INT)
								template.bytes = strOpcodeBytesAppendItem(template.bytes, item->value.Int);
							else if (item->type == OP_LEXER_KW) {
								if (0 == strcmp(item->value.kw, ";")) {
									templates = strOpcodeTemplateAppendItem(templates, ALLOCATE(template));
									goto registerTemplates;
								} else if (0 == strcmp(item->value.kw, ",")) {
									goto commaPassed;
								} else
									assert(0);
							} else
								assert(0);
							continue;
						commaPassed:;
							break;
						}
						// Look for arg keywords or flags
						for (;;) {
								long oldPos = pos;
							struct opLexerItem *item CLEANUP(opLexerItemDestroy) = lexItem(text, &pos, keywords, kwCount);
#define ARG_BY_NAME(arg, name)																																										\
							else if (0 == strcmp(item->value.kw, name)) {																				\
									struct opcodeTemplateArg __arg;																																\
									__arg.type = arg;																																														\
									__arg.isChangedByOp=0;																																									\
									long oldPos=pos;																																															\
									const char *kwText=NULL;lexKeyword(text, &pos, keywords, kwCount,&kwText);	\
									if(kwText) {if(0==strcmp(kwText,"@")) __arg.isChangedByOp=1; else pos=oldPos;}	\
									template.args = strOpcodeTemplateArgAppendItem(template.args, __arg ); \
						}
						if (item->type == OP_LEXER_INT) {
								pos = oldPos;
								goto opcodeFinish;
							} else if (item->type == OP_LEXER_KW) {
								if (0 == strcmp(item->value.kw, ";")) {
									templates = strOpcodeTemplateAppendItem(templates, ALLOCATE(template));
									goto registerTemplates;
								} else if (0 == strcmp(item->value.kw, "+R")) {
									assert(!template.addRegNum);
									template.addRegNum = 1;
								} else if (0 == strcmp(item->value.kw, "+I")) {
									assert(!template.addRegNum);
									template.addRegNum = 1;
								} else if (0 == strcmp(item->value.kw, "/R")) {
									assert(!template.modRMReg);
									template.modRMReg = 1;
								} else if (0 == strcmp(item->value.kw, "/0") || 0 == strcmp(item->value.kw, "/1") || 0 == strcmp(item->value.kw, "/2") ||
								           0 == strcmp(item->value.kw, "/3") || 0 == strcmp(item->value.kw, "/4") || 0 == strcmp(item->value.kw, "/5") ||
								           0 == strcmp(item->value.kw, "/6") || 0 == strcmp(item->value.kw, "/7")) {
									template.modRMExt = item->value.kw[1] - '0';
								} else if (0 == strcmp(item->value.kw, "IB") || 0 == strcmp(item->value.kw, "IW") || 0 == strcmp(item->value.kw, "ID")) {
									// Immediate literals set immediate type
								} else if (0 == strcmp(item->value.kw, "CB") || 0 == strcmp(item->value.kw, "CW") || 0 == strcmp(item->value.kw, "CD")) {
								}
								ARG_BY_NAME(OPC_TEMPLATE_ARG_RM8, "RM8")
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
										ARG_BY_NAME(OPC_TEMPLATE_ARG_XMM, "XMM")
								ARG_BY_NAME(OPC_TEMPLATE_ARG_M8, "M8")
								ARG_BY_NAME(OPC_TEMPLATE_ARG_M16, "M16")
								ARG_BY_NAME(OPC_TEMPLATE_ARG_M32, "M32")
								ARG_BY_NAME(OPC_TEMPLATE_ARG_M64, "M64")
								ARG_BY_NAME(OPC_TEMPLATE_ARG_MOFFS32, "MOFFS32")
								ARG_BY_NAME(OPC_TEMPLATE_ARG_MOFFS16, "MOFFS16")
								ARG_BY_NAME(OPC_TEMPLATE_ARG_MOFFS8, "MOFFS8")
								ARG_BY_NAME(OPC_TEMPLATE_ARG_SREG, "SREG")
								ARG_BY_NAME(OPC_TEMPLATE_ARG_STI, "STI")
								else if (0 == strcmp(item->value.kw, "!")) {
								}
								else if (0 == strcmp(item->value.kw, "&")) {
								}
								else if (0 == strcmp(item->value.kw, "%")) {
								}
								else if (0 == strcmp(item->value.kw, "=")) {
									assert(!template.needsREX);
									template.needsREX = 1;
								}
								else if (0 == strcmp(item->value.kw, "^")) {
								}
								else if (0 == strcmp(item->value.kw, "*")) {
									//+I is used to detirmine if we need to add ST(i)
								}
								else if (0 == strcmp(item->value.kw, "$")) {
								}
								else if (0 == strcmp(item->value.kw, ":")) {
									// Aliases
									templates = strOpcodeTemplateAppendItem(templates, ALLOCATE(template));
									for (;;) {
										struct opLexerItem *next CLEANUP(opLexerItemDestroy) = lexItem(text, &pos, keywords, kwCount);
										if (next->type == OP_LEXER_KW)
											if (0 == strcmp(next->value.kw, ";"))
												goto registerTemplates;
										// Insert clone into map
										assert(next->type == OP_LEXER_WORD);
										mapOpcodeTemplatesInsert(opcodes, next->value.name, strOpcodeTemplateClone2(templates));
									}
								}
							} else if (item->type == OP_LEXER_WORD) {
								// Check for register
								__auto_type find = mapRegGet(regsByName, item->value.name);
								assert(find);
								struct opcodeTemplateArg arg;
								arg.type = OPC_TEMPLATE_ARG_REG;
								arg.value.reg = *find;
								long oldPos=pos;
								const char *kwText=NULL;lexKeyword(text, &pos, keywords, kwCount,&kwText);
								if(kwText) {if(0==strcmp(kwText,"@")) arg.isChangedByOp=1; else pos=oldPos;}
								
								template.args = strOpcodeTemplateArgAppendItem(template.args, arg);
							} else {
								assert(0);
							}
						}
					opcodeFinish:;
						templates = strOpcodeTemplateAppendItem(templates, ALLOCATE(template));
					}
				}
			registerTemplates:
				mapOpcodeTemplatesInsert(opcodes, templates[0]->name, templates);
			}
		}
	}
	mapRegDestroy(regsByName, NULL);
}
static int sizeMatchUnsigned(const struct X86AddressingMode *mode, long size) {
	uint64_t upperBound;
	switch (size) {
	case 1:
		upperBound = UINT8_MAX;
		break;
	case 2:
		upperBound = UINT16_MAX;
		break;
	case 4:
		upperBound = UINT32_MAX;
		break;
	case 8:
		upperBound = UINT64_MAX;
		break;
	default:
		assert(0);
	}
	switch (mode->type) {
	case X86ADDRMODE_SINT:
		return mode->value.sint <= upperBound;
	case X86ADDRMODE_UINT:
		return mode->value.uint <= upperBound;
	case X86ADDRMODE_REG:
		return mode->value.reg->size == size;
	case X86ADDRMODE_VAR_VALUE:
	case X86ADDRMODE_MEM:
		if (mode->valueType)
			return objectSize(mode->valueType, NULL) == size;
		return 1;
	case X86ADDRMODE_SIZEOF:
	case X86ADDRMODE_LABEL:
		return 1;
	}
	return 0;
}
static int sizeMatchSigned(const struct X86AddressingMode *mode, long size) {
	int64_t upperBound, lowerBound;
	switch (size) {
	case 1:
		upperBound = INT8_MAX, lowerBound = INT8_MIN;
		break;
	case 2:
		upperBound = INT16_MAX, lowerBound = INT16_MIN;
		break;
	case 4:
		upperBound = INT32_MAX, lowerBound = INT32_MIN;
		break;
	case 8:
		upperBound = INT64_MAX, lowerBound = INT64_MIN;
		break;
	default:
		assert(0);
	}
	switch (mode->type) {
	case X86ADDRMODE_SINT:
			return 1;
			//return mode->value.sint >= lowerBound && mode->value.sint <= upperBound;
	case X86ADDRMODE_UINT:
			return 1;
			//return mode->value.uint <= upperBound;
	case X86ADDRMODE_REG:
		return mode->value.reg->size == size;
	case X86ADDRMODE_VAR_VALUE:
	case X86ADDRMODE_MEM:
		if (mode->valueType)
			return objectSize(mode->valueType, NULL) == size;
		return 1;
	case X86ADDRMODE_SIZEOF:
	case X86ADDRMODE_LABEL:
		return 1;
	}
	return 0;
}
int opcodeTemplateArgAcceptsAddrMode(const struct opcodeTemplateArg *arg, const struct X86AddressingMode *mode) {
		if(mode->type==X86ADDRMODE_MACRO) return 1;
	switch (arg->type) {
	case OPC_TEMPLATE_ARG_XMM: {
			if(mode->type==X86ADDRMODE_REG) {
					const struct reg *xmm[]={
							&regX86XMM0,
							&regX86XMM1,
							&regX86XMM2,
							&regX86XMM3,
							&regX86XMM4,
							&regX86XMM5,
							&regX86XMM6,
							&regX86XMM7,
					};
					for(long i=0;i!=8;i++)
							if(xmm[i]==mode->value.reg)
									return 1;
			}
			return 0;
	}
	case OPC_TEMPLATE_ARG_M16:
		if (mode->type == X86ADDRMODE_MEM ||mode->type==X86ADDRMODE_VAR_VALUE) {
			return sizeMatchUnsigned(mode, 2);
		} else
			goto fail;
	case OPC_TEMPLATE_ARG_M32:
		if (mode->type == X86ADDRMODE_MEM ||mode->type==X86ADDRMODE_VAR_VALUE) {
			return sizeMatchUnsigned(mode, 4);
		} else
			goto fail;
	case OPC_TEMPLATE_ARG_M64:
		if (mode->type == X86ADDRMODE_MEM ||mode->type==X86ADDRMODE_VAR_VALUE) {
			return sizeMatchUnsigned(mode, 8);
		} else
			goto fail;
	case OPC_TEMPLATE_ARG_M8:
		if (mode->type == X86ADDRMODE_MEM ||mode->type==X86ADDRMODE_VAR_VALUE) {
			return sizeMatchUnsigned(mode, 1);
		} else
			goto fail;
	case OPC_TEMPLATE_ARG_MOFFS16:
			if (mode->type == X86ADDRMODE_MEM&&mode->value.m.type==x86ADDR_INDIR_SIB) {
					if(mode->value.m.value.sib.base!=NULL||mode->value.m.value.sib.index!=NULL)
							goto fail;
					if(mode->value.m.value.sib.offset->type!=X86ADDRMODE_UINT&&mode->value.m.value.sib.offset->type!=X86ADDRMODE_SINT)
							goto fail;
					int64_t value = mode->value.m.value.sib.offset->value.sint;
					return INT16_MIN <= value && INT16_MAX >= value;
		} else
			goto fail;
	case OPC_TEMPLATE_ARG_MOFFS32:
		if (mode->type == X86ADDRMODE_MEM&&mode->value.m.type==x86ADDR_INDIR_SIB) {
				if(mode->value.m.value.sib.base!=NULL||mode->value.m.value.sib.index!=NULL)
						goto fail;
				if(mode->value.m.value.sib.offset->type!=X86ADDRMODE_UINT&&mode->value.m.value.sib.offset->type!=X86ADDRMODE_SINT)
						goto fail;
				
				int64_t value = mode->value.m.value.sib.offset->value.sint;
				return INT32_MIN <= value && INT32_MAX >= value;
		} else
			goto fail;
	case OPC_TEMPLATE_ARG_MOFFS8:
		if (mode->type == X86ADDRMODE_MEM&&mode->value.m.type==x86ADDR_INDIR_SIB) {
				if(mode->value.m.value.sib.base!=NULL||mode->value.m.value.sib.index!=NULL)
						goto fail;
				if(mode->value.m.value.sib.offset->type!=X86ADDRMODE_UINT&&mode->value.m.value.sib.offset->type!=X86ADDRMODE_SINT)
						goto fail;
				
				int64_t value = mode->value.m.value.sib.offset->value.sint;
			return INT8_MIN <= value && INT8_MAX >= value;
		} else
			goto fail;
	case OPC_TEMPLATE_ARG_R16:
		if (mode->type == X86ADDRMODE_REG) {
			return sizeMatchUnsigned(mode, 2);
		} else
			goto fail;
	case OPC_TEMPLATE_ARG_R32:
		if (mode->type == X86ADDRMODE_REG) {
			return sizeMatchUnsigned(mode, 4);
		} else
			goto fail;
	case OPC_TEMPLATE_ARG_R64:
		if (mode->type == X86ADDRMODE_REG) {
			return sizeMatchUnsigned(mode, 8);
		} else
			goto fail;
	case OPC_TEMPLATE_ARG_R8:
		if (mode->type == X86ADDRMODE_REG) {
			return sizeMatchUnsigned(mode, 1);
		} else
			goto fail;
	case OPC_TEMPLATE_ARG_REG:
		if (mode->type == X86ADDRMODE_REG) {
			return mode->value.reg == arg->value.reg;
		} else
			goto fail;
	case OPC_TEMPLATE_ARG_REL16:
		if (mode->type==X86ADDRMODE_LABEL) {
			return 1;
		} else
			goto fail;
	case OPC_TEMPLATE_ARG_SREG:
		if (mode->type == X86ADDRMODE_REG) {
			const struct reg *regs[] = {
			    &regX86ES, &regX86CS, &regX86SS, &regX86DS, &regX86FS, &regX86SS, &regX86GS,
			};
			for (long i = 0; i != sizeof(regs) / sizeof(*regs); i++)
				if (regs[i] == mode->value.reg)
					return 1;
			return 0;
		} else
			goto fail;
	case OPC_TEMPLATE_ARG_SINT8:
	case OPC_TEMPLATE_ARG_UINT8:
		if (mode->type == X86ADDRMODE_SINT || mode->type == X86ADDRMODE_UINT||mode->type==X86ADDRMODE_SIZEOF) {
			return sizeMatchSigned(mode, 1)||sizeMatchUnsigned(mode, 1);
		} else
			goto fail;
	case OPC_TEMPLATE_ARG_UINT16:
	case OPC_TEMPLATE_ARG_SINT16:
		if (mode->type == X86ADDRMODE_SINT || mode->type == X86ADDRMODE_UINT||mode->type==X86ADDRMODE_SIZEOF) {
				return sizeMatchSigned(mode, 2)||sizeMatchUnsigned(mode, 2);
		} else
			goto fail;
	case OPC_TEMPLATE_ARG_SINT32:
	case OPC_TEMPLATE_ARG_UINT32:
		if (mode->type == X86ADDRMODE_SINT || mode->type == X86ADDRMODE_UINT||mode->type==X86ADDRMODE_SIZEOF) {
			return sizeMatchSigned(mode, 4)||sizeMatchUnsigned(mode, 4);
		}
		if (ptrSize() == 4) {
			return mode->type == X86ADDRMODE_LABEL || mode->type == X86ADDRMODE_ITEM_ADDR||mode->type==X86ADDRMODE_VAR_VALUE || mode->type == X86ADDRMODE_STR||mode->type == X86ADDRMODE_LABEL;
		} else
			goto fail;
	case OPC_TEMPLATE_ARG_SINT64:
	case OPC_TEMPLATE_ARG_UINT64:
		if (mode->type == X86ADDRMODE_SINT || mode->type == X86ADDRMODE_UINT||mode->type==X86ADDRMODE_SIZEOF) {
			return sizeMatchSigned(mode, 8)||sizeMatchUnsigned(mode, 8);
		}
		if (ptrSize() == 8) {
			return mode->type == X86ADDRMODE_LABEL || mode->type == X86ADDRMODE_ITEM_ADDR||mode->type==X86ADDRMODE_VAR_VALUE || mode->type == X86ADDRMODE_STR||mode->type == X86ADDRMODE_LABEL;
		} else
			goto fail;
	case OPC_TEMPLATE_ARG_RM8:
		if (mode->type == X86ADDRMODE_REG || mode->type == X86ADDRMODE_MEM ||mode->type==X86ADDRMODE_VAR_VALUE) {
			return sizeMatchUnsigned(mode, 1);
		} else
			goto fail;
	case OPC_TEMPLATE_ARG_RM16:
		if (mode->type == X86ADDRMODE_REG || mode->type == X86ADDRMODE_MEM ||mode->type==X86ADDRMODE_VAR_VALUE) {
			return sizeMatchUnsigned(mode, 2);
		} else
			goto fail;
	case OPC_TEMPLATE_ARG_RM32:
		if (mode->type == X86ADDRMODE_REG || mode->type == X86ADDRMODE_MEM||mode->type==X86ADDRMODE_VAR_VALUE) {
			return sizeMatchUnsigned(mode, 4);
		} else
			goto fail;
	case OPC_TEMPLATE_ARG_RM64:
		if (mode->type == X86ADDRMODE_REG || mode->type == X86ADDRMODE_MEM ||mode->type==X86ADDRMODE_VAR_VALUE) {
			return sizeMatchUnsigned(mode, 8);
		} else
			goto fail;
	case OPC_TEMPLATE_ARG_REL8:
		return mode->type == X86ADDRMODE_LABEL;
	case OPC_TEMPLATE_ARG_REL32:
		return mode->type == X86ADDRMODE_LABEL ;
		;
	case OPC_TEMPLATE_ARG_STI:
		if (mode->type == X86ADDRMODE_REG) {
			const struct reg *fpuRegs[] = {&regX86ST0, &regX86ST1, &regX86ST2, &regX86ST3, &regX86ST4, &regX86ST5, &regX86ST6, &regX86ST7};
			for (long i = 0; i != sizeof(fpuRegs) / sizeof(*fpuRegs); i++)
				if (fpuRegs[i] == mode->value.reg)
					return 1;
		}
		return 0;
	}
fail:
	return 0;
}
static int imcompatWithArgs(const strX86AddrMode args, struct opcodeTemplate **template) {
	if (strOpcodeTemplateArgSize(template[0] -> args) != strX86AddrModeSize(args))
		return 1;
	for (long i = 0; i != strX86AddrModeSize(args); i++)
		if (!opcodeTemplateArgAcceptsAddrMode(&template[0] -> args[i], args[i]))
			return 1;
	return 0;
}
static struct opcodeTemplate *__X86OpcodesByArgs(const char *name, strX86AddrMode args) {
	__auto_type list = mapOpcodeTemplatesGet(opcodes, name);
	if (!list)
		return NULL;
	for(long i=0;i!=strOpcodeTemplateSize(*list);i++)
			if(!imcompatWithArgs(args, &list[0][i]))
					return list[0][i];
	return NULL;
}
struct X86AddressingMode *X86AddrModeUint(uint64_t imm) {
	struct X86AddressingMode retVal;
	retVal.type = X86ADDRMODE_UINT;
	retVal.value.uint = imm;
	retVal.valueType = (dataSize()==4)?&typeI32i:&typeI64i;
	return ALLOCATE(retVal);
}
struct X86AddressingMode *X86AddrModeSint(int64_t imm) {
	struct X86AddressingMode retVal;
	retVal.type = X86ADDRMODE_SINT;
	retVal.value.sint = imm;
	retVal.valueType = (dataSize()==4)?&typeI32i:&typeI64i;
	return ALLOCATE(retVal);
}
struct X86AddressingMode *X86AddrModeReg(struct reg *reg,struct object *valueType) {
	struct X86AddressingMode retVal;
	retVal.type = X86ADDRMODE_REG;
	retVal.value.reg = reg;
	retVal.valueType = valueType;
	return ALLOCATE(retVal);
}
struct X86AddressingMode *X86AddrModeIndirMem(uint64_t where, struct object *type) {
	struct X86AddressingMode retVal;
	retVal.type = X86ADDRMODE_MEM;
	retVal.value.m.type = x86ADDR_INDIR_SIB;
	retVal.value.m.value.sib.base = NULL;
	retVal.value.m.value.sib.index = NULL;
	retVal.value.m.value.sib.scale = 0;
	retVal.value.m.value.sib.offset = NULL;
	retVal.value.m.value.sib.offset2=0;
	retVal.value.m.value.sib.memberOffsets=NULL;
	retVal.valueType = type;
	return ALLOCATE(retVal);
}
struct X86AddressingMode *X86AddrModeIndirReg(struct reg *where, struct object *type) {
	struct X86AddressingMode retVal;
	retVal.type = X86ADDRMODE_MEM;
	retVal.value.m.type = x86ADDR_INDIR_SIB;
	retVal.value.m.value.sib.base = X86AddrModeReg(where,(dataSize()==4)?&typeI32i:&typeI64i);
	retVal.value.m.value.sib.index = NULL;
	retVal.value.m.value.sib.scale = 1;
	retVal.value.m.value.sib.offset = NULL;
	retVal.value.m.value.sib.memberOffsets=NULL;
	retVal.value.m.value.sib.offset2=0;
	retVal.valueType = type;
	return ALLOCATE(retVal);
}
void X86AddrModeIndirSIBAddOffset(struct X86AddressingMode *addrMode,int32_t offset) {
		assert(addrMode->type==X86ADDRMODE_MEM);
		assert(addrMode->value.m.type==x86ADDR_INDIR_SIB);
		addrMode->value.m.value.sib.offset2+=offset;
}
void X86AddrModeIndirSIBAddMemberOffset(struct X86AddressingMode *addrMode,struct objectMember *mem)  {
		addrMode->value.m.value.sib.memberOffsets=strObjectMemberPAppendItem(addrMode->value.m.value.sib.memberOffsets, mem);
		addrMode->valueType=mem->type;
}
struct X86AddressingMode *X86AddrModeIndirSIB(long scale, struct X86AddressingMode *index, struct X86AddressingMode *base, struct X86AddressingMode *offset,
                                              struct object *type) {
	struct X86AddressingMode retVal;
	retVal.type = X86ADDRMODE_MEM;
	retVal.value.m.type = x86ADDR_INDIR_SIB;
	retVal.value.m.value.sib.base = base;
	retVal.value.m.value.sib.index = index;
	retVal.value.m.value.sib.scale = scale;
	retVal.value.m.value.sib.offset = offset;
	retVal.value.m.value.sib.offset2=0;
	retVal.value.m.value.sib.memberOffsets=NULL;
	retVal.valueType = type;
	return ALLOCATE(retVal);
}
STR_TYPE_DEF(long, Long);
STR_TYPE_FUNCS(long, Long);
struct sizeTypePair {
	long size;
	struct object *type;
};
long opcodeTemplateArgSize(struct opcodeTemplateArg arg) {
	long operandSize;
	switch (arg.type) {
	case OPC_TEMPLATE_ARG_SREG:
		operandSize = 2;
		break;
	case OPC_TEMPLATE_ARG_REG:
		operandSize = arg.value.reg->size;
		break;
	case OPC_TEMPLATE_ARG_M8:
	case OPC_TEMPLATE_ARG_RM8:
	case OPC_TEMPLATE_ARG_R8:
	case OPC_TEMPLATE_ARG_MOFFS8:
		operandSize = 1;
		break;
	case OPC_TEMPLATE_ARG_M16:
	case OPC_TEMPLATE_ARG_RM16:
	case OPC_TEMPLATE_ARG_R16:
	case OPC_TEMPLATE_ARG_MOFFS16:
		operandSize = 2;
		break;
	case OPC_TEMPLATE_ARG_M32:
	case OPC_TEMPLATE_ARG_RM32:
	case OPC_TEMPLATE_ARG_R32:
	case OPC_TEMPLATE_ARG_MOFFS32:
		operandSize = 4;
		break;
	case OPC_TEMPLATE_ARG_M64:
	case OPC_TEMPLATE_ARG_RM64:
	case OPC_TEMPLATE_ARG_R64:
		operandSize = 8;
		break;
	case OPC_TEMPLATE_ARG_STI:
		operandSize = 10;
		break;
		// These values arn't mutually exlcusive,so give them a value of -1
	case OPC_TEMPLATE_ARG_REL16:
	case OPC_TEMPLATE_ARG_REL8:
	case OPC_TEMPLATE_ARG_REL32:
	case OPC_TEMPLATE_ARG_SINT8:
	case OPC_TEMPLATE_ARG_SINT16:
	case OPC_TEMPLATE_ARG_SINT32:
	case OPC_TEMPLATE_ARG_SINT64:
	case OPC_TEMPLATE_ARG_UINT8:
	case OPC_TEMPLATE_ARG_UINT16:
	case OPC_TEMPLATE_ARG_UINT32:
	case OPC_TEMPLATE_ARG_UINT64:;
		operandSize = -1;
		break;
	}
	return operandSize;
}
static struct opcodeTemplate *assumeTypes(strOpcodeTemplate templates, strX86AddrMode args) {
	strLong ambigArgs CLEANUP(strLongDestroy) = NULL;
	long operandSize = -1;
	for (long i = 0; i != strX86AddrModeSize(args); i++) {
		// Only assume type if the templates can't agree on a size for args[o]
		long firstSize = opcodeTemplateArgSize(templates[0]->args[i]);
		for (long t = 1; t < strOpcodeTemplateSize(templates); t++) {
			if (firstSize != opcodeTemplateArgSize(templates[t]->args[i]))
				goto ambiguous;
		}
		continue;
	ambiguous:
		if (args[i]->type == X86ADDRMODE_MEM) {
			if (args[i]->valueType == NULL) {
				ambigArgs = strLongAppendItem(ambigArgs, i);
			} else {
				__auto_type newSize = objectSize(args[i]->valueType, NULL);
				if (operandSize != -1)
					goto fail;
				else if (operandSize != newSize)
					goto fail;
				operandSize = newSize;
				;
			}
		} else if (args[i]->type == X86ADDRMODE_REG && args[i]->value.reg->type == REG_TYPE_GP) {
			__auto_type newSize = args[i]->value.reg->size;
			if (operandSize != -1)
				goto fail;
			else if (operandSize != newSize)
				goto fail;
			operandSize = newSize;
		}
	}
	{
		struct object *type = NULL;
		switch (operandSize) {
		case 1:
			type = &typeI8i;
			break;
		case 2:
			type = &typeI16i;
			break;
		case 4:
			type = &typeI32i;
			break;
		case 8:
			type = &typeI64i;
			break;
		}
		strX86AddrMode clone CLEANUP(strX86AddrModeDestroy) = strX86AddrModeClone(args);
		for (long i = 0; i != strLongSize(ambigArgs); i++) {
			if (clone[i]->type == X86ADDRMODE_MEM && clone[i]->valueType == NULL)
				if (!clone[i]->valueType)
					clone[i]->valueType = type;
		}
		return __X86OpcodesByArgs(templates[0]->name, clone);
	}
	fail:;
	return NULL;
}
strOpcodeTemplate X86OpcodesByName(const char *name) {
	__auto_type find = mapOpcodeTemplatesGet(opcodes, name);
	if (!find)
		return NULL;
	return strOpcodeTemplateClone(*find);
}
long X86OpcodesArgCount(const char *name) {
	__auto_type find = mapOpcodeTemplatesGet(opcodes, name);
	if (!find)
		return 0;
	return strOpcodeTemplateArgSize(find[0][0]->args);
}
struct opcodeTemplate *X86OpcodeByArgs(const char *name, strX86AddrMode args) {
		__auto_type find= __X86OpcodesByArgs(name, args);
		if(find)
				return find;
		return NULL;
}
struct X86AddressingMode *X86AddrModeFlt(double value) {
	struct X86AddressingMode flt;
	flt.type = X86ADDRMODE_FLT;
	flt.valueType = NULL;
	flt.value.flt = value;
	return ALLOCATE(flt);
}
struct X86AddressingMode *X86AddrModeItemValue(struct parserSymbol *item,long offset, struct object *type) {
	struct X86AddressingMode mode;
	mode.type = X86ADDRMODE_ITEM_ADDR;
	const char *name=parserGetGlobalSymLinkageName(item->name);
	mode.value.itemAddr.item = strcpy(calloc(strlen(name)+1, 1), name);
	mode.value.itemAddr.offset=offset;
	mode.valueType = type;
	return ALLOCATE(mode);
}
struct X86AddressingMode *X86AddrModeVar(struct parserVar *var,long offset) {
		struct X86AddressingMode mode;
		mode.type=X86ADDRMODE_VAR_VALUE;
		mode.value.varAddr.offset=offset;
		mode.value.varAddr.var=var;
		mode.valueType=var->type;
		mode.value.varAddr.memberOffsets=NULL;
		
		var->refCount++;
		return ALLOCATE(mode);
}
struct X86AddressingMode *X86AddrModeLabel(const char *name) {
	struct X86AddressingMode mode;
	mode.type = X86ADDRMODE_LABEL;
	mode.valueType = NULL;
	mode.value.label = calloc(strlen(name) + 1,1);
	strcpy(mode.value.label, name);
	return ALLOCATE(mode);
}
struct X86AddressingMode *X86AddrModeClone(struct X86AddressingMode *mode) {
	switch (mode->type) {
	case X86ADDRMODE_MACRO: {
			__auto_type clone=*mode;
			clone.value.macroName=strcpy(calloc(strlen(clone.value.macroName)+1, 1), clone.value.macroName);
			return ALLOCATE(clone);
	}
	case X86ADDRMODE_SIZEOF: {
			__auto_type clone=*mode;
			return ALLOCATE(clone);
	}
	case X86ADDRMODE_VAR_VALUE: {
			__auto_type clone=*mode;
			clone.value.varAddr.var->refCount++;
			return ALLOCATE(clone);
	}
	case X86ADDRMODE_STR: {
		__auto_type clone = *mode;
		clone.value.text = __vecAppendItem(NULL, mode->value.text, __vecSize(mode->value.text));
		return ALLOCATE(clone);
	}
	case X86ADDRMODE_MEM: {
		__auto_type clone = *mode;
		if (clone.value.m.type == x86ADDR_INDIR_SIB) {
			if (clone.value.m.value.sib.offset)
				clone.value.m.value.sib.offset = X86AddrModeClone(clone.value.m.value.sib.offset);
			if (clone.value.m.value.sib.base)
				clone.value.m.value.sib.base = X86AddrModeClone(clone.value.m.value.sib.base);
			if (clone.value.m.value.sib.index)
				clone.value.m.value.sib.index = X86AddrModeClone(clone.value.m.value.sib.index);
		}
		if (clone.value.m.type == x86ADDR_INDIR_LABEL)
			clone.value.m.value.label = X86AddrModeClone(clone.value.m.value.label);
		return ALLOCATE(clone);
	}
	case X86ADDRMODE_FLT:
	case X86ADDRMODE_REG:
	case X86ADDRMODE_SINT:
	case X86ADDRMODE_UINT:
	case X86ADDRMODE_ITEM_ADDR:
		return ALLOCATE((*mode));
	case X86ADDRMODE_LABEL:;
		__auto_type retVal = *mode;
		retVal.value.label = calloc(strlen(mode->value.label) + 1,1);
		strcpy(retVal.value.label, mode->value.label);
		return ALLOCATE(retVal);
	}
}
void X86AddrModeDestroy(struct X86AddressingMode **mode) {
	if (!mode[0])
		return;

	switch (mode[0]->type) {
	case X86ADDRMODE_MACRO: {
			free(mode[0]->value.macroName);
			break;
	}
	case X86ADDRMODE_VAR_VALUE: {
			variableDestroy(mode[0]->value.varAddr.var);
			break;
	}
	case X86ADDRMODE_STR:
		__vecDestroy(&mode[0]->value.text);
		break;
	case X86ADDRMODE_SIZEOF:
	case X86ADDRMODE_UINT:
	case X86ADDRMODE_SINT:
	case X86ADDRMODE_REG:
	case X86ADDRMODE_FLT:
	case X86ADDRMODE_ITEM_ADDR:
		break;
	case X86ADDRMODE_LABEL:
		free(mode[0]->value.label);
		break;
	case X86ADDRMODE_MEM:
		if (mode[0]->value.m.type == x86ADDR_INDIR_SIB) {
			if (mode[0]->value.m.value.sib.offset)
				X86AddrModeDestroy(&mode[0]->value.m.value.sib.offset);
			if (mode[0]->value.m.value.sib.base)
				X86AddrModeDestroy(&mode[0]->value.m.value.sib.base);
			if (mode[0]->value.m.value.sib.index)
				X86AddrModeDestroy(&mode[0]->value.m.value.sib.index);
			if(mode[0]->value.m.value.sib.memberOffsets)
					strObjectMemberPDestroy(&mode[0]->value.m.value.sib.memberOffsets);
		}
		if (mode[0]->value.m.type == x86ADDR_INDIR_LABEL)
			X86AddrModeDestroy(&mode[0]->value.m.value.label);
		break;
	}
	free(*mode);
}
struct X86AddressingMode *X86AddrModeStr(const char *text,long len) {
	struct X86AddressingMode mode;
	mode.valueType = objectPtrCreate(&typeU8i);
	mode.value.text = __vecAppendItem(NULL, text, len);
	mode.type = X86ADDRMODE_STR;
	return ALLOCATE(mode);
}
struct X86AddressingMode *X86AddrModeIndirLabel(const char *text, struct object *type) {
	struct X86AddressingMode mode;
	mode.type = X86ADDRMODE_MEM;
	mode.valueType = type;
	mode.value.m.segment = NULL;
	mode.value.m.type = x86ADDR_INDIR_LABEL;
	mode.value.m.value.label = X86AddrModeLabel(text);
	return ALLOCATE(mode);
}
struct X86AddressingMode *X86AddrModeGlblVarAddr(struct parserVar *var) {
	assert(var->isGlobal);
	__auto_type retval=X86AddrModeLabel(parserGetGlobalSymLinkageName(var->name));
	retval->valueType=objectPtrCreate(var->type);
	return retval;
}
struct X86AddressingMode *X86AddrModeFunc(struct parserFunction *func) {
	struct X86AddressingMode mode;
	mode.type = X86ADDRMODE_LABEL;
	mode.valueType = func->type;
	// Use name if global,else name mangle
	if (func->parentFunction == NULL) {
			__auto_type name=parserGetGlobalSymLinkageName(func->name);
			mode.value.label = strcpy(calloc(strlen(name) + 1,1), name);
	} else {
	loop:;
		__auto_type find = ptrMapAsmFuncNameGet(asmFuncNames, func);
		if (find) {
				mode.value.label = strcpy(calloc(strlen(*find) + 1,1), *find);
		} else {
			const char *fmt = "$lam_%s";
			long cnt = snprintf(NULL, 0, fmt, func->name);
			char buffer[cnt + 1];
			sprintf(buffer, fmt, func->name);

			char *newName = strcpy(calloc(strlen(buffer) + 1,1), buffer);
			ptrMapAsmFuncNameAdd(asmFuncNames, func, newName);
			goto loop;
		}
	}
	return ALLOCATE(mode);
}
struct X86AddressingMode *X86AddrModeSizeofObj(struct object *type) {
		struct X86AddressingMode mode;
		mode.type=X86ADDRMODE_SIZEOF;
		mode.value.objSizeof=type;
		mode.valueType=dftValType();
		return ALLOCATE(mode);
}
struct X86AddressingMode *X86AddrModeMacro(const char *name,struct object *type) {
		struct X86AddressingMode mode;
		mode.type=X86ADDRMODE_MACRO;
		mode.value.macroName=strcpy(calloc(strlen(name)+1, 1), name);
		mode.valueType=type;
		return ALLOCATE(mode);
}
