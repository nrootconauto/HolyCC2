#include "opcodesParser.h"
#include <assert.h>
#include "cleanup.h"
#include "object.h"
#include "registers.h"
#include <stdarg.h>
#include <stdlib.h>
typedef int (*regCmpType)(const struct reg **, const struct reg **);
struct reg regX86AL;
struct reg regX86BL;
struct reg regX86CL;
struct reg regX86DL;
struct reg regX86SPL;
struct reg regX86BPL;
struct reg regX86SIL;
struct reg regX86DIL;

struct reg regX86AH;
struct reg regX86BH;
struct reg regX86CH;
struct reg regX86DH;

struct reg regX86AX;
struct reg regX86BX;
struct reg regX86CX;
struct reg regX86DX;
struct reg regX86SI;
struct reg regX86DI;
struct reg regX86BP;
struct reg regX86SP;

struct reg regX86EAX;
struct reg regX86EBX;
struct reg regX86ECX;
struct reg regX86EDX;
struct reg regX86ESI;
struct reg regX86EDI;
struct reg regX86EBP;
struct reg regX86ESP;

struct reg regX86XMM0;
struct reg regX86XMM1;
struct reg regX86XMM2;
struct reg regX86XMM3;
struct reg regX86XMM4;
struct reg regX86XMM5;
struct reg regX86XMM6;
struct reg regX86XMM7;

struct reg regX86ST0;
struct reg regX86ST1;
struct reg regX86ST2;
struct reg regX86ST3;
struct reg regX86ST4;
struct reg regX86ST5;
struct reg regX86ST6;
struct reg regX86ST7;

struct reg regX86ES;
struct reg regX86CS;
struct reg regX86SS;
struct reg regX86DS;
struct reg regX86FS;
struct reg regX86GS;

static int ptrPtrCmp(const void *a, const void *b) {
		if(*(void**)a>*(void**)b)
				return 1;
		else if(*(void**)a<*(void**)b)
				return -1;
		return 0;
}
typedef int (*regPCmpType)(const struct reg **, const struct reg **);
struct regSlice createRegSlice(const struct reg *reg, int offset, int width) {
	struct regSlice slice;
	slice.reg = (void *)reg;
	slice.offset = offset;
	slice.widthInBits = width;
	slice.type = NULL;

	return slice;
}
static void createRegister(struct reg *writeTo,const char *name, struct reg *masterReg, int size, enum regType type, int affectsCount, ...) {
	struct reg retVal;
	retVal.masterReg = masterReg;
	retVal.size = size;
	retVal.name = name;
	retVal.type = type;
	retVal.affects = NULL;

	va_list list;
	va_start(list, affectsCount);
	for (int i = 0; i != affectsCount; i++) {
		__auto_type affects = va_arg(list, struct regSlice);

		retVal.affects = strRegSliceSortedInsert(retVal.affects, affects, regSliceCompare);
		// Clone the affects->reg move move by offset
		strRegSlice clone CLEANUP(strRegSliceDestroy) = strRegSliceClone(affects.reg->affects);
		for (long i = 0; i != strRegSliceSize(clone); i++)
			clone[i].offset += affects.offset;
		retVal.affects = strRegSliceSetUnion(retVal.affects, clone, regSliceCompare);
	}
	va_end(list);
	
	struct regSlice self;
	self.offset=0;
	self.reg=writeTo;
	self.type=NULL;
	self.widthInBits=size*8;
	retVal.affects = strRegSliceSortedInsert(retVal.affects, self, regSliceCompare);
	
	*writeTo=retVal;
}

static strRegP regsX86;
static strRegP regsAMD64;
static strRegP regsTest;
static strRegP regsX86FloatGiant;

struct reg regAMD64R8u8;
struct reg regAMD64R9u8;
struct reg regAMD64R10u8;
struct reg regAMD64R11u8;
struct reg regAMD64R12u8;
struct reg regAMD64R13u8;
struct reg regAMD64R14u8;
struct reg regAMD64R15u8;

struct reg regAMD64R8u16;
struct reg regAMD64R9u16;
struct reg regAMD64R10u16;
struct reg regAMD64R11u16;
struct reg regAMD64R12u16;
struct reg regAMD64R13u16;
struct reg regAMD64R14u16;
struct reg regAMD64R15u16;

struct reg regAMD64R8u32;
struct reg regAMD64R9u32;
struct reg regAMD64R10u32;
struct reg regAMD64R11u32;
struct reg regAMD64R12u32;
struct reg regAMD64R13u32;
struct reg regAMD64R14u32;
struct reg regAMD64R15u32;

