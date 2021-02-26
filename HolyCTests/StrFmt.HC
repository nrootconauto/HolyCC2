U0 PutChr(U8i chr) {
	asm {
		IMPORT chr;
		MOV EAX,4
		MOV EBX,1
		LEA ECX, [chr]
		MOV EDX,1
		INT 0x80
	}
}
static U0 PrintI32i_10(I32i num) {
							U8i *digits="0123456789";
							do {
										I32i d=num%10;
										PutChr(digits[d]);
										num/=10;
							} while(num!=0);
}
PrintI32i_10(123);
MOV EAX,1
MOV EBX,1
INT 0x80
