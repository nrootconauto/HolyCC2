class HCRT_Except {
 I32i rEAX,rEBX,ECX,rEDX,rEBP,rESP,rESI,rEDI;
	I32i rFlags;
	I32i rPC;
	HCRT_Except *parent;
};
static I32i HCRT_ExceptSize=sizeof(HCRT_Except);
extern U0 *malloc(I32i len);

static I32i rEAX,rEBX,ECX,rEDX,rEBP,rESP,rESI,rEDI;
static	I32i rFlags;
static I32i rPC;
static HCRT_Except *curExceptFrame;
extern U0 HCRT_ExceptStoreState();
static HCRT_Except *HCRT_ExceptMAlloc() {
							 HCRT_Except *frame=malloc(sizeof(HCRT_Except));
								HCRT_ExceptStoreState();
								frame->rEAX=rEAX;
								frame->rEBX=rEBX;
								frame->rECX=ECX;
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
asm {
HCRT_ExceptStoreState::
	IMPORT rEAX,rEBX,ECX,rEDX,rEBP,rESP,rESI,rEDI;
	IMPORT rFlags,rPC;

 MOV [&rEAX],EAX
	
 MOV EAX, [ESP] //Load old program pointer
	MOV [&rPC],EAX
	
	PUSHFD
	POP EAX

 MOV [&rFlags],EAX
	MOV [&rEBX],EBX
	MOV [&rECX],ECX
	MOV [&rEDX],EDX
	MOV [&rEBP],EBP
	MOV [&rESI],ESI
	MOV [&rEDI],EDI
	LEA EAX,[ESP+4]
	MOV [&rSBP],EAX
	
	RET
};
