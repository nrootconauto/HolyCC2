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
		{
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
				}
}
