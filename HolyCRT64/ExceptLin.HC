extern U0 printf(U8i*,...);
class HCRT_Except {
						HCRT_Except *parent;
						I64i rRAX,rRBX,rRCX,rRDX,rRBP,rRSP,rRSI,rRDI;
						I64i rR8,rR9,rR10,rR11,rR12,rR13,rR14,rR15;
						F64 rXMM0,rXMM1,rXMM2,rXMM3,rXMM4,rXMM5,rXMM6,rXMM7;
						U0 *rPC;
						U64i msg;
};
static I64i HCRT_ExceptSize=sizeof(HCRT_Except);
import U0 *malloc(I32i len);
import U0 *realloc(U0 *ptr,I32i len);
import U0 free(U0 *ptr);
extern U0 abort();
static HCRT_Except *curExceptFrame=0;
static HCRT_Except tmpExcept;
static HCRT_Except *HCRT_ExceptMAlloc();

asm {
HCRT_ExceptStoreState::
IMPORT tmpExcept;

MOVSD2 F64[tmpExcept.rXMM0],XMM0
MOVSD2 F64[tmpExcept.rXMM1],XMM1
MOVSD2 F64[tmpExcept.rXMM2],XMM2
MOVSD2 F64[tmpExcept.rXMM3],XMM3
MOVSD2 F64[tmpExcept.rXMM4],XMM4
MOVSD2 F64[tmpExcept.rXMM5],XMM5
MOVSD2 F64[tmpExcept.rXMM6],XMM6
MOVSD2 F64[tmpExcept.rXMM7],XMM7

MOV I64i [tmpExcept.rRAX],RAX
MOV I64i [tmpExcept.rRBX],RBX
MOV I64i [tmpExcept.rRCX],RCX
MOV I64i [tmpExcept.rRDX],RDX
MOV I64i [tmpExcept.rRBP],RBP
MOV I64i [tmpExcept.rRSI],RSI
MOV I64i [tmpExcept.rRDI],RDI
LEA RAX,[RSP]
MOV I64i [tmpExcept.rRSP],RAX
MOV RAX,[RSP]
MOV I64i [tmpExcept.rPC],RAX

CALL HCRT_ExceptMAlloc
RET
};
asm {
HCRT_ExceptRestoreState::
IMPORT tmpExcept;

MOVSD2 XMM0,F64[tmpExcept.rXMM0]
MOVSD2 XMM1,F64[tmpExcept.rXMM1]
MOVSD2 XMM2,F64[tmpExcept.rXMM2]
MOVSD2 XMM3,F64[tmpExcept.rXMM3]
MOVSD2 XMM4,F64[tmpExcept.rXMM4]
MOVSD2 XMM5,F64[tmpExcept.rXMM5]
MOVSD2 XMM6,F64[tmpExcept.rXMM6]
MOVSD2 XMM7,F64[tmpExcept.rXMM7]

MOV  RAX,[tmpExcept.rRAX]
MOV RBX,[tmpExcept.rRBX]
MOV RCX,[tmpExcept.rRCX]
MOV RDX,[tmpExcept.rRDX]
MOV RBP,[tmpExcept.rRBP]
MOV RSI,[tmpExcept.rRSI]
MOV RDI,[tmpExcept.rRDI]
MOV RSP, [tmpExcept.rRSP]
MOV RBP, [tmpExcept.rRBP]
JMP [tmpExcept.rPC]
};
U0 HCRT_ExceptRestoreState();

static HCRT_Except *HCRT_ExceptMAlloc() {
							 HCRT_Except *frame=malloc(sizeof(HCRT_Except));
								*frame=tmpExcept;						
								frame->parent=curExceptFrame;
								curExceptFrame=frame;
								return frame;
}
U0 throw(I32i msg) {
			if (curExceptFrame==0) {
						printf("No exception frame to jump to,aborting\n");
						abort();
			}
			curExceptFrame->msg=msg;
			HCRT_ExceptRestoreState();
}
U0 HCRT_ExceptPop() {
			HCRT_Except *par=curExceptFrame->parent;
			free(curExceptFrame);
			curExceptFrame=par;
}
extern I32i signal(I32i sig,U0(*func)(I32i));

//From musl libc
#define SIG_BLOCK     0
#define SIG_UNBLOCK   1
#define SIG_SETMASK   2
class sigset_t {
						U8i bytes[128]; //musl libc alltypes.h.in
};
extern U0 sigemptyset (sigset_t *x);
extern U0 sigaddset (sigset_t *x,I32i code);
extern I32i sigprocmask(I32i,sigset_t *set,sigset_t *old);
static U0 *throwSegfault(I32i num) {
							sigset_t set;
							sigemptyset(&set);
							sigaddset(&set,11);
							sigprocmask(SIG_UNBLOCK,&set,0);
							printf("SEGFLT\n");
							throw(num);
}
signal(11,&throwSegfault);