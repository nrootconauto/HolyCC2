#include <compile.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <registers.h>
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
		/*
		{
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
		{
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
						"    *(c-1)=3;\n"
						"    assertEq(b[0][0],3);\n"
						"}\n"
						putY
						exitStr;
				char *source=text2File(text);
				char *asmF=strDup(tmpnam(NULL));
				compileFile(source, asmF);				runTest(asmF,"y");
				free(asmF);	
				free(source);
				}
		/*
			*/
}
