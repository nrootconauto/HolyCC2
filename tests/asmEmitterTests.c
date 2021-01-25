#include <asmEmitter.h>
#include <opcodesParser.h>
#include <stdlib.h>
#include <stdarg.h>
#include <cleanup.h>
#include <assert.h>
#include <registers.h>
#include <IR.h>
#include <IR2asm.h>
static void runTest(const char *expected) {
		const char *name=tmpnam(NULL);
		X86EmitAsm2File(name);
		const char *commandText=
				"yasm -g dwarf2 -f elf -o /tmp/hccTest.o %s "
				"&& ld -o /tmp/hccTest /tmp/hccTest.o "
				"&& /tmp/hccTest > /tmp/hccResult";
		long count=snprintf(NULL, 0, commandText,  name);
		char buffer[count+1];
		sprintf(buffer, commandText, name);
		system(buffer);

		FILE *f=fopen("/tmp/hccResult", "r");

		fseek(f, 0, SEEK_END);
		long end=ftell(f);
		fseek(f, 0, SEEK_SET);
		long start=ftell(f);
		char buffer2[end-start+1];
		fread(buffer2, end-start, 1, f);
		buffer2[end-start]='\0';

		assert(0==strcmp(expected, buffer2));
		fclose(f);
}
static void strX86AddrModeDestroy2(strX86AddrMode *str) {
		for(long i=0;i!=strX86AddrModeSize(*str);i++)
				X86AddrModeDestroy(&str[0][i]);
		strX86AddrModeDestroy(str);
}
static void assembleInst(const char *name,...) {
		va_list args;
		va_start(args, name);
		strX86AddrMode args2 CLEANUP(strX86AddrModeDestroy2)=NULL;
		for(;;) {
				__auto_type arg=va_arg(args, struct X86AddressingMode *);
				if(!arg)
						break;
				args2=strX86AddrModeAppendItem(args2, arg);
		}
		va_end(args);
		
		strOpcodeTemplate  ops CLEANUP(strOpcodeTemplateDestroy)=X86OpcodesByArgs(name, args2 , NULL);
		assert(strOpcodeTemplateSize(ops));
		int err;
		X86EmitAsmInst(ops[0], args2, &err);
		assert(!err);
}
static void assembleExit() {
		assembleInst("MOV", X86AddrModeReg(&regX86EAX),X86AddrModeSint(0x01),NULL);
		assembleInst("MOV", X86AddrModeReg(&regX86EBX),X86AddrModeSint(0x00),NULL);
		assembleInst("INT", X86AddrModeSint(0x80),NULL);
}
static void assemblePrint(const char *text) {
		assembleInst("MOV", X86AddrModeReg(&regX86EAX),X86AddrModeSint(0x04),NULL);
		assembleInst("MOV", X86AddrModeReg(&regX86EBX),X86AddrModeSint(0x01),NULL);
		assembleInst("MOV", X86AddrModeReg(&regX86ECX),X86EmitAsmStrLit(text),NULL);
				assembleInst("MOV", X86AddrModeReg(&regX86EDX),X86AddrModeSint(strlen(text)),NULL);
				assembleInst("INT", X86AddrModeSint(0x80),NULL);
}
static struct regSlice reg2Slice(struct reg *reg,struct object *type) {
		struct regSlice slice;
		slice.offset=0;
		slice.reg=reg;
		slice.type=type;
		slice.widthInBits=reg->size*8;
		return slice;
}
void asmEmitterTests() {
		X86EmitAsmInit();
		{
				const char *text="Hello\n";
				free(X86EmitAsmLabel("_start"));
				assemblePrint(text);
				assembleExit();
				runTest(text);
		}
		X86EmitAsmInit();
		{
				free(X86EmitAsmLabel("_start"));
				strX86AddrMode dstSrc CLEANUP(strX86AddrModeDestroy2)=NULL;
				dstSrc=strX86AddrModeAppendItem(dstSrc, X86AddrModeSint(0));
				dstSrc=strX86AddrModeAppendItem(dstSrc, X86AddrModeSint(4));
				struct X86AddressingMode *lab CLEANUP(X86AddrModeDestroy)=X86EmitAsmDU32(dstSrc, 2);
				//Make  "frame" point to lab,we will then use that as a base
				assembleInst("MOV", X86AddrModeReg(&regX86EBP),X86AddrModeClone(lab),NULL);
				__auto_type a=IRCreateFrameAddress(4, &typeI32i);
				__auto_type b=IRCreateFrameAddress(4, &typeI32i);
				__auto_type out=IRCreateFrameAddress(0, &typeI32i);
				IRCreateAssign(IRCreateBinop(a, b, IR_ADD),out);
				IR2Asm(out);
				struct X86AddressingMode *f CLEANUP(X86AddrModeDestroy)=X86EmitAsmStrLit("f");
				struct X86AddressingMode *t CLEANUP(X86AddrModeDestroy)=X86EmitAsmStrLit("t");
				assembleInst("MOV", X86AddrModeReg(&regX86ECX),X86AddrModeClone(f),NULL);
				assembleInst("CMP", X86AddrModeIndirReg(&regX86EBP, &typeI32i),X86AddrModeSint(8),NULL);
				assembleInst("MOV", X86AddrModeReg(&regX86EBX),X86AddrModeClone(t),NULL);
				assembleInst("CMOVE", X86AddrModeReg(&regX86ECX), X86AddrModeReg(&regX86EBX));
				//Print
				assembleInst("MOV", X86AddrModeReg(&regX86EAX),X86AddrModeSint(0x04),NULL);
				assembleInst("MOV", X86AddrModeReg(&regX86EBX),X86AddrModeSint(1),NULL);
				//ECX either has t or f in it
				assembleInst("MOV", X86AddrModeReg(&regX86EDX),X86AddrModeSint(1),NULL);
				assembleInst("INT", X86AddrModeSint(0x80),NULL);
				assembleExit();
				runTest("t");
		}
		X86EmitAsmInit();
		{
				//Test register access
				free(X86EmitAsmLabel("_start"));
				__auto_type dSlice=reg2Slice(&regX86ECX,&typeI32i);
				__auto_type ecx=IRCreateRegRef(&dSlice);
				assembleInst("MOV", X86AddrModeReg(&regX86EDX),X86AddrModeSint(3),NULL);
				__auto_type edx1=IRCreateRegRef(&dSlice);
				__auto_type edx2=IRCreateRegRef(&dSlice);
				__auto_type edx3=IRCreateRegRef(&dSlice);
				IRCreateAssign(IRCreateBinop(edx1, edx2, IR_ADD), edx3);
					struct X86AddressingMode *f CLEANUP(X86AddrModeDestroy)=X86EmitAsmStrLit("f");
				struct X86AddressingMode *t CLEANUP(X86AddrModeDestroy)=X86EmitAsmStrLit("t");

				assembleInst("MOV", X86AddrModeReg(&regX86ECX), X86AddrModeClone(f),NULL);
				assembleInst("MOV", X86AddrModeReg(&regX86EBX),X86AddrModeClone(t),NULL);
				assembleInst("CMP", X86AddrModeIndirReg(&regX86EDX, &typeI32i),X86AddrModeSint(6),NULL);
				assembleInst("CMOVE", X86AddrModeReg(&regX86ECX), X86AddrModeReg(&regX86EBX));
		}
}
