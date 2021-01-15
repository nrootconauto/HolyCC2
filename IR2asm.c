#include <IR.h>
#include <stdio.h>
#include <cleanup.h>
#include <assert.h>
#include <registers.h>
#include <ptrMap.h>
#include <hashTable.h>
#include <asmEmitter.h>
#include <parserA.h>
#include <parserB.h>
#include <ctype.h>
#include <ieee754.h>
#include <IRTypeInference.h>
#include <frameLayout.h>
#include <regAllocator.h>
#include <IR2asm.h>
STR_TYPE_DEF(char,Char);
STR_TYPE_FUNCS(char,Char);
PTR_MAP_FUNCS(graphNodeIR , strChar, LabelNames);
PTR_MAP_FUNCS(graphNodeIR , int, CompiledNodes);
PTR_MAP_FUNCS(struct parserFunction *, strChar,FuncNames);
PTR_MAP_FUNCS(struct parserVar *,long,FrameOffset);
static __thread long labelsCount;
static __thread ptrMapFuncNames funcNames;
static __thread ptrMapLabelNames asmLabelNames;
static __thread ptrMapFrameOffset varFrameOffsets;
static __thread ptrMapCompiledNodes compiledNodes;
static __thread int insertLabelsForAsmCalled=0;
static graphNodeIR nodeDest(graphNodeIR node) {
		strGraphEdgeIRP out CLEANUP(strGraphEdgeIRPDestroy)=graphNodeIROutgoing(node);
		assert(strGraphEdgeIRPSize(out)==1);
		return graphEdgeIROutgoing(out[0]);
}
static void binopArgs(graphNodeIR node,graphNodeIR *a,graphNodeIR *b) {
		strGraphEdgeIRP in CLEANUP(strGraphEdgeIRPDestroy)=graphNodeIRIncoming(node);
		strGraphEdgeIRP inA CLEANUP(strGraphEdgeIRPDestroy)=IRGetConnsOfType(in, IR_CONN_SOURCE_A);
		strGraphEdgeIRP inB CLEANUP(strGraphEdgeIRPDestroy)=IRGetConnsOfType(in, IR_CONN_SOURCE_B);
		if(strGraphEdgeIRPSize(inA)==1)
				*a=graphEdgeIRIncoming(inA[0]);
		else assert(0);
		if(strGraphEdgeIRPSize(inB)==1)
				*b=graphEdgeIRIncoming(inB[0]);
		else assert(0);
}
static int isReg(graphNodeIR node) {
		if(graphNodeIRValuePtr(node)->type==IR_VALUE) {
				struct IRNodeValue *val=(void*)graphNodeIRValuePtr(node);
				return val->val.type==IR_VAL_REG;
		}
		return 0;
}
#define ALLOCATE(val) ({typeof(val) *r=malloc(sizeof(val));*r=val;r; })
static strChar strClone(const char *name) {
		return strCharAppendData(NULL, name, strlen(name)+1);
}
static strChar  unescapeString(const char *str) {
		char *otherValids="[]{}\\|;:\"\'<>?,./`~!@#$%^&*()-_+=";
		long len=strlen(str);
		strChar retVal=NULL;
		for(long i=0;i!=len;i++) {
				if(isalnum(str[i])) {
						retVal=strCharAppendItem(retVal, str[i]);
				} else {
						if(strchr(otherValids, str[i])) {
								retVal=strCharAppendItem(retVal, str[i]);
						} else {
								long count=snprintf(NULL, 0, "\\%02x",((uint8_t*)str)[i]);
								char buffer[count+1];
								sprintf(buffer,  "\\%02x",((uint8_t*)str)[i]);
								retVal=strCharAppendData(retVal, buffer, strlen(buffer));
						}
				}
		}
		return retVal;
}
static struct X86AddressingMode node2AddrMode(graphNodeIR start) {
		if(graphNodeIRValuePtr(start)->type==IR_VALUE) {
				struct IRNodeValue *value=(void*)graphNodeIRValuePtr(start);
				switch(value->val.type) {
				case __IR_VAL_MEM_FRAME: {
						if(getCurrentArch()==ARCH_X86_SYSV||getCurrentArch()==ARCH_X64_SYSV) {
								return X86AddrModeIndirSIB(0, NULL, (getCurrentArch()==ARCH_X86_SYSV)?&regX86ESP:&regAMD64RSP, value->val.value.__frame.offset, IRNodeType(start));
						} else {
								assert(0); //TODO  implement
						}
				}
				case __IR_VAL_MEM_GLOBAL:  {
						struct X86AddressingMode mode=X86AddrModeLabel(value->val.value.__global.symbol->name);
						mode.valueType=IRNodeType(start);
						return  mode;
				}
				case __IR_VAL_LABEL:  {
						graphNodeIR label=value->val.value.__label;
						const char *name=NULL;
						if(ptrMapLabelNamesGet(asmLabelNames, label)) {
								name=*ptrMapLabelNamesGet(asmLabelNames, label);
						} else {
								long count=snprintf(NULL, 0, "LBL%li", ++labelsCount);
								char *name2=malloc(count+1);
								sprintf(name2, "LBL%li", labelsCount);
								ptrMapLabelNamesAdd(asmLabelNames, label,strClone(name2));
								name=name2;
						}
						return X86AddrModeLabel(name);
				}
				case IR_VAL_REG: {
						return X86AddrModeReg(value->val.value.reg.reg);
				}
				case IR_VAL_VAR_REF: {
						fprintf(stderr, "CONVERT VARIABLE REFERENCES TO FRAME ADDRESSES!!!");
						assert(0);
				}
				case IR_VAL_FUNC: {
						struct X86AddressingMode mode;
						mode.type=X86ADDRMODE_LABEL;
						mode.valueType=NULL;
						const char *name=value->val.value.func->name;
						if(!name) {
								__auto_type find= ptrMapFuncNamesGet(funcNames , value->val.value.func);
								if(find) {
										return X86AddrModeLabel(*find);
								} else {
										long count=snprintf(NULL, 0, "$ANONF_%li", ++labelsCount);
										char buffer[count+1];
										sprintf(buffer,"$ANONF_%li", labelsCount);
										mode.value.label=strcpy(malloc(strlen(buffer)+1),buffer);
										ptrMapFuncNamesAdd(funcNames, value->val.value.func, strcpy(malloc(count+1),buffer));
										return X86AddrModeLabel(buffer);
								}
						}
						//Is a global (named) symbol?
						if(value->val.value.func->parentFunction==NULL) {
								__auto_type link=parserGlobalSymLinkage(name);
								if(link->fromSymbol) {
										return X86AddrModeLabel(link->fromSymbol);
								}
								return X86AddrModeLabel(name);
						} else {
								//Is a local function
								__auto_type find=ptrMapFuncNamesGet(funcNames, value->val.value.func);
								if(!find) {
										long count =snprintf(NULL, 0, "$LOCF_%s$%li",name,++labelsCount);
										char buffer[count+1];
										sprintf(buffer, "$LOCF_%s$%li",name,labelsCount);
										ptrMapFuncNamesAdd(funcNames, value->val.value.func,strcpy(malloc(count+1), buffer));
										find=ptrMapFuncNamesGet(funcNames, value->val.value.func);
								}
								return X86AddrModeLabel(*find);
						}
				}
				case IR_VAL_INT_LIT: {
						struct X86AddressingMode mode;
						mode.valueType=NULL;
						if(value->val.value.intLit.type==INT_SLONG) {
								return X86AddrModeSint(value->val.value.intLit.value.sLong);
						} else if(value->val.value.intLit.type==INT_ULONG) {
								return X86AddrModeSint(value->val.value.intLit.value.uLong);
						}
				}
				case IR_VAL_STR_LIT: {
						return X86EmitAsmStrLit(value->val.value.strLit);	
				}
				case IR_VAL_FLT_LIT: {
						uint64_t encoded=IEEE754Encode(value->val.value.fltLit);
						return X86EmitAsmDU64(&encoded, 1);
				}
				}
		}
		assert(0);
		return X86AddrModeSint(-1);
}
static void strX86AddrModeDestroy2(strX86AddrMode *str) {
		for(long i=0;i!=strX86AddrModeSize(*str);i++)
				X86AddrModeDestroy(&str[0][i]);
		strX86AddrModeDestroy(str);
}
static void assembleInst(const char *name,strX86AddrMode args) {
		strOpcodeTemplate  ops CLEANUP(strOpcodeTemplateDestroy)=X86OpcodesByArgs(name, args , NULL);
		assert(strOpcodeTemplateSize(ops));
		int err;
		X86EmitAsmInst(ops[0], args, &err);
		assert(!err);
}
static struct reg* destReg() {
		switch(getCurrentArch()) {
		case ARCH_X64_SYSV: return &regAMD64RDI;
		case ARCH_TEST_SYSV:
		case ARCH_X86_SYSV: return &regX86EDI;
		}
}
static long ptrSize() {
		switch(getCurrentArch()) {
		case ARCH_X64_SYSV: return 8;
		case ARCH_TEST_SYSV:
		case ARCH_X86_SYSV: return 4;
		}
}
static struct reg* sourceReg() {
		switch(getCurrentArch()) {
		case ARCH_X64_SYSV: return &regAMD64RSI;
		case ARCH_TEST_SYSV:
		case ARCH_X86_SYSV: return &regX86ESI;
		}
}
static int isX87FltReg(const struct reg *r) {
		const struct reg *regs[]={
				&regX86ST0,
				&regX86ST1,
				&regX86ST2,
				&regX86ST3,
				&regX86ST4,
				&regX86ST5,
				&regX86ST6,
				&regX86ST7,
		};
		for(long i=0;i!=sizeof(regs)/sizeof(*regs);i++)
				if(regs[i]==r)
						return 1;
		return 0;
}
static int isGPReg(const struct reg *r) {
		const struct reg *regs[]={
				&regAMD64RAX,
				&regAMD64RBX,
				&regAMD64RCX,
				&regAMD64RDX,
				&regAMD64RSP,
				&regAMD64RBP,
				&regAMD64RSI,
				&regAMD64RDI,
				&regAMD64R8u64,
				&regAMD64R9u64,
				&regAMD64R10u64,
				&regAMD64R11u64,
				&regAMD64R12u64,
				&regAMD64R13u64,
				&regAMD64R14u64,
				&regAMD64R15u64,
		};
		struct regSlice rSlice;
		rSlice.reg=(void*)r;
		rSlice.offset=0;
		rSlice.type=NULL;
		rSlice.widthInBits=r->size*8;
		for(long i=0;i!=sizeof(regs)/sizeof(*regs);i++) {
				struct regSlice rSlice2;
				rSlice2.reg=(void*)regs[i];
				rSlice2.offset=0;
				rSlice2.type=NULL;
				rSlice2.widthInBits=regs[i]->size*8;
				if(regSliceConflict(&rSlice , &rSlice2))
						return 1;
		}
		return 0;
}
static int isFuncEnd(const struct __graphNode *node,graphNodeIR end) {
		return node==end;
}
static int ptrPtrCmp(const void *a, const void *b) {
	if (*(void **)a > *(void **)b)
		return 1;
	else if (*(void **)a < *(void **)b)
		return -1;
	else
		return 0;
}
typedef int(*gnCmpType)(const graphNodeIR *,const graphNodeIR *);
static strGraphNodeIRP getFuncNodes(graphNodeIR startN) {
		struct IRNodeFuncStart *start=(void*)graphNodeIRValuePtr(startN);
		strGraphEdgeP allEdges CLEANUP(strGraphEdgePDestroy)= graphAllEdgesBetween(startN, start->end, (int(*)(const struct __graphNode *, const void *))isFuncEnd);
		strGraphNodeIRP allNodes=NULL;
		for(long i=0;i!=strGraphEdgePSize(allEdges);i++) {
				strGraphNodeIRP frontBack=strGraphNodeIRPAppendItem(NULL, graphEdgeIRIncoming(allEdges[i]));
				frontBack=strGraphNodeIRPSortedInsert(frontBack, graphEdgeIROutgoing(allEdges[i]),(gnCmpType)ptrPtrCmp);
				allNodes=strGraphNodeIRPSetUnion(allNodes, frontBack, (gnCmpType)ptrPtrCmp);
		}
		return allNodes;
}
STR_TYPE_DEF(struct parserVar *,PVar);
STR_TYPE_FUNCS(struct parserVar *,PVar);
typedef int(*PVarCmpType)(const struct parserVar **,const struct parserVar **);
static int isPrimitiveType(const struct object *obj) {
		const struct object *prims[]={
				&typeU0,
				&typeU8i,
				&typeU16i,
				&typeU32i,
				&typeU64i,
				&typeI8i,
				&typeI16i,
				&typeI32i,
				&typeI64i,
				&typeF64,
		};
		for(long i=0;i!=sizeof(prims)/sizeof(*prims);i++) {
				if(prims[i]==obj)
						return 1;
		}
		return 0;
}
static int isNotNoreg(const struct parserVar *var,const void *data) {
		return !var->isNoreg;
}
static int frameEntryCmp(const void *a,const  void *b) {
		const struct frameEntry *A=a;
		const struct frameEntry *B=b;
		return IRVarCmp(&A->var, &B->var);
}
static void compileFunction(graphNodeIR start) {
		strGraphNodeIRP nodes CLEANUP(strGraphNodeIRPDestroy)=getFuncNodes(start);
		// Get list of variables that will always be stored in memory
		// - variables that are referenced by ptr
		// - Classes/unions with primitive base that have members references(I64.u8[1] etc)
		// - Vars marked as noreg
		strPVar noregs CLEANUP(strPVarDestroy)=NULL;
		strPVar inRegs CLEANUP(strPVarDestroy)=NULL;
		for(long i=0;i!=strGraphNodeIRPSize(nodes);i++) {
				struct IRNodeValue *value=(void*)graphNodeIRValuePtr(nodes[i]);
				if(value->base.type!=IR_VALUE)
						continue;
				if(value->val.type!=IR_VAL_VAR_REF)
						continue;
				strGraphNodeIRP out CLEANUP(strGraphNodeIRPDestroy)=graphNodeIROutgoingNodes(nodes[i]);

				if(!isPrimitiveType(objectBaseType(IRNodeType(nodes[i]))))
						goto markAsNoreg;
				
				if(strGraphNodeIRPSize(out)==1) {
						if(graphNodeIRValuePtr(out[0])->type==IR_ADDR_OF)
								goto markAsNoreg;
						
						if(isPrimitiveType(objectBaseType(IRNodeType(nodes[i]))))
								if(graphNodeIRValuePtr(out[0])->type==IR_MEMBERS)
										goto markAsNoreg;
				}
				
				if(value->val.value.var.value.var->isNoreg)
						goto markAsNoreg;

				__auto_type var=value->val.value.var.value.var;
				if(!strPVarSortedFind(inRegs, var, (PVarCmpType)ptrPtrCmp))
								inRegs=strPVarSortedInsert(inRegs, var, (PVarCmpType)ptrPtrCmp);
				continue;
		markAsNoreg: {
						__auto_type var=value->val.value.var.value.var;
						if(!strPVarSortedFind(noregs, var, (PVarCmpType)ptrPtrCmp))
								noregs=strPVarSortedInsert(noregs, var, (PVarCmpType)ptrPtrCmp);
				}
		}
		for(long i=0;i!=strPVarSize(noregs);i++)
				noregs[i]->isNoreg=1;
		//
		// Disconnct rest of function from the rest of the nodes(We will reconnect later)
		// Do this so register allocator doesn't  try to allocate for things outside of function
		struct IRNodeFuncStart *startNode=(void*)graphNodeIRValuePtr(start);
		strGraphNodeIRP startIn CLEANUP(strGraphNodeIRPDestroy)=graphNodeIRIncomingNodes(start);
		for(long i=0;i!=strGraphNodeIRPSize(startIn);i++)
				graphEdgeIRKill(startIn[i], start, NULL, NULL, NULL);
		strGraphNodeIRP endOut CLEANUP(strGraphNodeIRPDestroy)=graphNodeIRIncomingNodes(startNode->end);
		for(long i=0;i!=strGraphNodeIRPSize(endOut);i++)
				graphEdgeIRKill(startNode->end,endOut[i], NULL, NULL, NULL);

		//TODO disconnect functions with function
		
		IRRegisterAllocate(start, NULL, NULL, isNotNoreg, noregs);
		
		strGraphNodeIRP regAllocedNodes CLEANUP(strGraphNodeIRPDestroy)=getFuncNodes(start);
		//Replace all spills/loads with variable,then we compute a frame layout
		for(long i=0;i!=strGraphNodeIRPSize(regAllocedNodes);i++) {
				if(graphNodeIRValuePtr(regAllocedNodes[i])->type==IR_SPILL_LOAD) {
						struct IRNodeSpill *spillLoad=(void*)graphNodeIRValuePtr(regAllocedNodes[i]);
						if(spillLoad->item.value.var.type==IR_VAL_VAR_REF) {
								__auto_type var=spillLoad->item.value.var.value.var;
								strGraphNodeP toReplace CLEANUP(strGraphNodePDestroy)=strGraphNodePAppendItem(NULL,regAllocedNodes[i]);
								graphReplaceWithNode(toReplace,IRCreateVarRef(var),NULL,(void(*)(void*))IRNodeDestroy, sizeof(enum IRConnType));
						} else {
								assert(0);
						}
				}
		}

		//"Push" the old frame layout
		__auto_type oldOffsets=varFrameOffsets;
		
		strFrameEntry layout CLEANUP(strFrameEntryDestroy)=IRComputeFrameLayout(start);
		varFrameOffsets=ptrMapFrameOffsetCreate();
		for(long i=0;i!=strFrameEntrySize(layout);i++)
				ptrMapFrameOffsetAdd(varFrameOffsets, layout[i].var.value.var, layout[i].offset);
		ptrMapFrameOffsetDestroy(varFrameOffsets,NULL);

		
		
		//"Pop" the old frame layout
		varFrameOffsets=oldOffsets;
		
}
static void assign(struct X86AddressingMode a,struct X86AddressingMode b,long size) {
		if(a.type==X86ADDRMODE_REG) {
				if(isX87FltReg(a.value.reg)) {
				}
		}
		strX86AddrMode args CLEANUP(strX86AddrModeDestroy)=NULL;
		strOpcodeTemplate ops CLEANUP(strOpcodeTemplateDestroy)=NULL;
		if(size==1||size==2||size==4||size==8) {
				args=strX86AddrModeAppendItem(NULL, a);
				args=strX86AddrModeAppendItem(NULL, b);
		}
		if(size==1) {
				ops=X86OpcodesByArgs("MOV", args, NULL);
				return;
		} else if(size==2) {
				ops=X86OpcodesByArgs("MOV", args, NULL);
				return;
		} else if(size==4) {
				ops=X86OpcodesByArgs("MOV", args, NULL);
				return;
		} else if(size==8) {
				ops=X86OpcodesByArgs("MOV", args, NULL);
				return;
		} else {
				long repCount,width;
				if(size%8) {
						if(size<=(8*0xffff)) {
								repCount=size/8;
								width=8;
						} else goto callMemcpy;
				} else if(size&4) {
						if(size<=(4*0xffff)) {
								repCount=size/4;
								width=4;
						} else goto callMemcpy;
				} else if(size&2) {
						if(size<=(2*0xffff)) {
								repCount=size/2;
								width=2;
						} else goto callMemcpy;
				} else if(size&1) {
						if(size<=(1*0xffff)) {
								repCount=size/1;
								width=1;
						} else goto callMemcpy;
				}
		repeatCode: {
						strX86AddrMode ppArgsCX CLEANUP(strX86AddrModeDestroy2)=strX86AddrModeAppendItem(NULL, X86AddrModeReg(&regX86CX));
						strX86AddrMode ppArgsRDI CLEANUP(strX86AddrModeDestroy2)=strX86AddrModeAppendItem(NULL, X86AddrModeReg(destReg()));
						strX86AddrMode ppArgsRSI CLEANUP(strX86AddrModeDestroy2)=strX86AddrModeAppendItem(NULL, X86AddrModeReg(sourceReg()));
						assembleInst("PUSH",ppArgsCX);
						assembleInst("PUSH",ppArgsRDI);
						assembleInst("PUSH",ppArgsRSI);
						struct X86AddressingMode cx CLEANUP(X86AddrModeDestroy)=X86AddrModeReg(&regX86CX);
						struct X86AddressingMode count CLEANUP(X86AddrModeDestroy)=X86AddrModeSint(repCount);
						assign(cx,count,2);
						struct X86AddressingMode rdi CLEANUP(X86AddrModeDestroy)=X86AddrModeReg(destReg());
						struct X86AddressingMode rsi CLEANUP(X86AddrModeDestroy)=X86AddrModeReg(sourceReg());
						assign(rdi,a,ptrSize());
						assign(rsi,b,ptrSize());
						switch(width) {
						case  1:
								assembleInst("REP_STOSB",NULL);break;
						case  2:
								assembleInst("REP_STOSW",NULL);break;
						case  4:
								assembleInst("REP_STOSD",NULL);break;
						case  8:
								assembleInst("REP_STOSQ",NULL);break;
						}
						assembleInst("PUSH",ppArgsRSI);
						assembleInst("PUSH",ppArgsRDI);
						assembleInst("POP",ppArgsCX);
						return;
				}
		}
	callMemcpy: {
				strRegP regs CLEANUP(strRegPDestroy)=NULL;
				switch (ptrSize()) {
				case 4:
						regs=regGetForType(&typeU32i);break;
				case 8:
						regs=regGetForType(&typeU64i);break;
				default:
						assert(0);
				}
				//PUSH rcx,rsi,rdi
				//MOV rcx,*size*
				//LABEL:
				//STOSB
				//LOOP LABEL
				//POP rdi,rsi,rcx
				
				strX86AddrMode ppArgsCount CLEANUP(strX86AddrModeDestroy2)=strX86AddrModeAppendItem(NULL, X86AddrModeReg(regs[0]));
				strX86AddrMode ppArgsRDI CLEANUP(strX86AddrModeDestroy2)=strX86AddrModeAppendItem(NULL, X86AddrModeReg(destReg()));
				strX86AddrMode ppArgsRSI CLEANUP(strX86AddrModeDestroy2)=strX86AddrModeAppendItem(NULL, X86AddrModeReg(sourceReg()));
				assembleInst("PUSH",ppArgsCount);
				assembleInst("PUSH",ppArgsRDI);
				assembleInst("PUSH",ppArgsRSI);
				char *labName=X86EmitAsmLabel(NULL);
				assembleInst("REP_MOVSB",NULL);
				strX86AddrMode labArg CLEANUP(strX86AddrModeDestroy2)=strX86AddrModeAppendItem(NULL, X86AddrModeLabel(labName));
				assembleInst("LOOP",NULL);
				free(labName);
				assembleInst("POP",ppArgsRSI);
				assembleInst("POP",ppArgsRDI);
				assembleInst("POP",ppArgsCount);
				return;
		}
}
static void assembleOpInt(graphNodeIR start,const char *opName) {
		graphNodeIR a,b,out=nodeDest(start);
		binopArgs(start, &a, &b);
		int hasReg=isReg(a)&&isReg(b);
		if(hasReg&&out) {
				//Load a into out then OP DEST,SRC as DEST=DEST OP SRC if sizeof(DEST)==sizeof(a)
				long oSize=objectSize(IRNodeType(out),NULL);
				long aSize=objectSize(IRNodeType(a),NULL);
				long bSize=objectSize(IRNodeType(b),NULL);
				assert(aSize==bSize&&aSize==oSize);

				assign(node2AddrMode(out), node2AddrMode(a), objectSize(IRNodeType(out), NULL));
				strX86AddrMode opArgs CLEANUP(strX86AddrModeDestroy)=strX86AddrModeAppendItem(NULL, node2AddrMode(out));
				opArgs=strX86AddrModeAppendItem(opArgs, node2AddrMode(b));
				assembleInst(opName, opArgs);
		} else if(out&&!hasReg) {
				long oSize=objectSize(IRNodeType(out),NULL);
				long aSize=objectSize(IRNodeType(a),NULL);
				long bSize=objectSize(IRNodeType(b),NULL);
				assert(aSize==bSize&&aSize==oSize);
						
				//Pick a register to store the result in,then push/pop that register
				strRegP regs CLEANUP(strRegPDestroy)=regGetForType(IRNodeType(out));
				__auto_type tmpReg=regs[0];
				strX86AddrMode ppArgs CLEANUP(strX86AddrModeDestroy)=strX86AddrModeAppendItem(NULL, X86AddrModeReg(tmpReg));
				assembleInst("PUSH", ppArgs);
						
				//tmpReg=a;
				assign(X86AddrModeReg(tmpReg), node2AddrMode(a), objectSize(IRNodeType(out), NULL));
				//OP tmpReg,b
				strX86AddrMode opArgs CLEANUP(strX86AddrModeDestroy)=strX86AddrModeAppendItem(NULL, X86AddrModeReg(tmpReg));
				opArgs=strX86AddrModeAppendItem(opArgs, node2AddrMode(b));
				assembleInst(opName, opArgs);
				//out=tmpReg
				assign(node2AddrMode(out), X86AddrModeReg(tmpReg), objectSize(IRNodeType(out), NULL));

				assembleInst("POP", ppArgs);
				return;
		}
}
static int isPtrNode(graphNodeIR start) {
		return objectBaseType(IRNodeType(start))->type==TYPE_PTR;
}
static int isFltNode(graphNodeIR start) {
		return objectBaseType(IRNodeType(start))==&typeF64;
}
static int isIntNode(graphNodeIR start) {
		const struct object *intTypes[]={
				&typeI8i,
				&typeI16i,
				&typeI32i,
				&typeI64i,
				&typeU8i,
				&typeU16i,
				&typeU32i,
				&typeU64i,
		};
		__auto_type obj=IRNodeType(start);
		for(long i=0;i!=sizeof(intTypes)/sizeof(*intTypes);i++)
				if(objectBaseType(obj)==intTypes[i])
						return 1;
		return 0;
}
static __thread strGraphNodeIRP fpuStackNodes;
STR_TYPE_DEF(long,Long);
STR_TYPE_FUNCS(long,Long);
static void compileX87Expr(graphNodeIR start) {
		strGraphNodeIRP execOrder CLEANUP(strGraphNodeIRPDestroy) =NULL;
		strGraphNodeIRP stack=strGraphNodeIRPAppendItem(NULL, start);
		const struct reg* fpuRegisters[]={
				&regX86ST0,
				&regX86ST1,
				&regX86ST2,
				&regX86ST3,
				&regX86ST4,
				&regX86ST5,
				&regX86ST6,
				&regX86ST7,
		};
		strX86AddrMode addrModeStack=NULL;
		while(strGraphNodeIRPSize(stack)) {
				graphNodeIR node;
				stack=strGraphNodeIRPPop(stack, &node);
				strGraphEdgeIRP in CLEANUP(strGraphEdgeIRPDestroy)=graphNodeIRIncoming(node);
				strGraphEdgeIRP dst CLEANUP(strGraphEdgeIRPDestroy)=IRGetConnsOfType(in, IR_CONN_DEST);
				if(strGraphEdgeIRPSize(dst)) {
						__auto_type inNode=graphEdgeIRIncoming(dst[0]);
						const char *instName=NULL;
						switch(graphNodeIRValuePtr(inNode)->type) {
						case IR_INC:
								goto unop;
						case IR_DEC:
								goto unop;
						case IR_ADD:
								goto binop;
						case IR_SUB:
								goto binop;
						case IR_POS:
								goto unop;
						case IR_NEG:
								goto unop;
						case IR_MULT:
								goto binop;
						case IR_DIV:
								goto binop;
						case IR_POW:
								goto binop;
						case IR_GT:
								goto binop;
						case IR_LT:
								goto binop;
						case IR_GE:
								goto binop;
						case IR_LE:
								goto binop;
						case IR_EQ:
								goto binop;
						case IR_NE:
								goto binop;
						case IR_VALUE:
								execOrder=strGraphNodeIRPAppendItem(execOrder, node);
								continue;
						case IR_TYPECAST:
								execOrder=strGraphNodeIRPAppendItem(execOrder, node);
								continue;
						default:
								assert(0);
						}
				unop: {
								strGraphEdgeIRP arg CLEANUP(strGraphEdgeIRPDestroy)=IRGetConnsOfType(in, IR_CONN_SOURCE_A);
								execOrder=strGraphNodeIRPAppendItem(execOrder, node);
								stack=strGraphNodeIRPAppendItem(stack, graphEdgeIRIncoming(arg[0]));
								continue;
						}
				binop: {
								strGraphEdgeIRP argA CLEANUP(strGraphEdgeIRPDestroy)=IRGetConnsOfType(in, IR_CONN_SOURCE_A);
								execOrder=strGraphNodeIRPAppendItem(execOrder, node);
								stack=strGraphNodeIRPAppendItem(stack, graphEdgeIRIncoming(argA[0]));
								
								strGraphEdgeIRP argB CLEANUP(strGraphEdgeIRPDestroy)=IRGetConnsOfType(in, IR_CONN_SOURCE_B);
								stack=strGraphNodeIRPAppendItem(stack, graphEdgeIRIncoming(argB[0]));
								continue;
						}
				}
		}
		execOrder=strGraphNodeIRPReverse(execOrder);
		for(long i=0;i!=strGraphNodeIRPSize(execOrder);i++) {
				strGraphEdgeIRP out CLEANUP(strGraphEdgeIRPDestroy)=graphNodeIROutgoing(execOrder[i]);
				strGraphEdgeIRP dst CLEANUP(strGraphEdgeIRPDestroy)=IRGetConnsOfType(out, IR_CONN_DEST);
				switch(graphNodeIRValuePtr(execOrder[i])->type) {
				case IR_INC: {
						assembleInst("FLD1", NULL);
						assembleInst("FADDP", NULL);
						continue;
				}
				case IR_DEC: {
						assembleInst("FLD1", NULL);
						assembleInst("FSUBP", NULL);
						continue;
				}
				case IR_ADD: {
						assembleInst("FADDP", NULL);
						continue;
				}
				case IR_SUB: {
						assembleInst("FSUBP", NULL);
						continue;
				}
				case IR_POS: {
						continue;
				}
				case IR_NEG:{
						assembleInst("FCHS", NULL);
						continue;
				}
				case IR_MULT:	{
						assembleInst("FMULP", NULL);
						continue;
				}
				case IR_DIV: {
						assembleInst("FDIVP", NULL);
						continue;
				}
				case IR_POW: {
						//TODO
						assembleInst("FMULP", NULL);
						continue;
				}
				case IR_GT: {
						assembleInst("FCOM", NULL);
						strX86AddrMode setccArgs CLEANUP(strX86AddrModeDestroy2)=NULL;
						setccArgs=strX86AddrModeAppendItem(setccArgs,node2AddrMode(graphEdgeIROutgoing(dst[0])));
						assembleInst("SETG",setccArgs);
						continue;
				}
				case IR_LT: {
						assembleInst("FCOM", NULL);
						strX86AddrMode setccArgs CLEANUP(strX86AddrModeDestroy2)=NULL;
						setccArgs=strX86AddrModeAppendItem(setccArgs,node2AddrMode(graphEdgeIROutgoing(dst[0])));
						assembleInst("SETL",setccArgs);
						continue;
				}
				case IR_GE:{
						assembleInst("FCOM", NULL);
						strX86AddrMode setccArgs CLEANUP(strX86AddrModeDestroy2)=NULL;
						setccArgs=strX86AddrModeAppendItem(setccArgs,node2AddrMode(graphEdgeIROutgoing(dst[0])));
						assembleInst("SETGE",setccArgs);
						continue;
				}
				case IR_LE: {
						assembleInst("FCOM", NULL);
						strX86AddrMode setccArgs CLEANUP(strX86AddrModeDestroy2)=NULL;
						setccArgs=strX86AddrModeAppendItem(setccArgs,node2AddrMode(graphEdgeIROutgoing(dst[0])));
						assembleInst("SETLE",setccArgs);
						continue;
				}
				case IR_EQ:{
						assembleInst("FCOM", NULL);
						strX86AddrMode setccArgs CLEANUP(strX86AddrModeDestroy2)=NULL;
						setccArgs=strX86AddrModeAppendItem(setccArgs,node2AddrMode(graphEdgeIROutgoing(dst[0])));
						assembleInst("SETE",setccArgs);
						continue;
				}
				case IR_NE:{
						assembleInst("FCOM", NULL);
						strX86AddrMode setccArgs CLEANUP(strX86AddrModeDestroy2)=NULL;
						setccArgs=strX86AddrModeAppendItem(setccArgs,node2AddrMode(graphEdgeIROutgoing(dst[0])));
						assembleInst("SETNE",setccArgs);
						continue;
				}
				case IR_VALUE: {
						struct X86AddressingMode b=node2AddrMode(execOrder[i]);
						if(b.type==X86ADDRMODE_FLT||b.type==X86ADDRMODE_UINT||b.type==X86ADDRMODE_SINT) {
								double value;
								if(b.type==X86ADDRMODE_SINT) {
										value=b.value.sint;
								} else if(b.type==X86ADDRMODE_UINT) {
										value=b.value.uint;		
								}  else if(b.type==X86ADDRMODE_FLT) {
										value=b.value.flt;
								}
								uint64_t enc =IEEE754Encode(b.value.flt);
								strX86AddrMode args CLEANUP(strX86AddrModeDestroy2)=NULL;
								args=strX86AddrModeAppendItem(NULL, b);
								__auto_type addr=X86EmitAsmDU64(&enc, 1);
								addr.valueType=&typeU64i;
								args=strX86AddrModeAppendItem(args, addr);
								assembleInst("FLD",args);
						} else if(b.type==X86ADDRMODE_REG) {
								if(isX87FltReg(b.value.reg)) {
										strX86AddrMode args CLEANUP(strX86AddrModeDestroy2)=NULL;
										args=strX86AddrModeAppendItem(NULL, b);
										assembleInst("FLD",args);
								} else if(isGPReg(b.value.reg)) {
										strX86AddrMode args CLEANUP(strX86AddrModeDestroy2)=NULL;
										args=strX86AddrModeAppendItem(NULL, b);
										assembleInst("FILD",args);
								} else {
										assert(0);
								}
						}
						continue;
				}
				case IR_TYPECAST: {
						strGraphEdgeIRP in CLEANUP(strGraphEdgeIRPDestroy)=graphNodeIRIncoming(execOrder[i]);
						strGraphEdgeIRP src CLEANUP(strGraphEdgeIRPDestroy)=IRGetConnsOfType(in, IR_CONN_SOURCE_A);
						__auto_type inNode=graphEdgeIRIncoming(src[0]);
						if(isIntNode(inNode)) {
								long size=objectSize(IRNodeType(inNode), NULL);
								switch(size) {
								case 1: {
										strRegP avail CLEANUP(strRegPDestroy)=regGetForType(&typeI64i);
										__auto_type tmpReg=avail[0];
										//PUSH reg16,
										//MOVSX reg16,rm8
										//FILD reg16
										//POP reg16

										//PUSH
										strX86AddrMode ppArgs CLEANUP(strX86AddrModeDestroy2)=NULL;
										ppArgs=strX86AddrModeAppendItem(ppArgs, X86AddrModeReg(tmpReg));
										assembleInst("PUSH", ppArgs);
										//MOVSX
										strX86AddrMode movsxArgs CLEANUP(strX86AddrModeDestroy2)=NULL;
										movsxArgs=strX86AddrModeAppendItem(movsxArgs, X86AddrModeReg(tmpReg));
										movsxArgs=strX86AddrModeAppendItem(movsxArgs, node2AddrMode(inNode));
										assembleInst("MOVSX", movsxArgs);
										//FILD reg16
										assembleInst("FILD", ppArgs);
										//POP reg16
										assembleInst("POP", ppArgs);
										break;
								}
								case 2:
								case 4:
								case 8: {
										strX86AddrMode fildArgs CLEANUP(strX86AddrModeDestroy2)=NULL;
										fildArgs=strX86AddrModeAppendItem(fildArgs, node2AddrMode(inNode));
										assembleInst("FILD", fildArgs);
										break;
								}
								}
						}
						continue;
				}
				default:
						assert(0);
				}
		}
}
void IR2Asm(graphNodeIR start,FILE *dumpTo) {
		strGraphEdgeIRP out CLEANUP(strGraphEdgeIRPDestroy)=graphNodeIROutgoing(start);
		switch(graphNodeIRValuePtr(start)->type) {
		case IR_ADD: {
				graphNodeIR a,b;
				binopArgs(start, &a, &b);
				if(isIntNode(a))
						assembleOpInt(start, "ADD");
				return;
		}
		case IR_ADDR_OF: {
				return ;
		}
		case IR_ARRAY:
		case IR_TYPECAST: {
				return;
		}
		case IR_STATEMENT_START:return;
		case IR_STATEMENT_END: return;
		case IR_INC: {
				strGraphEdgeIRP in CLEANUP(strGraphEdgeIRPDestroy)=graphNodeIRIncoming(start);
				__auto_type inNode=graphEdgeIRIncoming( in[0]);
				if(isIntNode(inNode)){
						strX86AddrMode args CLEANUP(strX86AddrModeDestroy2)=strX86AddrModeAppendItem(NULL, node2AddrMode(inNode));
						assembleInst("INC", args);
						return;
				} else if(isFltNode(inNode)) {
						switch(getCurrentArch()) {
						case ARCH_TEST_SYSV:
						case ARCH_X86_SYSV: {
								//ST(0) is a "accumulator register" available for trashing
								assign(X86AddrModeReg(&regX86ST0), node2AddrMode(inNode), 8);
								strX86AddrMode args CLEANUP(strX86AddrModeDestroy2)=strX86AddrModeAppendItem(NULL, X86AddrModeSint(1));
								assembleInst("FIADD",args);
								assign(node2AddrMode(inNode),X86AddrModeReg(&regX86ST0) , 8);
						}
						case ARCH_X64_SYSV:;
						}
				} else if(isPtrNode(inNode)) {
						struct objectPtr *ptr=(void*)IRNodeType(inNode);
						//ADD ptr,ptrSize
						strX86AddrMode addArgs CLEANUP(strX86AddrModeDestroy2)=strX86AddrModeAppendItem(NULL, node2AddrMode(start));
						addArgs=strX86AddrModeAppendItem(addArgs, X86AddrModeUint(objectSize(ptr->type, NULL)));
						assembleInst("ADD", addArgs);
				}
				if(strGraphEdgeIRPSize(out))
						assign(node2AddrMode(graphEdgeIROutgoing(out[0])), node2AddrMode(inNode),objectSize(IRNodeType(start),NULL));
				return;
		}
		case IR_DEC:  {
				strGraphEdgeIRP in CLEANUP(strGraphEdgeIRPDestroy)=graphNodeIRIncoming(start);
				__auto_type inNode=graphEdgeIRIncoming( in[0]);
				if(isIntNode(inNode)){
						strX86AddrMode args CLEANUP(strX86AddrModeDestroy2)=strX86AddrModeAppendItem(NULL, node2AddrMode(inNode));
						assembleInst("DEC", args);
						return;
				} else if(isFltNode(inNode)) {
						switch(getCurrentArch()) {
						case ARCH_TEST_SYSV:
						case ARCH_X86_SYSV: {
								//ST(0) is a "accumulator register" available for trashing
								assign(X86AddrModeReg(&regX86ST0), node2AddrMode(inNode), 8);
								strX86AddrMode args CLEANUP(strX86AddrModeDestroy2)=strX86AddrModeAppendItem(NULL, X86AddrModeSint(1));
								assembleInst("FISUB",args);
								assign(node2AddrMode(inNode),X86AddrModeReg(&regX86ST0) , 8);
						}
						case ARCH_X64_SYSV:;
						}
				} else if(isPtrNode(inNode)) {
						struct objectPtr *ptr=(void*)IRNodeType(inNode);
						//ADD ptr,ptrSize
						strX86AddrMode addArgs CLEANUP(strX86AddrModeDestroy2)=strX86AddrModeAppendItem(NULL, node2AddrMode(start));
						addArgs=strX86AddrModeAppendItem(addArgs, X86AddrModeUint(objectSize(ptr->type, NULL)));
						assembleInst("SUB", addArgs);
				}
				if(strGraphEdgeIRPSize(out))
						assign(node2AddrMode(graphEdgeIROutgoing(out[0])), node2AddrMode(inNode),objectSize(IRNodeType(start),NULL));
				return;
		}
		case IR_SUB: {
				graphNodeIR a,b;
				binopArgs(start, &a, &b);
				//Are assumed to be same type if valid IR graph
				if(isIntNode(a))
						assembleOpInt(start, "SUB");
				return;
		}
		case IR_POS: {
				strGraphEdgeIRP in CLEANUP(strGraphEdgeIRPDestroy)=graphNodeIRIncoming(start);
				__auto_type inNode=graphEdgeIRIncoming(in[0]);
				assign(node2AddrMode(graphEdgeIROutgoing(out[0])), node2AddrMode(inNode),objectSize(IRNodeType(start),NULL));
				return;
		}
		case IR_NEG: {
				strGraphEdgeIRP in CLEANUP(strGraphEdgeIRPDestroy)=graphNodeIRIncoming(start);
				__auto_type inNode=graphEdgeIRIncoming(in[0]);
				if(isIntNode(inNode)||isPtrNode(inNode)) {
						//MOV dest,source
						//NOT dest
						__auto_type outNode=graphEdgeIROutgoing(out[0]);
						strX86AddrMode movArgs CLEANUP(strX86AddrModeDestroy2)=strX86AddrModeAppendItem(NULL,node2AddrMode(outNode));
						strX86AddrMode nArgs CLEANUP(strX86AddrModeDestroy2)=strX86AddrModeAppendItem(NULL, node2AddrMode(inNode));
						assembleInst("MOV", movArgs);
						assembleInst("NEG", nArgs);
						return;
				} else if(isFltNode(inNode)) {
						switch(getCurrentArch()) {
						case ARCH_TEST_SYSV: 
						case ARCH_X86_SYSV: {
								
								return;
						}
						case ARCH_X64_SYSV:;
						}
				}
		}
		case IR_MULT:
		case IR_MOD:
		case IR_DIV:
		case IR_POW:
		case IR_LAND:
		case IR_LXOR:
		case IR_LOR:
		case IR_LNOT:
		case IR_BNOT:
		case IR_BAND:
		case IR_BXOR:
		case IR_BOR:
		case IR_LSHIFT:
		case IR_RSHIFT:
		case IR_ARRAY_ACCESS:
		case IR_ASSIGN:
		case IR_SIMD:
		case IR_GT:
		case IR_LT:
		case IR_GE:
		case IR_LE:
		case IR_EQ:
		case IR_NE:
		case IR_CHOOSE:
		case IR_COND_JUMP:
		case IR_JUMP_TAB:
		case IR_VALUE:
		case IR_LABEL:
		case IR_FUNC_ARG:
		case IR_FUNC_CALL:
		case IR_FUNC_RETURN:
		case IR_FUNC_START:
		case IR_FUNC_END:
		case IR_SUB_SWITCH_START_LABEL:
		case IR_DERREF:
		case IR_SPILL_LOAD:
		case IR_MEMBERS:
				;
		}
}
strChar uniqueLabel(const char *head) {
		long count=snprintf(NULL, 0, "$%s_%li", head,++labelsCount);
		char buffer[count+1];
		sprintf(buffer, 0, "$%s_%li", head,labelsCount);
		return strCharAppendData(NULL, buffer, count+1);
}
static strChar getLabelText(graphNodeIR node) {
	loop:;
		__auto_type find=ptrMapLabelNamesGet(asmLabelNames, node);
		if(find)
				return strClone(*find);
		ptrMapLabelNamesAdd(asmLabelNames, node,uniqueLabel(""));
		goto loop;
}
static void insertLabelsForAsm(strGraphNodeIRP nodes) {
		insertLabelsForAsmCalled=1;
		for(long i=0;i!=strGraphNodeIRPSize(nodes);i++) {
				__auto_type node =IRStmtStart(nodes[i]);
				strGraphEdgeIRP in CLEANUP(strGraphEdgeIRPDestroy)=graphNodeIRIncoming(node);
				strGraphEdgeIRP inFlow CLEANUP(strGraphEdgeIRPDestroy)=IRGetConnsOfType(in, IR_CONN_FLOW);
				if(strGraphEdgeIRPSize(inFlow)>1) {
				insertLabel:
						IRInsertBefore(IRCreateLabel(), node,node, IR_CONN_FLOW);
						continue;
				}
				for(long i=0;i!=strGraphEdgeIRPSize(in);i++) {
						switch(*graphEdgeIRValuePtr(in[0])) {
						case IR_CONN_CASE:
						case IR_CONN_COND_TRUE:
						case IR_CONN_COND_FALSE:
								goto insertLabel;
						default:;
						}
				}
		}
}
static void IR2AsmIf(graphNodeIR branch,FILE *dumpTo) {
		assert(insertLabelsForAsmCalled);
		strGraphEdgeIRP in CLEANUP(strGraphEdgeIRPDestroy)=graphNodeIRIncoming(branch);

		IR2Asm(graphEdgeIRIncoming(in[0]), dumpTo);
		strGraphEdgeIRP out CLEANUP(strGraphEdgeIRPDestroy)=graphNodeIROutgoing(branch);
		strGraphEdgeIRP t CLEANUP(strGraphEdgeIRPDestroy)=IRGetConnsOfType(out, IR_CONN_COND_TRUE);
		strGraphEdgeIRP f CLEANUP(strGraphEdgeIRPDestroy)=IRGetConnsOfType(out, IR_CONN_COND_FALSE);
		//insertLabelsForAsm will have inserted labels at true/false branch

		// CMP mode,0
		// JE *falseLabel*
		// *true code*
		// *falseLabel*:
		strX86AddrMode args CLEANUP(strX86AddrModeDestroy2)=strX86AddrModeAppendItem(NULL, node2AddrMode(graphEdgeIRIncoming(in[0])));
		args=strX86AddrModeAppendItem(args,X86AddrModeSint(0));
		assembleInst("CMP",args);
		strChar tLabText CLEANUP(strCharDestroy)=getLabelText(graphEdgeIROutgoing(t[0]));
		strChar fLabText CLEANUP(strCharDestroy)=getLabelText(graphEdgeIROutgoing(f[0]));
		strX86AddrMode jeArgs CLEANUP(strX86AddrModeDestroy2)=strX86AddrModeAppendItem(NULL, X86AddrModeLabel(fLabText));
		assembleInst("JE",jeArgs);
		IR2Asm(graphEdgeIROutgoing(t[0]), dumpTo);
		IR2Asm(graphEdgeIROutgoing(f[0]), dumpTo);
}
