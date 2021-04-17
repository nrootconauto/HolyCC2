#include "registers.h"
#include "object.h"
#include "parserA.h"
#include "IR.h"
#include "cleanup.h"
#include <assert.h>
#include <limits.h>
#include "IR2asm.h"
#include "abi.h"
#include "abiSysVx64.h"
#define ALLOCATE(x) ({typeof(x) *tmp=malloc(sizeof(x));*tmp=x;tmp;})
typedef int (*regCmpType)(const struct reg **, const struct reg **);
static int ptrPtrCmp(const void *a, const void *b) {
		if(*(void**)a>*(void**)b)
				return 1;
		else if(*(void**)a<*(void**)b)
				return -1;
		return 0;
}
static strRegP regsFromMode(struct X86AddressingMode *mode) {
	if (mode->type == X86ADDRMODE_REG) {
		return strRegPAppendItem(NULL, mode->value.reg);
	} else if (mode->type == X86ADDRMODE_MEM) {
		if (mode->value.m.type == x86ADDR_INDIR_SIB) {
			strRegP retVal = NULL;
			if (mode->value.m.value.sib.base)
				retVal = regsFromMode(mode->value.m.value.sib.base);
			if (mode->value.m.value.sib.index) {
				__auto_type tmp = strRegPSetUnion(regsFromMode(mode->value.m.value.sib.index), retVal, (regCmpType)ptrPtrCmp);
				strRegPDestroy(&retVal);
				retVal = tmp;
			}
			return retVal;
		}
	}
	return NULL;
}
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
				X86AddrModeIndirSIBAddOffset(mode, offset);
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
				for(long m=0;m!=strObjectMemberSize(cls->members);m++)
						retVal=strObjectAppendItem(retVal, cls->members[m].type);
		} else if(obj->type==TYPE_UNION) {
				struct objectUnion *un=(void*)obj;
				for(long m=0;m!=strObjectMemberSize(un->members);m++)
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
				__auto_type b=objectBaseType(types[t]);
				if(b->type==TYPE_CLASS||b->type==TYPE_UNION)
						retVal=strObjectConcat(retVal,flattenFields(getSubTypes(types[t])));
				else
						retVal=strObjectAppendItem(retVal,types[t]);
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
						offset+=objectSize(flat[f], NULL);
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
		for(long c=offset;c!=strLongSize(items);c++,count++) {
				if(items[c]!=first) break;
		}
		return count;
}
static int getConsumedInts(strAbiType classes,strLong groupings) {
		int consumedInts=0;
		for(long c=0;c<strAbiTypeSize(classes);) {
				if(classes[c]==ABI_INTEGER)
						consumedInts++;
				c+=getConsecCount(groupings, c);
		}
		return consumedInts;
}
static int isMemory(strAbiType classes) {
		for(long c=0;c!=strAbiTypeSize(classes);c++)
				if(classes[c]==ABI_MEMORY)
						return 1;
		return 0;
}
static int getConsumedSses(strAbiType classes,strLong groupings) {
		int consumedSses=0;
		for(long c=0;c<strAbiTypeSize(classes);) {
				if(classes[c]==ABI_SSE)
						consumedSses++;
				c+=getConsecCount(groupings, c);
		}
		return consumedSses;
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

		strAbiType _8byteTypes =NULL;

		long offset=0;
		for(;offset!=strLongSize(groupings);) {
				long consec=getConsecCount(groupings, offset);
				enum ABI_Type fields[consec];
				for(long i=0;i<consec;i++) {
						fields[i]=getAbiType(types[offset+i]);
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
struct formulaPair {
		struct reg *passInReg; //NULL for pass on stack
		strRegP depends;
		long argI;
		struct X86AddressingMode *mode;
};
STR_TYPE_DEF(struct formulaPair*,FormulaPair);
STR_TYPE_FUNCS(struct formulaPair*,FormulaPair);
static void strFormulaPairDestroy2(strFormulaPair *toDestroy) {
		for(long i=0;i!=strFormulaPairSize(*toDestroy);i++) {
				strRegPDestroy(&toDestroy[0][i]->depends);
				free(toDestroy[0][i]);
		}
		strFormulaPairDestroy(toDestroy);
}
static int formulaPairCmp(const struct formulaPair **a, const struct formulaPair **b) {
		if(*a>*b) return 1;
		else if(*a<*b) return -1;
		return 0;
}
static int formulaPairArgiOrderCmp(const struct formulaPair **a, const struct formulaPair **b) {
		return a[0]->argI-b[0]->argI;
}
STR_TYPE_DEF(strFormulaPair,StrFormulaPair);
STR_TYPE_FUNCS(strFormulaPair,StrFormulaPair);
static strFormulaPair canComeAhead(strFormulaPair in,struct formulaPair *have) {
		if(!have->passInReg) return strFormulaPairClone(in);
		strFormulaPair retVal=NULL;
		for(long i=0;i!=strFormulaPairSize(in);i++) {
				for(long d=0;d!=strRegPSize(in[i]->depends);d++)
						if(regConflict(have->passInReg, in[i]->depends[d]))
								goto next;
				retVal=strFormulaPairSortedInsert(retVal, in[i], formulaPairCmp);
		next:;
		}
		return retVal;
}
static void strStrFormulaPairDestroy2(strStrFormulaPair *toDestroy) {
		for(long i=0;i!=strStrFormulaPairSize(*toDestroy);i++)
				strFormulaPairDestroy(&toDestroy[0][i]);
		strStrFormulaPairDestroy(toDestroy);
}
static void dependResolver(strFormulaPair _in,strFormulaPair *out,strFormulaPair *spill) {
		strFormulaPair reversed =NULL,spilled=NULL;
		strFormulaPair in CLEANUP(strFormulaPairDestroy)=strFormulaPairClone(_in);
		
		for(;;) {
				strStrFormulaPair canComeAheads CLEANUP(strStrFormulaPairDestroy2)=strStrFormulaPairResize(NULL, strFormulaPairSize(in));
				for(long i=0;i!=strFormulaPairSize(in);i++) {
						canComeAheads[i]=canComeAhead(in, in[i]);
				}
				
				strFormulaPair un CLEANUP(strFormulaPairDestroy);
				if(0==strFormulaPairSize(in)) un=NULL;
				else un=strFormulaPairClone(canComeAheads[0]);
				for(long i=1;i<strFormulaPairSize(in);i++)
						un=strFormulaPairSetIntersection(un, canComeAheads[i], formulaPairCmp,NULL);
				reversed=strFormulaPairAppendData(reversed, (const struct formulaPair**)un, strFormulaPairSize(un));

				in=strFormulaPairSetDifference(in, un, formulaPairCmp);
				if(0==strFormulaPairSize(in)) {
				end:
						reversed=strFormulaPairReverse(reversed);
						if(out) *out=reversed;
						if(spill) *spill=spilled;

						//Assert the items dont conflict
						for(long r=0;r!=strFormulaPairSize(reversed);r++) {
								strFormulaPair ahead CLEANUP(strFormulaPairDestroy)=canComeAhead(_in, reversed[r]);
								for(long r2=r+1;r2!=strFormulaPairSize(reversed);r2++) {
										assert(strFormulaPairSortedInsert(ahead, reversed[r], formulaPairCmp));
								}
						}
						return;
				}

				if(1==strFormulaPairSize(in)) {
						reversed=strFormulaPairAppendItem(reversed, in[0]);
						in=strFormulaPairPop(in, NULL);
						continue;
				}
				
				strFormulaPair merged CLEANUP(strFormulaPairDestroy)= strFormulaPairClone(canComeAheads[0]);
				for(long i=1;i!=strFormulaPairSize(in);i++)
						merged=strFormulaPairMerge(merged, canComeAheads[i], formulaPairCmp);

				strLong mostSame CLEANUP(strLongDestroy)=strLongResize(NULL, strFormulaPairSize(in));
				for(long i=0;i!=strFormulaPairSize(in);i++)  {
						mostSame[i]=0;
						for(long p=0;p!=strFormulaPairSize(canComeAheads[i]);p++)
								for(long s=0;s!=strFormulaPairSize(merged);s++)
										if(regConflict(canComeAheads[i][p]->passInReg,merged[s]->passInReg))
												mostSame[i]++;
				}

				long lowestI=0;
				long lowestVal=LONG_MAX;
				for(long i=0;i<strFormulaPairSize(in);i++)
						if(lowestVal>mostSame[i]) lowestVal=mostSame[i],lowestI=i;
				spilled=strFormulaPairAppendItem(spilled, in[lowestI]);
				in=strFormulaPairRemoveItem(in, in[lowestI], formulaPairCmp);
		}
}
/**
	* Pushes RDI at end of frame if returns mem,THEREFORE MAKE ROOM FOR THIS "hidden" VALUE. 
	*/

void IR_ABI_SYSV_X64_Prologue(long frameSize) {
		CLEANUP(X86AddrModeDestroy) struct X86AddressingMode *esp=X86AddrModeReg(stackPointer(), objectPtrCreate(&typeU0));
		struct X86AddressingMode *ebp CLEANUP(X86AddrModeDestroy) = X86AddrModeReg(basePointer(),objectPtrCreate(&typeU0));
		pushReg(basePointer());
		asmAssign(NULL,ebp, esp, ptrSize(),0);

		
		CLEANUP(strX86AddrModeDestroy2) strX86AddrMode subArgs=NULL;
		subArgs=strX86AddrModeAppendItem(subArgs, X86AddrModeClone(esp));
		subArgs=strX86AddrModeAppendItem(subArgs, X86AddrModeSint(frameSize+8)); //+8 Makes room for structure return location 
		assembleOpcode(NULL, "SUB", subArgs);

		pushReg(&regAMD64RBX);
		pushReg(&regAMD64R12u64);
		pushReg(&regAMD64R13u64);
		pushReg(&regAMD64R14u64);
		pushReg(&regAMD64R15u64);
}
/**
	* We store a hidden 8 byte value at the end of the frame for the return addr,so add 8
	*/
static void loadPreservedRegs(long frameSize) {
		frameSize+=8;
		struct X86AddressingMode *ebpMode CLEANUP(X86AddrModeDestroy)=X86AddrModeReg(basePointer(),objectPtrCreate(&typeU0));
		struct X86AddressingMode *espMode CLEANUP(X86AddrModeDestroy)=X86AddrModeReg(stackPointer(),objectPtrCreate(&typeU0));
		asmAssign(NULL,espMode, ebpMode, ptrSize(), 0);
		
		strX86AddrMode subArgs CLEANUP(strX86AddrModeDestroy2)=NULL;
		subArgs=strX86AddrModeAppendItem(subArgs, X86AddrModeReg(stackPointer(),objectPtrCreate(&typeU0)));
		subArgs=strX86AddrModeAppendItem(subArgs, X86AddrModeSint(frameSize+5*8));
		assembleOpcode(NULL,"SUB", subArgs);

		popReg(&regAMD64R15u64);
		popReg(&regAMD64R14u64);
		popReg(&regAMD64R13u64);
		popReg(&regAMD64R12u64);
		popReg(&regAMD64RBX);
}
void IR_ABI_SYSV_X64_Return(graphNodeIR _ret,long frameSize) {
		if(!_ret) goto leave;
		if(0) {
		leave:
				loadPreservedRegs(frameSize);
				assembleOpcode(_ret, "LEAVE", NULL);
				assembleOpcode(_ret, "RET", NULL);
				return;
		}
		strGraphEdgeIRP in CLEANUP(strGraphEdgeIRPDestroy)=IREdgesByPrec(_ret);
		if(!strGraphEdgeIRPSize(in)) goto leave;
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
		strLong offsets CLEANUP(strLongDestroy)=NULL;
		get8byteFields(0, _retType, &_8byteFields, &offsets);
		
		int consumedInts=getConsumedInts(fields, offsets),consumedSses=getConsumedSses(fields, _8byteFields);
		if(consumedInts>2||consumedSses>2||isMemory(fields)) goto retMemory;
		{
				consumedSses=consumedInts=0;
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
				struct X86AddressingMode *raxMode CLEANUP(X86AddrModeDestroy)=X86AddrModeReg(&regAMD64RAX, objectPtrCreate(&typeU0));
				/**
					*Recall in IR_ABI_SYSV_X64_LoadArgs we pushed old RDI at end of stack freame
					*/	
				struct X86AddressingMode *retVarMode CLEANUP(X86AddrModeDestroy)=X86AddrModeIndirSIB(0, NULL, X86AddrModeReg(basePointer(),objectPtrCreate(&typeU0)), X86AddrModeSint(-frameSize-8), objectPtrCreate(&typeU0));
				asmTypecastAssign(_ret, raxMode, retVarMode, ASM_ASSIGN_X87FPU_POP);
				struct X86AddressingMode *indirMode CLEANUP(X86AddrModeDestroy)=X86AddrModeIndirReg(&regAMD64RAX, retType);
				asmTypecastAssign(_ret, indirMode, retMode, ASM_ASSIGN_X87FPU_POP);
				goto leave;
		}
}
static strX86AddrMode shuffleArgsByFormula(strX86AddrMode args,strFormulaPair order) {
		strX86AddrMode retVal=strX86AddrModeResize(NULL, strFormulaPairSize(order));
		for(long o=0;o!=strFormulaPairSize(order);o++)
				retVal[o]=X86AddrModeClone(args[order[o]->argI]);
		return retVal;
}
void IR_ABI_SYSV_X64_Call(graphNodeIR _call,struct X86AddressingMode *funcMode,strX86AddrMode args,struct X86AddressingMode *outMode) {
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

		for(long b=0;b!=strRegPSize(toBackup);b++)
				pushReg(toBackup[b]);
		
		assert(call->base.type==IR_FUNC_CALL);

		struct object *type=funcMode->valueType;
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
		int returnsMem=0;
		{
				strObject tmp CLEANUP(strObjectDestroy)=strObjectAppendItem(NULL, fType->retType); 
				strAbiType classes CLEANUP(strAbiTypeDestroy)=consider2Agg(tmp);
				returnsMem=stuffedInts=isMemory(classes)?1:0;
		}
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

		strFormulaPair toPassFormula CLEANUP(strFormulaPairDestroy2)=NULL;

		long spillStackSize=0;
		long pushedArgsSize=0;
		strFormulaPair outFormula CLEANUP(strFormulaPairDestroy)=NULL;
		strFormulaPair spillFormula CLEANUP(strFormulaPairDestroy)=NULL;

		if(returnsMem) {
				strX86AddrMode leaArgs CLEANUP(strX86AddrModeDestroy2)=NULL;
				leaArgs=strX86AddrModeAppendItem(leaArgs, X86AddrModeReg(&regAMD64RDI, objectPtrCreate(&typeU0)));
				leaArgs=strX86AddrModeAppendItem(leaArgs, X86AddrModeClone(outMode));
				leaArgs[1]->valueType=&typeU64i;
				assembleOpcode(_call, "LEA",  leaArgs);
		}
		for(long a=0;a!=strX86AddrModeSize(args);a++) {
				int consumedInts=0;
				int consumedSses=0;
				long offset=0;
				struct X86AddressingMode *aMode CLEANUP(X86AddrModeDestroy)=X86AddrModeClone(args[a]);
				
				__auto_type currType=objectBaseType(args[a]->valueType);
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

				consumedInts=getConsumedInts(classes, fields);
				consumedSses=getConsumedSses(classes, fields);
				
				if(stuffedInts+consumedInts>sizeof(intRegs)/sizeof(*intRegs)) goto pushToStack;
				if(stuffedSses+consumedSses>sizeof(sseRegs)/sizeof(*sseRegs)) goto pushToStack;
				if(isMemory(classes)) goto pushToStack;
				
				for(long c=0;c<strAbiTypeSize(classes);) {
						if(classes[c]==ABI_INTEGER) {
								//Stuff that stuff into a register
								__auto_type r=subRegOfType(intRegs[stuffedInts++],largestDataSizeInWidth(currTypeSize-offset));
								struct formulaPair pair;
								pair.depends=regsFromMode(args[a]);
								pair.passInReg=r;
								pair.argI=a;
								pair.mode=X86AddrModeClone(args[a]);
								toPassFormula=strFormulaPairSortedInsert(toPassFormula,ALLOCATE(pair),formulaPairCmp);
								addOffsetIfIndir(aMode,8);
						} else if(classes[c]==ABI_SSE) {
								__auto_type r=subRegOfType(sseRegs[stuffedSses++],&typeF64);
								struct formulaPair pair;
								pair.depends=regsFromMode(args[a]);
								pair.passInReg=r;
								pair.mode=X86AddrModeClone(args[a]);
								pair.argI=a;
								toPassFormula=strFormulaPairSortedInsert(toPassFormula,ALLOCATE(pair),formulaPairCmp);
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
				pushedArgsSize+=objectSize(aMode->valueType, NULL);
		}
		{
				strX86AddrMode args2 CLEANUP(strX86AddrModeDestroy)=strX86AddrModeClone(args);
				dependResolver(toPassFormula, &outFormula,&spillFormula);
				args2=shuffleArgsByFormula(args2, outFormula);
				
				for(long s=0;s!=strFormulaPairSize(spillFormula);s++) {
						pushMode(args[spillFormula[s]->argI]);
						spillStackSize+=objectSize(args[spillFormula[s]->argI]->valueType, NULL);
				}

				for(long p=0;p!=strFormulaPairSize(outFormula);p++) {
						struct X86AddressingMode *rMode CLEANUP(X86AddrModeDestroy)=X86AddrModeReg(outFormula[p]->passInReg, outFormula[p]->mode->valueType);
						asmTypecastAssign(_call, rMode, outFormula[p]->mode, ASM_ASSIGN_X87FPU_POP);
				}
				long currSpillStackOffset=0;
				for(long s=0;s!=strFormulaPairSize(spillFormula);s++) {
						struct X86AddressingMode *stackPtr CLEANUP(X86AddrModeDestroy)=X86AddrModeIndirReg(stackPointer(), spillFormula[s]->mode->valueType);
						X86AddrModeIndirSIBAddOffset(stackPtr, currSpillStackOffset);		
						assert(spillFormula[s]->passInReg);
						struct X86AddressingMode *rMode CLEANUP(X86AddrModeDestroy)=X86AddrModeReg(spillFormula[s]->passInReg, spillFormula[s]->mode->valueType);
						asmTypecastAssign(_call, rMode, stackPtr, ASM_ASSIGN_X87FPU_POP);

						currSpillStackOffset+=objectSize(spillFormula[s]->mode->valueType, NULL);
				}
				
				toPush=strX86AddrModeReverse(toPush);
				for(long p=0;p!=strX86AddrModeSize(toPush);p++)
						pushMode(toPush[p]);
		}
		
		//AL holds number of floating points passed in registers
		struct X86AddressingMode *alMode CLEANUP(X86AddrModeDestroy)=X86AddrModeReg(&regAMD64RAX, &typeU64i);
		struct X86AddressingMode *passedFltRegsMode CLEANUP(X86AddrModeDestroy)=X86AddrModeSint(stuffedSses);
		asmTypecastAssign(_call, alMode, passedFltRegsMode, ASM_ASSIGN_X87FPU_POP);
		
		strX86AddrMode callArgs CLEANUP(strX86AddrModeDestroy2)=strX86AddrModeAppendItem(NULL, X86AddrModeClone(funcMode));
		assembleOpcode(_call, "CALL",  callArgs);

		//Get Rid of the "spill" area and pushed arguments
		strX86AddrMode addArgs CLEANUP(strX86AddrModeDestroy2)=NULL;
		addArgs=strX86AddrModeAppendItem(addArgs, X86AddrModeReg(stackPointer(), objectPtrCreate(&typeU0)));
		addArgs=strX86AddrModeAppendItem(addArgs, X86AddrModeSint(spillStackSize+pushedArgsSize));
		assembleOpcode(_call, "ADD", addArgs);
		
		if(outMode) {
				struct X86AddressingMode *oMode =outMode;
				__auto_type outType=oMode->valueType;
				struct X86AddressingMode *raxMode CLEANUP(X86AddrModeDestroy)=X86AddrModeReg(&regAMD64RAX, promote(fType->retType));
				if(isIntType(outType)) {						
						asmTypecastAssign(_call, oMode, raxMode, ASM_ASSIGN_X87FPU_POP);
						goto pop;
				} else if(outType==&typeF64) {
						struct X86AddressingMode *xmm0Mode CLEANUP(X86AddrModeDestroy) =X86AddrModeReg(&regX86XMM0, &typeF64);
						asmTypecastAssign(_call, oMode, xmm0Mode, ASM_ASSIGN_X87FPU_POP);
						goto pop;
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
						strLong groupings CLEANUP(strLongDestroy)=NULL;
						strAbiType fields CLEANUP(strAbiTypeDestroy)=consider2Agg(types);
						get8byteFields(0, types, &groupings,&offsets);

						if(strAbiTypeSize(fields)==1) {
								if(fields[0]==ABI_MEMORY) {
										struct X86AddressingMode *indirRaxMode CLEANUP(X86AddrModeDestroy)=X86AddrModeIndirReg(&regAMD64RAX, fType->retType);
										asmTypecastAssign(_call, oMode, indirRaxMode, ASM_ASSIGN_X87FPU_POP);
										goto pop;
								}
						}

						//Remove returned registers from backup
						long __intRegsUsed=getConsumedInts(classes, groupings);
						for(long i=0;i!=__intRegsUsed;i++)
								toBackup=strRegPSortedInsert(toBackup,intRegs[i], (regCmpType)ptrPtrCmp);
						toBackup=mergeConflictingRegs(toBackup);
						for(long i=0;i!=__intRegsUsed;i++)
								toBackup=strRegPRemoveItem(toBackup, intRegs[i], (regCmpType)ptrPtrCmp);
						
						long __sseRegsUsed=getConsumedSses(classes, groupings);
						for(long s=0;s!=__sseRegsUsed;s++)
								toBackup=strRegPSortedInsert(toBackup,intRegs[s], (regCmpType)ptrPtrCmp);
						toBackup=mergeConflictingRegs(toBackup);
						for(long s=0;s!=__sseRegsUsed;s++)
								toBackup=strRegPRemoveItem(toBackup, sseRegs[s], (regCmpType)ptrPtrCmp);
						
						long totalWidth=objectSize(fType->retType, NULL);
						for(long o=0;o!=strLongSize(offsets);) {
								__auto_type field=fields[o];
								switch(field) {
								case ABI_INTEGER: {
										struct object *type=largestDataSizeInWidth(totalWidth-offsets[o]);
										__auto_type r=subRegOfType(intRegs[intRegsUsed++], type);
										struct X86AddressingMode *rMode CLEANUP(X86AddrModeDestroy)=X86AddrModeReg(r, type);
										oMode->valueType=type;
										asmTypecastAssign(_call, oMode, rMode, ASM_ASSIGN_X87FPU_POP);
										addOffsetIfIndir(oMode, 8);
										break;
								}
								case ABI_SSE: {
										struct X86AddressingMode *rMode CLEANUP(X86AddrModeDestroy)=X86AddrModeReg(sseRegs[sseRegsUsed++], &typeF64);
										oMode->valueType=type;
										asmTypecastAssign(_call, oMode, rMode, ASM_ASSIGN_X87FPU_POP);
										addOffsetIfIndir(oMode, 8);
										break;
								}
								case ABI_MEMORY: abort();
								case ABI_NO_CLASS: abort();
								}
								o+=getConsecCount(groupings, o);
						}

						for(long b=strRegPSize(toBackup)-1;b>=0;b--)
								popReg(toBackup[b]);
				} else abort(); 
		}
		return;
	pop:
		for(long b=strRegPSize(toBackup)-1;b>=0;b--)
								popReg(toBackup[b]);
}
static struct regSlice createRegSlice(struct reg *r,struct object *type) {
		struct regSlice slice;
		slice.offset=0;
		slice.reg=r;
		slice.type=type;
		slice.widthInBits=objectSize(type, NULL)*8;
		return slice;
}
static void transparentKillNode(graphNodeIR node) {
		strGraphEdgeIRP in CLEANUP(strGraphEdgeIRPDestroy)=graphNodeIRIncoming(node);
		strGraphEdgeIRP out CLEANUP(strGraphEdgeIRPDestroy)=graphNodeIROutgoing(node);
		for(long i=0;i!=strGraphEdgeIRPSize(in);i++) {
				for(long o=0;o!=strGraphEdgeIRPSize(out);o++)
						graphNodeIRConnect(graphEdgeIRIncoming(in[i]), graphEdgeIROutgoing(out[o]), *graphEdgeIRValuePtr(in[i]));
		}
		graphNodeIRKill(&node ,(void(*)(void*))IRNodeDestroy, NULL);
}
/**
	* Stores RDI at end of frame if returns mem 
	*/
void IR_ABI_SYSV_X64_LoadArgs(graphNodeIR start,long frameSize) {
		struct IRNodeFuncStart *fStart=(void*)graphNodeIRValuePtr(start);
		struct objectFunction *fType=(void*)fStart->func->type;
		
		struct reg *intRegs[]={
				&regAMD64RDI,
				&regAMD64RSI,
				&regAMD64RDX,
				&regAMD64RCX,
				&regAMD64R8u64,
				&regAMD64R9u64,
		};
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
		
		CLEANUP(strGraphNodeIRPDestroy) strGraphNodeIRP allNodes =graphNodeIRAllNodes(start);
		long stackOffset=16;
		int stuffedInts=0;
		int stuffedSses=0;
		for(long a=0;a!=strFuncArgSize(fType->args);a++) {
				__auto_type arg=fType->args[a];
				
				CLEANUP(strObjectDestroy) strObject types=strObjectAppendItem(NULL,fType->args[a].type);
				CLEANUP(strAbiTypeDestroy) strAbiType classes=consider2Agg(types);
				CLEANUP(strLongDestroy) strLong offsets=NULL;
				CLEANUP(strLongDestroy) strLong fields=NULL;
				get8byteFields(0, types, &fields, &offsets);

				long consumedInts=getConsumedInts(classes, fields);
				long consumedSses=getConsumedSses(classes, fields);
				if(consumedInts+stuffedInts>sizeof(intRegs)/sizeof(*intRegs)) goto mem;
				if(consumedSses+stuffedSses>sizeof(sseRegs)/sizeof(*sseRegs)) goto mem;
				if(isMemory(classes)) goto mem;
				__auto_type var=fType->args[a].var;
				if(isIntType(arg.type)) {
						assert(consumedInts==1);
						CLEANUP(X86AddrModeDestroy) struct X86AddressingMode *regMode=X86AddrModeReg(subRegOfType(intRegs[stuffedInts],arg.type), arg.type);
						CLEANUP(X86AddrModeDestroy) struct X86AddressingMode *varMode=X86AddrModeVar(var, 0);
						asmTypecastAssign(NULL,varMode, regMode,  ASM_ASSIGN_X87FPU_POP);
						goto next;
				} else if(objectBaseType(arg.type)==&typeF64) {
						assert(consumedSses==1);
						CLEANUP(X86AddrModeDestroy) struct X86AddressingMode *regMode=X86AddrModeReg(sseRegs[stuffedSses], &typeF64);
						CLEANUP(X86AddrModeDestroy) struct X86AddressingMode *varMode=X86AddrModeVar(var, 0);
						asmTypecastAssign(NULL,varMode, regMode,  ASM_ASSIGN_X87FPU_POP);
						goto next;
				} else {
						long totalSize=objectSize(arg.type, NULL);;
						CLEANUP(X86AddrModeDestroy) struct X86AddressingMode *varMode=X86AddrModeVar(var, 0);
						long consumedInts=0,consumedSses=0;
						for(long c=0;c!=strLongSize(offsets);) {
								__auto_type type=largestDataSizeInWidth(totalSize-offsets[c]);
								varMode->value.varAddr.offset=offsets[c];
								varMode->valueType=type;

								CLEANUP(X86AddrModeDestroy) struct X86AddressingMode *inMode=NULL; 
								if(classes[c]==ABI_INTEGER) {
										__auto_type tmp=intRegs[stuffedInts+consumedInts++];
										inMode=X86AddrModeReg(subRegOfType(tmp, type), type);
								} else if(classes[c]==ABI_SSE) {
										__auto_type tmp=sseRegs[stuffedSses+consumedSses++];
										inMode=X86AddrModeReg(tmp, &typeF64);
								} else {
										//???
										abort();
								}
								asmTypecastAssign(NULL, varMode, inMode, ASM_ASSIGN_X87FPU_POP);
										
								c+=getConsecCount(offsets, c);
						}
						;
						goto next;
				}
		next:
				stuffedInts+=consumedInts;
				stuffedSses+=consumedSses;
				continue;
		mem: {
						__auto_type var=fType->args[a].var;
						CLEANUP(X86AddrModeDestroy) struct X86AddressingMode *varMode=X86AddrModeVar(var, 0);
						CLEANUP(X86AddrModeDestroy) struct X86AddressingMode *inMode=X86AddrModeIndirReg(basePointer(), var->type);
						X86AddrModeIndirSIBAddOffset(inMode, stackOffset);
						asmTypecastAssign(NULL, varMode, inMode, ASM_ASSIGN_X87FPU_POP);

						long tSize=objectSize(var->type,NULL);
						stackOffset+=tSize/8*8+((tSize%8)?1:0);
				}
		}
		for(long n=0;n!=strGraphNodeIRPSize(allNodes);n++) {
				struct IRNodeFuncArg *arg=(void*)graphNodeIRValuePtr(allNodes[n]);
				if(arg->base.type!=IR_FUNC_ARG) continue;
				transparentKillNode(allNodes[n]);
		}

		strObject retType CLEANUP(strObjectDestroy)= strObjectAppendItem(NULL, fType->retType);
		strAbiType classes CLEANUP(strAbiTypeDestroy)=consider2Agg(retType);
		if(isMemory(classes)) {
				struct X86AddressingMode *retAddr=X86AddrModeIndirSIB(0, NULL, X86AddrModeReg(basePointer(), &typeU64i), X86AddrModeSint(-frameSize-8), &typeU64i);
				struct X86AddressingMode *rdiMode =X86AddrModeReg(&regAMD64RDI, &typeU64i);
				asmTypecastAssign(NULL, retAddr, rdiMode, ASM_ASSIGN_X87FPU_POP);
		}
}
