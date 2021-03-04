asm {
				PUSHAD
				MOV EAX,4
				MOV EBX,1
				MOV ECX,"Hello World\n"
				MOV EDX,12
				INT 0x80
				POPAD

				MOV	EAX,1
				MOV EBX,1
				INT 0x80
}