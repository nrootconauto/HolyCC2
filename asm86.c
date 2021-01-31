#include <asm86.h>
#include <hashTable.h>
#include <hashTable.h>
#include <lexer.h>
#include <registers.h>
#include <stdarg.h>
struct AsmX86Opcode {
	const char *name;
	strAsmX86AddrMode args;
};
STR_TYPE_DEF(struct AsmX86Opcode, X86Opcode);
STR_TYPE_FUNCS(struct AsmX86Opcode, X86Opcode);
MAP_TYPE_DEF(strX86Opcode, X86Opcodes);
MAP_TYPE_FUNCS(strX86Opcode, X86Opcodes);
static mapX86Opcodes X86Opcodes;
static void addOpcode(struct AsmX86Opcode op) {
loop:;
	__auto_type find = mapX86OpcodesGet(X86Opcodes, op.name);
	if (!find) {
		mapX86OpcodesInsert(X86Opcodes, op.name, NULL);
		goto loop;
	}
	*find = strX86OpcodeAppendItem(*find, op);
}
int ASMX86IsOpcode(const char *text) {
	char buffer[strlen(text) + 1];
	strcpy(buffer, text);
	for (long i = 0; i != strlen(text); i++)
		if (text[i] >= 'a' || text[i] <= 'z')
			buffer[i] = text[i] - 'a' + 'A';
	return NULL != mapX86OpcodesGet(X86Opcodes, text);
}
void ASMX86Init() {
	X86Opcodes = mapX86OpcodesCreate();
	const char *opcodeNames[] = {
	    "ADD",  "PUSH", "POP",   "OR",   "ADC",    "SBB",    "AND",   "DAA",    "SUB",    "DAS",  "XOR",    "AAA",     "CMP",   "PUSHA", "POPA",  "BOUND",  "ARPL",    "IMUL",   "INS",     "OUTS",    "JO",     "JNO",   "JB",    "JNAE",   "JC",    "JNB",     "JAE",     "JNC",    "JE",   "JZ",   "JNZ",   "JNE",    "JNBE",  "JA",     "JBE",   "JNA",     "JS",      "JNS",    "JP",    "JPE",    "JNP",     "JNPE",  "JNL",    "JGE",  "JLE",     "JNGE",   "TEST",    "XCHG", "MOV",     "LEA",   "CBW",     "CWDE",   "CWD",   "CDQ",    "FWAIT", "WAIT",  "PUSHF",  "POPF",   "SAHF",  "LAHF",   "MOVS",  "CMPS",  "STOS",   "LODS",   "SCAS",    "ROR",     "ROL",    "RCR",    "RCL",  "SHR",     "SHL",   "SAR",    "SAL",    "RET",    "LES",   "LDS",   "ENTER", "LEAVE", "INT",   "IRET", "AAM", "AMX",  "AAD", "ADX",  "SALC",  "SETALC", "XLAT", "FADD", "FMUL", "FCOM", "FCOMP", "FSUB", "FSUBP", "FSUBR", "FDIV", "FDIVR", "FLD", "FXCHG", "FST", "FLDENV", "FCHS", "FABS",
	    "FTST", "FXAM", "FLDCW", "FLD1", "FLDL2T", "FLDL2E", "FLDPI", "FLDLG2", "FLDLN2", "FLDZ", "FSTENV", "FNSTENV", "F2XM1", "FYL2X", "FPTAN", "FPATAN", "FXTRACT", "FPREM1", "FDECSTP", "FINCSTP", "FNSTCW", "FSTCW", "FPREM", "FL2XP1", "FSQRT", "FSINCOS", "FRNDINT", "FSCALE", "FSIN", "FCOS", "FIADD", "FCMOVB", "FIMUL", "FSMOVE", "FICOM", "FSMOVBE", "FICMOMP", "FCMOVU", "FISUB", "FISUBR", "FUCOMPP", "FIDIV", "FIDIVR", "FILD", "FCMOVNB", "FISTTP", "FCOMVNE", "FIST", "FUCOMBE", "FISTP", "FCOMVNU", "FNCLEX", "FCLEX", "FNINIT", "FINIT", "FFREE", "FRSTOR", "FNSAVE", "FSAVE", "FNSTSW", "FSTSW", "FMULP", "FICOMP", "FSUBRP", "FISUBRP", "FIDIVRP", "FIDIVP", "FFREEP", "FBLD", "FUCOMIP", "FBSTP", "FCOMIP", "LOOPNZ", "LOOPNE", "LOOPZ", "LOOPE", "LOOP",  "JCXZ",  "JECXZ", "IN",   "OUT", "CALL", "JMP", "LOCK", "REPNZ", "REPNE",  "REP",  "HLT",  "CMC",  "NOT",  "NEG",   "MUL",  "IDIV",  "CLC",   "STC",  "CLI",   "STI", "CLD",   "STD", "INC",    "DEC",
	};
	long count = sizeof(opcodeNames) / sizeof(opcodeNames);
	for (long i = 0; i != count; i++)
		mapX86OpcodesInsert(X86Opcodes, opcodeNames[i], NULL);
}
