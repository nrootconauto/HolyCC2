#include <registers.h>
#include <stdarg.h>
#include <stdlib.h>
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

strRegP regsX86;
strRegP regsTest;
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

	struct reg *gpX86[] = {
	    &regX86AL,  &regX86BL,  &regX86CL,  &regX86DL,

	    &regX86AH,  &regX86BH,  &regX86CH,  &regX86DH,

	    &regX86AX,  &regX86BX,

	    &regX86CX,  &regX86DX,  &regX86SI,  &regX86DI,  &regX86BP,  &regX86SP,

	    &regX86EAX, &regX86EBX, &regX86ECX, &regX86EDX, &regX86ESI, &regX86EDI,
	    &regX86EBP, &regX86ESP,
	};
	long len = sizeof(gpX86) / sizeof(*gpX86);
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