struct reg regAMD64RAX;
struct reg regAMD64RBX;
struct reg regAMD64RCX;
struct reg regAMD64RDX;
struct reg regAMD64RSP;
struct reg regAMD64RBP;
struct reg regAMD64RDI;
struct reg regAMD64RSI;
struct reg regAMD64R8u64;
struct reg regAMD64R9u64;
struct reg regAMD64R10u64;
struct reg regAMD64R11u64;
struct reg regAMD64R12u64;
struct reg regAMD64R13u64;
struct reg regAMD64R14u64;
struct reg regAMD64R15u64;

struct reg regX86MM0;
struct reg regX86MM1;
struct reg regX86MM2;
struct reg regX86MM3;
struct reg regX86MM4;
struct reg regX86MM5;
struct reg regX86MM6;
struct reg regX86MM7;
void initRegisters() {
		createRegister(&regX86MM0,"MM0", NULL, 8, REG_TYPE_UNUSED, 0);
	 createRegister(&regX86MM1,"MM1", NULL, 8, REG_TYPE_UNUSED, 0);
	 createRegister(&regX86MM2,"MM2", NULL, 8, REG_TYPE_UNUSED, 0);
	 createRegister(&regX86MM3,"MM3", NULL, 8, REG_TYPE_UNUSED, 0);
	 createRegister(&regX86MM4,"MM4", NULL, 8, REG_TYPE_UNUSED, 0);
	 createRegister(&regX86MM5,"MM5", NULL, 8, REG_TYPE_UNUSED, 0);
	 createRegister(&regX86MM6,"MM6", NULL, 8, REG_TYPE_UNUSED, 0);
	 createRegister(&regX86MM7,"MM7", NULL, 8, REG_TYPE_UNUSED, 0);

	 createRegister(&regX86ES,"ES", NULL, 2, REG_TYPE_GP, 0);
	 createRegister(&regX86CS,"CS", NULL, 2, REG_TYPE_GP, 0);
	 createRegister(&regX86SS,"SS", NULL, 2, REG_TYPE_GP, 0);
	 createRegister(&regX86DS,"DS", NULL, 2, REG_TYPE_GP, 0);
	 createRegister(&regX86FS,"FS", NULL, 2, REG_TYPE_GP, 0);
	 createRegister(&regX86GS,"GS", NULL, 2, REG_TYPE_GP, 0);

	 createRegister(&regAMD64R8u8,"R8u8", &regAMD64R8u64, 1, REG_TYPE_GP, 0);
	 createRegister(&regAMD64R9u8,"R9u8", &regAMD64R9u64, 1, REG_TYPE_GP, 0);
	 createRegister(&regAMD64R10u8,"R10u8", &regAMD64R10u64, 1, REG_TYPE_GP, 0);
	 createRegister(&regAMD64R11u8,"R11u8", &regAMD64R11u64, 1, REG_TYPE_GP, 0);
		createRegister(&regAMD64R12u8,"R12u8", &regAMD64R12u64, 1, REG_TYPE_GP, 0);
	 createRegister(&regAMD64R13u8,"R13u8", &regAMD64R13u64, 1, REG_TYPE_GP, 0);
	 createRegister(&regAMD64R14u8,"R14u8", &regAMD64R14u64, 1, REG_TYPE_GP, 0);
	 createRegister(&regAMD64R15u8,"R15u8", &regAMD64R15u64, 1, REG_TYPE_GP, 0);

	 createRegister(&regAMD64R8u16,"R8u16", &regAMD64R8u64, 2, REG_TYPE_GP, 1, createRegSlice(&regAMD64R8u8, 0, 8));
	 createRegister(&regAMD64R9u16,"R9u16", &regAMD64R9u64, 2, REG_TYPE_GP, 1, createRegSlice(&regAMD64R9u8, 0, 8));
		createRegister(&regAMD64R10u16,"R12u16", &regAMD64R10u64, 2, REG_TYPE_GP, 1, createRegSlice(&regAMD64R10u8, 0, 8));
	 createRegister(&regAMD64R11u16,"R11u16", &regAMD64R11u64, 2, REG_TYPE_GP, 1, createRegSlice(&regAMD64R11u8, 0, 8));
	 createRegister(&regAMD64R12u16,"R12u16", &regAMD64R12u64, 2, REG_TYPE_GP, 1, createRegSlice(&regAMD64R12u8, 0, 8));
	 createRegister(&regAMD64R13u16,"R13u16", &regAMD64R13u64, 2, REG_TYPE_GP, 1, createRegSlice(&regAMD64R13u8, 0, 8));
	 createRegister(&regAMD64R14u16,"R14u16", &regAMD64R14u64, 2, REG_TYPE_GP, 1, createRegSlice(&regAMD64R14u8, 0, 8));
	 createRegister(&regAMD64R15u16,"R15u16", &regAMD64R15u64, 2, REG_TYPE_GP, 1, createRegSlice(&regAMD64R15u8, 0, 8));

	 createRegister(&regAMD64R8u32,"R8u32", &regAMD64R8u64, 4, REG_TYPE_GP, 1, createRegSlice(&regAMD64R8u16, 0, 16));
	 createRegister(&regAMD64R9u32,"R9u32", &regAMD64R9u64, 4, REG_TYPE_GP, 1, createRegSlice(&regAMD64R9u16, 0, 16));
		createRegister(&regAMD64R10u32,"R12u32", &regAMD64R10u64, 4, REG_TYPE_GP, 1, createRegSlice(&regAMD64R10u16, 0, 16));
	 createRegister(&regAMD64R11u32,"R11u32", &regAMD64R11u64, 4, REG_TYPE_GP, 1, createRegSlice(&regAMD64R11u16, 0, 16));
	 createRegister(&regAMD64R12u32,"R12u32", &regAMD64R12u64, 4, REG_TYPE_GP, 1, createRegSlice(&regAMD64R12u16, 0, 16));
		createRegister(&regAMD64R13u32,"R13u32", &regAMD64R13u64, 4, REG_TYPE_GP, 1, createRegSlice(&regAMD64R13u16, 0, 16));
	 createRegister(&regAMD64R14u32,"R14u32", &regAMD64R14u64, 4, REG_TYPE_GP, 1, createRegSlice(&regAMD64R14u16, 0, 16));
	 createRegister(&regAMD64R15u32,"R15u32", &regAMD64R15u64, 4, REG_TYPE_GP, 1, createRegSlice(&regAMD64R15u16, 0, 16));

	 createRegister(&regAMD64R8u64,"R8u64", NULL, 8, REG_TYPE_GP, 1, createRegSlice(&regAMD64R8u32, 0, 32));
	 createRegister(&regAMD64R9u64,"R9u64", NULL, 8, REG_TYPE_GP, 1, createRegSlice(&regAMD64R9u32, 0, 32));
	 createRegister(&regAMD64R10u64,"R12u64", NULL, 8, REG_TYPE_GP, 1, createRegSlice(&regAMD64R10u32, 0, 32));
	 createRegister(&regAMD64R11u64,"R11u64", NULL, 8, REG_TYPE_GP, 1, createRegSlice(&regAMD64R11u32, 0, 32));
	 createRegister(&regAMD64R12u64,"R12u64", NULL, 8, REG_TYPE_GP, 1, createRegSlice(&regAMD64R12u32, 0, 32));
	 createRegister(&regAMD64R13u64,"R13u64", NULL, 8, REG_TYPE_GP, 1, createRegSlice(&regAMD64R13u32, 0, 32));
	 createRegister(&regAMD64R14u64,"R14u64", NULL, 8, REG_TYPE_GP, 1, createRegSlice(&regAMD64R14u32, 0, 32));
	 createRegister(&regAMD64R15u64,"R15u64", NULL, 8, REG_TYPE_GP, 1, createRegSlice(&regAMD64R15u32, 0, 32));

	 createRegister(&regX86AL,"AL", &regAMD64RAX, 1, REG_TYPE_GP, 0);
	 createRegister(&regX86BL,"BL", &regAMD64RBX, 1, REG_TYPE_GP, 0);
	 createRegister(&regX86CL,"CL", &regAMD64RCX, 1, REG_TYPE_GP, 0);
	 createRegister(&regX86DL,"DL", &regAMD64RDX, 1, REG_TYPE_GP, 0);
		createRegister(&regX86SPL,"SPL", &regAMD64RSP, 1, REG_TYPE_GP, 0);
	 createRegister(&regX86BPL,"BPL", &regAMD64RBP, 1, REG_TYPE_GP, 0);
	 createRegister(&regX86SIL,"SIL", &regAMD64RSI, 1, REG_TYPE_GP, 0);
	 createRegister(&regX86DIL,"DIL", &regAMD64RDI, 1, REG_TYPE_GP, 0);

	 createRegister(&regX86AH,"AH", &regAMD64RAX, 1, REG_TYPE_GP, 0);
	 createRegister(&regX86BH,"BH", &regAMD64RBX, 1, REG_TYPE_GP, 0);
	 createRegister(&regX86CH,"CH", &regAMD64RCX, 1, REG_TYPE_GP, 0);
	 createRegister(&regX86DH,"DH", &regAMD64RDX, 1, REG_TYPE_GP, 0);

	 createRegister(&regX86AX,"AX", &regAMD64RAX, 2, REG_TYPE_GP, 2, createRegSlice(&regX86AL, 0, 8), createRegSlice(&regX86AH, 8, 8));
	 createRegister(&regX86BX,"BX", &regAMD64RBX, 2, REG_TYPE_GP, 2, createRegSlice(&regX86BL, 0, 8), createRegSlice(&regX86BH, 8, 8));
	 createRegister(&regX86CX,"CX", &regAMD64RCX, 2, REG_TYPE_GP, 2, createRegSlice(&regX86CL, 0, 8), createRegSlice(&regX86CH, 8, 8));
	 createRegister(&regX86DX,"DX", &regAMD64RDX, 2, REG_TYPE_GP, 2, createRegSlice(&regX86DL, 0, 8), createRegSlice(&regX86DH, 8, 8));
	 createRegister(&regX86SI,"SI", &regAMD64RSI, 2, REG_TYPE_GP, 1, createRegSlice(&regX86SIL, 0, 8));
	 createRegister(&regX86DI,"DI", &regAMD64RDI, 2, REG_TYPE_GP, 1, createRegSlice(&regX86DIL, 0, 8));
	 createRegister(&regX86BP,"BP", &regAMD64RBP, 2, REG_TYPE_GP, 1, createRegSlice(&regX86BPL, 0, 8));
	 createRegister(&regX86SP,"SP", &regAMD64RSP, 2, REG_TYPE_GP, 1, createRegSlice(&regX86SPL, 0, 8));

	 createRegister(&regX86EAX,"EAX", &regAMD64RAX, 4, REG_TYPE_GP, 1, createRegSlice(&regX86AX, 0, 16));
	 createRegister(&regX86EBX,"EBX", &regAMD64RBX, 4, REG_TYPE_GP, 1, createRegSlice(&regX86BX, 0, 16));
	 createRegister(&regX86ECX,"ECX", &regAMD64RCX, 4, REG_TYPE_GP, 1, createRegSlice(&regX86CX, 0, 16));
	 createRegister(&regX86EDX,"EDX", &regAMD64RDX, 4, REG_TYPE_GP, 1, createRegSlice(&regX86DX, 0, 16));
	 createRegister(&regX86ESI,"ESI", &regAMD64RSI, 4, REG_TYPE_GP, 1, createRegSlice(&regX86SI, 0, 16));
	 createRegister(&regX86EDI,"EDI", &regAMD64RDI, 4, REG_TYPE_GP, 1, createRegSlice(&regX86DI, 0, 16));
	 createRegister(&regX86EBP,"EBP", &regAMD64RBP, 4, REG_TYPE_GP, 1, createRegSlice(&regX86BP, 0, 16));
	 createRegister(&regX86ESP,"ESP", &regAMD64RSP, 4, REG_TYPE_GP, 1, createRegSlice(&regX86SP, 0, 16));

	 createRegister(&regAMD64RAX,"RAX", NULL, 8, REG_TYPE_GP, 1, createRegSlice(&regX86EAX, 0, 32));
	 createRegister(&regAMD64RBX,"RBX", NULL, 8, REG_TYPE_GP, 1, createRegSlice(&regX86EBX, 0, 32));
	 createRegister(&regAMD64RCX,"RCX", NULL, 8, REG_TYPE_GP, 1, createRegSlice(&regX86ECX, 0, 32));
	 createRegister(&regAMD64RDX,"RDX", NULL, 8, REG_TYPE_GP, 1, createRegSlice(&regX86EDX, 0, 32));
	 createRegister(&regAMD64RSP,"RSP", NULL, 8, REG_TYPE_GP, 1, createRegSlice(&regX86ESP, 0, 32));
	 createRegister(&regAMD64RBP,"RBP", NULL, 8, REG_TYPE_GP, 1, createRegSlice(&regX86EBP, 0, 32));
	 createRegister(&regAMD64RDI,"RDI", NULL, 8, REG_TYPE_GP, 1, createRegSlice(&regX86EDI, 0, 32));
	 createRegister(&regAMD64RSI,"RSI", NULL, 8, REG_TYPE_GP, 1, createRegSlice(&regX86ESI, 0, 32));

	 createRegister(&regX86XMM0,"XMM0", NULL, 16, REG_TYPE_FLOATING, 0);
	 createRegister(&regX86XMM1,"XMM1", NULL, 16, REG_TYPE_FLOATING, 0);
	 createRegister(&regX86XMM2,"XMM2", NULL, 16, REG_TYPE_FLOATING, 0);
	 createRegister(&regX86XMM3,"XMM3", NULL, 16, REG_TYPE_FLOATING, 0);
	 createRegister(&regX86XMM4,"XMM4", NULL, 16, REG_TYPE_FLOATING, 0);
	 createRegister(&regX86XMM5,"XMM5", NULL, 16, REG_TYPE_FLOATING, 0);
	 createRegister(&regX86XMM6,"XMM6", NULL, 16, REG_TYPE_FLOATING, 0);
		createRegister(&regX86XMM7,"XMM7", NULL, 16, REG_TYPE_FLOATING, 0);

	 createRegister(&regX86ST0,"ST0", NULL, 16, REG_TYPE_FLOATING, 0);
	 createRegister(&regX86ST1,"ST1", NULL, 16, REG_TYPE_FLOATING, 0);
	 createRegister(&regX86ST2,"ST2", NULL, 16, REG_TYPE_FLOATING, 0);
	 createRegister(&regX86ST3,"ST3", NULL, 16, REG_TYPE_FLOATING, 0);
	 createRegister(&regX86ST4,"ST4", NULL, 16, REG_TYPE_FLOATING, 0);
	 createRegister(&regX86ST5,"ST5", NULL, 16, REG_TYPE_FLOATING, 0);
	 createRegister(&regX86ST6,"ST6", NULL, 16, REG_TYPE_FLOATING, 0);
	 createRegister(&regX86ST7,"ST7", NULL, 16, REG_TYPE_FLOATING, 0);

	// General purpose x86
	struct reg *gpX86[] = {
	    &regX86AL,  &regX86BL,  &regX86CL,  &regX86DL,

	    &regX86AH,  &regX86BH,  &regX86CH,  &regX86DH,

	    &regX86AX,  &regX86BX,

	    &regX86CX,  &regX86DX,  &regX86SI,  &regX86DI,  &regX86BP,  &regX86SP,

	    &regX86EAX, &regX86EBX, &regX86ECX, &regX86EDX, &regX86ESI, &regX86EDI, &regX86EBP, &regX86ESP,

	    &regX86ST0, &regX86ST1, &regX86ST2, &regX86ST3, &regX86ST4, &regX86ST5, &regX86ST6, &regX86ST7,
	};
	long len = sizeof(gpX86) / sizeof(*gpX86);
	qsort(gpX86, len, sizeof(*gpX86), ptrPtrCmp);
	regsX86 = strRegPAppendData(NULL, (void *)gpX86, len);

	struct reg *gpTest[] = {
	    &regX86AL,
	    &regX86BL,
	    &regX86CL,
	    //
	    &regX86AH,
	    &regX86BH,
	    &regX86CH,
	    //
	    &regX86AX,
	    &regX86BX,
	    &regX86CX,
	    //
	    &regX86EAX,
	    &regX86EBX,
	    &regX86ECX,
	    //
	    &regX86ST0,
	    &regX86ST1,
	    &regX86ST2,
	};
	len = sizeof(gpTest) / sizeof(*gpTest);
	qsort(gpTest, len, sizeof(*gpTest), ptrPtrCmp);
	regsTest = strRegPAppendData(NULL, (void *)gpTest, len);

	struct reg *amd64Regs[] = {
	    &regX86AL,      &regX86BL,      &regX86CL,       &regX86DL,       &regX86AH,       &regX86BH,       &regX86CH,       &regX86DH,
	    &regAMD64R8u8,  &regAMD64R9u8,  &regAMD64R10u8,  &regAMD64R11u8,  &regAMD64R12u8,  &regAMD64R13u8,  &regAMD64R14u8,  &regAMD64R15u8,

	    &regAMD64R8u16, &regAMD64R9u16, &regAMD64R10u16, &regAMD64R11u16, &regAMD64R12u16, &regAMD64R13u16, &regAMD64R14u16, &regAMD64R15u16,

	    &regAMD64R8u32, &regAMD64R9u32, &regAMD64R10u32, &regAMD64R11u32, &regAMD64R12u32, &regAMD64R13u32, &regAMD64R14u32, &regAMD64R15u32,

	    &regAMD64R8u64, &regAMD64R9u64, &regAMD64R10u64, &regAMD64R11u64, &regAMD64R12u64, &regAMD64R13u64, &regAMD64R14u64, &regAMD64R15u64,

	    &regX86AX,      &regX86BX,

	    &regX86CX,      &regX86DX,      &regX86SI,       &regX86DI,       &regX86BP,       &regX86SP,

	    &regX86EAX,     &regX86EBX,     &regX86ECX,      &regX86EDX,      &regX86ESI,      &regX86EDI,      &regX86EBP,      &regX86ESP,

	    &regAMD64RAX,   &regAMD64RBX,   &regAMD64RCX,    &regAMD64RDX,    &regAMD64RSI,    &regAMD64RDI,    &regAMD64RBP,    &regAMD64RSP,

	    &regX86ES,      &regX86CS,      &regX86SS,       &regX86DS,       &regX86FS,       &regX86SS,       &regX86GS,

	    &regX86ST0,     &regX86ST1,     &regX86ST2,      &regX86ST3,      &regX86ST4,      &regX86ST5,      &regX86ST6,      &regX86ST7,
	    &regX86MM0,     &regX86MM1,     &regX86MM2,      &regX86MM3,      &regX86MM4,      &regX86MM5,      &regX86MM6,      &regX86MM7,
	    &regX86XMM0,    &regX86XMM1,    &regX86XMM2,     &regX86XMM3,     &regX86XMM4,     &regX86XMM5,     &regX86XMM6,     &regX86XMM7,
	};
	len = sizeof(amd64Regs) / sizeof(*amd64Regs);
	qsort(amd64Regs, len, sizeof(*amd64Regs), ptrPtrCmp);
	regsAMD64 = strRegPAppendData(NULL, (void *)amd64Regs, len);
}
static enum archConfig currentArch = ARCH_X86_SYSV;
void setArch(enum archConfig Arch) {
	currentArch = Arch;
}
const strRegP getSIMDRegs() {
	assert(0);
	return NULL;
}
int regSliceConflict(const struct regSlice *a, const struct regSlice *b) {
	const struct reg *aSuper = a->reg;
	while (aSuper->masterReg)
		aSuper = aSuper->masterReg;
	const struct reg *bSuper = b->reg;
	while (bSuper->masterReg)
		bSuper = bSuper->masterReg;
	if (aSuper != bSuper)
		return 0;
	// Find a's register in super and get it's offset
	long aRegOffsetInSuper = -1;
	// Registers inherit the slices of their "children" registers
	for (long i = 0; i != strRegSliceSize(aSuper->affects); i++)
		if (aSuper->affects[i].reg == a->reg)
			aRegOffsetInSuper = aSuper->affects[i].offset;
	if(aRegOffsetInSuper==-1)
			assert(aRegOffsetInSuper != -1);
	// Same with b
	long bRegOffsetInSuper = -1;
	// Registers inherit the slices of their "children" registers
	for (long i = 0; i != strRegSliceSize(bSuper->affects); i++)
		if (bSuper->affects[i].reg == b->reg)
			bRegOffsetInSuper = bSuper->affects[i].offset;
	if(bRegOffsetInSuper == -1)
			assert(bRegOffsetInSuper != -1);

	int aEnd = a->offset + a->widthInBits + aRegOffsetInSuper;
	int bEnd = b->offset + b->widthInBits + bRegOffsetInSuper;
	int aOffset = a->offset + aRegOffsetInSuper;
	int bOffset = b->offset + bRegOffsetInSuper;

	if (aSuper != bSuper)
		return 0;

	if (aOffset >= bOffset)
		if (bEnd > aOffset)
			return 1;

	if (bOffset >= aOffset)
		if (aEnd > bOffset)
			return 1;

	return 0;
}
int regConflict(struct reg *a, struct reg *b) {
		if(a==b) return 1;
		struct regSlice A;
	A.offset = 0, A.reg = a, A.widthInBits = a->size * 8;
	struct regSlice B;
	B.offset = 0, B.reg = b, B.widthInBits = b->size * 8;
	return regSliceConflict(&A, &B);
}
strRegP regsForArch() {
	switch (currentArch) {
	case ARCH_TEST_SYSV:
		return strRegPClone(regsTest);
	case ARCH_X64_SYSV:
		return strRegPClone(regsAMD64);
	case ARCH_X86_SYSV:
		return strRegPClone(regsX86);
	}
}
strRegP regGetForType(struct object *type) {
	const struct object *ints[] = {
			&typeBool,&typeU8i, &typeU16i, &typeU32i, &typeU64i, &typeI8i, &typeI16i, &typeI32i, &typeI64i,
	};
	qsort(ints, sizeof(ints) / sizeof(*ints), sizeof(*ints), ptrPtrCmp);

	type = objectBaseType(type);

	int success;
	__auto_type size = objectSize(type, &success);
	if (!success)
		return NULL;

	strRegP retVal = NULL;
	strRegP avail CLEANUP(strRegPDestroy) = NULL;
	switch (currentArch) {
	case ARCH_X64_SYSV: {
		avail = regsForArch();
		// Reserved registers
		const struct reg *res[] = {&regX86XMM0,&regAMD64RSP,&regAMD64RBP,&regX86ESP, &regX86EBP, &regX86SP, &regX86BP, &regX86SPL, &regX86BPL,
				&regX86SS,  &regX86CS,  &regX86DS, &regX86ES, &regX86FS,  &regX86GS,&regAMD64RAX,&regX86EAX,&regX86AX,&regX86AH,&regX86AL,
				//REX doesn't permit A/B/C/DH
				&regX86BH,&regX86CH,&regX86DH,
		};
		long len = sizeof(res) / sizeof(*res);
		qsort(res, len, sizeof(*res), ptrPtrCmp);
		strRegP reserved CLEANUP(strRegPDestroy) = strRegPAppendData(NULL, res, len);
		avail = strRegPSetDifference(avail, reserved, (regPCmpType)ptrPtrCmp);
		break;
	}
	case ARCH_TEST_SYSV:
	case ARCH_X86_SYSV: {
		avail = regsForArch();
		// Reserved registers
		const struct reg *res[] = {&regX86ESP, &regX86EBP, &regX86SP, &regX86BP, &regX86SPL, &regX86BPL,
				&regX86SS,  &regX86CS,  &regX86DS, &regX86ES, &regX86FS,  &regX86GS,&regX86EAX,&regX86AX,&regX86AH,&regX86AL};
		long len = sizeof(res) / sizeof(*res);
		qsort(res, len, sizeof(*res), ptrPtrCmp);
		strRegP reserved CLEANUP(strRegPDestroy) = strRegPAppendData(NULL, res, len);
		avail = strRegPSetDifference(avail, reserved, (regPCmpType)ptrPtrCmp);
	}
	}

search:
	if (objectEqual(type, &typeF64)) {
		for (long i = 0; i != strRegPSize(avail); i++) {
			if (avail[i]->type & REG_TYPE_FLOATING) {
				retVal = strRegPSortedInsert(retVal, avail[i], (regPCmpType)ptrPtrCmp);
			}
			if (currentArch == ARCH_X64_SYSV) {
				// Exclude x87FPU registers
				struct reg *fpu[] = {
				    &regX86ST0, &regX86ST1, &regX86ST2, &regX86ST3, &regX86ST4, &regX86ST5, &regX86ST6, &regX86ST7,
				};
				long len = sizeof(fpu) / sizeof(*fpu);
				qsort(fpu, len, sizeof(*fpu), ptrPtrCmp);
				strRegP exclude CLEANUP(strRegPDestroy) = strRegPAppendData(NULL, (const struct reg **)fpu, len);
				retVal = strRegPSetDifference(retVal, exclude, (regPCmpType)ptrPtrCmp);
			}
		}
	} else {
		for (long i = 0; i != strRegPSize(avail); i++) {
			if (avail[i]->type & REG_TYPE_GP) {
				if (bsearch(&type, ints, sizeof(ints) / sizeof(*ints), sizeof(*ints), ptrPtrCmp) || type->type == TYPE_PTR||type->type==TYPE_ARRAY) {
					if (avail[i]->size == objectSize(type, NULL))
						retVal = strRegPSortedInsert(retVal, avail[i], (regPCmpType)ptrPtrCmp);
				}
			}
		}
	}

	return retVal;
}
int regSliceCompare(const struct regSlice *a, const struct regSlice *b) {
	if (a->reg != b->reg)
		return ptrPtrCmp(&a->reg, &b->reg);
	else if (a->offset != b->offset)
		return a->offset - b->offset;
	else if (a->widthInBits != b->widthInBits)
		return a->widthInBits != b->widthInBits;

	return 0;
}
enum archEndian archEndian() {
	switch (currentArch) {
	case ARCH_X86_SYSV:
	case ARCH_X64_SYSV:
	case ARCH_TEST_SYSV:
		return ENDIAN_LITTLE;
	}
}
enum archConfig getCurrentArch() {
	return currentArch;
}
struct reg *subRegOfType(struct reg *r, struct object *type) {
	long iSize = objectSize(type, NULL);
	if (iSize == r->size)
		return r;
	// Use the lower size part of register
	strRegSlice *affects = &r->affects;
	struct regSlice *subRegister = NULL;
	for (long i = 0; i != strRegSliceSize(*affects); i++) {
		if (affects[0][i].offset == 0 && iSize * 8 == affects[0][i].widthInBits) {
			subRegister = &affects[0][i];
			break;
		}
	}
	if (!subRegister)
		return NULL;
	// Certian sub registers aren't permissible in some archs
	strRegP exclude4Arch CLEANUP(strRegPDestroy) = NULL;
	switch (getCurrentArch()) {
	case ARCH_X64_SYSV:
		break;
	case ARCH_X86_SYSV:
	case ARCH_TEST_SYSV: {
		const struct reg *exclude[] = {
		    &regX86DIL,
		    &regX86SIL,
		    &regX86SPL,
		    &regX86BPL,
		};
		long len = sizeof(exclude) / sizeof(*exclude);
		qsort(exclude, len, sizeof(*exclude), ptrPtrCmp);
		exclude4Arch = strRegPAppendData(exclude4Arch, exclude, len);
		break;
	}
	}
	if (strRegPSortedFind(exclude4Arch, subRegister->reg, (regPCmpType)ptrPtrCmp))
		return NULL;
	return subRegister->reg;
}
long ptrSize() {
	switch (getCurrentArch()) {
	case ARCH_X64_SYSV:
		return 8;
	case ARCH_TEST_SYSV:
	case ARCH_X86_SYSV:
		return 4;
	}
}
long dataSize() {
	switch (getCurrentArch()) {
	case ARCH_X64_SYSV:
		return 8;
	case ARCH_TEST_SYSV:
	case ARCH_X86_SYSV:
		return 4;
	}
}
struct reg *basePointer() {
	return (ptrSize() == 4) ? &regX86EBP : &regAMD64RBP;
}
struct reg *stackPointer() {
	return ptrSize() == 4 ? &regX86ESP : &regAMD64RSP;
}
struct object *dftValType() {
	switch (getCurrentArch()) {
	case ARCH_TEST_SYSV:
	case ARCH_X86_SYSV:
		return &typeI32i;
	case ARCH_X64_SYSV:
		return &typeI64i;
	}
	assert(0);
}
struct X86AddressingMode *getAccumulatorForType(struct object *type) {
		if(getCurrentArch()==ARCH_X64_SYSV&&objectBaseType(type)==&typeF64)
				return X86AddrModeReg(&regX86XMM0,&typeF64);
		__auto_type sub=subRegOfType(&regAMD64RAX, type);
		if(!sub) return NULL;
		return X86AddrModeReg(sub, type);
}
static int containsRegister(struct reg *par, struct reg *r) {
		if(par==r)
				return 1;
	for (long a = 0; a != strRegSliceSize(par->affects); a++) {
		if (par->affects[a].reg == r)
			return 1;
	}
	return 0;
}
static struct reg *__registerContainingBoth(struct reg *master, struct reg *a, struct reg *b) {
		assert(a->masterReg == b->masterReg);
	for (long c = 0; c != strRegSliceSize(master->affects); c++) {
			if (containsRegister(master->affects[c].reg, a) && containsRegister(master->affects[c].reg, b)) {
					if(master->affects[c].reg==master) continue;
					return __registerContainingBoth(master->affects[c].reg, a, b);
			}
	}
	return master;
}
/**
 * Finds (smallest) register containing both registers
 */
