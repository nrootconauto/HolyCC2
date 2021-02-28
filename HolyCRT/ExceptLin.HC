class HCRT_Except {
 I32i rEAX,rEBX,rECX,rEDX,rEBP,rESP,rESI,rEDI;
	I32i rFlags;
	I32i rPC;
	HCRT_Except *parent;
};
static I32i HCRT_ExceptSize=sizeof(HCRT_Except);
import U0 *malloc(I32i len);


static HCRT_Except *curExceptFrame;
static HCRT_Except tmpExcept;
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
	MOV I32i [tmpExcept.rEBP],EAX
	
	RET
};
extern U0 HCRT_ExceptStoreState();
static HCRT_Except *HCRT_ExceptMAlloc() {
							 HCRT_Except *frame=malloc(sizeof(HCRT_Except));
								HCRT_ExceptStoreState();
								*frame=tmpExcept;						
								frame->parent=curExceptFrame;
								curExceptFrame=frame;
								return frame;
}
import U0 printf(U8i fmt,...);
U0 main() {
			printf("Hello World,%li\n",32);
			return;
}