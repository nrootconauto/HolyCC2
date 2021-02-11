#include <registers.h>
#include <assert.h>
#include <asmEmitter.h>
#include <cleanup.h>
#include <IR.h>
#include <ptrMap.h>
static __thread strRegP fpuRegsInUse=NULL;
static __thread strGraphNodeIRP nodeStack=NULL;
typedef int (*regCmpType)(const struct reg **, const struct reg **);
typedef int (*gnCmpType)(const graphNodeIR *, const graphNodeIR*);
PTR_MAP_FUNCS(struct parserVar*, long, RefCount);
static int ptrPtrCmp(const void *a, const void *b) {
	if (*(void **)a > *(void **)b)
		return 1;
	else if (*(void **)a < *(void **)b)
		return -1;
	else
		return 0;
}
void X87FpuPushReg() {
		long cur=strRegPSize(fpuRegsInUse);
		struct reg *stack[]={
				&regX86ST0,
				&regX86ST1,
				&regX86ST2,
				&regX86ST3,
				&regX86ST4,
				&regX86ST5,
				&regX86ST6,
				&regX86ST7,
		};
		long max=sizeof(stack)/sizeof(*stack);
		if(max>cur+1)
				return;
		fpuRegsInUse=strRegPSortedInsert(fpuRegsInUse, stack[cur], (regCmpType)ptrPtrCmp);
}
void X87FpuPopReg() {
		long cur=strRegPSize(fpuRegsInUse);
		struct reg *stack[]={
				&regX86ST0,
				&regX86ST1,
				&regX86ST2,
				&regX86ST3,
				&regX86ST4,
				&regX86ST5,
				&regX86ST6,
				&regX86ST7,
		};
		if(!cur)
				return;
		fpuRegsInUse=strRegPRemoveItem(fpuRegsInUse, stack[cur-1], (regCmpType)ptrPtrCmp);
}
strRegP X87FpuRegsInUse() {
		return strRegPClone(fpuRegsInUse);
}
static struct regSlice st0slice() {
		struct regSlice slice;
		slice.offset=0;
		slice.reg=&regX86ST0;
		slice.type=&typeF64;
		slice.widthInBits=80;
		return slice;
}
static void exprRecur(graphNodeIR start,ptrMapRefCount refCounts) {
		graphNodeIR pushToStack=NULL;

		if(objectBaseType(IRNodeType(start))==&typeF64) {
				strGraphEdgeIRP out CLEANUP(strGraphEdgeIRPDestroy)=graphNodeIROutgoing(start);
				strGraphEdgeIRP outDest CLEANUP(strGraphEdgeIRPDestroy)=IRGetConnsOfType(out, IR_CONN_DEST);
				if(strGraphEdgeIRPSize(outDest)==1)
						pushToStack=graphEdgeIROutgoing(outDest[0]);
		}
		int isFunc=graphNodeIRValuePtr(start)->type==IR_FUNC_CALL;
		strGraphEdgeIRP in CLEANUP(strGraphEdgeIRPDestroy)=IREdgesByPrec(start);
		for(long i =0;i!=strGraphEdgeIRPSize(in);i++)
				exprRecur(graphEdgeIRIncoming(in[i]),refCounts);

		if(isFunc) {
				for(long s=0;s!=strGraphNodeIRPSize(nodeStack);s++) {
						//Functions require all fpu registers be empty priror to entering them(SYSTEM-V i386),so store them in variables not registers
						struct IRNodeValue *val=(void*)graphNodeIRValuePtr(nodeStack[s]);
						if(val->val.type==IR_VAL_REG) {
								strGraphNodeIRP toReplace CLEANUP(strGraphNodeIRPDestroy)=strGraphNodeIRPAppendItem(NULL, nodeStack[s]);
								__auto_type varRef=IRCreateVarRef(IRCreateVirtVar(&typeF64));
								graphIRReplaceNodes(toReplace, varRef, NULL, (void(*)(void*))IRNodeDestroy);
						}
				}
		} else if(graphNodeIRValuePtr(start)->type!=IR_VALUE){
				// Is a binop or unop,so replace single use variables(and temporary) with registers ,or if used elsewhere,assign into a register
				// If not a variable,assign into register
				for(long i =0;i!=strGraphEdgeIRPSize(in);i++) {
						__auto_type node=graphEdgeIRIncoming(in[i]);
						struct IRNodeValue *value=(void*)graphNodeIRValuePtr(node);
						if(value->val.type==IR_VAL_VAR_REF) {
								__auto_type var=value->val.value.var.var;
								if(var->isTmp&&*ptrMapRefCountGet(refCounts, var)==1) {
										strGraphNodeIRP toReplace CLEANUP(strGraphNodeIRPDestroy)=strGraphNodeIRPAppendItem(NULL, node);
										__auto_type slice=st0slice();
										__auto_type st0=IRCreateRegRef( &slice);
										graphIRReplaceNodes(toReplace, st0, NULL, (void(*)(void*))IRNodeDestroy);
										nodeStack=strGraphNodeIRPPop(nodeStack, NULL);
										continue;
								}
						}
						//Assign value into register
						__auto_type slice=st0slice();
						__auto_type st0=IRCreateRegRef( &slice);
						IRInsertAfter(node, st0, st0, IR_CONN_DEST);
						nodeStack=strGraphNodeIRPPop(nodeStack, NULL);
				}
		} else if(graphNodeIRValuePtr(start)->type==IR_VALUE) {
				//Do nothing
		} else {
				fputs("Can't make sense of this node as an expression.\n", stderr);
				abort();
		}
		if(pushToStack)
				nodeStack=strGraphNodeIRPAppendItem(nodeStack, pushToStack);
}
void IRRegisterAllocateX87(graphNodeIR start) {
		strGraphNodeIRP allNodes CLEANUP(strGraphNodeIRPDestroy)=graphNodeIRAllNodes(start);
		strGraphNodeIRP exprEnds CLEANUP(strGraphNodeIRPDestroy)=NULL;

		ptrMapRefCount refCounts=ptrMapRefCountCreate();
		for(long n=0;n!=strGraphNodeIRPSize(allNodes);n++) {
				__auto_type end=IREndOfExpr(allNodes[n]);
				if(!end)
						continue;
				if(end==allNodes[n])
						continue;
				exprEnds=strGraphNodeIRPSortedInsert(exprEnds, allNodes[n], (gnCmpType)ptrPtrCmp);
				
				struct IRNodeValue *val=(void*)graphNodeIRValuePtr(allNodes[n]);
				if(val->base.type==IR_VALUE) {
						if(val->val.type==IR_VAL_VAR_REF) {
						refLoop:;
								__auto_type find=ptrMapRefCountGet(refCounts, val->val.value.var.var);
								if(!find) {
										ptrMapRefCountAdd(refCounts,  val->val.value.var.var, 1);
								} else
										++*find;
						}
				}
		}
		for(long e=0;e!=strGraphNodeIRPSize(exprEnds);e++) {
				exprRecur(exprEnds[e],refCounts);
		}
}
