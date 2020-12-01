#include <registers.h>
#include <stdarg.h>
#include <stdlib.h>
#include <assert.h>
static void init() __attribute__((constructor));

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
static struct reg createRegister(const char *name, int size, ...) {
	struct reg retVal;
	retVal.size = size;
	retVal.name = name;
	retVal.affects = NULL;

	va_list list;
	va_start(list, size);
	for (;;) {
		__auto_type affects = va_arg(list, struct reg *);
		if (!affects)
			break;

		retVal.affects =
		    strRegPSortedInsert(retVal.affects, affects, (regPCmpType)ptrPtrCmp);
	}
	va_end(list);

	return retVal;
}
static strRegP regsX86GP;
static strRegP regsTestGP;

static strRegP regsX86Float;
static strRegP regsX86FloatGiant;
static strRegP regsTestFloat;


static void init() {
	regX86AL = createRegister("AL", 1, NULL);
	regX86DL = createRegister("BL", 1, NULL);
	regX86CL = createRegister("CL", 1, NULL);
	regX86DL = createRegister("DL", 1, NULL);

	regX86AH = createRegister("AH", 1, NULL);
	regX86DH = createRegister("BH", 1, NULL);
	regX86CH = createRegister("CH", 1, NULL);
	regX86DH = createRegister("DH", 1, NULL);

	regX86AX = createRegister("AX", 2, &regX86AL, &regX86AH, NULL);
	regX86BX = createRegister("BX", 2, &regX86BL, &regX86BH, NULL);
	regX86CX = createRegister("CX", 2, &regX86CL, &regX86CH, NULL);
	regX86DX = createRegister("DX", 2, &regX86DL, &regX86DH, NULL);
	regX86SI = createRegister("SI", 2, NULL);
	regX86DI = createRegister("DI", 2, NULL);
	regX86BP = createRegister("BP", 2, NULL);
	regX86SP = createRegister("SP", 2, NULL);

	regX86EAX = createRegister("EAX", 4, &regX86AL, &regX86AH, &regX86AX, NULL);
	regX86EBX = createRegister("EBX", 4, &regX86BL, &regX86BH, &regX86BX, NULL);
	regX86ECX = createRegister("ECX", 4, &regX86CL, &regX86CH, &regX86CX, NULL);
	regX86EDX = createRegister("EDX", 4, &regX86DL, &regX86DH, &regX86DX, NULL);
	regX86ESI = createRegister("ESI", 4, &regX86SI, NULL);
	regX86EDI = createRegister("EDI", 4, &regX86DI, NULL);
	regX86EBP = createRegister("EBP", 4, &regX86SP, NULL);
	regX86ESP = createRegister("ESP", 4, &regX86BP, NULL);

	regX86XMM0=createRegister("XMM0", 16, NULL);
	regX86XMM1=createRegister("XMM1", 16, NULL);
	regX86XMM2=createRegister("XMM2", 16, NULL);
	regX86XMM3=createRegister("XMM3", 16, NULL);
	regX86XMM4=createRegister("XMM4", 16, NULL);
	regX86XMM5=createRegister("XMM5", 16, NULL);
	regX86XMM6=createRegister("XMM6", 16, NULL);
	regX86XMM7=createRegister("XMM7", 16, NULL);

	regX86ST0=createRegister("ST0", 16, NULL);
	regX86ST1=createRegister("ST1", 16, NULL);
	regX86ST2=createRegister("ST2", 16, NULL);
	regX86ST3=createRegister("ST3", 16, NULL);
	regX86ST4=createRegister("ST4", 16, NULL);
	regX86ST5=createRegister("ST5", 16, NULL);
	regX86ST6=createRegister("ST6", 16, NULL);
	regX86ST7=createRegister("ST7", 16, NULL);

	//Regular floats X86
	struct reg *fltX86[]={
			&regX86XMM0,&regX86XMM1,&regX86XMM2,&regX86XMM3,&regX86XMM4,&regX86XMM5,&regX86XMM6,&regX86XMM7,
	};
	long len=sizeof(fltX86)/sizeof(*fltX86);
	qsort(fltX86, len, sizeof(*fltX86), ptrPtrCmp);
	regsX86Float=strRegPAppendData(NULL, (void*)fltX86, len);

	//Gaint floats X86
	struct reg *fltGiantX86[]={
			&regX86ST0,&regX86ST1,&regX86ST2,&regX86ST3,&regX86ST4,&regX86ST5,&regX86ST6,&regX86ST7
	};
	len=sizeof(fltGiantX86)/sizeof(*fltGiantX86);
	qsort(fltX86, len, sizeof(*fltGiantX86), ptrPtrCmp);
	regsX86FloatGiant=strRegPAppendData(NULL, (void*)fltGiantX86, len);

	//Floats test
	struct reg *fltTest[]={
			&regX86XMM0,&regX86XMM1,&regX86XMM2,
	};
	len=sizeof(fltTest)/sizeof(*fltTest);
	qsort(fltX86, len, sizeof(*fltTest), ptrPtrCmp);
	regsTestFloat=strRegPAppendData(NULL, (void*)fltTest, len);

	//General purpose x86
	struct reg *gpX86[] = {
	    &regX86AL,  &regX86BL,  &regX86CL,  &regX86DL,

	    &regX86AH,  &regX86BH,  &regX86CH,  &regX86DH,

	    &regX86AX,  &regX86BX,

	    &regX86CX,  &regX86DX,  &regX86SI,  &regX86DI,  &regX86BP,  &regX86SP,

	    &regX86EAX, &regX86EBX, &regX86ECX, &regX86EDX, &regX86ESI, &regX86EDI,
	    &regX86EBP, &regX86ESP,
	};
	len = sizeof(gpX86) / sizeof(*gpX86);
	qsort(gpX86, len, sizeof(*gpX86), ptrPtrCmp);
	regsX86GP = strRegPAppendData(NULL, (void *)gpX86, len);

	struct reg *gpTest[] = {
	    &regX86AL,
	    &regX86BL,
	    &regX86CL,
	};
	len = sizeof(gpTest) / sizeof(*gpTest);
	qsort(gpTest, len, sizeof(*gpTest), ptrPtrCmp);
	regsTestGP = strRegPAppendData(NULL, (void *)gpTest, len);
}
static __thread enum archConfig currentArch=ARCH_X86_SYSV; 
void setArch(enum archConfig Arch) {
		currentArch=Arch;
}
const strRegP getIntRegs() {
		switch(currentArch) {
		case ARCH_TEST_SYSV:
				return regsTestGP;
		case ARCH_X64_SYSV:
				return regsX86GP;
		case ARCH_X86_SYSV:
				assert(0);
				return NULL;
		}
} 
const strRegP getFloatRegs() {
		switch(currentArch) {
		case ARCH_TEST_SYSV:
				return regsTestFloat;
		case ARCH_X64_SYSV:
				return regsX86Float;
		case ARCH_X86_SYSV:
				assert(0);
				return NULL;
		}
}
const strRegP getSIMDRegs() {
		assert(0);
		return NULL;
}
