#include "X86AsmSharedVars.h"
#include "parserA.h"
__thread ptrMapAsmFuncName asmFuncNames = NULL;
__thread ptrMapFrameOffset localVarFrameOffsets = NULL;
__thread strVar asmFuncArgVars = NULL;
 long bytesOnStack=0;
