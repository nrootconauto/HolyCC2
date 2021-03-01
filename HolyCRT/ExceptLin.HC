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
	//
	// Reserve 4 bytes after stack from caller to make room for return address for throwing exception
	//
	MOV EAX, [ESP]
	PUSH EAX

	RET
};
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
	//
	// Eariler we made a spot with the return address(we pushed it) 
	//
	LEA ESP, -4[ESP]
	RET
};
extern U0 HCRT_ExceptRestoreState();
extern U0 HCRT_ExceptStoreState();
static HCRT_Except *HCRT_ExceptMAlloc() {
							 HCRT_Except *frame=malloc(sizeof(HCRT_Except));
								*frame=tmpExcept;						
								frame->parent=curExceptFrame;
								curExceptFrame=frame;
								return frame;
}
#define storeStore asm {CALL HCRT_ExceptRestoreState}
import U0 printf(U8i fmt,...);
U0 main() {
			asm {
							MOV EAX,1
							MOV EBX,2
							MOV ECX,3
							MOV EDX,4
							MOV ESI,5
							MOV EDI,6
			};
			I32i run=0;
			I32i *dummy=&run;
			storeStore
			if(run)
				return;
			printf("frame->EAX ,%li\n",curExceptFrame->rEAX);
			printf("frame->EBX ,%li\n",curExceptFrame->rEBX);
			printf("frame->ECX ,%li\n",curExceptFrame->rECX);
			printf("frame->EDX ,%li\n",curExceptFrame->rEDX);
			printf("frame->ESI ,%li\n",curExceptFrame->rESI);
			printf("frame->EDI ,%li\n",curExceptFrame->rEDI);
			run=1;
			HCRT_ExceptRestoreState();
			return;
}