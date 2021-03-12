#include <assert.h>
#include "cleanup.h"
#include "object.h"
#include "registers.h"
#include <stdarg.h>
#include <stdlib.h>
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
		return *(void**)a-*(void**)b;
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
static struct reg createRegister(const char *name, struct reg *masterReg, int size, enum regType type, int affectsCount, ...) {
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

	return retVal;
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
	regX86MM0 = createRegister("MM0", NULL, 8, REG_TYPE_GP, 0);
	regX86MM1 = createRegister("MM1", NULL, 8, REG_TYPE_GP, 0);
	regX86MM2 = createRegister("MM2", NULL, 8, REG_TYPE_GP, 0);
	regX86MM3 = createRegister("MM3", NULL, 8, REG_TYPE_GP, 0);
	regX86MM4 = createRegister("MM4", NULL, 8, REG_TYPE_GP, 0);
	regX86MM5 = createRegister("MM5", NULL, 8, REG_TYPE_GP, 0);
	regX86MM6 = createRegister("MM6", NULL, 8, REG_TYPE_GP, 0);
	regX86MM7 = createRegister("MM7", NULL, 8, REG_TYPE_GP, 0);

	regX86ES = createRegister("ES", NULL, 2, REG_TYPE_GP, 0);
	regX86CS = createRegister("CS", NULL, 2, REG_TYPE_GP, 0);
	regX86SS = createRegister("SS", NULL, 2, REG_TYPE_GP, 0);
	regX86DS = createRegister("DS", NULL, 2, REG_TYPE_GP, 0);
	regX86FS = createRegister("FS", NULL, 2, REG_TYPE_GP, 0);
	regX86GS = createRegister("GS", NULL, 2, REG_TYPE_GP, 0);

	regAMD64R8u8 = createRegister("R8u8", &regAMD64R8u64, 1, REG_TYPE_GP, 0);
	regAMD64R9u8 = createRegister("R9u8", &regAMD64R9u64, 1, REG_TYPE_GP, 0);
	regAMD64R10u8 = createRegister("R10u8", &regAMD64R10u64, 1, REG_TYPE_GP, 0);
	regAMD64R11u8 = createRegister("R11u8", &regAMD64R11u64, 1, REG_TYPE_GP, 0);
	regAMD64R12u8 = createRegister("R12u8", &regAMD64R12u64, 1, REG_TYPE_GP, 0);
	regAMD64R13u8 = createRegister("R13u8", &regAMD64R13u64, 1, REG_TYPE_GP, 0);
	regAMD64R14u8 = createRegister("R14u8", &regAMD64R14u64, 1, REG_TYPE_GP, 0);
	regAMD64R15u8 = createRegister("R15u8", &regAMD64R15u64, 1, REG_TYPE_GP, 0);

	regAMD64R8u16 = createRegister("R8u16", &regAMD64R8u64, 1, REG_TYPE_GP, 1, createRegSlice(&regAMD64R8u8, 0, 8));
	regAMD64R9u16 = createRegister("R9u16", &regAMD64R8u64, 1, REG_TYPE_GP, 1, createRegSlice(&regAMD64R9u8, 0, 8));
	regAMD64R10u16 = createRegister("R12u16", &regAMD64R10u64, 1, REG_TYPE_GP, 1, createRegSlice(&regAMD64R10u8, 0, 8));
	regAMD64R11u16 = createRegister("R11u16", &regAMD64R11u64, 1, REG_TYPE_GP, 1, createRegSlice(&regAMD64R11u8, 0, 8));
	regAMD64R12u16 = createRegister("R12u16", &regAMD64R12u64, 1, REG_TYPE_GP, 1, createRegSlice(&regAMD64R12u8, 0, 8));
	regAMD64R13u16 = createRegister("R13u16", &regAMD64R13u64, 1, REG_TYPE_GP, 1, createRegSlice(&regAMD64R13u8, 0, 8));
	regAMD64R14u16 = createRegister("R14u16", &regAMD64R14u64, 1, REG_TYPE_GP, 1, createRegSlice(&regAMD64R14u8, 0, 8));
	regAMD64R15u16 = createRegister("R15u16", &regAMD64R15u64, 1, REG_TYPE_GP, 1, createRegSlice(&regAMD64R15u8, 0, 8));

