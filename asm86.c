#include <hashTable.h>
#include <lexer.h>
#include <registers.h>
#include <stdarg.h>
#include<hashTable.h>
struct AsmX86AddrMode {
		enum {
				ASM_X86_AM_IMM_SINT,
				ASM_X86_AM_IMM_UINT,
				ASM_X86_AM_IMM_FLT,
				ASM_X86_AM_REG_GP,
				ASM_X86_AM_REG,
				ASM_X86_AM_INDIR,
				ASM_X86_AM_INDIR_REG,
				ASM_X86_AM_INDIR_INDEX,
		} type;
		union {
				struct lexerFloating fltLit;
				uint64_t uint;
				int64_t sint;
				struct reg *reg;
				struct {
						struct reg *base;
						struct reg *index; 
						int scale;
						int64_t offset;
				} indirect;
		};
};
#define X86_AM_SINT(value) (struct AsmX86AddrMode){.type=ASM_X86_AM_IMM_SINT,.sint=value}
#define X86_AM_UINT(value) (struct AsmX86AddrMode){.type=ASM_X86_AM_IMM_UINT,.uint=value}
#define X86_AM_FLT(value) (struct AsmX86AddrMode){.type=ASM_X86_AM_IMM_FLT,.fltLit=value}
#define X86_AM_GP (struct AsmX86AddrMode){.type=ASM_X86_AM_REG_GP}
#define X86_AM_REG(reg) (struct AsmX86AddrMode){.type=ASM_X86_AM_REG,.reg=&reg}
#define X86_AM_INDIR(addr) (struct AsmX86AddrMode){.type=ASM_X86_AM_INDIR,.indirect.offset=addr}
#define X86_AM_INDIR_REG(reg) (struct AsmX86AddrMode){.type=ASM_X86_AM_INDIR_REG,.indirect.base=&reg}
#define X86_AM_INDIR_INDEX(base,index,scale) (struct AsmX86AddrMode){.type=ASM_X86_AM_INDIR_INDEX,.indirect.base=&base,.indirect.index=&index,.indirect.scale=scale,.indirect.offset=0}
#define X86_AM_INDIR_INDEX_OFFSET(base,index,scale,off) (struct AsmX86AddrMode){.type=ASM_X86_AM_INDIR_INDEX,.indirect.base=&base,.indirect.index=&index,.indirect.scale=scale,.indirect.offset=off}
STR_TYPE_DEF(struct AsmX86AddrMode ,AsmX86AddrMode);
STR_TYPE_FUNCS(struct AsmX86AddrMode ,AsmX86AddrMode)
struct AsmX86Opcode {
		const char *name;
		strAsmX86AddrMode args;
};

