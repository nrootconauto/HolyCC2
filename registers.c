#include <assert.h>
#include <object.h>
#include <registers.h>
#include <stdarg.h>
#include <stdlib.h>
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
	}
	va_end(list);

	return retVal;
}
static strRegP regsX86;
static strRegP regsTest;

static strRegP regsX86FloatGiant;

void initRegisters() {

	regX86AL = createRegister("AL", 1, REG_TYPE_GP, 0);
	regX86BL = createRegister("BL", 1, REG_TYPE_GP, 0);
	regX86CL = createRegister("CL", 1, REG_TYPE_GP, 0);
	regX86DL = createRegister("DL", 1, REG_TYPE_GP, 0);

	regX86AH = createRegister("AH", 1, REG_TYPE_GP, 0);
	regX86DH = createRegister("BH", 1, REG_TYPE_GP, 0);
	regX86CH = createRegister("CH", 1, REG_TYPE_GP, 0);
	regX86DH = createRegister("DH", 1, REG_TYPE_GP, 0);

	regX86AX =
	    createRegister("AX", 2, REG_TYPE_GP, 2, createRegSlice(&regX86EAX, 0, 8),
	                   createRegSlice(&regX86EAX, 8, 8));
	regX86BX =
	    createRegister("BX", 2, REG_TYPE_GP, 2, createRegSlice(&regX86EBX, 0, 8),
	                   createRegSlice(&regX86EBX, 8, 8));
	regX86CX =
	    createRegister("CX", 2, REG_TYPE_GP, 2, createRegSlice(&regX86ECX, 0, 8),
	                   createRegSlice(&regX86ECX, 8, 8));
	regX86DX =
	    createRegister("DX", 2, REG_TYPE_GP, 2, createRegSlice(&regX86EDX, 0, 8),
	                   createRegSlice(&regX86EDX, 8, 8));
	regX86SI = createRegister("SI", 2, REG_TYPE_GP, 0);
	regX86DI = createRegister("DI", 2, REG_TYPE_GP, 0);
	regX86BP = createRegister("BP", 2, REG_TYPE_GP, 0);
	regX86SP = createRegister("SP", 2, REG_TYPE_GP, 0);

	regX86EAX = createRegister(
	    "EAX", 4, REG_TYPE_GP, 3, createRegSlice(&regX86EAX, 0, 8),
	    createRegSlice(&regX86EAX, 8, 8), createRegSlice(&regX86EAX, 0, 16));
	regX86EBX = createRegister(
	    "EBX", 4, REG_TYPE_GP, 3, createRegSlice(&regX86EBX, 0, 8),
	    createRegSlice(&regX86EBX, 8, 8), createRegSlice(&regX86EBX, 0, 16));
	regX86ECX = createRegister(
	    "ECX", 4, REG_TYPE_GP, 3, createRegSlice(&regX86ECX, 0, 8),
	    createRegSlice(&regX86ECX, 8, 8), createRegSlice(&regX86ECX, 0, 16));
	regX86EDX = createRegister(
	    "EDX", 4, REG_TYPE_GP, 3, createRegSlice(&regX86EDX, 0, 8),
	    createRegSlice(&regX86EDX, 8, 8), createRegSlice(&regX86EDX, 0, 16));
	regX86ESI = createRegister("ESI", 4, REG_TYPE_GP, 1,
	                           createRegSlice(&regX86ESI, 0, 16));
	regX86EDI = createRegister("EDI", 4, REG_TYPE_GP, 1,
	                           createRegSlice(&regX86EDI, 0, 16));
	regX86EBP = createRegister("EBP", 4, REG_TYPE_GP, 1,
	                           createRegSlice(&regX86EBP, 0, 16));
	regX86ESP = createRegister("ESP", 4, REG_TYPE_GP, 1,
	                           createRegSlice(&regX86ESP, 0, 16));

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

	// Gaint floats X86
	struct reg *fltGiantX86[] = {&regX86ST0, &regX86ST1, &regX86ST2, &regX86ST3,
	                             &regX86ST4, &regX86ST5, &regX86ST6, &regX86ST7};
	long len = sizeof(fltGiantX86) / sizeof(*fltGiantX86);
	qsort(fltGiantX86, len, sizeof(*fltGiantX86), ptrPtrCmp);
	regsX86FloatGiant = strRegPAppendData(NULL, (void *)fltGiantX86, len);

	// General purpose x86
	struct reg *gpX86[] = {
	    &regX86AL,   &regX86BL,   &regX86CL,   &regX86DL,

	    &regX86AH,   &regX86BH,   &regX86CH,   &regX86DH,

	    &regX86AX,   &regX86BX,

	    &regX86CX,   &regX86DX,   &regX86SI,   &regX86DI,
	    &regX86BP,   &regX86SP,

	    &regX86EAX,  &regX86EBX,  &regX86ECX,  &regX86EDX,
	    &regX86ESI,  &regX86EDI,  &regX86EBP,  &regX86ESP,
	    &regX86XMM0, &regX86XMM1, &regX86XMM2, &regX86XMM3,
	    &regX86XMM4, &regX86XMM5, &regX86XMM6, &regX86XMM7,
	};
	len = sizeof(gpX86) / sizeof(*gpX86);
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
	    &regX86XMM0,
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