	regAMD64R8u32 = createRegister("R8u32", &regAMD64R8u64, 4, REG_TYPE_GP, 1, createRegSlice(&regAMD64R8u16, 0, 16));
	regAMD64R9u32 = createRegister("R9u32", &regAMD64R9u64, 4, REG_TYPE_GP, 1, createRegSlice(&regAMD64R9u16, 0, 16));
	regAMD64R10u32 = createRegister("R12u32", &regAMD64R10u64, 4, REG_TYPE_GP, 1, createRegSlice(&regAMD64R10u16, 0, 16));
	regAMD64R11u32 = createRegister("R11u32", &regAMD64R11u64, 4, REG_TYPE_GP, 1, createRegSlice(&regAMD64R11u16, 0, 16));
	regAMD64R12u32 = createRegister("R12u32", &regAMD64R12u64, 4, REG_TYPE_GP, 1, createRegSlice(&regAMD64R12u16, 0, 16));
	regAMD64R13u32 = createRegister("R13u32", &regAMD64R14u64, 4, REG_TYPE_GP, 1, createRegSlice(&regAMD64R13u16, 0, 16));
	regAMD64R14u32 = createRegister("R14u32", &regAMD64R14u64, 4, REG_TYPE_GP, 1, createRegSlice(&regAMD64R14u16, 0, 16));
	regAMD64R15u32 = createRegister("R15u32", &regAMD64R15u64, 4, REG_TYPE_GP, 1, createRegSlice(&regAMD64R15u16, 0, 16));

	regAMD64R8u64 = createRegister("R8u64", NULL, 8, REG_TYPE_GP, 1, createRegSlice(&regAMD64R8u32, 0, 32));
	regAMD64R9u64 = createRegister("R9u64", NULL, 8, REG_TYPE_GP, 1, createRegSlice(&regAMD64R9u32, 0, 32));
	regAMD64R10u64 = createRegister("R12u64", NULL, 8, REG_TYPE_GP, 1, createRegSlice(&regAMD64R10u32, 0, 32));
	regAMD64R11u64 = createRegister("R11u64", NULL, 8, REG_TYPE_GP, 1, createRegSlice(&regAMD64R11u32, 0, 32));
	regAMD64R12u64 = createRegister("R12u64", NULL, 8, REG_TYPE_GP, 1, createRegSlice(&regAMD64R12u32, 0, 32));
	regAMD64R13u64 = createRegister("R13u64", NULL, 8, REG_TYPE_GP, 1, createRegSlice(&regAMD64R13u32, 0, 32));
	regAMD64R14u64 = createRegister("R14u64", NULL, 8, REG_TYPE_GP, 1, createRegSlice(&regAMD64R14u32, 0, 32));
	regAMD64R15u64 = createRegister("R15u64", NULL, 8, REG_TYPE_GP, 1, createRegSlice(&regAMD64R15u32, 0, 32));

	regX86AL = createRegister("AL", &regAMD64RAX, 1, REG_TYPE_GP, 0);
	regX86BL = createRegister("BL", &regAMD64RBX, 1, REG_TYPE_GP, 0);
	regX86CL = createRegister("CL", &regAMD64RCX, 1, REG_TYPE_GP, 0);
	regX86DL = createRegister("DL", &regAMD64RDX, 1, REG_TYPE_GP, 0);
	regX86SPL = createRegister("SPL", &regAMD64RSP, 1, REG_TYPE_GP, 0);
	regX86BPL = createRegister("BPL", &regAMD64RBP, 1, REG_TYPE_GP, 0);
	regX86SIL = createRegister("SIL", &regAMD64RSI, 1, REG_TYPE_GP, 0);
	regX86DIL = createRegister("DIL", &regAMD64RDI, 1, REG_TYPE_GP, 0);

