#include <assert.h>
#include <object.h>
#include <registers.h>
#include <stdarg.h>
#include <stdlib.h>
#include <cleanup.h>
struct reg regX86AL;
struct reg regX86BL;
struct reg regX86CL;
struct reg regX86DL;

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
	if (*(void **)a > *(void **)b)
		return 1;
	else if (*(void **)a < *(void **)b)
		return -1;
	else
		return 0;
}
typedef int (*regPCmpType)(const struct reg **, const struct reg **);
struct regSlice createRegSlice(const struct reg *reg, int offset, int width) {
	struct regSlice slice;
	slice.reg = (void *)reg;
	slice.offset = offset;
	slice.widthInBits = width;
	slice.type=NULL;
	
	return slice;
}
static struct reg createRegister(const char *name, int size, enum regType type,
                                 int affectsCount, ...) {
	struct reg retVal;
	retVal.size = size;
	retVal.name = name;
	retVal.type = type;
	retVal.affects = NULL;

	va_list list;
	va_start(list, affectsCount);
	for (int i = 0; i != affectsCount; i++) {
		__auto_type affects = va_arg(list, struct regSlice *);
		if (!affects)
			break;

		retVal.affects = strRegSliceSortedInsert(
		    retVal.affects, *affects,
		    (int (*)(const struct regSlice *, const struct regSlice *))ptrPtrCmp);
		//Clone the affects->reg move move by offset
		strRegSlice clone CLEANUP(strRegSliceDestroy)=strRegSliceClone(affects->reg->affects);
		for(long i=0;i!=strRegSliceSize(clone);i++)
				clone[i].offset+=affects->offset;
		retVal.affects=strRegSliceSetUnion(retVal.affects, clone, regSliceCompare); 
	}
	va_end(list);

	return retVal;
}
static strRegP regsX86;
static strRegP regsAMD64;
static strRegP regsTest;
static strRegP regsX86FloatGiant;
static strRegP regsAMD64;

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
		regX86MM0=createRegister("MM0", 8, REG_TYPE_GP, 0);
		regX86MM1=createRegister("MM1", 8, REG_TYPE_GP, 0);
		regX86MM2=createRegister("MM2", 8, REG_TYPE_GP, 0);
		regX86MM3=createRegister("MM3", 8, REG_TYPE_GP, 0);
		regX86MM4=createRegister("MM4", 8, REG_TYPE_GP, 0);
		regX86MM5=createRegister("MM5", 8, REG_TYPE_GP, 0);
		regX86MM6=createRegister("MM6", 8, REG_TYPE_GP, 0);
		regX86MM7=createRegister("MM7", 8, REG_TYPE_GP, 0);
		
		regX86ES=createRegister("ES", 1, REG_TYPE_GP, 0 );
		regX86CS=createRegister("CS", 1, REG_TYPE_GP, 0 );
		regX86SS=createRegister("SS", 1, REG_TYPE_GP, 0 );
		regX86DS=createRegister("DS", 1, REG_TYPE_GP, 0 );
		regX86FS=createRegister("FS", 1, REG_TYPE_GP, 0 );
		regX86GS=createRegister("GS", 1, REG_TYPE_GP, 0 );
		
		regAMD64R8u8=createRegister("R8u8", 1, REG_TYPE_GP, 0 );
		regAMD64R8u8=createRegister("R9u8", 1, REG_TYPE_GP, 0 );
		regAMD64R10u8=createRegister("R10u8", 1, REG_TYPE_GP, 0 );
		regAMD64R11u8=createRegister("R11u8", 1, REG_TYPE_GP, 0 );
		regAMD64R12u8=createRegister("R12u8", 1, REG_TYPE_GP, 0 );
		regAMD64R13u8=createRegister("R13u8", 1, REG_TYPE_GP, 0 );
		regAMD64R14u8=createRegister("R14u8", 1, REG_TYPE_GP, 0 );
		regAMD64R15u8=createRegister("R15u8", 1, REG_TYPE_GP, 0 );

		regAMD64R8u16=createRegister("R8u16", 1, REG_TYPE_GP, 1 ,createRegSlice(&regAMD64R8u8, 0, 8));
		regAMD64R9u16=createRegister("R9u16", 1, REG_TYPE_GP, 1 ,createRegSlice(&regAMD64R9u8, 0, 8) );
		regAMD64R10u16=createRegister("R12u16", 1, REG_TYPE_GP, 1 ,createRegSlice(&regAMD64R10u8, 0, 8) );
		regAMD64R11u16=createRegister("R11u16", 1, REG_TYPE_GP, 1 ,createRegSlice(&regAMD64R11u8, 0, 8) );
		regAMD64R12u16=createRegister("R12u16", 1, REG_TYPE_GP, 1 ,createRegSlice(&regAMD64R12u8, 0, 8) );
		regAMD64R13u16=createRegister("R13u16", 1, REG_TYPE_GP, 1 ,createRegSlice(&regAMD64R13u8, 0, 8) );
		regAMD64R14u16=createRegister("R14u16", 1, REG_TYPE_GP, 1 ,createRegSlice(&regAMD64R14u8, 0, 8) );
		regAMD64R15u16=createRegister("R15u16", 1, REG_TYPE_GP, 1 ,createRegSlice(&regAMD64R15u8, 0, 8) );

		regAMD64R8u32=createRegister("R8u32", 4, REG_TYPE_GP,1 ,createRegSlice(&regAMD64R8u16, 0, 16));
		regAMD64R9u32=createRegister("R9u32", 4, REG_TYPE_GP, 1 ,createRegSlice(&regAMD64R9u16, 0, 16));
		regAMD64R10u32=createRegister("R12u32", 4, REG_TYPE_GP, 1 ,createRegSlice(&regAMD64R10u16, 0, 16));
		regAMD64R11u32=createRegister("R11u32", 4, REG_TYPE_GP, 1 ,createRegSlice(&regAMD64R11u16, 0, 16));
		regAMD64R12u32=createRegister("R12u32", 4, REG_TYPE_GP, 1 ,createRegSlice(&regAMD64R12u16, 0, 16));
		regAMD64R13u32=createRegister("R13u32", 4, REG_TYPE_GP, 1 ,createRegSlice(&regAMD64R13u16, 0, 16));
		regAMD64R14u32=createRegister("R14u32", 4, REG_TYPE_GP, 1 ,createRegSlice(&regAMD64R14u16, 0, 16));
		regAMD64R15u32=createRegister("R15u32", 4, REG_TYPE_GP, 1 ,createRegSlice(&regAMD64R15u16, 0, 16));

		regAMD64R8u64=createRegister("R8u64", 8, REG_TYPE_GP, 1 ,createRegSlice(&regAMD64R8u32, 0, 32));
		regAMD64R9u64=createRegister("R9u64", 8, REG_TYPE_GP, 1 ,createRegSlice(&regAMD64R9u32, 0, 32));
		regAMD64R10u64=createRegister("R12u64", 8, REG_TYPE_GP, 1 ,createRegSlice(&regAMD64R10u32, 0, 32));
		regAMD64R11u64=createRegister("R11u64", 8, REG_TYPE_GP, 1 ,createRegSlice(&regAMD64R11u32, 0, 32));
		regAMD64R12u64=createRegister("R12u64", 8, REG_TYPE_GP, 1 ,createRegSlice(&regAMD64R12u32, 0, 32));
		regAMD64R13u64=createRegister("R13u64", 8, REG_TYPE_GP, 1 ,createRegSlice(&regAMD64R13u32, 0, 32));
		regAMD64R14u64=createRegister("R14u64", 8, REG_TYPE_GP, 1 ,createRegSlice(&regAMD64R14u32, 0, 32));
		regAMD64R15u64=createRegister("R15u64", 8, REG_TYPE_GP, 1 ,createRegSlice(&regAMD64R15u32, 0, 32));
		
	regX86AL = createRegister("AL", 1, REG_TYPE_GP, 0);
	regX86BL = createRegister("BL", 1, REG_TYPE_GP, 0);
	regX86CL = createRegister("CL", 1, REG_TYPE_GP, 0);
	regX86DL = createRegister("DL", 1, REG_TYPE_GP, 0);

	regX86AH = createRegister("AH", 1, REG_TYPE_GP, 0);
	regX86DH = createRegister("BH", 1, REG_TYPE_GP, 0);
	regX86CH = createRegister("CH", 1, REG_TYPE_GP, 0);
	regX86DH = createRegister("DH", 1, REG_TYPE_GP, 0);

	regX86AX =
	    createRegister("AX", 2, REG_TYPE_GP, 2, createRegSlice(&regX86AL, 0, 8),
	                   createRegSlice(&regX86AH, 8, 8));
	regX86BX =
	    createRegister("BX", 2, REG_TYPE_GP, 2, createRegSlice(&regX86BL, 0, 8),
	                   createRegSlice(&regX86EBX, 8, 8));
	regX86CX =
			createRegister("CX", 2, REG_TYPE_GP, 2, createRegSlice(&regX86CL, 0, 8),
	                   createRegSlice(&regX86CH, 8, 8));
	regX86DX =
	    createRegister("DX", 2, REG_TYPE_GP, 2, createRegSlice(&regX86DL, 0, 8),
	                   createRegSlice(&regX86DH, 8, 8));
	regX86SI = createRegister("SI", 2, REG_TYPE_GP, 0);
	regX86DI = createRegister("DI", 2, REG_TYPE_GP, 0);
	regX86BP = createRegister("BP", 2, REG_TYPE_GP, 0);
	regX86SP = createRegister("SP", 2, REG_TYPE_GP, 0);

	regX86EAX = createRegister(
	    "EAX", 4, REG_TYPE_GP, 1, createRegSlice(&regX86AX, 0, 16));
	regX86EBX = createRegister(
	    "EBX", 4, REG_TYPE_GP, 1, createRegSlice(&regX86BX, 0, 16));
	regX86ECX = createRegister(
	    "ECX", 4, REG_TYPE_GP, 1, createRegSlice(&regX86CX, 0, 16));
	regX86EDX = createRegister(
	    "EDX", 4, REG_TYPE_GP, 1, createRegSlice(&regX86DX, 0, 16));
	regX86ESI = createRegister("ESI", 4, REG_TYPE_GP, 1,
	                           createRegSlice(&regX86SI, 0, 16));
	regX86EDI = createRegister("EDI", 4, REG_TYPE_GP, 1,
	                           createRegSlice(&regX86DI, 0, 16));
	regX86EBP = createRegister("EBP", 4, REG_TYPE_GP, 1,
	                           createRegSlice(&regX86BP, 0, 16));
	regX86ESP = createRegister("ESP", 4, REG_TYPE_GP, 1,
																												createRegSlice(&regX86SP, 0, 16));

	regAMD64RAX=createRegister("RAX", 8, REG_TYPE_GP, 1, createRegSlice(&regX86EAX, 0, 32));
	regAMD64RBX=createRegister("RBX", 8, REG_TYPE_GP, 1, createRegSlice(&regX86EBX, 0, 32));
	regAMD64RCX=createRegister("RCX", 8, REG_TYPE_GP, 1, createRegSlice(&regX86ECX, 0, 32));
	regAMD64RDX=createRegister("RDX", 8, REG_TYPE_GP, 1, createRegSlice(&regX86EDX, 0, 32));
	regAMD64RSP=createRegister("RSP", 8, REG_TYPE_GP, 1, createRegSlice(&regX86ESP, 0, 32));
	regAMD64RBP=createRegister("RBP", 8, REG_TYPE_GP, 1, createRegSlice(&regX86EBP, 0, 32));
	regAMD64RDI=createRegister("RDI", 8, REG_TYPE_GP, 1, createRegSlice(&regX86EDI, 0, 32));
	regAMD64RSI=createRegister("RSI", 8, REG_TYPE_GP, 1, createRegSlice(&regX86ESI, 0, 32));
	
	regX86XMM0 = createRegister("XMM0", 16, REG_TYPE_FLOATING, 0);
	regX86XMM1 = createRegister("XMM1", 16, REG_TYPE_FLOATING, 0);
	regX86XMM2 = createRegister("XMM2", 16, REG_TYPE_FLOATING, 0);
	regX86XMM3 = createRegister("XMM3", 16, REG_TYPE_FLOATING, 0);
	regX86XMM4 = createRegister("XMM4", 16, REG_TYPE_FLOATING, 0);
	regX86XMM5 = createRegister("XMM5", 16, REG_TYPE_FLOATING, 0);
	regX86XMM6 = createRegister("XMM6", 16, REG_TYPE_FLOATING, 0);
	regX86XMM7 = createRegister("XMM7", 16, REG_TYPE_FLOATING, 0);

	regX86ST0 = createRegister("ST0", 16, REG_TYPE_FLOATING, 0);
	regX86ST1 = createRegister("ST1", 16, REG_TYPE_FLOATING, 0);
	regX86ST2 = createRegister("ST2", 16, REG_TYPE_FLOATING, 0);
	regX86ST3 = createRegister("ST3", 16, REG_TYPE_FLOATING, 0);
	regX86ST4 = createRegister("ST4", 16, REG_TYPE_FLOATING, 0);
	regX86ST5 = createRegister("ST5", 16, REG_TYPE_FLOATING, 0);
	regX86ST6 = createRegister("ST6", 16, REG_TYPE_FLOATING, 0);
	regX86ST7 = createRegister("ST7", 16, REG_TYPE_FLOATING, 0);
	
	// General purpose x86
	struct reg *gpX86[] = {
	    &regX86AL,   &regX86BL,   &regX86CL,   &regX86DL,

	    &regX86AH,   &regX86BH,   &regX86CH,   &regX86DH,

	    &regX86AX,   &regX86BX,

	    &regX86CX,   &regX86DX,   &regX86SI,   &regX86DI,
	    &regX86BP,   &regX86SP,

	    &regX86EAX,  &regX86EBX,  &regX86ECX,  &regX86EDX,
	    &regX86ESI,  &regX86EDI,  &regX86EBP,  &regX86ESP,

					&regX86ES, &regX86CS,&regX86SS,&regX86DS,&regX86FS,&regX86SS,

					&regX86ST0,&regX86ST1,&regX86ST2,&regX86ST3,&regX86ST4,&regX86ST5,&regX86ST6,&regX86ST7,
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
}
static enum archConfig currentArch = ARCH_X86_SYSV;
void setArch(enum archConfig Arch) { currentArch = Arch; }
const strRegP getSIMDRegs() {
	assert(0);
	return NULL;
}
int regSliceConflict(struct regSlice *a, struct regSlice *b) {
	if (a->reg != b->reg)
		return 0;

	int aEnd = a->offset + a->widthInBits;
	int bEnd = b->offset + b->widthInBits;

	if (a->offset >= b->offset)
		if (bEnd >= a->offset)
			return 1;

	if (b->offset >= a->offset)
		if (aEnd >= b->offset)
			return 1;

	return 0;
}
strRegP regsForArch() {
		switch(currentArch) {
		case ARCH_TEST_SYSV:
				return strRegPClone(regsTest);
		case ARCH_X64_SYSV:
				assert(0);
				return NULL;
		case ARCH_X86_SYSV:
				return strRegPClone(regsX86);
		}
}
strRegP regGetForType(struct object *type) {
	const struct object *ints[] = {
	    &typeU8i, &typeU16i, &typeU32i, &typeU64i,
	    &typeI8i, &typeI16i, &typeI32i, &typeI64i,
	};
	qsort(ints, sizeof(ints) / sizeof(*ints), sizeof(*ints), ptrPtrCmp);

	__auto_type base = objectBaseType(type);

	int success;
	__auto_type size = objectSize(base, &success);
	if (!success)
		return NULL;

	strRegP retVal = NULL;
	strRegP avail;
	switch (currentArch) {
	case ARCH_X64_SYSV:
		assert(0);
		return NULL;
	case ARCH_TEST_SYSV: {
		avail = regsTest;
		break;
	}
	case ARCH_X86_SYSV: {
		avail = regsX86;
	}
	}

search:
	if (objectEqual(base, &typeF64)) {
		for (long i = 0; i != strRegPSize(avail); i++) {
			if (avail[i]->type & REG_TYPE_FLOATING)
				retVal = strRegPSortedInsert(retVal, avail[i], (regPCmpType)ptrPtrCmp);
		}
	} else {
		for (long i = 0; i != strRegPSize(avail); i++) {
			if (avail[i]->type & REG_TYPE_GP) {
				if (bsearch(&type, ints, sizeof(ints) / sizeof(*ints), sizeof(*ints),
				            ptrPtrCmp)) {
					if (avail[i]->size == objectSize(type, NULL))
						retVal =
						    strRegPSortedInsert(retVal, avail[i], (regPCmpType)ptrPtrCmp);
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
