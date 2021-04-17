#include "../IR2asm.h"
#include "../cleanup.h"
#include "../asmEmitter.h"
#include "../abi.h"
#include <assert.h>
struct opTextPair {
		enum IRNodeType irType;
		const char *text;
};
static struct opTextPair binopTextPairs[]={
		{IR_ADD,"ADD"},
		{IR_SUB,"SUB"},
		{IR_MULT,"MULT"},
		{IR_MOD,"MOD"},
		{IR_DIV,"DIV"},
		{IR_LAND,"LAND"},
		{IR_LXOR,"LXOR"},
		{IR_LOR,"LOR"},
		{IR_BAND,"BAND"},
		{IR_BOR,"BOR"},
		{IR_BXOR,"BXOR"},
		{IR_LSHIFT,"LSHIFT"},
		{IR_RSHIFT,"RSHIFT"},
		{IR_GT,"GT"},
		{IR_LT,"LT"},
		{IR_GE,"GE"},
		{IR_LE,"LE"},
		{IR_EQ,"EQ"},
		{IR_NE,"NE"},
};
static struct opTextPair unopTextPairs[]={
		{IR_POS,"POS"},
		{IR_NEG,"NEG"},
		{IR_LNOT,"LNOT"},
		{IR_BNOT,"BNOT"},
};
static struct parserFunction *includeHCRTFunc(const char *name) {
		__auto_type sym=parserGetFuncByName(name);
		if(!sym) {
				fprintf(stderr, "Include HCRT for PowF64");
				abort();
		}
		return sym;
}
static graphNodeIR genUnop(struct opTextPair pair,graphNodeIR connectTo,struct object *obj,long a,long res,struct reg *src1,struct reg *dst) {
		__auto_type funcRef=IRCreateFuncRef(includeHCRTFunc("printf"));

		struct regSlice reg;
		reg.offset=0,reg.reg=src1,reg.type=obj,reg.widthInBits=objectSize(obj, NULL);
		__auto_type src1Asn=IRCreateAssign(IRCreateIntLit(a), IRCreateRegRef(&reg));

		__auto_type unop=IRCreateUnop(src1Asn, pair.irType);
		reg.offset=0,reg.reg=dst,reg.type=obj,reg.widthInBits=objectSize(obj, NULL);
		
		__auto_type dstAsn=IRCreateAssign(unop, IRCreateRegRef(&reg));
		__auto_type eq=IRCreateAssign(IRCreateBinop(dstAsn, IRCreateIntLit(res), IR_EQ),IRCreateRegRef(&reg));
		
		graphNodeIR lab;
		{
				long len=snprintf(NULL, 0, "%s_%li", pair.text,a);
				char buffer[len+1];
				sprintf(buffer,"%s_%li", pair.text,a);
				lab=IRCreateGlobalLabel(buffer);
		}

		graphNodeIR strLit;
		{
				char *typeName=object2Str(obj);
				const char *fmt="\n%s_%s_%li(%s=%s) %%li\n";
				long len=snprintf(NULL, 0, fmt, pair.text,typeName,a,dst->name,src1->name);
				char buffer[len+1];
				sprintf(buffer,fmt, pair.text,typeName,a,dst->name,src1->name);
				__auto_type lab=IRCreateGlobalLabel(buffer);
				strLit=IRCreateStrLit(buffer);
				free(typeName);
		}
		
		__auto_type retVal=IRCreateFuncCall(funcRef, strLit,eq,NULL);
		graphNodeIRConnect(lab,IRStmtStart(retVal),IR_CONN_FLOW);
		graphNodeIRConnect(connectTo, lab, IR_CONN_FLOW);
		return dstAsn;
}
static graphNodeIR genBinopF64(struct opTextPair pair,graphNodeIR connectTo,struct object *obj,double a,double b,double res,struct reg *src1,struct reg *src2,struct reg *dst) {
		struct regSlice reg;
		reg.offset=0,reg.reg=src1,reg.type=obj,reg.widthInBits=objectSize(obj, NULL);
		__auto_type src1Asn=IRCreateAssign(IRCreateFloat(a), IRCreateRegRef(&reg));

		reg.offset=0,reg.reg=src2,reg.type=obj,reg.widthInBits=objectSize(obj, NULL);
		__auto_type src2Asn=IRCreateAssign(IRCreateFloat(b), IRCreateRegRef(&reg));

		__auto_type binop=IRCreateBinop(src1Asn, src2Asn, pair.irType);
		reg.offset=0,reg.reg=dst,reg.type=&typeF64,reg.widthInBits=64;
		__auto_type dstAsn=IRCreateAssign(binop, IRCreateRegRef(&reg));
		reg.offset=0,reg.reg=&regAMD64R8u64,reg.type=&typeI64i,reg.widthInBits=64;
		__auto_type eq=IRCreateAssign(IRCreateBinop(dstAsn, IRCreateFloat(res), IR_EQ),IRCreateRegRef(&reg));

		graphNodeIR lab;
		{
				long len=snprintf(NULL, 0, "%s_%lf", pair.text,a);
				char buffer[len+1];
				sprintf(buffer,"%s_%lf", pair.text,a);
				lab=IRCreateGlobalLabel(buffer);
		}

		graphNodeIR strLit;
		{
				char *typeName=object2Str(obj);
				const char *fmt="\n%s_%s_%lf(%s=%s) %%li\n";
				long len=snprintf(NULL, 0, fmt, pair.text,typeName,res,dst->name,src1->name);
				char buffer[len+1];
				sprintf(buffer,fmt, pair.text,typeName,res,dst->name,src1->name);
				__auto_type lab=IRCreateGlobalLabel(buffer);
				strLit=IRCreateStrLit(buffer);
				free(typeName);
		}
		
		__auto_type funcRef=IRCreateFuncRef(includeHCRTFunc("printf"));
		__auto_type retVal=IRCreateFuncCall(funcRef, strLit,eq,NULL);
		graphNodeIRConnect(lab,IRStmtStart(retVal),IR_CONN_FLOW);
		graphNodeIRConnect(connectTo, lab, IR_CONN_FLOW);
		
		return retVal;
}
static graphNodeIR genBinop(struct opTextPair pair,graphNodeIR connectTo,struct object *obj,long a,long b,long res,struct reg *src1,struct reg *src2,struct reg *dst) {
		__auto_type funcRef=IRCreateFuncRef(includeHCRTFunc("printf"));

		struct regSlice reg;
		reg.offset=0,reg.reg=src1,reg.type=obj,reg.widthInBits=objectSize(obj, NULL);
		__auto_type src1Asn=IRCreateAssign(IRCreateIntLit(a), IRCreateRegRef(&reg));

		reg.offset=0,reg.reg=src2,reg.type=obj,reg.widthInBits=objectSize(obj, NULL);
		__auto_type src2Asn=IRCreateAssign(IRCreateIntLit(b), IRCreateRegRef(&reg));

		__auto_type binop=IRCreateBinop(src1Asn, src2Asn, pair.irType);
		reg.offset=0,reg.reg=dst,reg.type=obj,reg.widthInBits=objectSize(obj, NULL);
		
		__auto_type dstAsn=IRCreateAssign(binop, IRCreateRegRef(&reg));
		__auto_type eq=IRCreateAssign(IRCreateBinop(dstAsn, IRCreateIntLit(res), IR_EQ),IRCreateRegRef(&reg));
		
		graphNodeIR lab;
		{
				long len=snprintf(NULL, 0, "%s_%li_%li", pair.text,a,b);
				char buffer[len+1];
				sprintf(buffer,"%s_%li_%li", pair.text,a,b);
				lab=IRCreateGlobalLabel(buffer);
		}

		graphNodeIR strLit;
		{
				char *typeName=object2Str(obj);
				const char *fmt="\n%s_%s_%li_%li(%s=%s,%s) %%li\n";
				long len=snprintf(NULL, 0, fmt, pair.text,typeName,a,b,dst->name,src1->name,src2->name);
				char buffer[len+1];
				sprintf(buffer,fmt, pair.text,typeName,a,b,dst->name,src1->name,src2->name);
				__auto_type lab=IRCreateGlobalLabel(buffer);
				strLit=IRCreateStrLit(buffer);
				free(typeName);
		}
		
		__auto_type retVal=IRCreateFuncCall(funcRef, strLit,eq,NULL);
		graphNodeIRConnect(lab,IRStmtStart(retVal),IR_CONN_FLOW);
		graphNodeIRConnect(connectTo, lab, IR_CONN_FLOW);
		return dstAsn;
}
static graphNodeIR genTypecast(graphNodeIR connectTo,struct object *from,struct object *to,graphNodeIR value,graphNodeIR expected,struct reg *src1,struct reg *dst) {
		struct regSlice reg;
		reg.offset=0,reg.reg=src1,reg.type=from,reg.widthInBits=objectSize(from, NULL);
		__auto_type Src=IRCreateRegRef(&reg);
		reg.offset=0,reg.reg=dst,reg.type=to,reg.widthInBits=objectSize(to, NULL);
		__auto_type Dst=IRCreateRegRef(&reg);

		__auto_type eq=IRCreateBinop(IRCreateAssign(IRCreateTypecast(IRCreateAssign(value, Src), from, to),Dst), expected, IR_EQ) ;
		consumeRegister(src1),consumeRegister(dst);
		struct reg *tmp=regForTypeExcludingConsumed(&typeI32i);
		reg.offset=0,reg.reg=tmp,reg.type=&typeI32i,reg.widthInBits=32;
		unconsumeRegister(src1),unconsumeRegister(dst);
		eq=IRCreateAssign(eq, IRCreateRegRef(&reg));
		
		__auto_type funcRef=IRCreateFuncRef(includeHCRTFunc("printf"));

		char *fromStr=object2Str(from);
		char *toStr=object2Str(to);
		long len=snprintf(NULL, 0, "(%s->%s) %%li\n", fromStr,toStr);
		char buffer[len+1];
		sprintf(buffer,"(%s->%s) %%li\n", fromStr,toStr);
		__auto_type retval= IRCreateFuncCall(funcRef,IRCreateStrLit(buffer),eq,NULL);
		graphNodeIRConnect(connectTo, IRStmtStart(retval),IR_CONN_FLOW);

		return retval;
}
static graphNodeIR genBinopPtrArith(struct opTextPair pair,graphNodeIR connectTo,struct object *obj,long a,long b,long res,struct reg *src1,struct reg *src2,struct reg *dst) {
		__auto_type funcRef=IRCreateFuncRef(includeHCRTFunc("printf"));

		struct regSlice reg;
		reg.offset=0,reg.reg=src1,reg.type=objectPtrCreate(obj),reg.widthInBits=objectSize(objectPtrCreate(obj), NULL);
		__auto_type src1Asn=IRCreateAssign(IRCreateIntLit(a), IRCreateRegRef(&reg));
		
		reg.offset=0,reg.reg=src2,reg.type=&typeI32i,reg.widthInBits=objectSize(&typeI32i, NULL);
		__auto_type src2Asn=IRCreateAssign(IRCreateIntLit(b), IRCreateRegRef(&reg));

		__auto_type binop=IRCreateBinop(src1Asn, src2Asn, pair.irType);
		reg.offset=0,reg.reg=dst,reg.type=&typeI32i,reg.widthInBits=objectSize(&typeI32i, NULL);
		
		__auto_type dstAsn=IRCreateAssign(binop, IRCreateRegRef(&reg));
		__auto_type eq=IRCreateAssign(IRCreateBinop(dstAsn, IRCreateIntLit(res), IR_EQ),IRCreateIntLit(1));
		
		graphNodeIR lab;
		{
				long len=snprintf(NULL, 0, "%s_%li_%li", pair.text,a,b);
				char buffer[len+1];
				sprintf(buffer,"%s_%li_%li", pair.text,a,b);
				lab=IRCreateGlobalLabel(buffer);
		}

		graphNodeIR strLit;
		{
				char *typeName=object2Str(obj);
				const char *fmt="\n%s_%s_%li_%li(%s=%s,%s) %%li\n";
				long len=snprintf(NULL, 0, fmt, pair.text,typeName,a,b,dst->name,src1->name,src2->name);
				char buffer[len+1];
				sprintf(buffer,fmt, pair.text,typeName,a,b,dst->name,src1->name,src2->name);
				__auto_type lab=IRCreateGlobalLabel(buffer);
				strLit=IRCreateStrLit(buffer);
				free(typeName);
		}
		
		__auto_type retVal=IRCreateFuncCall(funcRef, strLit,eq,NULL);
		graphNodeIRConnect(lab,IRStmtStart(retVal),IR_CONN_FLOW);
		graphNodeIRConnect(connectTo, lab, IR_CONN_FLOW);
		return dstAsn;
}
static void assembleTest(graphNodeIR start) {
		X86EmitAsmInit();
		X86EmitAsmEnterFileStartCode();
		IR2AsmInit();
		IRComputeABIInfo(start);
		IRABIAsmPrologue(0);
		IR2Asm(start);
		IRABIReturn2Asm(NULL, 0);
		X86EmitAsmLeaveFunc(NULL);
										
		/*{
				X86EmitAsmEnterFunc(includeHCRTFunc("main"));
				IRABIAsmPrologue(0);
				IRABIReturn2Asm(NULL, 0);
				X86EmitAsmLeaveFunc(NULL);
				}*/
										
		X86EmitAsm2File("/tmp/binopTests.s",NULL);
		system("yasm -g dwarf2 -f elf32 /tmp/binopTests.s -o /tmp/test.o && gcc -m32 /tmp/test.o -o /tmp/test &&/tmp/test");
		graphNodeIRKill(&start, (void(*)(void*))IRNodeDestroy, NULL);
}
static void assembleTest64(graphNodeIR start) {
		initIR();
		X86EmitAsmInit();
		X86EmitAsmEnterFileStartCode();
		IR2AsmInit();
		IRComputeABIInfo(start);
		IRABIAsmPrologue(0);
		IR2Asm(start);
		IRABIReturn2Asm(NULL, 0);
		X86EmitAsmLeaveFunc(NULL);
										
		{
				X86EmitAsmEnterFunc(includeHCRTFunc("main"));
				IRABIAsmPrologue(0);
				IRABIReturn2Asm(NULL, 0);
				X86EmitAsmLeaveFunc(NULL);
		}
										
		X86EmitAsm2File("/tmp/binopTests.s",NULL);
		system("yasm -g dwarf2 -f elf64 /tmp/binopTests.s -o /tmp/test.o && gcc -m64 /tmp/test.o -o /tmp/test &&/tmp/test && echo $?");
		graphNodeIRKill(&start, (void(*)(void*))IRNodeDestroy, NULL);
}
void fuzzTestBinops() {
		long a=127,b=3;
		/*
		for(long t=0;t!=sizeof(types)/sizeof(*types);t++) {
				strRegP regs CLEANUP(strRegPDestroy)=regGetForType(types[t]);
				for(long r1=0;r1!=strRegPSize(regs);r1++) {
						for(long r2=0;r2!=strRegPSize(regs);r2++)  {
								for(long r3=0;r3!=strRegPSize(regs);r3++) {
										if(r1==r2)
												continue;
										if(regConflict(&regAMD64RAX, regs[r1]))continue;
										if(regConflict(&regAMD64RAX, regs[r2]))continue;
										if(regConflict(&regAMD64RAX, regs[r3]))continue;
										
										graphNodeIR start=IRCreateLabel();
										//__auto_type res=genBinop((struct opTextPair){IR_ADD,"ADD"},start, types[t], a, b, a+b, regs[r1],regs[r2],regs[r3]);
										//__auto_type res=genBinop((struct opTextPair){IR_SUB,"SUB"},start, types[t], a, b, a-b, regs[r1],regs[r2],regs[r3]);
										//__auto_type res=genBinop((struct opTextPair){IR_MULT,"MULT"},start, types[t], a, b, a*b, regs[r1],regs[r2],regs[r3]);
										//__auto_type res=genBinop((struct opTextPair){IR_DIV,"DIV"},start, types[t], a, b, a/b, regs[r1],regs[r2],regs[r3]);
										//__auto_type res=genBinop((struct opTextPair){IR_MOD,"MOD"},start, types[t], a, b, a%b, regs[r1],regs[r2],regs[r3]);
										//__auto_type res=genBinop((struct opTextPair){IR_RSHIFT,"RSHIFT"},start, types[t], a, b, a>>b, regs[r1],regs[r2],regs[r3]);
										//__auto_type res=genBinop((struct opTextPair){IR_BAND,"BAND"}, start, types[t], a, b, a&b, regs[r1], regs[r2], regs[r3]);
										//__auto_type res=genBinop((struct opTextPair){IR_BOR,"BOR"}, start, types[t], a, b, a|b, regs[r1], regs[r2], regs[r3]);
										//__auto_type res=genBinop((struct opTextPair){IR_BXOR,"XOR"}, start, types[t], a, b, a^b, regs[r1], regs[r2], regs[r3]);
										//__auto_type res=genUnop((struct opTextPair){IR_NEG,"NEG"}, start, types[t], a, -a, regs[r1], regs[r3]);
										//__auto_type res=genUnop((struct opTextPair){IR_BNOT,"BNOT"}, start, types[t], a, ~a, regs[r1], regs[r3]);
										/ *for(long a=0;a!=2;a++) {
												for(long b=0;b!=2;b++) {
														//__auto_type res=genBinop((struct opTextPair){IR_LXOR,"LXOR"}, start, types[t], a, b, a^b, regs[r1], regs[r2], regs[r3]);
														//__auto_type res=genBinop((struct opTextPair){IR_LOR,"LOR"}, start, types[t], a, b, a|b, regs[r1], regs[r2], regs[r3]);
														__auto_type res=genBinop((struct opTextPair){IR_LAND,"LAND"}, start, types[t], a, b, a&b, regs[r1], regs[r2], regs[r3]);
														assembleTest(res);
												}
												}* /
										//for(long a=0;a!=2;a++) {
										//__auto_type res=genUnop((struct opTextPair){IR_LNOT,"LNOT"}, start, types[t], a, !a, regs[r1], regs[r3]);
										//	assembleTest(res);
										//}
										//assembleTest(res);
								}
						}
				}
		}*/
		/*
		{
				strRegP regs CLEANUP(strRegPDestroy)=regGetForType(objectPtrCreate(&typeU0)); 
				for(long t=0;t!=sizeof(types)/sizeof(*types);t++) {
						for(long r1=0;r1!=strRegPSize(regs);r1++) {
								for(long r2=0;r2!=strRegPSize(regs);r2++)  {
										for(long r3=0;r3!=strRegPSize(regs);r3++) {
												if(r1==r2) continue;
												__auto_type start=IRCreateLabel();
												//__auto_type res=genBinopPtrArith((struct opTextPair){IR_ADD,"PTR_ADD"}, start, types[t], a, b, a+b*objectSize(types[t], NULL), regs[r1], regs[r2], regs[r3]);
												__auto_type res=genBinopPtrArith((struct opTextPair){IR_SUB,"PTR_SUB"}, start, types[t], a, b, a-b*objectSize(types[t], NULL), regs[r1], regs[r2], regs[r3]);
												assembleTest(res);
										}
								}
						}
				}
				}*/
		/*{
				struct object *types[]={&typeI8i,&typeI16i,&typeI32i,&typeU8i,&typeU16i,&typeU32i,&typeF64};
				long len=sizeof(types)/sizeof(*types);
				for(long t1=0;t1!=len;t1++) {
						for(long t2=0;t2!=len;t2++) {
								strRegP regs1 CLEANUP(strRegPDestroy)=regGetForType(types[t1]);
								for(long r1=0;r1!=strRegPSize(regs1);r1++) {
										strRegP regs2 CLEANUP(strRegPDestroy)=regGetForType(types[t2]);
										for(long r2=0;r2!=strRegPSize(regs2);r2++)  {
												graphNodeIR src,dst;
												if(types[t1]==&typeF64) src=IRCreateFloat(32); else src=IRCreateIntLit(32);
												if(types[t2]==&typeF64) dst=IRCreateFloat(32); else dst=IRCreateIntLit(32);
												if(types[t1]==&typeF64&&regs1[r1]!=&regX86ST0) continue; 
												if(types[t2]==&typeF64&&regs2[r2]!=&regX86ST0) continue; 
												__auto_type start=IRCreateLabel();
												genTypecast(start, types[t1], types[t2], src, dst, regs1[r1], regs2[r2]);
												assembleTest(start);
										}
								}
						}
				}
				}*/
		{
				double a=256.5;
				double b=2.0;
				setArch(ARCH_X64_SYSV);
				strRegP regs CLEANUP(strRegPDestroy)=regGetForType(&typeF64);
				for(long r1=0;r1!=strRegPSize(regs);r1++) {
						for(long r2=0;r2!=strRegPSize(regs);r2++)  {
								for(long r3=0;r3!=strRegPSize(regs);r3++) {
										if(r1==r2)
												continue;
										if(regConflict(&regX86XMM0, regs[r1]))continue;
										if(regConflict(&regX86XMM0, regs[r2]))continue;
										if(regConflict(&regX86XMM0, regs[r3]))continue;
										__auto_type start=IRCreateLabel();
										//__auto_type res=genBinopF64((struct opTextPair){IR_ADD,"ADD"},start, &typeF64, a, b, a+b, regs[r1],regs[r2],regs[r3]);
										//__auto_type res=genBinopF64((struct opTextPair){IR_SUB,"SUB"},start, &typeF64, a, b, a-b, regs[r1],regs[r2],regs[r3]);
										//__auto_type res=genBinopF64((struct opTextPair){IR_DIV,"DIV"},start, &typeF64, a, b, a/b, regs[r1],regs[r2],regs[r3]);
										__auto_type res=genBinopF64((struct opTextPair){IR_MOD,"MOD"},start, &typeF64, a, b, 0.5, regs[r1],regs[r2],regs[r3]);
										assembleTest64(start);
										
								}
						}
				}
		}
}