static struct reg *smallestRegContainingBoth(struct reg *a, struct reg *b) {
		if(!a->masterReg) return __registerContainingBoth(a,a,b);
		return __registerContainingBoth(a->masterReg, a, b);
}
strRegP mergeConflictingRegs(strRegP conflicts) {
	//
	// If 2 items in conflicts have a common master register "merge" the registers and replace with the common register
	// 2 birds 1 stone.
	//
mergeLoop:
	for (long c1 = 0; c1 != strRegPSize(conflicts); c1++) {
		for (long c2 = 0; c2 != strRegPSize(conflicts); c2++) {
			if (conflicts[c1] == conflicts[c2])
				continue;
			if (conflicts[c1]->masterReg != conflicts[c2]->masterReg)
				continue;
			__auto_type common = smallestRegContainingBoth(conflicts[c1], conflicts[c2]);
			// Do a set differnece,then insert the common register
			strRegP dummy CLEANUP(strRegPDestroy) = NULL;
			dummy = strRegPSortedInsert(dummy, conflicts[c1], (regCmpType)ptrPtrCmp);
			dummy = strRegPSortedInsert(dummy, conflicts[c2], (regCmpType)ptrPtrCmp);
			conflicts = strRegPSetDifference(conflicts, dummy, (regCmpType)ptrPtrCmp);
			conflicts = strRegPSortedInsert(conflicts, common, (regCmpType)ptrPtrCmp);
			goto mergeLoop;
		}
	}

	return conflicts;
}