	regX86AH = createRegister("AH", &regAMD64RAX, 1, REG_TYPE_GP, 0);
	regX86BH = createRegister("BH", &regAMD64RBX, 1, REG_TYPE_GP, 0);
	regX86CH = createRegister("CH", &regAMD64RCX, 1, REG_TYPE_GP, 0);
	regX86DH = createRegister("DH", &regAMD64RDX, 1, REG_TYPE_GP, 0);

	regX86AX = createRegister("AX", &regAMD64RAX, 2, REG_TYPE_GP, 2, createRegSlice(&regX86AL, 0, 8), createRegSlice(&regX86AH, 8, 8));
	regX86BX = createRegister("BX", &regAMD64RBX, 2, REG_TYPE_GP, 2, createRegSlice(&regX86BL, 0, 8), createRegSlice(&regX86BH, 8, 8));
	regX86CX = createRegister("CX", &regAMD64RCX, 2, REG_TYPE_GP, 2, createRegSlice(&regX86CL, 0, 8), createRegSlice(&regX86CH, 8, 8));
	regX86DX = createRegister("DX", &regAMD64RDX, 2, REG_TYPE_GP, 2, createRegSlice(&regX86DL, 0, 8), createRegSlice(&regX86DH, 8, 8));
	regX86SI = createRegister("SI", &regAMD64RSI, 2, REG_TYPE_GP, 1, createRegSlice(&regX86SIL, 0, 8));
	regX86DI = createRegister("DI", &regAMD64RDI, 2, REG_TYPE_GP, 1, createRegSlice(&regX86DIL, 0, 8));
	regX86BP = createRegister("BP", &regAMD64RBP, 2, REG_TYPE_GP, 1, createRegSlice(&regX86BPL, 0, 8));
	regX86SP = createRegister("SP", &regAMD64RSP, 2, REG_TYPE_GP, 1, createRegSlice(&regX86SPL, 0, 8));

	regX86EAX = createRegister("EAX", &regAMD64RAX, 4, REG_TYPE_GP, 1, createRegSlice(&regX86AX, 0, 16));
	regX86EBX = createRegister("EBX", &regAMD64RBX, 4, REG_TYPE_GP, 1, createRegSlice(&regX86BX, 0, 16));
	regX86ECX = createRegister("ECX", &regAMD64RCX, 4, REG_TYPE_GP, 1, createRegSlice(&regX86CX, 0, 16));
	regX86EDX = createRegister("EDX", &regAMD64RDX, 4, REG_TYPE_GP, 1, createRegSlice(&regX86DX, 0, 16));
	regX86ESI = createRegister("ESI", &regAMD64RSI, 4, REG_TYPE_GP, 1, createRegSlice(&regX86SI, 0, 16));
	regX86EDI = createRegister("EDI", &regAMD64RDI, 4, REG_TYPE_GP, 1, createRegSlice(&regX86DI, 0, 16));
	regX86EBP = createRegister("EBP", &regAMD64RBP, 4, REG_TYPE_GP, 1, createRegSlice(&regX86BP, 0, 16));
	regX86ESP = createRegister("ESP", &regAMD64RSP, 4, REG_TYPE_GP, 1, createRegSlice(&regX86SP, 0, 16));

	regAMD64RAX = createRegister("RAX", NULL, 8, REG_TYPE_GP, 1, createRegSlice(&regX86EAX, 0, 32));
	regAMD64RBX = createRegister("RBX", NULL, 8, REG_TYPE_GP, 1, createRegSlice(&regX86EBX, 0, 32));
	regAMD64RCX = createRegister("RCX", NULL, 8, REG_TYPE_GP, 1, createRegSlice(&regX86ECX, 0, 32));
	regAMD64RDX = createRegister("RDX", NULL, 8, REG_TYPE_GP, 1, createRegSlice(&regX86EDX, 0, 32));
	regAMD64RSP = createRegister("RSP", NULL, 8, REG_TYPE_GP, 1, createRegSlice(&regX86ESP, 0, 32));
	regAMD64RBP = createRegister("RBP", NULL, 8, REG_TYPE_GP, 1, createRegSlice(&regX86EBP, 0, 32));
	regAMD64RDI = createRegister("RDI", NULL, 8, REG_TYPE_GP, 1, createRegSlice(&regX86EDI, 0, 32));
	regAMD64RSI = createRegister("RSI", NULL, 8, REG_TYPE_GP, 1, createRegSlice(&regX86ESI, 0, 32));

