#pragma once
#include <object.h>
#include <str.h>
struct reg;
STR_TYPE_DEF(struct reg *, RegP);
STR_TYPE_FUNCS(struct reg *, RegP);
STR_TYPE_DEF(struct regSlice, RegSlice);
enum regType {
	REG_TYPE_GP = 1,
	REG_TYPE_FLOATING = 2,
	REG_TYPE_SYSTEM = 4,
	REG_TYPE_STACK = 8,
	REG_TYPE_FRAME_PTR = 16,
	REG_TYPE_FRAME_SIMD = 32,
};
struct reg {
	const char *name;
	strRegSlice affects;
	int size;
	enum regType type;
};
struct regSlice {
	struct reg *reg;
	int offset, widthInBits;
		struct object *type;
};
STR_TYPE_FUNCS(struct regSlice, RegSlice);
extern struct reg regX86AL;
extern struct reg regX86BL;
extern struct reg regX86CL;
extern struct reg regX86DL;

extern struct reg regX86AH;
extern struct reg regX86BH;
extern struct reg regX86CH;
extern struct reg regX86DH;

extern struct reg regX86AX;
extern struct reg regX86BX;
extern struct reg regX86CX;
extern struct reg regX86DX;
extern struct reg regX86SI;
extern struct reg regX86DI;
extern struct reg regX86BP;
extern struct reg regX86SP;

extern struct reg regX86EAX;
extern struct reg regX86EBX;
extern struct reg regX86ECX;
extern struct reg regX86EDX;
extern struct reg regX86ESI;
extern struct reg regX86EDI;
extern struct reg regX86EBP;
extern struct reg regX86ESP;

extern struct reg regX86XMM0;
extern struct reg regX86XMM1;
extern struct reg regX86XMM2;
extern struct reg regX86XMM3;
extern struct reg regX86XMM4;
extern struct reg regX86XMM5;
extern struct reg regX86XMM6;
extern struct reg regX86XMM7;

extern struct reg regX86ST0;
extern struct reg regX86ST1;
extern struct reg regX86ST2;
extern struct reg regX86ST3;
extern struct reg regX86ST4;
extern struct reg regX86ST5;
extern struct reg regX86ST6;
extern struct reg regX86ST7;

enum archConfig {
	ARCH_TEST_SYSV,
	ARCH_X86_SYSV,
	ARCH_X64_SYSV,
};
void setArch(enum archConfig Arch);
int regSliceConflict(struct regSlice *a, struct regSlice *b);
strRegP regGetForType(struct object *type);
void initRegisters();
int regSliceCompare(const struct regSlice *a, const struct regSlice *b);
