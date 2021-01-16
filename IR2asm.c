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
PTR_MAP_FUNCS(struct parserFunction *,strChar,FuncNames);
PTR_MAP_FUNCS(struct parserVar *,long,FrameOffset);
static __thread long labelsCount;
static __thread ptrMapFuncNames funcNames;
static __thread ptrMapLabelNames asmLabelNames;
static __thread ptrMapFrameOffset varFrameOffsets;
static __thread ptrMapCompiledNodes compiledNodes;
static __thread int insertLabelsForAsmCalled=0;
strChar uniqueLabel(const char *head) {
		long count=snprintf(NULL, 0, "$%s_%li", head,++labelsCount);
		char buffer[count+1];
		sprintf(buffer, 0, "$%s_%li", head,labelsCount);
		return strCharAppendData(NULL, buffer, count+1);
}
static struct reg *regForTypeExlcuding(struct object *type,struct reg *exclude) {
		strRegP regs CLEANUP(strRegPDestroy)=regGetForType(type);
		for(long i=0;i!=strRegPSize(regs);i++) {
				if(regs[i]==exclude)
						continue;
				return regs[i];
		}
		return NULL;
}
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
						case IR_T:
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
						if(isIntNode(inNode)||isPtrNode(inNode)) {
								long size=objectSize(IRNodeType(inNode), NULL);
								switch(size) {
								case 1: {
										__auto_type tmpReg=regForTypeExlcuding(&typeI64i,NULL);
												//PUSH g16,
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
						} else if(isFltNode(inNode)) {
						}
						continue;
				}
				default:
						assert(0);
				}
		}
}
static int typeIsSigned(struct object *obj) {
		const struct object *signedTypes[]={
				&typeI8i,
				&typeI16i,
				&typeI32i,
				&typeI64i,
				&typeF64,
		};
		for(int i=0;i!=sizeof(signedTypes)/sizeof(*signedTypes);i++) {
				if(objectBaseType(obj)==signedTypes[i])
						return 1;
		}
		return 0;
}
static const struct object *getTypeForSize(long size) {
		switch(size) {
		case 1:
				return &typeI8i;
		case 2:
				return &typeI16i;
		case 4:
				return &typeI32i;
		case 8:
				return &typeI64i;
		}
		return &typeU0;
}
static void __typecastSignExt(graphNodeIR outNode,graphNodeIR inNode) {
		long iSize=objectSize(IRNodeType(inNode), NULL);
		long oSize=objectSize(IRNodeType(outNode), NULL);
		struct  X86AddressingMode inMode CLEANUP(X86AddrModeDestroy)=node2AddrMode(inNode);
		struct  X86AddressingMode outMode CLEANUP(X86AddrModeDestroy)=node2AddrMode(outNode);
		
		strX86AddrMode ppArgs CLEANUP(strX86AddrModeDestroy2)=NULL;
		struct reg *dumpToReg=NULL;
		//PUSH reg if assigning to non-reg dest(movsx needs reg as dst)
		if(outMode.type!=X86ADDRMODE_REG) {
				__auto_type tmpReg=regForTypeExlcuding(&typeI64i,NULL);
				ppArgs=strX86AddrModeAppendItem(ppArgs, X86AddrModeReg(tmpReg));
				dumpToReg=tmpReg;
				assembleInst("PUSH", ppArgs);
		} else {
				dumpToReg=outMode.value.reg;
		}
		strX86AddrMode movArgs CLEANUP(strX86AddrModeDestroy2)=NULL;
		movArgs=strX86AddrModeAppendItem(movArgs, X86AddrModeReg(dumpToReg));
		movArgs=strX86AddrModeAppendItem(movArgs, X86AddrModeClone(&inMode));
		if(oSize==8&&iSize==4) {
				assembleInst("MOVSX", movArgs);
		} else {
				assembleInst("MOVSXD", movArgs);
		}
		struct X86AddressingMode dumpToRegMode CLEANUP(X86AddrModeDestroy)=X86AddrModeReg(dumpToReg);
		assign(outMode,dumpToRegMode,objectSize(IRNodeType(outNode), NULL));
		//POP reg if assigning to non-reg dest(movsx needs reg as dst)
		if(outMode.type!=X86ADDRMODE_REG) {
				assembleInst("POP", ppArgs);
		}
}
static struct reg *subRegOfType(struct reg *r,struct object *type) {
		long iSize=objectSize(type, NULL);
		//Use the lower size part of register
		strRegSlice *affects=&r->affects;
		struct regSlice *subRegister=NULL;
		for(long i=0;i!=strRegSliceSize(*affects);i++) {
				if(affects[0][i].offset==0&&iSize*8==affects[0][i].widthInBits) {
						subRegister=&affects[0][i];
						break;
				}
		}
		assert(subRegister);
		return subRegister->reg;
}
static struct X86AddressingMode demoteAddrMode(struct X86AddressingMode addr,struct object *type) {
		__auto_type mode=X86AddrModeClone(&addr);
		switch(mode.type) {
		case X86ADDRMODE_REG:
				mode.value.reg=subRegOfType(mode.value.reg,type);
				break;
		case X86ADDRMODE_FLT:
		case X86ADDRMODE_ITEM_ADDR:
		case X86ADDRMODE_LABEL:
		case X86ADDRMODE_MEM:
		case X86ADDRMODE_SINT:
		case X86ADDRMODE_UINT:
				mode.valueType=type;
				break;
		}
		return mode;
}
void IR2Asm(graphNodeIR start,FILE *dumpTo) {
		strGraphEdgeIRP out CLEANUP(strGraphEdgeIRPDestroy)=graphNodeIROutgoing(start);
		if(isFltNode(start)) {
		switch(getCurrentArch()) {
						case ARCH_TEST_SYSV: 
						case ARCH_X86_SYSV: {
								compileX87Expr(start);
								return;
						}
						case ARCH_X64_SYSV:;
						}
		}
		switch(graphNodeIRValuePtr(start)->type) {
		case IR_ADD: {
				graphNodeIR a,b;
				binopArgs(start, &a, &b);
				if(isIntNode(a)||isPtrNode(a))
						assembleOpInt(start, "ADD");
				return;
		}
		case IR_ADDR_OF: {
				return ;
		}
		case IR_ARRAY:
		case IR_TYPECAST: {
				strGraphEdgeIRP in CLEANUP(strGraphEdgeIRPDestroy)=graphNodeIRIncoming(start);
				__auto_type inNode=graphEdgeIRIncoming( in[0]);
				strGraphEdgeIRP out CLEANUP(strGraphEdgeIRPDestroy)=graphNodeIROutgoing(start);
				if(!strGraphEdgeIRPSize(out))
						return;
				__auto_type outNode=graphEdgeIRIncoming( out[0]);
				
				struct  X86AddressingMode inMode CLEANUP(X86AddrModeDestroy)=node2AddrMode(inNode);
				struct  X86AddressingMode outMode CLEANUP(X86AddrModeDestroy)=node2AddrMode(outNode);
				switch(inMode.type) {
				case X86ADDRMODE_FLT: {
						if(isIntNode(outNode)||isPtrNode(outNode)) {
								int64_t value=inMode.value.flt;
								assign(outMode,X86AddrModeSint(value),objectSize(IRNodeType(outNode), NULL));
						} else if(isFltNode(inNode)) {
								assign(outMode,inMode,objectSize(IRNodeType(outNode), NULL));
						} else assert(0);
						return;
				}
				case X86ADDRMODE_MEM:
				case X86ADDRMODE_LABEL:
				case X86ADDRMODE_ITEM_ADDR:
				case X86ADDRMODE_REG: { 
						//If destination is bigger than source,sign extend if dest is signed
						if(isPtrNode(outNode)||isIntNode(outNode)) {
								long iSize=objectSize(IRNodeType(inNode), NULL);
								long oSize=objectSize(IRNodeType(outNode), NULL);
								if(oSize>iSize) {
										if(typeIsSigned(IRNodeType(inNode))) {
												__typecastSignExt(outNode, inNode);
										} else {
												strX86AddrMode ppArgs CLEANUP(strX86AddrModeDestroy2)=NULL;
												struct reg *dumpToReg=NULL;
												//PUSH reg if assigning to non-reg dest(movzx needs reg as dst)
												if(outMode.type!=X86ADDRMODE_REG) {
														strRegP regs CLEANUP(strRegPDestroy)=regGetForType((struct object*)getTypeForSize(oSize));
														__auto_type tmpReg=regs[0];
														ppArgs=strX86AddrModeAppendItem(ppArgs, X86AddrModeReg(tmpReg));
														dumpToReg=tmpReg;
														assembleInst("PUSH", ppArgs);
												} else {
														dumpToReg=outMode.value.reg;
												}
												strX86AddrMode movzxArgs CLEANUP(strX86AddrModeDestroy2)=NULL;
												struct X86AddressingMode dumpToRegMode CLEANUP(X86AddrModeDestroy)=X86AddrModeReg(dumpToReg);
												movzxArgs=strX86AddrModeAppendItem(movzxArgs, X86AddrModeClone(&dumpToRegMode));
												movzxArgs=strX86AddrModeAppendItem(movzxArgs, X86AddrModeClone(&inMode));
												assembleInst("MOVZX", movzxArgs);
												assign(outMode,dumpToRegMode,objectSize(IRNodeType(outNode), NULL));
												//POP reg if assigning to non-reg dest(movzx needs reg as dst)
												if(outMode.type!=X86ADDRMODE_REG) {
														assembleInst("POP", ppArgs);
												}
										}
										return;
								} else if(iSize>oSize) {
										struct X86AddressingMode mode CLEANUP(X86AddrModeDestroy)=demoteAddrMode(outMode,IRNodeType(outNode));
										assign(outMode, mode, objectSize(IRNodeType(outNode), NULL));
										return;
								}
						} else if(isFltNode(outNode)) {
								//Assign handles flt<-int
								assign(outMode,inMode,objectSize(IRNodeType(outNode), NULL));
						} else assert(0);
						return;
				}
				case X86ADDRMODE_UINT:
				case X86ADDRMODE_SINT:
						assign(outMode, inMode, objectSize(IRNodeType(outNode), NULL));
						return ;
				}
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
						case ARCH_X86_SYSV:
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
						case ARCH_X86_SYSV:
								;
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
				if(isIntNode(a)||isPtrNode(a))
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
		case IR_MULT: {
				graphNodeIR a,b;
				binopArgs(start, &a, &b);
				//Are assumed to be same type if valid IR graph
				if(isIntNode(a)||isPtrNode(a)) {
						if(typeIsSigned(IRNodeType(a)))
								assembleOpInt(start, "IMUL");
						else
								assembleOpInt(start, "MUL");
				}
				return;
		}
		case IR_MOD:
		case IR_DIV: {
				graphNodeIR a,b;
				binopArgs(start, &a, &b);
				struct X86AddressingMode outMode CLEANUP(X86AddrModeDestroy)=node2AddrMode(graphEdgeIROutgoing(out[0]));
				struct reg *outReg=NULL;
				if(outMode.type==X86ADDRMODE_REG)
						outReg=outMode.value.reg;

				//
				// We will either use EAX or RAX if x86 or x86 respeectivly
				//
				// Also,we will not push/pop rdx or rax if it conflicts with the result register
				//
#define IF_NOT_CONFLICT(reg,code) ({if(out) {if(!regConflict(&reg,outReg)) {code;}} else {code;}})
				
				strX86AddrMode ppRAX CLEANUP(strX86AddrModeDestroy2)=NULL;
				strX86AddrMode ppRDX CLEANUP(strX86AddrModeDestroy2)=NULL;
				switch(objectSize(IRNodeType(start), NULL)) {
				case 1:
						IF_NOT_CONFLICT(regX86AH,ppRAX=strX86AddrModeAppendItem(NULL, X86AddrModeReg(&regX86AH)));
						IF_NOT_CONFLICT(regX86AL,ppRDX=strX86AddrModeAppendItem(NULL, X86AddrModeReg(&regX86AL)));
						break;
				case 2:
						IF_NOT_CONFLICT(regX86AX,ppRAX=strX86AddrModeAppendItem(NULL, X86AddrModeReg(&regX86AX)));
						IF_NOT_CONFLICT(regX86DX,ppRDX=strX86AddrModeAppendItem(NULL, X86AddrModeReg(&regX86DX)));
						break;
				case 4:
						IF_NOT_CONFLICT(regX86EAX,ppRAX=strX86AddrModeAppendItem(NULL, X86AddrModeReg(&regX86EAX)));
						IF_NOT_CONFLICT(regX86EDX,ppRDX=strX86AddrModeAppendItem(NULL, X86AddrModeReg(&regX86EDX)));
						break;
				case 8:
						IF_NOT_CONFLICT(regAMD64RAX,ppRAX=strX86AddrModeAppendItem(NULL, X86AddrModeReg(&regAMD64RAX)));
						IF_NOT_CONFLICT(regAMD64RDX,ppRDX=strX86AddrModeAppendItem(NULL, X86AddrModeReg(&regAMD64RDX)));
						break;
				}
				if(ppRAX) assembleInst("PUSH", ppRAX);
				if(ppRDX) assembleInst("PUSH", ppRDX);
				strX86AddrMode movRaxArgs CLEANUP(strX86AddrModeDestroy2)=NULL;
				strX86AddrMode dxorArgs CLEANUP(strX86AddrModeDestroy2)=NULL;
				strX86AddrMode divArgs CLEANUP(strX86AddrModeDestroy2)=NULL;
				const char *op=typeIsSigned(IRNodeType(start))?"IDIV":"DIV";
				int isDivOrMod=graphNodeIRValuePtr(start)->type==IR_DIV;
				switch(objectSize(IRNodeType(start), NULL)) {
				case 1:
						movRaxArgs=strX86AddrModeAppendItem(movRaxArgs,  X86AddrModeReg(&regX86AX));
						movRaxArgs=strX86AddrModeAppendItem(movRaxArgs,  node2AddrMode(a));
						divArgs=strX86AddrModeAppendItem(divArgs, node2AddrMode(b));
						assembleInst("MOV", movRaxArgs);
						assembleInst(op, divArgs);
						if(isDivOrMod) {
								struct X86AddressingMode al CLEANUP(X86AddrModeDestroy)=X86AddrModeReg(&regX86AL);
								assign(outMode,al,1);		
						} else {
								struct X86AddressingMode ah CLEANUP(X86AddrModeDestroy)=X86AddrModeReg(&regX86AH);
								assign(outMode,ah,1);
						}
						break;
				case 2:
						movRaxArgs=strX86AddrModeAppendItem(movRaxArgs,  X86AddrModeReg(&regX86AX));
						movRaxArgs=strX86AddrModeAppendItem(movRaxArgs,  node2AddrMode(a));
						divArgs=strX86AddrModeAppendItem(divArgs, node2AddrMode(b));
						dxorArgs=strX86AddrModeAppendItem(dxorArgs, X86AddrModeReg(&regX86DX));
						dxorArgs=strX86AddrModeAppendItem(dxorArgs, X86AddrModeReg(&regX86DX));
						assembleInst("MOV", movRaxArgs);
						assembleInst("XOR",dxorArgs);
						assembleInst(op, divArgs);
						if(isDivOrMod) {
								struct X86AddressingMode ax CLEANUP(X86AddrModeDestroy)=X86AddrModeReg(&regX86AX);
								assign(outMode,ax,1);		
						} else {
								struct X86AddressingMode dx CLEANUP(X86AddrModeDestroy)=X86AddrModeReg(&regX86DX);
								assign(outMode,dx,1);
						}
						break;
				case 4:
						movRaxArgs=strX86AddrModeAppendItem(movRaxArgs,  X86AddrModeReg(&regX86EAX));
						movRaxArgs=strX86AddrModeAppendItem(movRaxArgs,  node2AddrMode(a));
						divArgs=strX86AddrModeAppendItem(divArgs, node2AddrMode(b));
						dxorArgs=strX86AddrModeAppendItem(dxorArgs, X86AddrModeReg(&regX86EDX));
						dxorArgs=strX86AddrModeAppendItem(dxorArgs, X86AddrModeReg(&regX86EDX));
						assembleInst("MOV", movRaxArgs);
						assembleInst("XOR",dxorArgs);
						assembleInst(op, divArgs);
						if(isDivOrMod) {
								struct X86AddressingMode eax CLEANUP(X86AddrModeDestroy)=X86AddrModeReg(&regX86EAX);
								assign(outMode,eax,1);		
						} else {
								struct X86AddressingMode edx CLEANUP(X86AddrModeDestroy)=X86AddrModeReg(&regX86EDX);
								assign(outMode,edx,1);
						}
						break;
				case 8:
						movRaxArgs=strX86AddrModeAppendItem(movRaxArgs,  X86AddrModeReg(&regAMD64RAX));
						movRaxArgs=strX86AddrModeAppendItem(movRaxArgs,  node2AddrMode(a));
						divArgs=strX86AddrModeAppendItem(divArgs, node2AddrMode(b));
						dxorArgs=strX86AddrModeAppendItem(dxorArgs, X86AddrModeReg(&regAMD64RDX));
						dxorArgs=strX86AddrModeAppendItem(dxorArgs, X86AddrModeReg(&regAMD64RDX));
						assembleInst("MOV", movRaxArgs);
						assembleInst("XOR",dxorArgs);
						assembleInst(op, divArgs);
						if(isDivOrMod) {
								struct X86AddressingMode rdx CLEANUP(X86AddrModeDestroy)=X86AddrModeReg(&regAMD64RDX);
								assign(outMode,rdx,1);		
						} else {
								struct X86AddressingMode rax CLEANUP(X86AddrModeDestroy)=X86AddrModeReg(&regAMD64RAX);
								assign(outMode,rax,1);
						}
						break;
				}
				if(ppRAX) assembleInst("POP", ppRAX);
				if(ppRDX) assembleInst("POP", ppRDX);
		}
		case IR_POW: {
				//TODO
				assert(0);
		}
		case IR_LOR: {
				struct X86AddressingMode outMode CLEANUP(X86AddrModeDestroy)=node2AddrMode(graphEdgeIROutgoing(out[0]));
				graphNodeIR a,b;
				binopArgs(start, &a, &b);
				struct X86AddressingMode aMode CLEANUP(X86AddrModeDestroy)=node2AddrMode(a);
				struct X86AddressingMode bMode CLEANUP(X86AddrModeDestroy)=node2AddrMode(b);
				// CMP a,0
				// JNE next1
				// MOV out,1
				// next1:
				// CMP b,0
				// JNE next2
				// MOV out,1
				// JMP end
				// next2:
				// MOV out,0
				// end:
				strChar endLab CLEANUP(strCharDestroy)=uniqueLabel("LOR");
				struct X86AddressingMode one CLEANUP(X86AddrModeDestroy)=X86AddrModeSint(1);
				for(int i=0;i!=2;i++) {
						strX86AddrMode cmpArgs CLEANUP(strX86AddrModeDestroy2)=strX86AddrModeAppendItem(NULL , X86AddrModeClone(i?&aMode:&bMode));
						cmpArgs=strX86AddrModeAppendItem(cmpArgs ,X86AddrModeSint(0));
						assembleInst("CMP", cmpArgs);

						strChar nextLab CLEANUP(strCharDestroy)=uniqueLabel("LOR");
						strX86AddrMode jneArgs CLEANUP(strX86AddrModeDestroy2)=strX86AddrModeAppendItem(NULL, X86AddrModeLabel(nextLab));
						assembleInst("JNE",jneArgs);
						assign(outMode, one, objectSize(IRNodeType(start), NULL));

						strX86AddrMode jmpArgs CLEANUP(strX86AddrModeDestroy2)=strX86AddrModeAppendItem(NULL, X86AddrModeLabel(endLab));
						assembleInst("JMP",jmpArgs);
						
						//Returns copy of label name
						free(X86EmitAsmLabel(nextLab));
				}
				//MOV out,0
				struct X86AddressingMode zero CLEANUP(X86AddrModeDestroy)=X86AddrModeSint(0);
				assign(outMode, zero, objectSize(IRNodeType(start), NULL));
				
				//Returns copy of label name
				free(X86EmitAsmLabel(endLab));
				return;
		}
		case IR_LXOR: {
				struct X86AddressingMode outMode CLEANUP(X86AddrModeDestroy)=node2AddrMode(graphEdgeIROutgoing(out[0]));
				graphNodeIR a,b;
				binopArgs(start, &a, &b);
				struct X86AddressingMode aMode CLEANUP(X86AddrModeDestroy)=node2AddrMode(a);
				struct X86AddressingMode bMode CLEANUP(X86AddrModeDestroy)=node2AddrMode(b);
				// MOV outMode,0
				// CMP aMode,0
				// SETNE outMode
				// CMP bMode,0
				// JE END
				// XOR outNode,1
				// end:
				struct X86AddressingMode zero CLEANUP(X86AddrModeDestroy)=X86AddrModeSint(0);
				struct X86AddressingMode one CLEANUP(X86AddrModeDestroy)=X86AddrModeSint(1);
				assign(outMode,zero, objectSize( IRNodeType(start),NULL));

				strX86AddrMode cmpAArgs CLEANUP(strX86AddrModeDestroy2)=strX86AddrModeAppendItem(NULL, X86AddrModeClone(&aMode));
				cmpAArgs=strX86AddrModeAppendItem(cmpAArgs, X86AddrModeSint(0));
				assembleInst("CMP", cmpAArgs);
				
				//outMode must be demoted to 8bit as SETNE takes byte only
				outMode=demoteAddrMode(outMode,&typeI8i);
				strX86AddrMode setneArgs CLEANUP(strX86AddrModeDestroy2)=strX86AddrModeAppendItem(NULL, X86AddrModeClone(&outMode));
				assembleInst("SETNE",setneArgs);

				strX86AddrMode cmpBArgs CLEANUP(strX86AddrModeDestroy2)=strX86AddrModeAppendItem(NULL, X86AddrModeClone(&bMode));
				cmpBArgs=strX86AddrModeAppendItem(cmpBArgs, X86AddrModeSint(0));
				assembleInst("CMP", cmpBArgs);

				strChar endLabel CLEANUP(strCharDestroy)=uniqueLabel("XOR");
				strX86AddrMode jmpeArgs CLEANUP(strX86AddrModeDestroy2)=strX86AddrModeAppendItem(NULL, X86AddrModeLabel(endLabel));
				assembleInst("JE", jmpeArgs);

				strX86AddrMode xorArgs CLEANUP(strX86AddrModeDestroy2)=strX86AddrModeAppendItem(NULL, X86AddrModeClone(&outMode));
				xorArgs=strX86AddrModeAppendItem(xorArgs, X86AddrModeClone(&one));

				X86EmitAsmLabel(endLabel);
				return ;
		}
		case IR_LAND: {
				__auto_type outNode=graphEdgeIROutgoing(out[0]);
				struct X86AddressingMode outMode CLEANUP(X86AddrModeDestroy)=node2AddrMode(outNode);
				graphNodeIR a,b;
				binopArgs(start, &a, &b);
				struct X86AddressingMode aMode CLEANUP(X86AddrModeDestroy)=node2AddrMode(a);
				struct X86AddressingMode bMode CLEANUP(X86AddrModeDestroy)=node2AddrMode(b);
				// MOV outMode,0
				// CMP aMode,0
				// JE emd
				// CMP bMode,0
				// JE end
				// MOV outMode,1
				// end:
				struct X86AddressingMode zero CLEANUP(X86AddrModeDestroy)=X86AddrModeSint(0);
				struct X86AddressingMode one CLEANUP(X86AddrModeDestroy)=X86AddrModeSint(1);
				assign(outMode, zero, objectSize(IRNodeType(outNode), NULL));

				strX86AddrMode cmpAArgs CLEANUP(strX86AddrModeDestroy2)=strX86AddrModeAppendItem(NULL, X86AddrModeClone(&aMode));
				cmpAArgs=strX86AddrModeAppendItem(cmpAArgs, X86AddrModeSint(0));
				assembleInst("CMP", cmpAArgs);

				strChar endLabel CLEANUP(strCharDestroy)=uniqueLabel("XOR");
				strX86AddrMode jmpeArgs CLEANUP(strX86AddrModeDestroy2)=strX86AddrModeAppendItem(NULL, X86AddrModeLabel(endLabel));
				assembleInst("JE", jmpeArgs);

				strX86AddrMode cmpBArgs CLEANUP(strX86AddrModeDestroy2)=strX86AddrModeAppendItem(NULL, X86AddrModeClone(&aMode));
				cmpBArgs=strX86AddrModeAppendItem(cmpBArgs, X86AddrModeSint(0));
				assembleInst("CMP", cmpBArgs);

				assembleInst("JE", jmpeArgs);

				assign(outMode, one, objectSize(IRNodeType(outNode), NULL));
				
				X86EmitAsmLabel(endLabel);
				return;
		}
		case IR_LNOT: {
				strGraphEdgeIRP in CLEANUP(strGraphEdgeIRPDestroy)=graphNodeIRIncoming(start);
				__auto_type inNode=graphEdgeIRIncoming(in[0]);
				__auto_type outNode=graphEdgeIROutgoing(out[0]);
				// MOV outMode,0 
				// CMP inMode,0
				// SETE outMode

				struct X86AddressingMode zero CLEANUP(X86AddrModeDestroy)=X86AddrModeSint(0);
				struct X86AddressingMode oMode CLEANUP(X86AddrModeDestroy)=node2AddrMode(outNode);
				struct X86AddressingMode iMode CLEANUP(X86AddrModeDestroy)=node2AddrMode(inNode);

				assign(oMode,zero,objectSize(IRNodeType(outNode),NULL));

				strX86AddrMode cmpArgs CLEANUP(strX86AddrModeDestroy2)=strX86AddrModeAppendItem(NULL, X86AddrModeClone(&iMode));
				assembleInst("CMP", cmpArgs);

				//SETE takes a byte only so demote oMode to write to first byte
				struct X86AddressingMode oMode2 CLEANUP(X86AddrModeDestroy)=demoteAddrMode(oMode,  IRNodeType(outNode));
				strX86AddrMode seteArgs CLEANUP(strX86AddrModeDestroy2)=strX86AddrModeAppendItem(NULL, X86AddrModeClone(&oMode2));
				assembleInst("SETE" ,seteArgs);
				
				return;
		}
		case IR_BNOT: {
				strGraphEdgeIRP in CLEANUP(strGraphEdgeIRPDestroy)=graphNodeIRIncoming(start);
				__auto_type inNode=graphEdgeIRIncoming(in[0]);
				__auto_type outNode=graphEdgeIROutgoing(out[0]);
				struct X86AddressingMode oMode CLEANUP(X86AddrModeDestroy)=node2AddrMode(outNode);
				struct X86AddressingMode iMode CLEANUP(X86AddrModeDestroy)=node2AddrMode(inNode);

				// MOv out,in
				// NOT out
				assign(oMode, iMode ,objectSize(IRNodeType(outNode),NULL));

				strX86AddrMode notArgs CLEANUP(strX86AddrModeDestroy2)=strX86AddrModeAppendItem(NULL, X86AddrModeClone(&oMode));
				assembleInst("NOT", notArgs);
				return;
		} 
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