	regX86XMM0 = createRegister("XMM0", NULL, 16, REG_TYPE_FLOATING, 0);
	regX86XMM1 = createRegister("XMM1", NULL, 16, REG_TYPE_FLOATING, 0);
	regX86XMM2 = createRegister("XMM2", NULL, 16, REG_TYPE_FLOATING, 0);
	regX86XMM3 = createRegister("XMM3", NULL, 16, REG_TYPE_FLOATING, 0);
	regX86XMM4 = createRegister("XMM4", NULL, 16, REG_TYPE_FLOATING, 0);
	regX86XMM5 = createRegister("XMM5", NULL, 16, REG_TYPE_FLOATING, 0);
	regX86XMM6 = createRegister("XMM6", NULL, 16, REG_TYPE_FLOATING, 0);
	regX86XMM7 = createRegister("XMM7", NULL, 16, REG_TYPE_FLOATING, 0);

	regX86ST0 = createRegister("ST0", NULL, 16, REG_TYPE_FLOATING, 0);
	regX86ST1 = createRegister("ST1", NULL, 16, REG_TYPE_FLOATING, 0);
	regX86ST2 = createRegister("ST2", NULL, 16, REG_TYPE_FLOATING, 0);
	regX86ST3 = createRegister("ST3", NULL, 16, REG_TYPE_FLOATING, 0);
	regX86ST4 = createRegister("ST4", NULL, 16, REG_TYPE_FLOATING, 0);
	regX86ST5 = createRegister("ST5", NULL, 16, REG_TYPE_FLOATING, 0);
	regX86ST6 = createRegister("ST6", NULL, 16, REG_TYPE_FLOATING, 0);
	regX86ST7 = createRegister("ST7", NULL, 16, REG_TYPE_FLOATING, 0);

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
	assert(aRegOffsetInSuper != -1);
	// Same with b
	long bRegOffsetInSuper = -1;
	// Registers inherit the slices of their "children" registers
	for (long i = 0; i != strRegSliceSize(bSuper->affects); i++)
		if (bSuper->affects[i].reg == b->reg)
			bRegOffsetInSuper = bSuper->affects[i].offset;
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

	__auto_type base = objectBaseType(type);

	int success;
	__auto_type size = objectSize(base, &success);
	if (!success)
		return NULL;

	strRegP retVal = NULL;
	strRegP avail CLEANUP(strRegPDestroy) = NULL;
	switch (currentArch) {
	case ARCH_X64_SYSV:
	case ARCH_TEST_SYSV:
	case ARCH_X86_SYSV: {
		avail = regsForArch();
		// Reserved registers
		const struct reg *res[] = {&regX86ESP, &regX86EBP, &regX86SP, &regX86BP, &regX86SPL, &regX86BPL,
		                           &regX86SS,  &regX86CS,  &regX86DS, &regX86ES, &regX86FS,  &regX86GS};
		long len = sizeof(res) / sizeof(*res);
		qsort(res, len, sizeof(*res), ptrPtrCmp);
		strRegP reserved CLEANUP(strRegPDestroy) = strRegPAppendData(NULL, res, len);
		avail = strRegPSetDifference(avail, reserved, (regPCmpType)ptrPtrCmp);
	}
	}

search:
	if (objectEqual(base, &typeF64)) {
		for (long i = 0; i != strRegPSize(avail); i++) {
			if (avail[i]->type & REG_TYPE_FLOATING) {
				retVal = strRegPSortedInsert(retVal, avail[i], (regPCmpType)ptrPtrCmp);
			}
			if (currentArch == ARCH_X64_SYSV) {
				// Exclude x87FPU registets
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
