#include "registers.h"
#include "object.h"
#include "parserA.h"
#include "IR.h"
#include "cleanup.h"
#include <assert.h>
#include "IR2asm.h"
#include "abi.h"
typedef int (*regCmpType)(const struct reg **, const struct reg **);
static int ptrPtrCmp(const void *a, const void *b) {
		if(*(void**)a>*(void**)b)
				return 1;
		else if(*(void**)a<*(void**)b)
				return -1;
		return 0;
}
static __thread struct parserVar *retVar=NULL;
static int isIntType(struct object *obj) {
	const struct object *intTypes[] = {
	    &typeI8i, &typeI16i, &typeI32i, &typeI64i, &typeU8i, &typeU16i, &typeU32i, &typeU64i,
	};
	for (long i = 0; i != sizeof(intTypes) / sizeof(*intTypes); i++)
		if (objectBaseType(obj) == intTypes[i])
			return 1;
	if(obj->type==TYPE_PTR||obj->type==TYPE_ARRAY) return 1;
	return 0;
}
static struct object *promote(struct object *currType) {
		if(isIntType(currType)) {
				if(&typeU64i==currType) return &typeU64i;
				else return &typeI64i;
		} else {
				return currType;
		}
}
static void addOffsetIfIndir(struct X86AddressingMode *mode,long offset) {
		if(mode->type==X86ADDRMODE_MEM) {
				mode->value.m.value.sib.offset+=offset;
		} else if(mode->type==X86ADDRMODE_ITEM_ADDR) {
				mode->value.itemAddr.offset+=offset;
		} else if(mode->type==X86ADDRMODE_VAR_VALUE) {
				mode->value.varAddr.offset+=offset;
		}
}
static strGraphNodeIRP getFuncArgs(graphNodeIR call) {
	strGraphEdgeIRP in CLEANUP(strGraphEdgeIRPDestroy) = IREdgesByPrec(call);
	strGraphNodeIRP args = NULL;
	for (long i = 0; i != strGraphEdgeIRPSize(in); i++) {
		switch (*graphEdgeIRValuePtr(in[i])) {
		default:
			break;
		case IR_CONN_FUNC_ARG_1 ... IR_CONN_FUNC_ARG_128:
			args = strGraphNodeIRPAppendItem(args, graphEdgeIRIncoming(in[i]));
		}
	}
	return args;
}
STR_TYPE_DEF(struct object *,Object);
STR_TYPE_FUNCS(struct object *,Object);
enum ABI_Type {
		ABI_NO_CLASS,
		ABI_INTEGER,
		ABI_SSE,
		ABI_MEMORY,
};
STR_TYPE_DEF(long,Long);
STR_TYPE_FUNCS(long,Long);
STR_TYPE_DEF(enum ABI_Type,AbiType);
STR_TYPE_FUNCS(enum ABI_Type,AbiType);
static strObject getSubTypes(struct object *obj) {
		strObject retVal=NULL;
		if(obj->type==TYPE_CLASS) {
				struct objectClass *cls=(void*)obj;
				for(long m=0;strObjectMemberSize(cls->members);m++)
						retVal=strObjectAppendItem(retVal, cls->members[m].type);
		} else if(obj->type==TYPE_UNION) {
				struct objectUnion *un=(void*)obj;
				for(long m=0;strObjectMemberSize(un->members);m++)
						retVal=strObjectAppendItem(retVal, un->members[m].type);
		} else if(obj->type==TYPE_ARRAY) {
				struct objectArray *arr=(void*)obj;
				long count;
				objectArrayDimValues(obj, &count, NULL);
				long dims[count];
				objectArrayDimValues(obj,NULL,dims);
				assert(count!=OBJECT_ARRAY_DIM_UNDEF);

				struct object *baseType=arr->type;
				strObject repeat =NULL;
				//dims[0] is the dimension of the outermost array
				for(long e=0;e!=dims[0];e++) {
						if(baseType->type==TYPE_ARRAY)
								repeat=strObjectConcat(repeat, getSubTypes(baseType));
						else
								repeat=strObjectAppendItem(repeat, baseType);
				}
				retVal=repeat;
		} else {
				retVal=strObjectAppendItem(NULL, obj);
		}
		return retVal;
}
static strObject flattenFields(strObject types) {
		strObject retVal=NULL;
		for(long t=0;t!=strObjectSize(types);t++) {
				retVal=strObjectConcat(retVal,flattenFields(getSubTypes(types[t])));
		}
		return retVal;
}
static long get8byteFields(long offset,strObject types,strLong *writeTo,strLong *offsets) {
		for(long t=0;t!=strObjectSize(types);t++) {
				strObject flat CLEANUP(strObjectDestroy)=getSubTypes(types[t]);
				if(types[t]->type==TYPE_UNION) {
						get8byteFields(offset, flat, writeTo,offsets);
						offset+=objectSize(types[t], NULL);
						continue;
				} if(types[t]->type==TYPE_CLASS) {
						offset=get8byteFields(offset,flat,writeTo,offsets);
						continue;
				}
				for(long f=0;f!=strObjectSize(flat);f++) {
						if(writeTo) *writeTo=strLongAppendItem(*writeTo, offset/8);
						if(offsets) *offsets=strLongAppendItem(*offsets, offset);
						offset+=objectSize(flat[t], NULL);
				}
		}
		return offset;
}
static enum ABI_Type getAbiType(struct object *obj) {
		obj=objectBaseType(obj);
		switch(obj->type) {
		case TYPE_FUNCTION:
		case TYPE_ARRAY:
		case TYPE_PTR:
				return ABI_INTEGER;
		case TYPE_Bool:
		case TYPE_U8i:
		case TYPE_U16i:
		case TYPE_U32i:
		case TYPE_U64i:
		case TYPE_I8i:
		case TYPE_I16i:
		case TYPE_I32i:
		case TYPE_I64i:
				return ABI_INTEGER;
		case TYPE_F64:
				return ABI_SSE;
		case TYPE_U0:
		case TYPE_FORWARD:
				abort();
		case TYPE_CLASS:
		case TYPE_UNION:
				return ABI_NO_CLASS;
		}
}
static long getConsecCount(strLong items,long offset) {
		long first=items[offset];
		long count=0;
		for(long c=offset;c!=strLongSize(items);c++) {
				if(items[c]!=first) break;
				c++;
		}
		return count;
}
//aggregate
static strAbiType consider2Agg(strObject _types) {
		strObject types CLEANUP(strObjectDestroy)=flattenFields(_types);
		strLong groupings CLEANUP(strLongDestroy)=NULL;
		get8byteFields(0, types, &groupings,NULL);
		assert(strLongSize(groupings)==strObjectSize(types));

		long sizeSum=0;
		for(long i=0;i<strObjectSize(types);i++) {
				sizeSum+=objectSize(types[i], NULL);
		}
		/**
https://ntuck-neu.site/2020-09/cs3650/asm/x86-64-sysv-abi.pdf
					if the size of the aggregate exceeds two eightbytes and the first eightbyte isn’tSSE or any other eightbyte isn’t SSEUP, the whole argument is passed in mem-ory.
			*/
		//SSEUP is never used in this compiler so it is always 2 8bytes or below 
		if(sizeSum>2*8) {
				return strAbiTypeAppendItem(NULL, ABI_MEMORY);
		}

		strAbiType _8byteTypes CLEANUP(strAbiTypeDestroy)=NULL;

		long offset=0;
		for(;offset!=strLongSize(groupings);) {
				long consec=getConsecCount(groupings, offset);
				enum ABI_Type fields[consec];
				for(long i=0;i<strObjectSize(types);i++) {
						fields[i]=getAbiType(types[i]);
				}

		loop:
				for(long i=0;i<consec;i++) {
						__auto_type a=fields[i];
						__auto_type b=ABI_NO_CLASS;
						if(i+1<consec)	b=fields[i+1];
						//(a)
						if(a==b) continue;
						//(b)
						if(a==ABI_NO_CLASS) {
								fields[i]=b;
								goto next;
						} else if(b==ABI_NO_CLASS) {
								fields[i]=a;
								goto next;
						}
						//(c)
						if(a==ABI_MEMORY) {
								fields[i]=ABI_MEMORY;
								goto next;
						} else if(b==ABI_MEMORY) {
								fields[i]=ABI_MEMORY;
								goto next;
						}
						//(d)
						if(a==ABI_INTEGER) {
								fields[i]=ABI_INTEGER;
								goto next;
						} else if(b==ABI_INTEGER) {
								fields[i]=ABI_INTEGER;
								goto next;
						}
						//(e) Tell it to the toads.(Slang for X87 not used in this compiler for x64 SysV)
						//(f)
						fields[i]=ABI_SSE;
				next:
						_8byteTypes=strAbiTypeAppendItem(_8byteTypes,fields[i]);
				}
				offset+=consec;
		}
		//Postmerger
		//(a) If 1 item is in memory,the whole thing is passed in memory
		for(long f=0;f!=strAbiTypeSize(_8byteTypes);f++) {
				if(_8byteTypes[f]==ABI_MEMORY) {
						strAbiTypeDestroy(&_8byteTypes);
						_8byteTypes=strAbiTypeAppendItem(NULL, ABI_MEMORY);
						break;
				}
		}
		return _8byteTypes;
}
static void stuffModesIntoReg64(graphNodeIR atNode,strGraphNodeIRP nodes,strObject expected,struct reg *r) {
		long run=0;
		long size=0;
		for(long a=0;a!=strGraphNodeIRPSize(nodes);a++) {
				size+=objectSize(IRNodeType(nodes[a]),NULL);
		}
		struct X86AddressingMode *zero CLEANUP(X86AddrModeDestroy)=X86AddrModeSint(0);
		pushMode(zero);
		//Store stack pointer in RAX as assigns may modify stack
		{
				struct X86AddressingMode *raxMode CLEANUP(X86AddrModeDestroy)=X86AddrModeReg(&regAMD64RAX, &typeU64i);
				struct X86AddressingMode *spMode CLEANUP(X86AddrModeDestroy)=X86AddrModeReg(stackPointer(), &typeU64i);
				asmTypecastAssign(atNode, raxMode, spMode, ASM_ASSIGN_X87FPU_POP);
		}
		
		struct X86AddressingMode *top CLEANUP(X86AddrModeDestroy)=X86AddrModeIndirReg(&regAMD64RAX, &typeU0);
		X86AddrModeIndirSIBAddOffset(top,-8);
		for(long a=0;a!=strGraphNodeIRPSize(nodes);a++) {
				top->valueType=expected[a];
				struct X86AddressingMode *inMode CLEANUP(X86AddrModeDestroy)=IRNode2AddrMode(nodes[a]);
				asmTypecastAssign(atNode, top, inMode, ASM_ASSIGN_X87FPU_POP);
				X86AddrModeIndirSIBAddOffset(top, objectSize(expected[a], NULL));
		}

		popReg(r);
}
static struct object *largestDataSizeInWidth(long width) {
		if(width>=8) return &typeU64i;
		if(width>=4) return &typeU32i;
		if(width>=2) return &typeU16i;
		return &typeU8i;
}
static void strX86AddrModeDestroy2(strX86AddrMode *str) {
		for (long i = 0; i != strX86AddrModeSize(*str); i++)
				X86AddrModeDestroy(&str[0][i]);
		strX86AddrModeDestroy(str);
}
void IR_ABI_SYS_X64_Return(graphNodeIR _ret) {
		strGraphEdgeIRP in CLEANUP(strGraphEdgeIRPDestroy)=IREdgesByPrec(_ret);
		if(!strGraphEdgeIRPSize(in)) {
		leave:
				assembleOpcode(_ret, "LEAVE", NULL);
				assembleOpcode(_ret, "RET", NULL);
				return;
		}
		struct reg *intRegs[]={
				&regAMD64RAX,
				&regAMD64RDX,
		};
		struct reg *sseRegs[]={
				&regX86XMM0,
				&regX86XMM1,
		};
		struct X86AddressingMode *retMode CLEANUP(X86AddrModeDestroy)=IRNode2AddrMode(graphEdgeIRIncoming(in[0]));
		__auto_type retType=IRNodeType(graphEdgeIRIncoming(in[0]));
		strObject _retType CLEANUP(strObjectDestroy)=strObjectAppendItem(NULL , retType);
		strAbiType fields CLEANUP(strAbiTypeDestroy)=consider2Agg(_retType);
		strLong _8byteFields CLEANUP(strLongDestroy)=NULL; 
		get8byteFields(0, _retType, &_8byteFields, NULL);
		
		int consumedInts=0,consumedSses=0;
		for(long c=0;c!=strLongSize(_8byteFields);) {
				enum ABI_Type type=fields[c];
				if(type==ABI_INTEGER) consumedInts++;
				else if(type==ABI_SSE) consumedSses++;
				else if(type==ABI_NO_CLASS) abort();
				else if(type==ABI_MEMORY) goto retMemory;
				
				//Seek next field
				long first=_8byteFields[c];
				for(;c!=strLongSize(_8byteFields);c++)
						if(_8byteFields[c]!=first) break;
		}
		if(consumedInts>2||consumedSses>2) goto retMemory;
		{
				for(long c=0;c!=strLongSize(_8byteFields);)  {
						enum ABI_Type type=fields[c];
						if(type==ABI_INTEGER) {
								struct X86AddressingMode *r CLEANUP(X86AddrModeDestroy)=X86AddrModeReg(intRegs[consumedInts++], &typeU64i);
								retMode->valueType=&typeU64i;
								asmTypecastAssign(_ret,  r, retMode, ASM_ASSIGN_X87FPU_POP);
								addOffsetIfIndir(retMode, 8);
						} else if(type==ABI_SSE) {
								struct X86AddressingMode *r CLEANUP(X86AddrModeDestroy)=X86AddrModeReg(sseRegs[consumedSses++], &typeF64);
								asmTypecastAssign(_ret,  r, retMode, ASM_ASSIGN_X87FPU_POP);
								addOffsetIfIndir(retMode, 8);
						}
						
						//Seek next field
						long first=_8byteFields[c];
						for(;c!=strLongSize(_8byteFields);c++)
								if(_8byteFields[c]!=first) break;
				}
		}
		goto leave;
	retMemory: {
				assert(retVar);
				struct X86AddressingMode *raxMode CLEANUP(X86AddrModeDestroy)=X86AddrModeReg(&regAMD64RAX, objectPtrCreate(&typeU0));
				struct X86AddressingMode *retVarMode CLEANUP(X86AddrModeDestroy)=X86AddrModeVar(retVar, 0);
				asmTypecastAssign(_ret, raxMode, retVarMode, ASM_ASSIGN_X87FPU_POP);
				struct X86AddressingMode *indirMode CLEANUP(X86AddrModeDestroy)=X86AddrModeIndirReg(&regAMD64RAX, retType);
				asmTypecastAssign(_ret, indirMode, retMode, ASM_ASSIGN_X87FPU_POP);

				goto leave;
		}
}
void IR_ABI_SYSV_X64_Call(graphNodeIR _call) {
		struct IRNodeFuncCall *call=(void*)graphNodeIRValuePtr(_call);
		__auto_type abiInfo=llIRAttrFind(graphNodeIRValuePtr(_call)->attrs,IR_ATTR_ABI_INFO,IRAttrGetPred);
		assert(abiInfo);
		struct IRAttrABIInfo *abiInfoValue=(void*)llIRAttrValuePtr(abiInfo);

		struct reg *calleeSaved[]={
				&regAMD64RBX,
				&regAMD64R12u64,
				&regAMD64R13u64,
				&regAMD64R14u64,
				&regAMD64R15u64,
		};
		
		strRegP toBackup CLEANUP(strRegPDestroy)=NULL;
		for(long r=0;r!=strRegPSize(abiInfoValue->liveIn);r++) {
				if(!strRegPSortedFind(toBackup,abiInfoValue->liveIn[r], (regCmpType)ptrPtrCmp))
						toBackup=strRegPSortedInsert(toBackup, abiInfoValue->liveIn[r], (regCmpType)ptrPtrCmp);
		}

		//Merge the callee saved registers then remove them from toBackup(this consumes sub-registers)
		for(long b=0;b!=sizeof(calleeSaved)/sizeof(*calleeSaved);b++) {
				if(!strRegPSortedFind(toBackup,calleeSaved[b], (regCmpType)ptrPtrCmp))
						toBackup=strRegPSortedInsert(toBackup, calleeSaved[b], (regCmpType)ptrPtrCmp);
		}
		toBackup=mergeConflictingRegs(toBackup);
		for(long b=0;b!=sizeof(calleeSaved)/sizeof(*calleeSaved);b++)
				toBackup=strRegPRemoveItem(toBackup, calleeSaved[b], (regCmpType)ptrPtrCmp);
		
		assert(call->base.type==IR_FUNC_CALL);
		strGraphNodeIRP args CLEANUP(strGraphNodeIRPDestroy)=getFuncArgs(_call);
		strGraphEdgeIRP in CLEANUP(strGraphEdgeIRPDestroy)=graphNodeIRIncoming(_call);
		strGraphEdgeIRP inFunc CLEANUP(strGraphEdgeIRPDestroy)=IRGetConnsOfType(in, IR_CONN_FUNC);

		struct object *type=IRNodeType(graphEdgeIRIncoming(inFunc[0]));
		if(type->type==TYPE_PTR)
				type=((struct objectPtr*)type)->type;
		assert(type->type==TYPE_FUNCTION);
		struct objectFunction *fType=(void*)type;
		
		strObject types CLEANUP(strObjectDestroy)=NULL;
		for(long i=0;i!=strFuncArgSize(fType->args);i++)
				types=strObjectAppendItem(types, fType->args[i].type);

		struct reg *intRegs[]={
				&regAMD64RDI,
				&regAMD64RSI,
				&regAMD64RDX,
				&regAMD64RCX,
				&regAMD64R8u64,
				&regAMD64R9u64,
		};
		int stuffedInts=0;
		struct reg *sseRegs[]={
				&regX86XMM0,
				&regX86XMM1,
				&regX86XMM2,
				&regX86XMM3,
				&regX86XMM4,
				&regX86XMM5,
				&regX86XMM6,
				&regX86XMM7,
		};
		int stuffedSses=0;
		strX86AddrMode  toPush CLEANUP(strX86AddrModeDestroy2)=NULL;
		for(long a=0;a!=strGraphNodeIRPSize(args);a++) {
				int consumedInts=0;
				int consumedSses=0;
				long offset=0;
				struct X86AddressingMode *aMode CLEANUP(X86AddrModeDestroy)=IRNode2AddrMode(args[a]);
				
				__auto_type currType=objectBaseType(IRNodeType(args[a]));
				long currTypeSize=objectSize(currType, NULL);

				struct object *argType=NULL;
				if(a<strFuncArgSize(fType->args)) {
						argType=fType->args[a].type;
				} else  {
						argType=promote(currType);
				}
				strObject types CLEANUP(strObjectDestroy)=strObjectAppendItem(NULL,argType);
				strAbiType classes CLEANUP(strAbiTypeDestroy)=consider2Agg(types);
				strLong offsets CLEANUP(strLongDestroy)=NULL;
				strLong fields CLEANUP(strLongDestroy)=NULL;
				get8byteFields(0, types, &fields,&offsets);

				for(long c=0;c<strAbiTypeSize(classes);c+=2) {
						if(classes[c]==ABI_INTEGER) {
								consumedInts++;
						} else if(classes[c]==ABI_SSE) {
								consumedSses++;
						} else if(classes[c]==ABI_MEMORY) {
								consumedSses=consumedInts=0;
								goto pushToStack;
						} else if(classes[c]==ABI_NO_CLASS)
								abort();
				}
				
				if(stuffedInts+consumedInts>=sizeof(intRegs)/sizeof(*intRegs)) goto pushToStack;
				if(stuffedSses+consumedSses>=sizeof(sseRegs)/sizeof(*sseRegs)) goto pushToStack;
				
				for(long c=0;c<strAbiTypeSize(classes);) {
						if(classes[c]==ABI_INTEGER) {
								//Stuff that stuff into a register
								__auto_type r=subRegOfType(intRegs[stuffedInts++],largestDataSizeInWidth(currTypeSize-offset));
								struct X86AddressingMode *rMode CLEANUP(X86AddrModeDestroy)=X86AddrModeReg(r,largestDataSizeInWidth(currTypeSize-offset));
								asmTypecastAssign(_call,rMode,aMode, ASM_ASSIGN_X87FPU_POP);
								addOffsetIfIndir(aMode,8);
						} else if(classes[c]==ABI_SSE) {
								__auto_type r=subRegOfType(sseRegs[stuffedSses++],&typeF64);
								struct X86AddressingMode *rMode CLEANUP(X86AddrModeDestroy)=X86AddrModeReg(r,&typeF64);
								asmTypecastAssign(_call,rMode,aMode, ASM_ASSIGN_X87FPU_POP);
								addOffsetIfIndir(aMode,8);
						} else if(classes[c]==ABI_MEMORY) {
								abort();
						} else if(classes[c]==ABI_NO_CLASS)
								abort();
						
						offset+=8;
						
						//Goto next field
						long first=fields[c];
						for(;c!=strAbiTypeSize(classes);c++)
								if(fields[c]!=first) break;
				}
				continue;
		pushToStack:
				toPush=strX86AddrModeAppendItem(toPush,X86AddrModeClone(aMode));
		}

		toPush=strX86AddrModeReverse(toPush);
		for(long p=0;p!=strX86AddrModeSize(toPush);p++) {
				pushMode(toPush[p]);
		}

		for(long b=0;b!=strRegPSize(toBackup);b++)
				pushReg(toBackup[b]);
		strX86AddrMode callArgs CLEANUP(strX86AddrModeDestroy2)=strX86AddrModeAppendItem(NULL, IRNode2AddrMode(graphEdgeIRIncoming(inFunc[0])));
		assembleOpcode(_call, "CALL",  callArgs);

		strGraphEdgeIRP out CLEANUP(strGraphEdgeIRPDestroy)=graphNodeIROutgoing(_call);
		strGraphEdgeIRP outAsn CLEANUP(strGraphEdgeIRPDestroy)=IRGetConnsOfType(out, IR_CONN_ASSIGN_FROM_PTR);
		if(strGraphEdgeIRPSize(outAsn)==0) {
		} else {
				__auto_type out=graphEdgeIROutgoing(outAsn[0]);
				__auto_type outType=objectBaseType(IRNodeType(out));
				struct X86AddressingMode *oMode CLEANUP(X86AddrModeDestroy)=IRNode2AddrMode(out);
				struct X86AddressingMode *raxMode CLEANUP(X86AddrModeDestroy)=X86AddrModeReg(&regAMD64RAX, promote(fType->retType));
				if(isIntType(outType)) {
						asmTypecastAssign(_call, oMode, raxMode, ASM_ASSIGN_X87FPU_POP);
				} else if(outType==&typeF64) {
						struct X86AddressingMode *xmm0Mode CLEANUP(X86AddrModeDestroy) =X86AddrModeReg(&regX86XMM0, &typeF64);
						asmTypecastAssign(_call, oMode,  xmm0Mode, ASM_ASSIGN_X87FPU_POP);
				} else if(outType->type==TYPE_CLASS||outType->type==TYPE_UNION) {
						struct reg *intRegs[]={
								&regAMD64RAX,
								&regAMD64RDX,
						};
						struct reg *sseRegs[]={
								&regX86XMM0,
								&regX86XMM1,
						};
						long intRegsUsed=0,sseRegsUsed=0;
						
						strObject types CLEANUP(strObjectDestroy)=strObjectAppendItem(NULL,fType->retType);
						strAbiType classes CLEANUP(strAbiTypeDestroy)=consider2Agg(types);
						strLong offsets CLEANUP(strLongDestroy)=NULL;
						strAbiType fields CLEANUP(strAbiTypeDestroy)=consider2Agg(types);
						get8byteFields(0, types, NULL,&offsets);

						if(strAbiTypeSize(fields)==1) {
								if(fields[0]==ABI_MEMORY) {
										struct X86AddressingMode *indirRaxMode CLEANUP(X86AddrModeDestroy)=X86AddrModeIndirReg(&regAMD64RAX, fType->retType);
										asmTypecastAssign(_call, oMode, indirRaxMode, ASM_ASSIGN_X87FPU_POP);
										goto pop;
								}
						}

						long totalWidth=objectSize(fType->retType, NULL);
						for(long o=0;o!=strLongSize(offsets);) {
								__auto_type field=fields[o];
								switch(field) {
								case ABI_INTEGER: {
										struct object *type=largestDataSizeInWidth(totalWidth-offsets[o]);
										__auto_type r=subRegOfType(intRegs[intRegsUsed++], type);
										struct X86AddressingMode *rMode CLEANUP(X86AddrModeDestroy)=X86AddrModeReg(r, type);
										asmTypecastAssign(_call, oMode, NULL, ASM_ASSIGN_X87FPU_POP);
										addOffsetIfIndir(oMode, 8);
										break;
								}
								case ABI_SSE: {
										struct X86AddressingMode *rMode CLEANUP(X86AddrModeDestroy)=X86AddrModeReg(sseRegs[sseRegsUsed++], &typeF64);
										asmTypecastAssign(_call, oMode, rMode, ASM_ASSIGN_X87FPU_POP);
										addOffsetIfIndir(oMode, 8);
										break;
								}
								case ABI_MEMORY: abort();
								case ABI_NO_CLASS: abort();
								}
								o+=getConsecCount(offsets, o);
						}
				} else abort(); 
		}
	pop:
		for(long b=strRegPSize(toBackup)-1;b>=0;b--)
				pushReg(toBackup[b]);
}
