I32i union I32 {
					U8i u8[4];
					U16i u16[2];
};
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
							I32i count=0;
							I32i num2=num;
							do {
										num2/=10;
										count+=1;
							} while(num2!=0);
							U8i buffer[count];
							I32i count2=count;
							do {
										I32i d=num%10;
										num/=10;
										count-=1;
										buffer[count]=digits[d];
							} while(num!=0);
							for(I32i c=0;c!=count2;c++)
																PutChr(buffer[c]);
																PutChr(10);
}
PrintI32i_10(123000);
I32 test=0x11223344;
PrintI32i_10(0x11223344);
PrintI32i_10(test.u8[0]);
PrintI32i_10(test.u8[1]);
PrintI32i_10(test.u8[2]);
PrintI32i_10(test.u8[3]);
MOV EAX,1
MOV EBX,1
INT 0x80
