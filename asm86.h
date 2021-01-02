#pragma once
#include <lexer.h>
#include <stdint.h>
#include <str.h>
void ASMX86Init();
int ASMX86IsOpcode(const char *text);
struct AsmX86AddrMode {
		enum {
				ASM_X86_AM_IMM_SINT,
				ASM_X86_AM_IMM_UINT,
				ASM_X86_AM_IMM_FLT,
				ASM_X86_AM_REG_GP,
				ASM_X86_AM_REG,
				ASM_X86_AM_INDIR,
				ASM_X86_AM_INDIR_REG,
				ASM_X86_AM_INDIR_INDEX,
		} type;
		union {
				struct lexerFloating fltLit;
				uint64_t uint;
				int64_t sint;
				struct reg *reg;
				struct {
						struct reg *base;
						struct reg *index; 
						int scale;
						int64_t offset;
				} indirect;
		};
};
STR_TYPE_DEF(struct AsmX86AddrMode ,AsmX86AddrMode);
STR_TYPE_FUNCS(struct AsmX86AddrMode ,AsmX86AddrMode);
#define X86_AM_SINT(value) (struct AsmX86AddrMode){.type=ASM_X86_AM_IMM_SINT,.sint=value}
#define X86_AM_UINT(value) (struct AsmX86AddrMode){.type=ASM_X86_AM_IMM_UINT,.uint=value}
#define X86_AM_FLT(value) (struct AsmX86AddrMode){.type=ASM_X86_AM_IMM_FLT,.fltLit=value}
#define X86_AM_GP (struct AsmX86AddrMode){.type=ASM_X86_AM_REG_GP}
#define X86_AM_REG(reg) (struct AsmX86AddrMode){.type=ASM_X86_AM_REG,.reg=&reg}
#define X86_AM_INDIR(addr) (struct AsmX86AddrMode){.type=ASM_X86_AM_INDIR,.indirect.offset=addr}
#define X86_AM_INDIR_REG(reg) (struct AsmX86AddrMode){.type=ASM_X86_AM_INDIR_REG,.indirect.base=&reg}
#define X86_AM_INDIR_INDEX(base,index,scale) (struct AsmX86AddrMode){.type=ASM_X86_AM_INDIR_INDEX,.indirect.base=&base,.indirect.index=&index,.indirect.scale=scale,.indirect.offset=0}
#define X86_AM_INDIR_INDEX_OFFSET(base,index,scale,off) (struct AsmX86AddrMode){.type=ASM_X86_AM_INDIR_INDEX,.indirect.base=&base,.indirect.index=&index,.indirect.scale=scale,.indirect.offset=off}
