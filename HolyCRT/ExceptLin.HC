class HCRT_Except {
 I32i rEAX,rEBX,rECX,rEDX,rEBP,rESP,rESI,rEDI;
	I32i rFlags;
	I32i rPC;
	HCRT_Except *parent;
	I32i msg;
};
static I32i HCRT_ExceptSize=sizeof(HCRT_Except);
import U0 *malloc(I32i len);
import U0 *realloc(U0 *ptr,I32i len);
import U0 free(U0 *ptr);

static HCRT_Except *curExceptFrame;
static HCRT_Except tmpExcept;
static HCRT_Except *HCRT_ExceptMAlloc();
asm {
HCRT_ExceptStoreState::
	IMPORT tmpExcept;

 MOV I32i [tmpExcept.rEAX],EAX
	
 MOV EAX, [ESP] //Load old program pointer
	MOV I32i [tmpExcept.rPC],EAX
	
	PUSHFD
	POP EAX

 MOV I32i [tmpExcept.rFlags],EAX
	MOV I32i [tmpExcept.rEBX],EBX
	MOV I32i [tmpExcept.rECX],ECX
	MOV I32i [tmpExcept.rEDX],EDX
	MOV I32i [tmpExcept.rEBP],EBP
	MOV I32i [tmpExcept.rESI],ESI
	MOV I32i [tmpExcept.rEDI],EDI
	LEA EAX, 4[ESP]
	MOV I32i [tmpExcept.rESP],EAX
	
	CALL HCRT_ExceptMAlloc
	RET
};
static HCRT_Except *HCRT_ExceptPop() {
							tmpExcept=*curExceptFrame;
							HCRT_Except *old=curExceptFrame;
							curExceptFrame=curExceptFrame->parent;
							free(old);
}


U0 HCRT_ExceptRestoreState();
U0 BAAAR() {};
asm {
HCRT_ExceptRestoreState::
	IMPORT tmpExcept;

 MOV EAX,I32i [tmpExcept.rEAX]
	MOV EBX,I32i [tmpExcept.rEBX]
	MOV ECX,I32i [tmpExcept.rECX]
	MOV EDX,I32i [tmpExcept.rEDX]
	MOV ESI,I32i [tmpExcept.rESI]
	MOV EDI,I32i [tmpExcept.rEDI]
	MOV EBP,I32i [tmpExcept.rEBP]
	MOV ESP,I32i [tmpExcept.rESP]
	JMP I32i [tmpExcept.rPC]
};
static HCRT_Except *HCRT_ExceptMAlloc() {
							 HCRT_Except *frame=malloc(sizeof(HCRT_Except));
								*frame=tmpExcept;						
								frame->parent=curExceptFrame;
								curExceptFrame=frame;
								return frame;
}
U0 throw(I32i msg) {
			curExceptFrame->msg=msg;
			HCRT_ExceptRestoreState();
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
static U0 *throwSegfault(I32i num,I32i code) {
							sigset_t set;
							sigemptyset(&set);
							sigaddset(&set,11);
							sigprocmask(SIG_UNBLOCK,&set,0);
							throw(code);
}
signal(11,&throwSegfault);