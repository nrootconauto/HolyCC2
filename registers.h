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
	struct reg *masterReg;
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
extern struct reg regX86SPL;
extern struct reg regX86BPL;
extern struct reg regX86SIL;
extern struct reg regX86DIL;

extern struct reg regX86AH;
extern struct reg regX86BH;
extern struct reg regX86CH;
extern struct reg regX86DH;

extern struct reg regAMD64R8u8;
extern struct reg regAMD64R9u8;
extern struct reg regAMD64R10u8;
extern struct reg regAMD64R11u8;
extern struct reg regAMD64R12u8;
extern struct reg regAMD64R13u8;
extern struct reg regAMD64R14u8;
extern struct reg regAMD64R15u8;

extern struct reg regX86AX;
extern struct reg regX86BX;
extern struct reg regX86CX;
extern struct reg regX86DX;
extern struct reg regX86SI;
extern struct reg regX86DI;
extern struct reg regX86BP;
extern struct reg regX86SP;
extern struct reg regAMD64R8u16;
extern struct reg regAMD64R9u16;
extern struct reg regAMD64R10u16;
extern struct reg regAMD64R11u16;
extern struct reg regAMD64R12u16;
extern struct reg regAMD64R13u16;
extern struct reg regAMD64R14u16;
extern struct reg regAMD64R15u16;

extern struct reg regX86EAX;
extern struct reg regX86EBX;
extern struct reg regX86ECX;
extern struct reg regX86EDX;
extern struct reg regX86ESI;
extern struct reg regX86EDI;
extern struct reg regX86EBP;
extern struct reg regX86ESP;
extern struct reg regAMD64R8u32;
extern struct reg regAMD64R9u32;
extern struct reg regAMD64R10u32;
extern struct reg regAMD64R11u32;
extern struct reg regAMD64R12u32;
extern struct reg regAMD64R13u32;
extern struct reg regAMD64R14u32;
extern struct reg regAMD64R15u32;

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

extern struct reg regAMD64RAX;
extern struct reg regAMD64RBX;
extern struct reg regAMD64RCX;
extern struct reg regAMD64RDX;
extern struct reg regAMD64RSP;
extern struct reg regAMD64RBP;
extern struct reg regAMD64RDI;
extern struct reg regAMD64RSI;

extern struct reg regAMD64R8u64;
extern struct reg regAMD64R9u64;
extern struct reg regAMD64R10u64;
extern struct reg regAMD64R11u64;
extern struct reg regAMD64R12u64;
extern struct reg regAMD64R13u64;
extern struct reg regAMD64R14u64;
extern struct reg regAMD64R15u64;

extern struct reg regX86MM0;
extern struct reg regX86MM1;
extern struct reg regX86MM2;
extern struct reg regX86MM3;
extern struct reg regX86MM4;
extern struct reg regX86MM5;
extern struct reg regX86MM6;
extern struct reg regX86MM7;

extern struct reg regX86ES, regX86CS, regX86SS, regX86DS, regX86FS, regX86GS;
enum archConfig {
	ARCH_TEST_SYSV,
	ARCH_X86_SYSV,
	ARCH_X64_SYSV,
};
enum archEndian {
	ENDIAN_LITTLE,
	ENDIAN_BIG,
};
void setArch(enum archConfig Arch);
int regSliceConflict(const struct regSlice *a, const struct regSlice *b);
strRegP regGetForType(struct object *type);
void initRegisters();
int regSliceCompare(const struct regSlice *a, const struct regSlice *b);
strRegP regsForArch();
enum archEndian archEndian();
enum archConfig getCurrentArch();
int regConflict(struct reg *a, struct reg *b);
struct reg *subRegOfType(struct reg *r, struct object *type);
long ptrSize();
struct reg *basePointer();
struct reg *stackPointer();
long dataSize();
