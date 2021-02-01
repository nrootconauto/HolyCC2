#include <IR.h>
#include <registers.h>
#include <opcodesParser.h>
#include <asmEmitter.h>
extern void *IR_ATTR_ABI_INFO;
struct IRAttrABIInfo {
		struct IRAttr base;
		strRegP toPushPop;
};
void IRAttrABIInfoDestroy(struct IRAttr *a);
void IR2ABI_i386(graphNodeIR start);
