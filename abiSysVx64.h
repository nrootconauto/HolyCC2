#include "IR.h"
void IR_ABI_SYSV_X64_Call(graphNodeIR _call,struct X86AddressingMode *funcMode,strX86AddrMode args,struct X86AddressingMode *outMode);
void IR_ABI_SYSV_X64_Prologue(long frameSize);
void IR_ABI_SYSV_X64_Return(graphNodeIR _ret,long frameSize);
void IR_ABI_SYSV_X64_LoadArgs(graphNodeIR start,long frameSize);
