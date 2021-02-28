class HCRT_Except {
 I32i rEAX,rEBX,rECX,rEDX,rEBP,rESP,rESI,rEDI;
	I32i rFlags;
	I32i rPC;
	HCRT_Except *parent;
};
static I32i HCRT_ExceptSize=sizeof(HCRT_Except);
import U0 *malloc(I32i len);

static I32i rEAX,rEBX,rECX,rEDX,rEBP,rESP,rESI,rEDI;
static	I32i rFlags;
static I32i rPC;
static HCRT_Except *curExceptFrame;
asm {
HCRT_ExceptStoreState::
	IMPORT rEAX,rEBX,rECX,rEDX,rEBP,rESP,rESI,rEDI;
	IMPORT rFlags,rPC;

 MOV I32i [rEAX],EAX
	
 MOV EAX, [ESP] //Load old program pointer
	MOV I32i [rPC],EAX
	
	PUSHFD
	POP EAX

 MOV I32i [rFlags],EAX
	MOV I32i [rEBX],EBX
	MOV I32i [rECX],ECX
	MOV I32i [rEDX],EDX
	MOV I32i [rEBP],EBP
	MOV I32i [rESI],ESI
	MOV I32i [rEDI],EDI
	LEA EAX, 4[ESP]
	MOV I32i [rEBP],EAX
	
	RET
};
extern U0 HCRT_ExceptStoreState();
static HCRT_Except *HCRT_ExceptMAlloc() {
							 HCRT_Except *frame=malloc(sizeof(HCRT_Except));
								HCRT_ExceptStoreState();
								frame->rEAX=rEAX;
								frame->rEBX=rEBX;
								frame->rECX=rECX;
								frame->rEDX=rEDX;
								frame->rEBP=rEBP;
								frame->rESP=rESP;
								frame->rESI=rESI;
								frame->rEDI=rEDI;
								frame->rFlags=rFlags;
								frame->rPC=rPC;
								frame->parent=curExceptFrame;
								curExceptFrame=frame;
								return frame;
}
import U0 printf(U8i fmt,...);
U0 main() {
			printf("Hello World,%li\n",32);
			return;
}