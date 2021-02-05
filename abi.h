#include <IR.h>
#include <registers.h>
#include <opcodesParser.h>
#include <asmEmitter.h>
#include <basicBlocks.h>
extern void *IR_ATTR_ABI_INFO;
struct IRAttrABIInfo {
		struct IRAttr base;
		strRegP toPushPop;
};
void IR2ABI_i386(graphNodeIR start);
void IRComputeABIInfo(graphNodeIR start);
strVar IRABIInsertLoadArgs(graphNodeIR start);
void IRABIAsmPrologue();
void IRABICall2Asm(graphNodeIR start);
void IRABIReturn2Asm(graphNodeIR start);
