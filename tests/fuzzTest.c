#include "../IR2asm.h"
#include "../cleanup.h"
#include "../asmEmitter.h"
#include "../abi.h"
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
static void assembleTest(graphNodeIR start) {
		X86EmitAsmInit();
		X86EmitAsmEnterFileStartCode();
		IR2AsmInit();
		IRComputeABIInfo(start);
		IRABIAsmPrologue(0);
		IR2Asm(start);
		IRABIReturn2Asm(NULL, 0);
		X86EmitAsmLeaveFunc(NULL);
										
		{
				X86EmitAsmEnterFunc("main");
				IRABIAsmPrologue(0);
				IRABIReturn2Asm(NULL, 0);
				X86EmitAsmLeaveFunc(NULL);
		}

										
		X86EmitAsm2File("/tmp/binopTests.s",NULL);
		system("yasm -g dwarf2 -f elf32 /tmp/binopTests.s -o /tmp/test.o && gcc -m32 -lm /tmp/test.o -o /tmp/test &&/tmp/test");
		graphNodeIRKill(&start, (void(*)(void*))IRNodeDestroy, NULL);
}
void fuzzTestBinops() {
		struct object *types[]={
				&typeI8i,
				&typeI16i,
				&typeI32i,
				&typeI64i,
				&typeU8i,
				&typeU16i,
				&typeU32i,
				&typeU64i,
		};
		long a=127,b=3;
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
										__auto_type res=genUnop((struct opTextPair){IR_NEG,"NEG"}, start, types[t], a, -a, regs[r1], regs[r3]);
										assembleTest(res);
								}
						}
				}
		}
		
}
