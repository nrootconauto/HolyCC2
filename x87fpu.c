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
		return *(void**)a-*(void**)b;
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
struct reg *X87FpuPopReg(graphNodeIR node) {
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
				return NULL;

		struct IRAttr attr;
		attr.destroy=NULL;
		attr.name=IR_ATTR_X87FPU_POP_AT;
		__auto_type ll=__llCreate(&attr, sizeof(attr));
		IRAttrReplace(node, ll);
		
		fpuRegsInUse=strRegPRemoveItem(fpuRegsInUse, stack[cur-1], (regCmpType)ptrPtrCmp);
		return stack[cur-1];
}
strRegP X87FpuRegsInUse() {
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
static void exprRecur(graphNodeIR start,ptrMapRefCount refCounts,int parentIsFpuOp) {
		if(graphNodeIRValuePtr(start)->type==IR_TYPECAST)
				return;
		if(graphNodeIRValuePtr(start)->type==IR_DERREF)
				return;
		
		int isFunc=graphNodeIRValuePtr(start)->type==IR_FUNC_CALL;
		strGraphEdgeIRP in CLEANUP(strGraphEdgeIRPDestroy)=IREdgesByPrec(start);
		if(isX87CmpOp(start))
				in=strGraphEdgeIRPReverse(in);
		
		if(isFunc) {
				for(long a=0;a!=strGraphEdgeIRPSize(in);a++) {
						__auto_type node=graphEdgeIRIncoming(in[a]);
						exprRecur(node,refCounts,0);
						//epxrRecur may have modified incoming edges so recompute
						strGraphEdgeIRPDestroy(&in);
						in=IREdgesByPrec(start);
						if(isX87CmpOp(start))
								in=strGraphEdgeIRPReverse(in);
						
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
								X87FpuPopReg(varRef);
						}
				}
				if(objectBaseType(IRNodeType(start))!=&typeF64)
						return;
				goto output;
		} else if(graphNodeIRValuePtr(start)->type!=IR_VALUE) {
				int isTypecastFromF64=0;
				if(graphNodeIRValuePtr(start)->type==IR_TYPECAST) {
						if(objectBaseType(IRNodeType(graphEdgeIRIncoming(in[0])))==&typeF64) {
								//Just return if typecast to F64 from F64
								if( objectBaseType(IRNodeType(start))==&typeF64)
										return;
								isTypecastFromF64=1;
						}
				}
				// Is a binop or unop,so replace single use variables(and temporary) with registers ,or if used elsewhere,assign into a register
				// If not a variable,assign into register
				int isFpOp=0;
				//Node may be a typecast to a F64,but the input(and operation) may not be floating point operation
				if(objectBaseType(IRNodeType(start))==&typeF64&&!isTypecastFromF64) {
						isFpOp=1;
				} else if(isX87CmpOp(start)) {
						isFpOp=1;
				} else if(!isTypecastFromF64) 
						return;
				for(long i =0;i!=strGraphEdgeIRPSize(in);i++) {
						__auto_type node=graphEdgeIRIncoming(in[i]);
						exprRecur(node,refCounts,isFpOp||isTypecastFromF64);
						if(isFpOp)
								nodeStack=strGraphNodeIRPAppendItem(nodeStack,node);
				}
				if(isFpOp||isTypecastFromF64) {
						for(long i=strGraphEdgeIRPSize(in)-1;i>=0;i--) {
								__auto_type node=graphEdgeIRIncoming(in[i]);
								struct IRNodeValue *value=(void*)graphNodeIRValuePtr(node);
								if(value->val.type==IR_VAL_VAR_REF) {
										__auto_type var=value->val.value.var.var;
										if(var->isTmp&&*ptrMapRefCountGet(refCounts, var)==1) {
												strGraphNodeIRP toReplace CLEANUP(strGraphNodeIRPDestroy)=strGraphNodeIRPAppendItem(NULL, node);
												/*
												__auto_type slice=pushedStXslice();
												__auto_type st0=IRCreateRegRef( &slice);
												graphIRReplaceNodes(toReplace, st0, NULL, (void(*)(void*))IRNodeDestroy);
												nodeStack=strGraphNodeIRPPop(nodeStack, NULL);
												*/												
												continue;
										}
							}
								//Assign value into register
								/*
								__auto_type slice=pushedStXslice();
								__auto_type st0=IRCreateRegRef( &slice);
								IRInsertAfter(node, st0, st0, IR_CONN_DEST);
								nodeStack=strGraphNodeIRPPop(nodeStack, NULL);
								*/
								__auto_type varRef=IRCreateVarRef(IRCreateVirtVar(&typeF64));
								IRInsertAfter(node, varRef, varRef, IR_CONN_DEST);
								X87FpuPopReg(varRef);
						}
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
										X87FpuPopReg(graphEdgeIRIncoming(in2[i]));
						}
				}
				//No need to insert ST register at output if typecast unless typecast to flaoting type
				if(isTypecastFromF64) {
						struct IRNodeTypeCast *cast=(void*)graphNodeIRValuePtr(start);
						if(objectBaseType(cast->out)!=&typeF64)
								return;
				}
				goto output;
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
						if(val->base.type!=IR_VALUE)
								return;
						if(val->val.type!=IR_VAL_REG)
								return;
						if(!isX87Reg(val->val.value.reg.reg))
								return;
						//TODO assert that it i ST(0)
						X87FpuPopReg(graphEdgeIRIncoming(dst[0]));
						return;
				}

				//If is a X87fpu register that leads to nowhere,can pop
				if(graphNodeIRValuePtr(start)->type==IR_VALUE) {
						struct IRNodeValue *val=(void*)graphNodeIRValuePtr(start);
						if(val->val.type!=IR_VAL_REG)
								return ;
						if(!isX87Reg(val->val.value.reg.reg))
								return ;
						
						strGraphEdgeIRP out CLEANUP(strGraphEdgeIRPDestroy)=graphNodeIROutgoing(start);
						strGraphEdgeIRP outDst1 CLEANUP(strGraphEdgeIRPDestroy)=IRGetConnsOfType(out, IR_CONN_DEST);
						strGraphEdgeIRP outDst2 CLEANUP(strGraphEdgeIRPDestroy)=IRGetConnsOfType(out, IR_CONN_ASSIGN_FROM_PTR);
						if(strGraphEdgeIRPSize(outDst1)==0&&strGraphEdgeIRPSize(outDst2)==0) {
								X87FpuPopReg(start);
								return;
						}
				}
		} else {
				fputs("Can't make sense of this node as an expression.\n", stderr);
				abort();
		}
		return;
	output:;
		if(isX87CmpOp(start))
				return;
		
		strGraphEdgeIRP out CLEANUP(strGraphEdgeIRPDestroy)=graphNodeIROutgoing(start);
		strGraphEdgeIRP outDst1 CLEANUP(strGraphEdgeIRPDestroy)=IRGetConnsOfType(out, IR_CONN_DEST);
		strGraphEdgeIRP outDst2 CLEANUP(strGraphEdgeIRPDestroy)=IRGetConnsOfType(out, IR_CONN_ASSIGN_FROM_PTR);
		strGraphEdgeIRP outDst=(strGraphEdgeIRPSize(outDst1))?outDst1:outDst2;
		
		if(strGraphEdgeIRPSize(outDst)==1) {
				struct IRNodeValue *val=(void*)graphNodeIRValuePtr(graphEdgeIROutgoing(outDst[0]));
				if(val->val.type!=IR_VAL_REG)
						goto insertSt0;
				if(val->val.value.reg.reg!=&regX86ST0)
						goto insertSt0;
				return ;
		insertSt0: {
						/*
						__auto_type slice=pushedStXslice();
						__auto_type st0=IRCreateRegRef(&slice);
						IRInsertAfter(start, st0, st0, IR_CONN_DEST);
						*/

						/*
						__auto_type varRef=IRCreateVarRef(IRCreateVirtVar(&typeF64));
						IRInsertAfter(start, varRef, varRef, IR_CONN_DEST);
						X87FpuPopReg(varRef);
						*/
				}
		}
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
				strGraphNodeIRPDestroy(&nodeStack);
				nodeStack=NULL;
				exprRecur(exprEnds[e],refCounts,0);
				assert(!strRegPSize(fpuRegsInUse));
		}
}
