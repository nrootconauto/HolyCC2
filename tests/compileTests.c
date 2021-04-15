#include "../compile.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../registers.h"
#include "../commandLine.h"
static char *strDup(const char *text) {
		char *retVal=malloc(strlen(text)+1);
		return strcpy(retVal, text);
}
static char *text2File(const char *text) {
		char *name=strDup(tmpnam(NULL));
		FILE *f=fopen(name, "w");
		fwrite(text, strlen(text), 1, f);
		fclose(f);
		return name;
}
static void registerInterfereTestX64() {
		setArch(ARCH_X64_SYSV);
		{
				struct reg *a[]={&regAMD64RAX,&regX86EAX,&regX86AX,&regX86AH,&regX86AL};
				for(long r=0;r!=3;r++)
						for(long r2=0;r2!=5;r2++)
								assert(regConflict(a[r], a[r2]));
				assert(!regConflict(a[3], a[4]));
				for(long r=3;r!=5;r++)
						for(long r2=0;r2!=3;r2++)
								assert(regConflict(a[r], a[r2]));
		}
		{
				struct reg *b[]={&regAMD64RBX,&regX86EBX,&regX86BX,&regX86BH,&regX86BL};
				for(long r=0;r!=3;r++)
						for(long r2=0;r2!=5;r2++)
								assert(regConflict(b[r], b[r2]));
				assert(!regConflict(b[3], b[4]));
				for(long r=3;r!=5;r++)
						for(long r2=0;r2!=3;r2++)
								assert(regConflict(b[r], b[r2]));
		}
		{
				struct reg *c[]={&regAMD64RCX,&regX86ECX,&regX86CX,&regX86CH,&regX86CL};
				for(long r=0;r!=3;r++)
						for(long r2=0;r2!=5;r2++)
								assert(regConflict(c[r], c[r2]));
				assert(!regConflict(c[3], c[4]));
				for(long r=3;r!=5;r++)
						for(long r2=0;r2!=3;r2++)
								assert(regConflict(c[r], c[r2]));
		}
		{
				struct reg *d[]={&regAMD64RDX,&regX86EDX,&regX86DX,&regX86DH,&regX86DL};
				for(long r=0;r!=3;r++)
						for(long r2=0;r2!=5;r2++)
								assert(regConflict(d[r], d[r2]));
				assert(!regConflict(d[3], d[4]));
				for(long r=3;r!=5;r++)
						for(long r2=0;r2!=3;r2++)
								assert(regConflict(d[r], d[r2]));
		}
		{
				struct reg *sp[]={&regAMD64RSP,&regX86ESP,&regX86SP,&regX86SPL};
				for(long r=0;r!=4;r++)
						for(long r2=0;r2!=4;r2++)
								assert(regConflict(sp[r], sp[r2]));
		}
		{
				struct reg *bp[]={&regAMD64RBP,&regX86EBP,&regX86BP,&regX86BPL};
				for(long r=0;r!=4;r++)
						for(long r2=0;r2!=4;r2++)
								assert(regConflict(bp[r], bp[r2]));
		}
		{
				struct reg *si[]={&regAMD64RSI,&regX86ESI,&regX86SI,&regX86SIL};
				for(long r=0;r!=4;r++)
						for(long r2=0;r2!=4;r2++)
								assert(regConflict(si[r], si[r2]));
		}
		{
				struct reg *di[]={&regAMD64RDI,&regX86EDI,&regX86DI,&regX86DIL};
				for(long r=0;r!=4;r++)
						for(long r2=0;r2!=4;r2++)
								assert(regConflict(di[r], di[r2]));
		}
		{
				struct reg *di[]={&regAMD64R8u64,&regAMD64R8u32,&regAMD64R8u16,&regAMD64R8u8};
				for(long r=0;r!=4;r++)
						for(long r2=0;r2!=4;r2++)
								assert(regConflict(di[r], di[r2]));
		}
		{
				struct reg *r9[]={&regAMD64R9u64,&regAMD64R9u32,&regAMD64R9u16,&regAMD64R9u8};
				for(long r=0;r!=4;r++)
						for(long r2=0;r2!=4;r2++)
								assert(regConflict(r9[r], r9[r2]));
		}
		{
				struct reg *r10[]={&regAMD64R10u64,&regAMD64R10u32,&regAMD64R10u16,&regAMD64R10u8};
				for(long r=0;r!=4;r++)
						for(long r2=0;r2!=4;r2++)
								assert(regConflict(r10[r], r10[r2]));
		}
		{
				struct reg *r11[]={&regAMD64R11u64,&regAMD64R11u32,&regAMD64R11u16,&regAMD64R11u8};
				for(long r=0;r!=4;r++)
						for(long r2=0;r2!=4;r2++)
								assert(regConflict(r11[r], r11[r2]));
		}
		{
				struct reg *r12[]={&regAMD64R12u64,&regAMD64R12u32,&regAMD64R12u16,&regAMD64R12u8};
				for(long r=0;r!=4;r++)
						for(long r2=0;r2!=4;r2++)
								assert(regConflict(r12[r], r12[r2]));
		}
		{
				struct reg *r13[]={&regAMD64R13u64,&regAMD64R13u32,&regAMD64R13u16,&regAMD64R13u8};
				for(long r=0;r!=4;r++)
						for(long r2=0;r2!=4;r2++)
								assert(regConflict(r13[r], r13[r2]));
		}
		{
				struct reg *r14[]={&regAMD64R14u64,&regAMD64R14u32,&regAMD64R14u16,&regAMD64R14u8};
				for(long r=0;r!=4;r++)
						for(long r2=0;r2!=4;r2++)
								assert(regConflict(r14[r], r14[r2]));
		}
}
static void registerInterfereTestX86() {
		setArch(ARCH_X86_SYSV);
		struct reg *a[]={&regX86EAX,&regX86AX,&regX86AH,&regX86AL};
		for(long r=0;r!=2;r++)
				for(long r2=0;r2!=2;r2++)
						assert(regConflict(a[r], a[r2]));
		assert(!regConflict(a[2], a[3]));
		for(long r=0;r!=2;r++)
				for(long r2=2;r2!=4;r2++)
						assert(regConflict(a[r], a[r2]));
		
		struct reg *b[]={&regX86EBX,&regX86BX,&regX86BH,&regX86BL};
		for(long r=0;r!=2;r++)
				for(long r2=0;r2!=2;r2++)
						assert(regConflict(b[r], b[r2]));
		assert(!regConflict(b[2], b[3]));
		for(long r=0;r!=2;r++)
				for(long r2=2;r2!=4;r2++)
						assert(regConflict(b[r], b[r2]));
		
			struct reg *c[]={&regX86ECX,&regX86CX,&regX86CH,&regX86CL};
			for(long r=0;r!=2;r++)
				for(long r2=0;r2!=2;r2++)
						assert(regConflict(c[r], c[r2]));
			assert(!regConflict(c[2], c[3]));
		for(long r=0;r!=2;r++)
				for(long r2=2;r2!=4;r2++)
						assert(regConflict(c[r], c[r2]));
			
		struct reg *d[]={&regX86EDX,&regX86DX,&regX86DH,&regX86DL};
			for(long r=0;r!=2;r++)
				for(long r2=0;r2!=2;r2++)
						assert(regConflict(d[r], d[r2]));
			assert(!regConflict(d[2], d[3]));
			for(long r=0;r!=2;r++)
					for(long r2=2;r2!=4;r2++)
							assert(regConflict(d[r], d[r2]));

			struct reg *sp[]={&regX86ESP,&regX86SP};
			for(long r=0;r!=2;r++)
					for(long r2=0;r2!=2;r2++)
							assert(regConflict(sp[r], sp[r2]));
			
			struct reg *bp[]={&regX86EBP,&regX86BP};
			for(long r=0;r!=2;r++)
					for(long r2=0;r2!=2;r2++)
							assert(regConflict(bp[r], bp[r2]));

			struct reg *si[]={&regX86ESI,&regX86SI};
			for(long r=0;r!=2;r++)
					for(long r2=0;r2!=2;r2++)
							assert(regConflict(si[r], si[r2]));
			
			struct reg *di[]={&regX86EDI,&regX86DI};
			for(long r=0;r!=2;r++)
					for(long r2=0;r2!=2;r2++)
							assert(regConflict(di[r], di[r2]));
}
static void runTest(const char *asmFile,const char *expected) {
		const char *commandText=
				"yasm -g dwarf2 -f elf -o /tmp/hccTest.o %s "
				"&& ld -o /tmp/hccTest /tmp/hccTest.o "
				"&& /tmp/hccTest > /tmp/hccResult";
		long count=snprintf(NULL, 0, commandText,  asmFile);
		char buffer[count+1];
		sprintf(buffer, commandText, asmFile);
		system(buffer);

		FILE *f=fopen("/tmp/hccResult", "r");

		fseek(f, 0, SEEK_END);
		long end=ftell(f);
		fseek(f, 0, SEEK_SET);
		long start=ftell(f);
		char buffer2[end-start+1];
		fread(buffer2, end-start, 1, f);
		buffer2[end-start]='\0';

		assert(0==strcmp(expected, buffer2));
		fclose(f);
}
#define exitStr																																	\
		"MOV EAX,1\n"																																	\
				"MOV EBX,0\n"																															\
				"INT 0x80\n"
