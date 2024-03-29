#pragma once
#include <stdint.h>
#include "str.h"
#include "parserA.h"
#include "parserB.h"
#include "object.h"
struct X86AddressingMode;
struct X86MemoryLoc {
	enum {
		x86ADDR_INDIR_SIB, // Indirect scale-index-base aka [SCALE*REG+off/REG]
		x86ADDR_INDIR_LABEL,
	} type;
	union {
		struct {
				struct X86AddressingMode *index;
				struct X86AddressingMode *base;
				int scale;
				struct X86AddressingMode *offset;
				long offset2;
				strObjectMemberP memberOffsets;
		} sib;
		struct X86AddressingMode *label;
	} value;
	struct reg *segment;
};
struct X86AddressingMode {
	enum {
			X86ADDRMODE_REG,
			X86ADDRMODE_SINT,
			X86ADDRMODE_UINT,
			X86ADDRMODE_FLT,
			X86ADDRMODE_MEM,
			X86ADDRMODE_LABEL,
			X86ADDRMODE_ITEM_ADDR,
			X86ADDRMODE_VAR_VALUE,
			X86ADDRMODE_STR,
			X86ADDRMODE_SIZEOF,
			X86ADDRMODE_MACRO,
	} type;
	union {
			struct object *objSizeof;
			uint64_t uint;
			int64_t sint;
			double flt;
			struct reg *reg;
			struct X86MemoryLoc m;
			struct {
					char *item;
					long offset;
			} itemAddr;
			struct {
					struct parserVar *var;
					long offset;
					strObjectMemberP memberOffsets;
			} varAddr;
			char *label;
			struct __vec *text;
			char *macroName;
	} value;
		struct object *valueType;
};
struct X86AddressingMode *X86AddrModeVar(struct parserVar *var,long offset);
struct X86AddressingMode *X86AddrModeFlt(double value);
struct X86AddressingMode *X86AddrModeUint(uint64_t imm);
struct X86AddressingMode *X86AddrModeSint(int64_t imm);struct X86AddressingMode *X86AddrModeReg(struct reg *reg,struct object *valueType);
struct X86AddressingMode *X86AddrModeIndirMem(uint64_t where, struct object *type);
struct X86AddressingMode *X86AddrModeLabel(const char *name);
struct X86AddressingMode *X86AddrModeIndirReg(struct reg *where, struct object *type);
struct X86AddressingMode *X86AddrModeIndirSIB(long scale, struct X86AddressingMode *index, struct X86AddressingMode *base, struct X86AddressingMode *offset, struct object *type);
struct X86AddressingMode *X86AddrModeItemValue(struct parserSymbol *item,long offset, struct object *type);
struct X86AddressingMode *X86AddrModeSizeofObj(struct object *type);
struct opcodeTemplateArg {
	enum {
		OPC_TEMPLATE_ARG_REG,
		OPC_TEMPLATE_ARG_SREG, // Segment register
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
		OPC_TEMPLATE_ARG_XMM,
	} type;
	union {
		uint64_t uint;
		int64_t sint;
		struct reg *reg;
	} value;
		unsigned int isChangedByOp:1;
};
STR_TYPE_DEF(struct opcodeTemplateArg, OpcodeTemplateArg);
STR_TYPE_FUNCS(struct opcodeTemplateArg, OpcodeTemplateArg);
STR_TYPE_DEF(struct X86AddressingMode *, X86AddrMode);
STR_TYPE_FUNCS(struct X86AddressingMode *, X86AddrMode);
void parseOpcodeFile();
struct opcodeTemplate;
STR_TYPE_DEF(struct opcodeTemplate *, OpcodeTemplate);
STR_TYPE_FUNCS(struct opcodeTemplate *, OpcodeTemplate);
struct opcodeTemplate *X86OpcodeByArgs(const char *name, strX86AddrMode args);
const char *opcodeTemplateName(struct opcodeTemplate *template);
strOpcodeTemplate X86OpcodesByName(const char *name);
long X86OpcodesArgCount(const char *name);
const char *opcodeTemplateIntelAlias(const struct opcodeTemplate *template);
struct X86AddressingMode *X86AddrModeClone(struct X86AddressingMode *mode);
void X86AddrModeDestroy(struct X86AddressingMode **mode);
struct X86AddressingMode *X86AddrModeIndirLabel(const char *text, struct object *type);
struct X86AddressingMode *X86AddrModeStr(const char *text,long len);
struct X86AddressingMode *X86AddrModeGlblVarAddr(struct parserVar *var);
struct X86AddressingMode *X86AddrModeFunc(struct parserFunction *func);
int opcodeTemplateArgAcceptsAddrMode(const struct opcodeTemplateArg *arg, const struct X86AddressingMode *mode);
STR_TYPE_DEF(uint8_t, OpcodeBytes);
STR_TYPE_FUNCS(uint8_t, OpcodeBytes);
struct opcodeTemplate {
	strOpcodeBytes bytes;
	strOpcodeTemplateArg args;
	char *name;
	char *intelAlias;
	unsigned int needsREX : 1;
	unsigned int usesSTI : 1;
	unsigned int notIn64mode : 1;
	unsigned int addRegNum : 1;
	unsigned int modRMReg : 1;
	unsigned int modRMExt : 3;
};
long opcodeTemplateArgSize(struct opcodeTemplateArg arg);
void X86AddrModeIndirSIBAddOffset(struct X86AddressingMode *addrMode,int32_t offset);
void X86AddrModeIndirSIBAddMemberOffset(struct X86AddressingMode *addrMode,struct objectMember *mem);
struct X86AddressingMode *X86AddrModeMacro(const char *name,struct object *type);
