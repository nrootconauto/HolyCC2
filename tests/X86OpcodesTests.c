#include <assert.h>
#include <opcodesParser.h>
#include <registers.h>
#include <cleanup.h>
void X86OpcodesTests() {
		const char *keywords[]={
				"!", //?
				"&", //Defualt
				"%", //32bit only
				"=", //Requires REX if 64bit mode,
				"`", //REX if R8 or R15,
				"^", //?
				"*", //ST(i) like
				"$", //?
				",",
				"+R",
				"+I",
				"/R",
				"/0",
				"/1",
				"/2",
				"/3",
				"/4",
				"/5",
				"/6",
				"/7",
				"IB",
				"IW",
				"ID",
				"RM16",
				"RM32",
				"RM64",
				"R64",
				"MOFFS8",
				"MOFFS16",
				"MOFFS32",
				"REL8",
				"REL16",
				"REL32",
		};
		{
				//reg(CS)
				__auto_type seg=X86AddrModeReg(&regX86CS);
				strX86AddrMode args CLEANUP(strX86AddrModeDestroy)=strX86AddrModeAppendItem(NULL, seg);
				strOpcodeTemplate  find CLEANUP(strOpcodeTemplateDestroy)=X86OpcodesByArgs("PUSH",  args,NULL);
				assert(strOpcodeTemplateSize(find)!=0);
		}
		{
				//R16
				__auto_type reg=X86AddrModeReg(&regX86DX);
				strX86AddrMode args CLEANUP(strX86AddrModeDestroy)=strX86AddrModeAppendItem(NULL, reg);
				strOpcodeTemplate  find CLEANUP(strOpcodeTemplateDestroy)=X86OpcodesByArgs("PUSH",  args,NULL);
				assert(strOpcodeTemplateSize(find)!=0);
		}
		{
				//IMM8
				__auto_type reg=X86AddrModeSint(2);
				strX86AddrMode args CLEANUP(strX86AddrModeDestroy)=strX86AddrModeAppendItem(NULL, reg);
				strOpcodeTemplate  find CLEANUP(strOpcodeTemplateDestroy)=X86OpcodesByArgs("PUSH",  args,NULL);
				assert(strOpcodeTemplateSize(find)!=0);
		}
		{
				//IMM16
				__auto_type reg=X86AddrModeSint(256);
				strX86AddrMode args CLEANUP(strX86AddrModeDestroy)=strX86AddrModeAppendItem(NULL, reg);
				strOpcodeTemplate  find CLEANUP(strOpcodeTemplateDestroy)=X86OpcodesByArgs("PUSH",  args,NULL);
				assert(strOpcodeTemplateSize(find)!=0);
		}
		{
				//IMM32
				__auto_type reg=X86AddrModeSint(INT16_MAX+1ll);
				strX86AddrMode args CLEANUP(strX86AddrModeDestroy)=strX86AddrModeAppendItem(NULL, reg);
				strOpcodeTemplate  find CLEANUP(strOpcodeTemplateDestroy)=X86OpcodesByArgs("PUSH",  args,NULL);
				assert(strOpcodeTemplateSize(find)!=0);
		}
		{
				//R32
				__auto_type reg=X86AddrModeReg(&regX86EDX);
				strX86AddrMode args CLEANUP(strX86AddrModeDestroy)=strX86AddrModeAppendItem(NULL, reg);
				strOpcodeTemplate  find CLEANUP(strOpcodeTemplateDestroy)=X86OpcodesByArgs("PUSH",  args,NULL);
				assert(strOpcodeTemplateSize(find)!=0);
		}
		{
				//STI
				__auto_type reg=X86AddrModeReg(&regX86ST3);
				strX86AddrMode args CLEANUP(strX86AddrModeDestroy)=strX86AddrModeAppendItem(NULL, reg);
				strOpcodeTemplate  find CLEANUP(strOpcodeTemplateDestroy)=X86OpcodesByArgs("FLD",  args,NULL);
				assert(strOpcodeTemplateSize(find)!=0);
		}
		{
				//M64
				__auto_type indir=X86AddrModeIndirReg(&regAMD64RAX, &typeI64i);
				strX86AddrMode args CLEANUP(strX86AddrModeDestroy)=strX86AddrModeAppendItem(NULL, indir);
				strOpcodeTemplate  find CLEANUP(strOpcodeTemplateDestroy)=X86OpcodesByArgs("FSTP",  args,NULL);
				assert(strOpcodeTemplateSize(find)!=0);
		}
		{
				//M32
				__auto_type reg=X86AddrModeIndirReg(&regAMD64RAX, &typeI32i);
				strX86AddrMode args CLEANUP(strX86AddrModeDestroy)=strX86AddrModeAppendItem(NULL, reg);
				strOpcodeTemplate  find CLEANUP(strOpcodeTemplateDestroy)=X86OpcodesByArgs("FSTP",  args,NULL);
				assert(strOpcodeTemplateSize(find)!=0);
		}
		{
				//M16
				__auto_type mem=X86AddrModeIndirReg(&regAMD64RAX, &typeI16i);
				__auto_type st0=X86AddrModeReg(&regX86ST0);
				strX86AddrMode args CLEANUP(strX86AddrModeDestroy)=strX86AddrModeAppendItem(NULL, st0);
				args=strX86AddrModeAppendItem(args, mem);
				strOpcodeTemplate find CLEANUP(strOpcodeTemplateDestroy)=X86OpcodesByArgs("FIMUL",  args,NULL);
				assert(strOpcodeTemplateSize(find)!=0);
		}
		{
				//SREG
				__auto_type reg=X86AddrModeReg(&regX86AX);
				__auto_type sreg=X86AddrModeReg(&regX86ES);
				strX86AddrMode args CLEANUP(strX86AddrModeDestroy)=strX86AddrModeAppendItem(NULL, reg);
				args=strX86AddrModeAppendItem(args, sreg);
				strOpcodeTemplate  find CLEANUP(strOpcodeTemplateDestroy)=X86OpcodesByArgs("MOV",  args,NULL);
				assert(strOpcodeTemplateSize(find)!=0);
		}
		{
				//RM8 and R8
				__auto_type rm8=X86AddrModeIndirReg(&regAMD64RAX, &typeI8i);
				__auto_type r8=X86AddrModeReg(&regX86DL);
				strX86AddrMode args CLEANUP(strX86AddrModeDestroy)=strX86AddrModeAppendItem(NULL, rm8);
				args=strX86AddrModeAppendItem(args, r8);
				strOpcodeTemplate  find CLEANUP(strOpcodeTemplateDestroy)=X86OpcodesByArgs("MOV",  args,NULL);
				assert(strOpcodeTemplateSize(find)!=0);
		}
		{
				//UIMM8
				__auto_type uimm=X86AddrModeSint(255);
				__auto_type r8=X86AddrModeReg(&regX86AL);
				strX86AddrMode args CLEANUP(strX86AddrModeDestroy)=strX86AddrModeAppendItem(NULL, r8);
				args=strX86AddrModeAppendItem(args, uimm);
				strOpcodeTemplate  find CLEANUP(strOpcodeTemplateDestroy)=X86OpcodesByArgs("XOR",  args,NULL);
				assert(strOpcodeTemplateSize(find)!=0);
		}
}
