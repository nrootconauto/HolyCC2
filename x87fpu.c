#include "registers.h"
#include <assert.h>
#include "asmEmitter.h"
#include "cleanup.h"
#include "IR.h"
#include "ptrMap.h"
void *IR_ATTR_X87FPU_POP_AT="X87_POP";
static __thread strRegP fpuRegsInUse=NULL;
static __thread strGraphNodeIRP nodeStack=NULL;
typedef int (*regCmpType)(const struct reg **, const struct reg **);
typedef int (*gnCmpType)(const graphNodeIR *, const graphNodeIR*);
PTR_MAP_FUNCS(struct parserVar*, long, RefCount);
static int ptrPtrCmp(const void *a, const void *b) {
		if(*(void**)a>*(void**)b)
				return 1;
		else if(*(void**)a<*(void**)b)
				return -1;
		return 0;
}
struct reg *X87FpuPushReg() {
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
		if(max<=cur+1)
				return NULL;
		fpuRegsInUse=strRegPSortedInsert(fpuRegsInUse, stack[cur], (regCmpType)ptrPtrCmp);
		return stack[cur];
}
static int isX87Reg(struct reg *r) {
		struct reg *regs[]={
				&regX86ST0,
				&regX86ST1,
				&regX86ST2,
				&regX86ST3,
				&regX86ST4,
				&regX86ST5,
				&regX86ST6,
				&regX86ST7,
		};
		for(long i=0;i!=8;i++)
				if(regs[i]==r)
						return 1;
		return 0;
} 
static void X87FpuPopReg() {
		fpuRegsInUse=strRegPPop(fpuRegsInUse, NULL);
		nodeStack=strGraphNodeIRPPop(nodeStack, NULL);
}
static strRegP X87FpuRegsInUse() {
		return strRegPClone(fpuRegsInUse);
}
static struct regSlice pushedStXslice() {
		struct regSlice slice;
		slice.offset=0;
		slice.reg=X87FpuPushReg();
		slice.type=&typeF64;
		slice.widthInBits=80;
		return slice;
}
static int isX87CmpOp(graphNodeIR node) {
		switch(graphNodeIRValuePtr(node)->type) {
		case IR_GE:	
		case IR_LE:
		case IR_GT:
		case IR_LT:
		case IR_EQ:
		case IR_NE: {
				strGraphEdgeIRP incoming CLEANUP(strGraphEdgeIRPDestroy)=IREdgesByPrec(node);
				if(IRNodeType(graphEdgeIRIncoming(incoming[0]))->type==TYPE_F64)
						return 1;
				if(IRNodeType(graphEdgeIRIncoming(incoming[1]))->type==TYPE_F64)
						return 1;
		}
				default:
						return 0;
		}
}
static graphNodeIR insertFpuValue(graphNodeIR start) {
		if(strRegPSize(fpuRegsInUse)<6) {
		insertSt0:;
				__auto_type slice=pushedStXslice();
				__auto_type st0=IRCreateRegRef( &slice);
				IRInsertAfter(start, st0, st0, IR_CONN_DEST);
				nodeStack=strGraphNodeIRPAppendItem(nodeStack,st0);
				return st0;
		} else {
				long size=strRegPSize(fpuRegsInUse);
				fpuRegsInUse=strRegPPop(fpuRegsInUse, NULL);
				//Spill poped register
				__auto_type topNode=nodeStack[size-1];
				strGraphNodeIRP toReplace CLEANUP(strGraphNodeIRPDestroy)=strGraphNodeIRPAppendItem(NULL, topNode);
				__auto_type spill=IRCreateVarRef(IRCreateVirtVar(&typeF64));
				graphIRReplaceNodes(toReplace, spill, NULL, (void(*)(void*))IRNodeDestroy);
				goto insertSt0;
		}
}
static void spillFpuStack() {
		for(long s=0;s!=strGraphNodeIRPSize(nodeStack);s++) {
				if(strRegPSize(fpuRegsInUse)) {
						__auto_type topNode=nodeStack[strGraphNodeIRPSize(nodeStack)-1];
						strGraphNodeIRP toReplace CLEANUP(strGraphNodeIRPDestroy)=strGraphNodeIRPAppendItem(NULL, topNode);
						__auto_type spill=IRCreateVarRef(IRCreateVirtVar(&typeF64));
						graphIRReplaceNodes(toReplace, spill, NULL, (void(*)(void*))IRNodeDestroy);
						X87FpuPopReg();
				} else {
						strGraphNodeIRPDestroy(&nodeStack);
						nodeStack=NULL;
						return ;
				}
		}
}
static void exprRecur(graphNodeIR start,ptrMapRefCount refCounts,int parentIsFpuOp) {
		if(graphNodeIRValuePtr(start)->type==IR_DERREF)
				return;

		if(graphNodeIRValuePtr(start)->type==IR_VALUE) {
				struct IRNodeValue *val=(void*)graphNodeIRValuePtr(start);
				if(val->val.type==IR_VAL_VAR_REF&&objectBaseType(IRNodeType(start))==&typeF64) {
								if(*ptrMapRefCountGet(refCounts, val->val.value.var.var)==1&&val->val.value.var.var->isTmp) {
										strGraphEdgeIRP in CLEANUP(strGraphEdgeIRPDestroy)=IREdgesByPrec(start);
										for(long i=0;i!=strGraphEdgeIRPSize(in);i++)
												exprRecur(graphEdgeIRIncoming(in[i]),refCounts,parentIsFpuOp);
										strGraphEdgeIRP in2 CLEANUP(strGraphEdgeIRPDestroy)=graphNodeIRIncoming(start);
										strGraphEdgeIRP out2 CLEANUP(strGraphEdgeIRPDestroy)=graphNodeIROutgoing(start);
										for(long i=0;i!=strGraphEdgeIRPSize(in2);i++)
												for(long o=0;o!=strGraphEdgeIRPSize(out2);o++)
														graphNodeIRConnect(graphEdgeIRIncoming(in2[i]), graphEdgeIROutgoing(out2[o]), *graphEdgeIRValuePtr(out2[o]));
										graphNodeIRKill(&start, (void(*)(void*))IRNodeDestroy, NULL);
								}
								return ;
				}
		}
		
		int isFunc=graphNodeIRValuePtr(start)->type==IR_FUNC_CALL;
		strGraphEdgeIRP in CLEANUP(strGraphEdgeIRPDestroy)=IREdgesByPrec(start);
		
		if(isFunc) {
				spillFpuStack();
				for(long a=0;a!=strGraphEdgeIRPSize(in);a++) {
						__auto_type node=graphEdgeIRIncoming(in[a]);
						exprRecur(node,refCounts,0);
						//epxrRecur may have modified incoming edges so recompute
						strGraphEdgeIRPDestroy(&in);
						in=IREdgesByPrec(start);
						
						node=graphEdgeIRIncoming(in[a]);
						//Functions require all fpu registers be empty priror to entering them(SYSTEM-V i386),so store them in variables not registers
						struct IRNodeValue *val=(void*)graphNodeIRValuePtr(node);
						if(val->base.type!=IR_VALUE)
								continue;
						if(val->val.type==IR_VAL_REG) {
								if(!isX87Reg(val->val.value.reg.reg))
										continue;
								__auto_type varRef=IRCreateVarRef(IRCreateVirtVar(&typeF64));
								IRInsertAfter(node, varRef, varRef, IR_CONN_DEST);
								X87FpuPopReg();
						}
				}
				if(objectBaseType(IRNodeType(start))!=&typeF64)
						return;
				else
						insertFpuValue(start);
		} else if(graphNodeIRValuePtr(start)->type!=IR_VALUE) {
				int isTypecastFromF64=0;
				if(graphNodeIRValuePtr(start)->type==IR_TYPECAST) {
						exprRecur(graphEdgeIRIncoming(in[0]), refCounts, 0);
						strGraphEdgeIRPDestroy(&in);
						in =IREdgesByPrec(start);
						
						if(objectBaseType(IRNodeType(graphEdgeIRIncoming(in[0])))==&typeF64) {
								//Just return if typecast to F64 from F64
								if( objectBaseType(IRNodeType(start))==&typeF64)
										return;
								isTypecastFromF64=1;
								struct IRNodeValue *val=(void*)graphNodeIRValuePtr(graphEdgeIRIncoming(in[0]));
								if(val->base.type==IR_VALUE)
										if(val->val.type==IR_VAL_REG)
												if(isX87Reg(val->val.value.reg.reg))
														X87FpuPopReg();
						}
						//If cast to F64
						if(objectBaseType(IRNodeType(start))==&typeF64) {
								insertFpuValue(start);
								return;
						}
						if(isTypecastFromF64) return;
				}
				// Is a binop or unop,so replace single use variables(and temporary) with registers ,or if used elsewhere,assign into a register
				// If not a variable,assign into register
				int isFpOp=0;
				//Node may be a typecast to a F64,but the input(and operation) may not be floating point operation
				if(objectBaseType(IRNodeType(start))==&typeF64&&!isTypecastFromF64) {
						isFpOp=1;
				} else if(isX87CmpOp(start)) {
						isFpOp=1;
				}

				strGraphEdgeIRPDestroy(&in);
				in =IREdgesByPrec(start);
				for(long i =0;i!=strGraphEdgeIRPSize(in);i++) {
						__auto_type node=graphEdgeIRIncoming(in[i]);
						exprRecur(node,refCounts,isFpOp||isTypecastFromF64);
				}

				//Pop incoming registers
				//Recompute incoming as recursing may have modified incoming nodes
				strGraphEdgeIRP in2 CLEANUP(strGraphEdgeIRPDestroy)=IREdgesByPrec(start);
				for(long i=0;i!=strGraphEdgeIRPSize(in2);i++) {
						struct IRNodeValue *val=(void*)graphNodeIRValuePtr(graphEdgeIRIncoming(in2[i]));
						if(val->base.type!=IR_VALUE)
								continue;
						if(val->val.type==IR_VAL_REG) {
								__auto_type reg=val->val.value.reg.reg;
								if(isX87Reg(reg))
										X87FpuPopReg();
						}
				}
				//Push result
				if(isFpOp&&!isX87CmpOp(start))
						insertFpuValue(start);
		} else if(graphNodeIRValuePtr(start)->type==IR_VALUE) {
				if(strGraphEdgeIRPSize(in)==1)
						exprRecur(graphEdgeIRIncoming(in[0]),refCounts,0);

				//REcompute incoming as recursing may have modified incoming nodes
				strGraphEdgeIRPDestroy(&in);
				in=IREdgesByPrec(start);

				//Can pop incoming register if assigning as it isn't usefull anymore
				strGraphEdgeIRP dst1 CLEANUP(strGraphEdgeIRPDestroy)=IRGetConnsOfType(in, IR_CONN_DEST);
				strGraphEdgeIRP dst2 CLEANUP(strGraphEdgeIRPDestroy)=IRGetConnsOfType(in, IR_CONN_ASSIGN_FROM_PTR);
				if(strGraphEdgeIRPSize(dst1)||strGraphEdgeIRPSize(dst2)) {
						__auto_type dst=strGraphEdgeIRPSize(dst1)?dst1:dst2;
						struct IRNodeValue *val=(void*)graphNodeIRValuePtr(graphEdgeIRIncoming(dst[0]));
						if(val->base.type==IR_VALUE)
								if(val->val.type==IR_VAL_REG)
										if(isX87Reg(val->val.value.reg.reg))
												X87FpuPopReg();
				}

		} else {
				fputs("Can't make sense of this node as an expression.\n", stderr);
				abort();
		}
		return;
}
static void debugShowGraphIR(graphNodeIR enter) {
		const char *name = tmpnam(NULL);
	__auto_type map = graphNodeCreateMapping(enter, 1);
	IRGraphMap2GraphViz(map, "viz", name, NULL, NULL, NULL, NULL);
	char buffer[1024];
	sprintf(buffer, "sleep 1 &&dot -Tsvg %s > /tmp/dot.svg && firefox /tmp/dot.svg & ", name);

	system(buffer);
}