#define putY																																				\
		"    asm {\n"																																	\
		"        PUSHAD\n"																												\
		"        MOV EAX,4\n"																									\
		"        MOV EBX,1\n"																									\
		"        MOV ECX,\"y\"\n"																					\
		"        MOV EDX,1\n"																									\
		"        INT 0x80\n"																										\
		"        POPAD\n"																													\
		"    }\n"
#define putN																																				\
		"    asm {\n"																																	\
		"        PUSHAD\n"																												\
		"        MOV EAX,4\n"																									\
		"        MOV EBX,1\n"																									\
		"        MOV ECX,\"n\"\n"																					\
		"        MOV EDX,1\n"																									\
		"        INT 0x80\n"																										\
		"        POPAD\n"																													\
		"    }\n"
void compileTests() {		
		/*{
		const char * text=
				"asm {\n"
				"    MOV EAX,4\n"
				"    MOV EBX,1\n"
				"    MOV ECX,\"123\"\n"
				"    MOV EDX,3\n"
				"    INT 0x80\n"
				exitStr
				"}\n";
		char *source=text2File(text);
		char *asmF=strDup(tmpnam(NULL));
		compileFile(source, asmF);
		runTest(asmF,"123");
		free(asmF);	
		free(source);
}*/
		/*{
				const char * text=
						"{\n"
						"    for(I32i x=0;x!=3;x=x+1) {\n"
						"    asm {"
						"        PUSHAD\n"
						"        MOV EAX,4\n"
						"        MOV EBX,1\n"
						"        MOV ECX,\"x\"\n"
						"        MOV EDX,1\n"
						"        INT 0x80\n"
						"        POPAD\n"
						"    }\n"
						"    }\n"
						"}\n"
						exitStr;
				char *source=text2File(text);
				char *asmF=strDup(tmpnam(NULL));
				compileFile(source, asmF);
				runTest(asmF,"xxx");
				free(asmF);	
				free(source);
		}*/
		/*		{
				const char * text=
						"{\n"
						"    I32i a=2,b=3;\n"
						"    if(a+b==5) {\n"
						putY // 1
						"    }\n"
						"    if(a*b==6) {\n"
						putY //2
						"    }\n"
						"}\n"
						"asm {\n"
						exitStr
						"}\n";
				char *source=text2File(text);
				char *asmF=strDup(tmpnam(NULL));
				compileFile(source, asmF);
				runTest(asmF,"yy");
				free(asmF);	
				free(source);
				}*/
		/*		{
				const char * text=
						"{\n"
						"    I32i a=0;\n"
						"    asm {\n"
						"         IMPORT a;\n"
						"         MOV U32i [a],10\n"
						"    }\n"
						"    if(a==10) {\n"
						putY // 1
						"    }\n"
						"}\n"
						"asm {\n"
						exitStr
						"}\n";
				char *source=text2File(text);
				char *asmF=strDup(tmpnam(NULL));
				compileFile(source, asmF);
				runTest(asmF,"y");
				free(asmF);	
				free(source);
				}
		*/
		/*{
				const char * text=
						"I32i printY(I32i times) {\n"
						"    for(I32i x=times;x>0;x=x-1) {\n"
						putY
						"    }\n"
						"    return times+5;"
						"}\n"
						"if(8==printY(3)) {\n"
						putY
						"}\n"
						"asm {\n"
						exitStr
						"}\n";
				char *source=text2File(text);
				char *asmF=strDup(tmpnam(NULL));
				compileFile(source, asmF);
				runTest(asmF,"yyyy");
				free(asmF);	
				free(source);
				}*/
		/*
				{
				const char * text=
				"{\n"
				"    I32i a=10;\n"
				"    I32i *b=&a;\n"
				"    *b=20;\n"
				"    if(a==20) {\n"
				putY
				"    }\n"
				"}\n"
				"asm {\n"
				exitStr
				"}\n";
				char *source=text2File(text);
				char *asmF=strDup(tmpnam(NULL));
				compileFile(source, asmF);
				runTest(asmF,"y");
				free(asmF);	
				free(source);
				}
		*/
		/*{
				const char * text=
						"{\n"
						"    class abc {\n"
						"        I32i a,b,c;"
						"    };\n"
						"    abc x;\n"
						"    x.a=1,x.b=2,x.c=3;\n"
						"    if(x.a==1&&x.b==2&&x.c==3) {\n"
						putY
						"    }\n"
						"}\n"
						"asm {\n"
						exitStr
						"}\n";
				char *source=text2File(text);
				char *asmF=strDup(tmpnam(NULL));
				compileFile(source, asmF);
				runTest(asmF,"y");
				free(asmF);	
				free(source);
				}
		{
				const char * text=
						"{\n"
						"    class abc2 {\n"
						"        I32i a,b,c;"
						"    };\n"
						"    abc2 X;\n"
						"    abc2 *x=&X;\n"
						"    x->a=1,x->b=2,x->c=3;\n"
						"    if(x->a==1&&x->b==2&&x->c==3) {\n"
						putY
						"    }\n"
						"}\n"
						"asm {\n"
						exitStr
						"}\n";
				char *source=text2File(text);
				char *asmF=strDup(tmpnam(NULL));
				compileFile(source, asmF);
				runTest(asmF,"y");
				free(asmF);	
				free(source);
				}*/
		/*{
				const char * text=
						"{\n"
						"    class abc3 {\n"
						"        I32i a,b,c;"
						"    };\n"
						"    abc3 x,y;\n"
						"    x.a=1,x.b=2,x.c=3;\n"
						"    y=x;\n"
						"    if(y.a==1&&y.b==2&&y.c==3) {\n"
						putY
						"    }\n"
						"}\n"
						"asm {\n"
						exitStr
						"}\n";
				char *source=text2File(text);
				char *asmF=strDup(tmpnam(NULL));
				compileFile(source, asmF);
				runTest(asmF,"y");
				free(asmF);	
				free(source);
				}
		*/
		/*{
				const char * text=
						"U0 assertEq(U8i *A,U8i *B,I32i len) {\n"
						"    for(I32i b=0;b!=len;b=b+1) {\n"
						"        if(A[b]!=B[b]) {\n"
						"            asm {\n"
						exitStr
						"            }\n"
						"        }\n"
						"    }\n"
						putY
						"}\n"
						"{\n"
						"    class abc3 {\n"
						"        I32i a,b,c;"
						"    };\n"
						"    abc3 x,y;\n"
						"    x.a=1,x.b=2,x.c=3;\n"
						"    y=x;\n"
						"    assertEq(&x,&y,sizeof(x));\n"
						"}\n"
						"asm {\n"
						exitStr
						"}\n";
				char *source=text2File(text);
				char *asmF=strDup(tmpnam(NULL));
				compileFile(source, asmF);
				runTest(asmF,"y");
				free(asmF);	
				free(source);
				}*/
		/*{
				const char * text=
						"asm {\n"
						"    MOV EAX,1"
						"    MOV ECX,2"
						"    MOV EBX,3"
						"    MOV EDX,4"
						"    MOV ESI,5"
						"    MOV EDI,6"
						"}\n"
						"I32i rEax,rEcx,rEdx,rEbx,rEsi,rEdi;\n"
						"I32i rEax2,rEcx2,rEdx2,rEbx2,rEsi2,rEdi2;"
						"if(0) {"
						"asm {\n"
						"    gpDump::"
						"    IMPORT rEax,rEcx,rEdx,rEbx,rEsi,rEdi;\n"
						"    MOV I32i [rEax], EAX \n"
						"    MOV I32i [rEcx], ECX \n"
						"    MOV I32i [rEdx], EDX \n"
						"    MOV I32i [rEbx], EBX \n"
						"    MOV I32i [rEsi], ESI \n"
						"    MOV I32i [rEdi], EDI \n"
						"    RET\n"
						"}\n"
						"}\n"
						"extern U0 gpDump();\n"
						"U0 moveGpTo2() {\n"
						"    rEax2=rEax,rEcx2=rEcx,rEdx2=rEdx,rEbx2=rEbx,rEsi2=rEsi,rEdi2=rEdi;\n"
						"}\n"
						"U0 assertEq() {\n"
						"    if(rEax2!=rEax) goto fail;\n"
						"    if(rEcx2!=rEcx) goto fail;\n"
						"    if(rEdx2!=rEdx) goto fail;\n"
						"    if(rEbx2!=rEbx) goto fail;\n"
						"    if(rEsi2!=rEsi) goto fail;\n"
						"    if(rEdi2!=rEdi) goto fail;\n"
						"    return;\n"
						"    fail:\n"
						"    asm {\n"
						exitStr
						"    }\n"
						"}\n"
						// We will assign into global variables which will not be in registers,
						// we will then ensure no general purpose operations have been changed during the operation
						"I32i a=2,b=15,c;\n"
						"Bool d;\n"
#define operationTest(text) "gpDump();moveGpTo2();\n" text "gpDump();assertEq();\n"
						operationTest("c=b+b;\n")
						operationTest("c=a-b;\n")
						operationTest("c=a*b;\n")
						operationTest("c=a%b;\n")
						operationTest("c=a/b;\n")
						operationTest("c=a>>b;\n")
						operationTest("c=a<<b;\n")
						operationTest("d=a<b;\n")
						operationTest("d=a>b;\n")
						operationTest("d=a<=b;\n")
						operationTest("d=a>=b;\n")
						operationTest("d=a==b;\n")
						operationTest("d=a!=b;\n")
						operationTest("c=a&b;\n")
						operationTest("c=a|b;\n")
						operationTest("c=a^b;\n")
						operationTest("c=~b;\n")
						putY
						"asm {\n"
						exitStr
						"}\n";
				char *source=text2File(text);
				char *asmF=strDup(tmpnam(NULL));
				compileFile(source, asmF);
				runTest(asmF,"y");
				free(asmF);	
				free(source);
				}*/
		/*{
				const char * text=
						"U0 assertEq(I32i a,I32i b) {\n"
						"    if(a!=b) {\n"
						"         asm {\n"
						exitStr
						"         }\n"
						"    }\n"
						"}\n"
						"I32i a,b,c;\n"
						"a=25,b=5;\n"
						"c=a+b;\n"
						"assertEq(c,30);\n"
						"c=a-b;\n"
						"assertEq(c,20);\n"
						"c=a/b;\n"
						"assertEq(c,5);\n"
						"c=a*b;\n"
						"assertEq(c,125);\n"
						"c=a%b;\n"
						"assertEq(c,0);\n"
						"c=a<<b;\n"
						"assertEq(c,800);\n"
						"c=a>>b;\n"
						"assertEq(c,0);\n"
						"c=a>b;\n"
						"assertEq(c,1);\n"
						"c=a<b;\n"
						"assertEq(c,0);\n"
						"c=a>=25;\n"
						"assertEq(c,1);\n"
						"c=a<=25;\n"
						"assertEq(c,1);\n"
						"c=a==b;\n"
						"assertEq(c,0);\n"
						"c=a!=b;\n"
						"assertEq(c,1);\n"
						"c=a!=b;\n"
						"assertEq(c,1);\n"
						"c=a&b;\n"
						"assertEq(c,1);\n"
						"c=a|b;\n"
						"assertEq(c,29);\n"
						"c=a^b;\n"
						"assertEq(c,28);\n"
						"c=~~b;\n"
						"assertEq(c,5);\n"
						"c=!b;\n"
						"assertEq(c,0);\n"
						"c=a&&b;\n"
						"assertEq(c,1);\n"
						"c=a||b;\n"
						"assertEq(c,1);\n"
						"c=a^^b;\n"
						"assertEq(c,0);\n"
						putY
						"asm {\n"
						exitStr
						"}\n";
				char *source=text2File(text);
				char *asmF=strDup(tmpnam(NULL));
				compileFile(source, asmF);
				runTest(asmF,"y");
				free(asmF);	
				free(source);
				}*/
		/*{
				const char *text=
						"{\n"
						"    F64 a=1.2,b=3.4,c;\n"
						"    c=a+b;\n"
						"    asm {\n"
						exitStr
						"    }\n"
						"}\n";
				char *source=text2File(text);
				char *asmF=strDup(tmpnam(NULL));
				compileFile(source, asmF);				runTest(asmF,"y");
				free(asmF);	
				free(source);
				}*/
		/*{
				const char *text=
						"I32i StrLen(U8i *str) {\n"
						"    U8i *start=str;"
						"    while(*str!='\\0') {"
						"       str=str+1; "
						"    }\n"
						"   return str-start;"
						"}\n"
						"    U8i x[3];\n"
						"    x[0]='h';\n"
						"    x[1]='i';\n"
						"    x[2]='\\0';\n"
						"    asm {\n"
						"        IMPORT x;\n"
						"        PUSHAD\n"
						"        MOV EAX,4\n"
						"        MOV EBX,1\n"
						"        MOV ECX, [x]\n"
						"        MOV EDX,3\n"
						"        INT 0x80\n"
						"        POPAD\n"
						"    }\n"
						exitStr;
				char *source=text2File(text);
				char *asmF=strDup(tmpnam(NULL));
				compileFile(source, asmF);				runTest(asmF,"hi");
				free(asmF);	
				free(source);
				}*/
		/*{
				const char *text=
						"{\n"
						"    for(I32i i=0;i!=5;i=i+1) {\n"
						"        switch(i) {\n"
						"            start:"
						putY
						"            case 4:\n"
						"            case 2:\n"
						"            case 0:\n"
						putY
						"                end:"
						"                break;\n"
						"            case 3:\n"
						"            case 1:\n"
						"                break;\n"
						"        }\n"
						"    }\n"
						"}\n"
						exitStr;
				char *source=text2File(text);
				char *asmF=strDup(tmpnam(NULL));
				compileFile(source, asmF);				runTest(asmF,"yyyyyy");
				free(asmF);	
				free(source);
				}*/
		/*{
				const char *text=
						"U0 assertEq(I32i a,I32i b) {\n"
						"    if(a!=b) {\n"
						"         asm {\n"
						exitStr
						"         }\n"
						"    }\n"
						"}\n"
						"{\n"
						"    I32i a[1][2]={{1,2}};\n"
						"    assertEq(a[0][0],1);\n"
						"    assertEq(a[0][1],2);\n"
						"    I32i b[][]={{1,2}};\n"
						"    assertEq(b[0][0],1);\n"
						"    assertEq(b[0][1],2);\n"
						"    I32i *c=&b[0][1];\n"
						"    *&c[-1]=3;\n"
						"    assertEq(b[0][0],3);\n"
						"}\n"
						putY
						exitStr;
				char *source=text2File(text);
				char *asmF=strDup(tmpnam(NULL));
				compileFile(source, asmF);				runTest(asmF,"y");
				free(asmF);	
				free(source);
				}*/
		/*{
						const char *text=
								"I32i StrNCmp(U8i *str1,U8i *str2,I32i n) {\n"
								"    for(I32i c=0;c!=n;c=c+1) {\n"
								"        if(str1[c]!=str2[c]) return str1[c]-str2[c];"
								"    }\n"
								"    return 0;"
								"}\n"
								"static U0 putC(U8i chr) {\n"
								"    asm {\n"
								"        IMPORT chr;\n"
								"        PUSHAD\n"
								"        MOV EAX,4\n"
								"        MOV EBX,1\n"
								"        LEA ECX,[chr]\n"
								"        MOV EDX,1\n"
								"        INT 0x80\n"
								"        POPAD\n"
								"    }\n"
								"}\n"
								"U0 assertEq(I32i a,I32i b) {\n"
								"    if(a!=b) {\n"
								"         asm {\n"
								exitStr
								"         }\n"
								"    }\n"
								"}\n"
								"U8i *StrChr(U8i *str,U8i chr) {\n"
								"    for(;;str=str+1) {\n"
								"        if(*str==chr) return str;\n"
								"        if(*str=='\\0') return 0;\n"
								"    }\n"
								"    return str;"
								"}\n"
								"static U0 printNum(I32i num) {"
								"    do {\n"
								"        U8i *digits=\"0123456789\";\n"
								"        putC(digits[num%10]);\n"
								"        num/=10;\n"
								"    } while(num!=0);"
								"}\n"
								"static U0 printf(U8i *str) {\n"
								"    I32i len=StrChr(str,'\\0');\n"
								"    U8i *ptr=str;\n"
								"    while(*ptr!='\\0') {\n"
								"          U8i *dol=StrChr(ptr,'$');\n"
								"          if(!dol) goto dumpRest;\n"
								"          for(;ptr!=dol;ptr=ptr+1) {\n"
								"              putC(*ptr);"
								"          }\n"
								"          ptr=dol;\n"
								"    }\n"
								"    dumpRest:\n"
								"    for(;*ptr!='\\0';ptr=ptr+1)"
								"        putC(*ptr);"
								"}\n"
								"printNum(123);"
								exitStr;
						char *source=text2File(text);
						char *asmF=strDup(tmpnam(NULL));
						compileFile(source, asmF);				runTest(asmF,"hi");
						free(asmF);	
						free(source);
						}*/
		/*
			*/
		/*{
				const char * text=
						"class CTest {\n"
						"     I32i a,b;\n"
						"};\n"
						"CTest CTestNew(I32i a,I32i b) {\n"
						"    CTest retVal;retVal.a=a;retVal.b=b;\n"
						"    return retVal;"
						"};\n"
						"U0 assertEq(I32i a,I32i b) {\n"
						"    if(a!=b) {\n"
						"         asm {\n"
						exitStr
						"         }\n"
						"    }\n"
						"}\n"
						"{\n"
						"    CTest tmp=CTestNew(1,2);\n"
						"    assertEq(tmp.a,1);\n"
						"    assertEq(tmp.b,2);\n"
						"}\n"
						putY
						"asm {\n"
						exitStr
						"}\n";
				char *source=text2File(text);
				char *asmF=strDup(tmpnam(NULL));
				compileFile(source, asmF);
				runTest(asmF,"y");
				free(asmF);	
				free(source);
				}*/
		/*		{
				const char * text=
						"U0 assertEq(I32i a,I32i b) {\n"
						"    if(a!=b) {\n"
						"         asm {\n"
						exitStr
						"         }\n"
						"    }\n"
						"}\n"
						"{\n"
						"    I32i v=1;\n"
						"    I32i z=v+1;\n"
						"    I32i x=v*z;\n"
						"    I32i y=x*2;\n"
						"    I32i w=x+z*y;\n"
						"    I32i u=z+2;\n"
						"    v=u+w+y;\n"
						"}\n"
						putY
						"asm {\n"
						exitStr
						"}\n";
				char *source=text2File(text);
				char *asmF=strDup(tmpnam(NULL));
				compileFile(source, asmF);
				runTest(asmF,"y");
				free(asmF);	
				free(source);
				}*/
		/*{
				const char * text=
						"F64 Add(F64 a,F64 b) {\n"
						"    return a+b;"
						"}\n"
						"{\n"
						"    F64 a=1.3,b=3.5;\n"
						"    F64 c=Add(Add(9.0,1.0),Add(a+0.2,b)+2.0);\n"
						"}\n"
						putY
						"asm {\n"
						exitStr
						"}\n";
				char *source=text2File(text);
				char *asmF=strDup(tmpnam(NULL));
				compileFile(source, asmF);
				runTest(asmF,"y");
				free(asmF);	
				free(source);
				}*/
		/*{
				const char * text=
						"U0 OneTwoThree(...) {\n"
						"    if(argc!=3) goto fail;\n"
						"    if(argv[0]!=1) goto fail;\n"
						"    if(argv[1]!=2) goto fail;\n"
						"    if(argv[2]!=3) goto fail;\n"
						"    return;"
						"    fail:\n"
						"    asm {\n"
						exitStr
						"   }\n"
						"}\n"
						"OneTwoThree(1,2,3);\n"
						putY
						"asm {\n"
						exitStr
						"}\n";
				char *source=text2File(text);
				char *asmF=strDup(tmpnam(NULL));
				compileFile(source, asmF);
				runTest(asmF,"y");
				free(asmF);	
				free(source);
				}*/
/*
		const char *argv[]={
				"hcc",
				"/home/tc/projects/holycc2/HolyCTests/StrFmt.HC"
		};
		parseCommandLineArgs(2, argv);
		system("./a.out >res.txt");
		FILE *f=fopen("res.txt", "r");
		fseek(f, 0, SEEK_END);
		long end=ftell(f);
		fseek(f, 0, SEEK_SET);
		long start=ftell(f);
		if(end==0)
				return;
		char buffer[end-start+1];
		fread(buffer, end-start, 1, f);
		buffer[end-start-1]='\0';
		printf("%s\n", buffer);
*/
		/*{
						const char *argv[]={
								"hcc",
								"-c",
								"/home/tc/projects/holycc2/HolyCTests/IntPow.HC",
								"-o",
								"/tmp/IntPow.o"
						};
						parseCommandLineArgs(5, argv);
						}*/
		//registerInterfereTestX86();
		registerInterfereTestX64();
		{
						const char *argv[]={
								"hcc",
								"-dd",
								"-c",
								"/home/tc/projects/HI64.HC",
								"-o",
								"/tmp/HCRT.o"
						};
						parseCommandLineArgs(6, argv);
		}
}
