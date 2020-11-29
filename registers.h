#pragma once
#include <str.h>
struct reg;
STR_TYPE_DEF(struct reg *, RegP);
STR_TYPE_FUNCS(struct reg *, RegP);
struct reg {
	const char *name;
	strRegP affects;
	int size;
};
struct regSlice {
	struct reg *reg;
	int offset, widthInBits;
};
extern strRegP regsX86GP;
extern strRegP regsTestGP;

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
