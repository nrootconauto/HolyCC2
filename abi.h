#include "IR.h"
#include "registers.h"
#include "opcodesParser.h"
#include "asmEmitter.h"
#include "basicBlocks.h"
extern void *IR_ATTR_ABI_INFO;
struct IRAttrABIInfo {
		struct IRAttr base;
		strRegP liveIn;
		strRegP liveOut;
		strRegP toPushPop;
};
void IR2ABI_i386(graphNodeIR start);
void IRComputeABIInfo(graphNodeIR start);
strVar IRABIInsertLoadArgs(graphNodeIR start,long frameSize);
void IRABIAsmPrologue(long frameSize);
void IRABICall2Asm(graphNodeIR start ,struct X86AddressingMode *funcMode,strX86AddrMode args,struct X86AddressingMode *outMode);
void IRABIReturn2Asm(graphNodeIR start,long frameSize);
void findRegisterLiveness(graphNodeIR start);
void IRABIFuncNode2Asm(graphNodeIR start);
