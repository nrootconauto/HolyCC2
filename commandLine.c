#include <stdio.h>
#include "compile.h"
#include "hashTable.h"
#include <string.h>
#include "str.h"
#include "cleanup.h"
#include <assert.h>
void init();
//
// argi,moves the argument index to adavance the consumed arguments
//
typedef void(*commlFlagCallback)(int *argi,int argc,const  char **argv);
struct commlFlag {
		const char *shortName;
		const char *longName;
		const char *helpStr;
		commlFlagCallback cb;
};
MAP_TYPE_DEF(struct commlFlag, Flags);
MAP_TYPE_FUNCS(struct commlFlag, Flags);
mapFlags clFlagsShort;
mapFlags clFlagsLong;
static struct commlFlag *getFlag(const char *text) {
		if(0==strncmp(text,"--",2)) {
				return mapFlagsGet(clFlagsLong, text+2);
		} else if(text[0]=='-') {
				return mapFlagsGet(clFlagsShort, text+1);
		}
		return NULL;
}
static void helpCallback(int *argi,int argc,const char **argv) {
		long count;
		mapFlagsKeys(clFlagsLong, NULL, &count);
		const char *keys[count];
		mapFlagsKeys(clFlagsLong, keys, NULL);

		for(long f=0;f!=count;f++) {
				__auto_type flag=mapFlagsGet(clFlagsLong, keys[f]);
				fprintf(stdout, "--%s(%s)    //%s\n", flag->longName,flag->shortName,flag->helpStr);
		}
}
static int nextFlagI(int argi,int argc,const char **argv) {
		long i=argi;
		for(;i!=argc;i++) {
				if(getFlag(argv[i]))
						break;
		}
		return i;
}
typedef const char *constChar;
STR_TYPE_DEF(constChar, ConstChar);
STR_TYPE_FUNCS(constChar, ConstChar);
static strConstChar toCompile=NULL;
static void compileCallback(int *argi,int argc,const char **argv) {
		long end=nextFlagI(*argi+1,argc,argv);
		for(long i=*argi+1;i!=end;i++)
				toCompile=strConstCharAppendItem(toCompile, argv[i]);
		*argi=end;
}
static const char *outputFile=NULL;
static void outputCallback(int *argi,int argc,const char **argv) {
		long end=nextFlagI(*argi+1,argc,argv);
		if(end!=*argi+2||outputFile) {
				fputs("Only one output file is allowed.\n", stderr);
				abort();
		}
		outputFile=argv[*argi+1];
		*argi=end;
}
static void registerCLIFlag(struct commlFlag *f) {
		mapFlagsInsert(clFlagsShort, f->shortName, *f);
		mapFlagsInsert(clFlagsLong, f->longName, *f);
}
STR_TYPE_DEF(char,Char);
STR_TYPE_FUNCS(char,Char);
STR_TYPE_DEF(strChar,StrChar);
STR_TYPE_FUNCS(strChar,StrChar);
static void strStrCharDestroy2(strStrChar *str) {
		for(long i=0;i!=strStrCharSize(*str);i++)
				strCharDestroy(&str[0][i]);
		strStrCharDestroy(str);
}
static strChar strCharDup(const char *text) {
		return strCharAppendData(NULL, text, strlen(text)+1);
}
static strStrChar assembleSources(strConstChar sources) {
		strStrChar toAssemble=NULL;
		for(long i=0;i!=strConstCharSize(sources);i++) {
				__auto_type dumpAsmTo=strCharDup(tmpnam(NULL));
				toAssemble=strStrCharAppendItem(toAssemble,dumpAsmTo);
				compileFile(sources[i], dumpAsmTo);
		}
		//Assemble the files 
		for(long i=0;i!=strStrCharSize(toAssemble);i++) {
				const char *assembler="yasm";
				const char *commFmt="%s -g dwarf2 -f elf -o %s.o %s";
				long len=snprintf(NULL,0, commFmt, assembler,toAssemble[i],toAssemble[i]);
				char buffer[len+1];
				sprintf(buffer, commFmt, assembler,toAssemble[i],toAssemble[i]);
				system(buffer);
		}
		return toAssemble;
} 
void parseCommandLineArgs(int argc,const char **argv) {
		init();
		clFlagsLong=mapFlagsCreate();
		clFlagsShort=mapFlagsCreate();
		struct commlFlag help={
				"h",
				"help",
				"Lists help for HCC.",
				helpCallback,
		};
		registerCLIFlag(&help);
		struct commlFlag compile={
				"c",
				"compile",
				"Compiles a file but doesn't link it.",
				compileCallback,
		};
		registerCLIFlag(&compile);
		struct commlFlag output={
				"o",
				"out",
				"Specifies the output file,either an executable or object.",
				outputCallback,
		};
		registerCLIFlag(&output);

		strConstChar sources CLEANUP(strConstCharDestroy)=NULL;
		
		for(int i=1;i!=argc;) {
				__auto_type f=getFlag(argv[i]);
				if(!f) {
						sources=strConstCharAppendItem(sources, argv[i]);
						i++;
				} else {
						f->cb(&i,argc,argv);
				}
		}

		if(outputFile) {
				if(strConstCharSize(toCompile)>1) {
						fputs("Can't compile multiple files to one file,-o requires 1 file.", stderr);
						abort();
				}
		}
		
		const char *hcrt="/home/tc/projects/holycc2/HolyCRT/HCRT.HC";
		strConstChar hcrtSources CLEANUP(strConstCharDestroy)=strConstCharAppendItem(NULL, hcrt);
		strStrChar toLink CLEANUP(strStrCharDestroy2)=assembleSources(hcrtSources);
		assert(strStrCharSize(toLink)==1);
		toLink[0]=strCharAppendData(toLink[0], "\0\0\0", 3);
		strcat(toLink[0], ".o");
		
		if(strConstCharSize(sources)||strConstCharSize(toCompile)) {
				if(strConstCharSize(sources)&&strConstCharSize(toCompile)&&outputFile) {
						fputs("Can't route sources and files to compile to a single output file.", stderr);
						abort();
				}
				sources=strConstCharAppendData(sources, toCompile, strConstCharSize(toCompile));
				strStrChar toAssemble CLEANUP(strStrCharDestroy2)=assembleSources(sources);
				const char *commHeader="gcc -m32 -lm -o ";
				strChar linkCommand CLEANUP(strCharDestroy)=strCharAppendData(NULL,commHeader,strlen(commHeader));
				if(!outputFile)
						outputFile="a.out";
				linkCommand=strCharAppendData(linkCommand, outputFile,strlen(outputFile));

				//Link in hcrt
				linkCommand=strCharAppendItem(linkCommand, ' ');
				linkCommand=strCharAppendData(linkCommand, toLink[0], strlen(toLink[0]));
				
				for(long i=0;i!=strStrCharSize(toAssemble);i++) {
						linkCommand=strCharAppendItem(linkCommand, ' ');
						const char *fmt="%s.o";
						long len=snprintf(NULL, 0, fmt, toAssemble[i]);
						char buffer[len+1];
						sprintf(buffer, fmt, toAssemble[i]);

						linkCommand=strCharAppendData(linkCommand, buffer, len);
				}
				linkCommand=strCharAppendItem(linkCommand, '\0');
				system(linkCommand);
		}
		if(strConstCharSize(sources)==0) {
				helpCallback(0, 0, NULL);
		}
		
		mapFlagsDestroy(clFlagsShort, NULL);
		mapFlagsDestroy(clFlagsLong, NULL);
}