enum __AsmX86Opcode {
		ASM_X86_OPC_ADD,
		ASM_X86_OPC_PUSH,
		ASM_X86_OPC_POP,
		ASM_X86_OPC_OR,
		ASM_X86_OPC_ADC,
		ASM_X86_OPC_SBB,
		ASM_X86_OPC_AND,
		ASM_X86_OPC_DAA,
		ASM_X86_OPC_SUB,
		ASM_X86_OPC_DAS,
		ASM_X86_OPC_XOR,
		ASM_X86_OPC_AAA,
		ASM_X86_OPC_CMP,
		ASM_X86_OPC_PUSHA,
		ASM_X86_OPC_POPA ,
		ASM_X86_OPC_BOUND,
		ASM_X86_OPC_ARPL,
		ASM_X86_OPC_IMUL,
		ASM_X86_OPC_INS,
		ASM_X86_OPC_OUTS,
		ASM_X86_OPC_JO,
		ASM_X86_OPC_JNO,
		ASM_X86_OPC_JB,
		ASM_X86_OPC_JNAE,
		ASM_X86_OPC_JC,
		ASM_X86_OPC_JNB,
		ASM_X86_OPC_JAE,
		ASM_X86_OPC_JNC,
		ASM_X86_OPC_JE,
		ASM_X86_OPC_JZ,
		ASM_X86_OPC_JNZ,
		ASM_X86_OPC_JNE,
		ASM_X86_OPC_JNBE,
		ASM_X86_OPC_JA,
		ASM_X86_OPC_JBE,
		ASM_X86_OPC_JNA,
		ASM_X86_OPC_JS,
		ASM_X86_OPC_JNS,
		ASM_X86_OPC_JP,
		ASM_X86_OPC_JPE,
		ASM_X86_OPC_JNP,
		ASM_X86_OPC_JNPE,
		ASM_X86_OPC_JNL,
		ASM_X86_OPC_JGE,
		ASM_X86_OPC_JLE,
		ASM_X86_OPC_JNGE,
		ASM_X86_OPC_TEST,
		ASM_X86_OPC_XCHG,
		ASM_X86_OPC_MOV,
		ASM_X86_OPC_LEA,
		ASM_X86_OPC_CBW,
		ASM_X86_OPC_CWDE,
		ASM_X86_OPC_CWD,
		ASM_X86_OPC_CDQ,
		ASM_X86_OPC_FWAIT,
		ASM_X86_OPC_WAIT,
		ASM_X86_OPC_PUSHF,
		ASM_X86_OPC_POPF,
		ASM_X86_OPC_SAHF,
		ASM_X86_OPC_LAHF,
		ASM_X86_OPC_MOVS,
		ASM_X86_OPC_CMPS,
		ASM_X86_OPC_STOS,
		ASM_X86_OPC_LODS,
		ASM_X86_OPC_SCAS,
		ASM_X86_OPC_ROR,
		ASM_X86_OPC_ROL,
		ASM_X86_OPC_RCR,
		ASM_X86_OPC_RCL,
		ASM_X86_OPC_SHR,
		ASM_X86_OPC_SHL,
		ASM_X86_OPC_SAR,
		ASM_X86_OPC_SAL,
		ASM_X86_OPC_RET,
		ASM_X86_OPC_LES,
		ASM_X86_OPC_LDS,
		ASM_X86_OPC_ENTER,
		ASM_X86_OPC_LEAVE,
		ASM_X86_OPC_INT,
		ASM_X86_OPC_IRET,
		ASM_X86_OPC_AAM,
		ASM_X86_OPC_AMX,
		ASM_X86_OPC_AAD,
		ASM_X86_OPC_ADX,
		ASM_X86_OPC_SALC,
		ASM_X86_OPC_SETALC,
		ASM_X86_OPC_XLAT,
		ASM_X86_OPC_FADD,
		ASM_X86_OPC_FMUL,
		ASM_X86_OPC_FCOM,
		ASM_X86_OPC_FCOMP,
		ASM_X86_OPC_FSUB,
		ASM_X86_OPC_FSUBP,
		ASM_X86_OPC_FSUBR,
		ASM_X86_OPC_FDIV,
		ASM_X86_OPC_FDIVR,
		ASM_X86_OPC_FLD,
		ASM_X86_OPC_FXCHG,
		ASM_X86_OPC_FST,
		ASM_X86_OPC_FLDENV,
		ASM_X86_OPC_FCHS,
		ASM_X86_OPC_FABS,
		ASM_X86_OPC_FTST,
		ASM_X86_OPC_FXAM,
		ASM_X86_OPC_FLDCW,
		ASM_X86_OPC_FLD1,
		ASM_X86_OPC_FLDL2T,
		ASM_X86_OPC_FLDL2E,
		ASM_X86_OPC_FLDPI,
		ASM_X86_OPC_FLDLG2,
		ASM_X86_OPC_FLDLN2,
		ASM_X86_OPC_FLDZ,
		ASM_X86_OPC_FSTENV,
		ASM_X86_OPC_FNSTENV,
		ASM_X86_OPC_F2XM1,
		ASM_X86_OPC_FYL2X,
		ASM_X86_OPC_FPTAN,
		ASM_X86_OPC_FPATAN,
		ASM_X86_OPC_FXTRACT,
		ASM_X86_OPC_FPREM1,
		ASM_X86_OPC_FDECSTP,
		ASM_X86_OPC_FINCSTP,
		ASM_X86_OPC_FNSTCW,
		ASM_X86_OPC_FSTCW,
		ASM_X86_OPC_FPREM,
		ASM_X86_OPC_FL2XP1,
		ASM_X86_OPC_FSQRT,
		ASM_X86_OPC_FSINCOS,
		ASM_X86_OPC_FRNDINT,
		ASM_X86_OPC_FSCALE,
		ASM_X86_OPC_FSIN,
		ASM_X86_OPC_FCOS,
		ASM_X86_OPC_FIADD,
		ASM_X86_OPC_FCMOVB,
		ASM_X86_OPC_FIMUL,
		ASM_X86_OPC_FSMOVE,
		ASM_X86_OPC_FICOM,
		ASM_X86_OPC_FSMOVBE,
		ASM_X86_OPC_FICMOMP,
		ASM_X86_OPC_FCMOVU,
		ASM_X86_OPC_FISUB,
		ASM_X86_OPC_FISUBR,
		ASM_X86_OPC_FUCOMPP,
		ASM_X86_OPC_FIDIV,
		ASM_X86_OPC_FIDIVR,
		ASM_X86_OPC_FILD,
		ASM_X86_OPC_FCMOVNB,
		ASM_X86_OPC_FISTTP,
		ASM_X86_OPC_FCOMVNE,
		ASM_X86_OPC_FIST,
		ASM_X86_OPC_FUCOMBE,
		ASM_X86_OPC_FISTP,
		ASM_X86_OPC_FCOMVNU,
		ASM_X86_OPC_FNCLEX,
		ASM_X86_OPC_FCLEX,
		ASM_X86_OPC_FNINIT,
		ASM_X86_OPC_FINIT,
		ASM_X86_OPC_FFREE,
		ASM_X86_OPC_FRSTOR,
		ASM_X86_OPC_FNSAVE,
		ASM_X86_OPC_FSAVE,
		ASM_X86_OPC_FNSTSW,
		ASM_X86_OPC_FSTSW,
		ASM_X86_OPC_FMULP,
		ASM_X86_OPC_FICOMP,
		ASM_X86_OPC_FSUBRP,
		ASM_X86_OPC_FISUBRP,
		ASM_X86_OPC_FIDIVRP,
		ASM_X86_OPC_FIDIVP,
		ASM_X86_OPC_FFREEP,
		ASM_X86_OPC_FBLD,
		ASM_X86_OPC_FUCOMIP,
		ASM_X86_OPC_FBSTP,
		ASM_X86_OPC_FCOMIP,
		ASM_X86_OPC_LOOPNZ,
		ASM_X86_OPC_LOOPNE,
		ASM_X86_OPC_LOOPZ,
		ASM_X86_OPC_LOOPE,
		ASM_X86_OPC_LOOP,
		ASM_X86_OPC_JCXZ,
		ASM_X86_OPC_JECXZ,
		ASM_X86_OPC_IN,
		ASM_X86_OPC_OUT,
		ASM_X86_OPC_CALL,
		ASM_X86_OPC_JMP,
		ASM_X86_OPC_LOCK,
		ASM_X86_OPC_REPNZ,
		ASM_X86_OPC_REPNE,
		ASM_X86_OPC_REP,
		ASM_X86_OPC_HLT,
		ASM_X86_OPC_CMC,
		ASM_X86_OPC_NOT,
		ASM_X86_OPC_NEG,
		ASM_X86_OPC_MUL,
		ASM_X86_OPC_IDIV,
		ASM_X86_OPC_CLC,
		ASM_X86_OPC_STC,
		ASM_X86_OPC_CLI,
		ASM_X86_OPC_STI,
		ASM_X86_OPC_CLD,
		ASM_X86_OPC_STD,
		ASM_X86_OPC_INC,
		ASM_X86_OPC_DEC,
};
static struct AsmX86Opcode createOpcode(const char *name,long argc,...) {
		struct AsmX86Opcode opc;
		opc.name=name;
		opc.args=strAsmX86AddrModeResize(NULL, argc);
		va_list args;
		va_start(args, argc);
		for(long i=0;i!=argc;i++) {
				opc.args[i]=va_arg(args, struct AsmX86AddrMode);
		}
		va_end(args);
		return opc;
}
STR_TYPE_DEF(struct AsmX86Opcode,X86Opcode);
STR_TYPE_FUNCS(struct AsmX86Opcode,X86Opcode);
MAP_TYPE_DEF(strX86Opcode, X86Opcodes);
MAP_TYPE_FUNCS(strX86Opcode, X86Opcodes);
static mapX86Opcodes X86Opcodes;
static void addOpcode(struct AsmX86Opcode op) {
	loop:;
		__auto_type find=mapX86OpcodesGet(X86Opcodes, op.name);
		if(!find ) {
				mapX86OpcodesInsert(X86Opcodes , op.name, NULL);
				goto loop;
		}
		*find=strX86OpcodeAppendItem(*find, op);
}
void ASMX86Init() {
		X86Opcodes=mapX86OpcodesCreate();
		;
#define ADD_OPCODE_W_ARGS(name,...) {struct AsmX86AddrMode args[]={__VA_ARGS__};long argc=sizeof(args)/sizeof(*args);addOpcode(createOpcode(name, argc, __VA_ARGS__));}
		ADD_OPCODE_W_ARGS("ADD",X86_AM_);
		
}