void IRRegisterAllocateX87(graphNodeIR start) {
		strGraphNodeIRP allNodes CLEANUP(strGraphNodeIRPDestroy)=graphNodeIRAllNodes(start);
		strGraphNodeIRP exprEnds CLEANUP(strGraphNodeIRPDestroy)=NULL;

		ptrMapRefCount refCounts=ptrMapRefCountCreate();
		for(long n=0;n!=strGraphNodeIRPSize(allNodes);n++) {
				__auto_type end=IREndOfExpr(allNodes[n]);
				__auto_type start=IRStmtStart(allNodes[n]);
				if(!end)
						continue;
				if(start==end)
						continue;
				if(end!=allNodes[n])
						continue;
				if(!strGraphNodeIRPSortedFind(exprEnds, end, (gnCmpType)ptrPtrCmp))
						exprEnds=strGraphNodeIRPSortedInsert(exprEnds, end, (gnCmpType)ptrPtrCmp);
		}
		for(long n=0;n!=strGraphNodeIRPSize(allNodes);n++) {
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
				nodeStack=NULL;
				fpuRegsInUse=NULL;
				//Is connected to a return node
				int isConnected2Ret=0;
				strGraphNodeIRP out CLEANUP(strGraphNodeIRPDestroy)=graphNodeIROutgoingNodes(exprEnds[e]);
				if(strGraphNodeIRPSize(out)==1)
 						if(graphNodeIRValuePtr(out[0])->type==IR_FUNC_RETURN)
								isConnected2Ret=1;
				
				exprRecur(exprEnds[e],refCounts,1);
				if(strRegPSize(fpuRegsInUse)) {
						if(strRegPSize(fpuRegsInUse)==1) {
								if(isConnected2Ret)
										goto next;
								spillFpuStack();
						} else {
								debugShowGraphIR(start);
								assert(0);
						}
				}
		next:;
				strGraphNodeIRPDestroy(&nodeStack);
				strRegPDestroy(&fpuRegsInUse);
		}
}
