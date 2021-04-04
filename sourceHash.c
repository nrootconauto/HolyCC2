#include "lexer.h"
#include <stdlib.h>
#include <stdio.h>
#include "escaper.h"
#include "cacheDir.h"
#include <unistd.h>
#include "sourceHash.h"
#include "parserB.h"
#include "object.h"
#include "cacheDir.h"
#include "filePath.h"
#include <string.h>
#include <assert.h>
MAP_TYPE_DEF(int , Changed);
MAP_TYPE_FUNCS(int , Changed);
static __thread mapChanged changedGVars=NULL;
static __thread char *basefile=NULL;
static int charPCmp(const char **a,const char **b) {
		return strcmp(*a, *b);
}
static void printSInt(FILE *dumpTo,int64_t value,int base,const char *chars) {
		if(value<0) {
				fputc('-', dumpTo);
				value=-value;
		}
		__auto_type value2=value;
		long digits=0;
		do {
				digits++;
				value2/=base;
		} while(value2);
		char buffer[digits+1];
		buffer[digits]='\0';
		for(long d=0;d!=digits;d++) {
				buffer[digits-d-1]=chars[value%base];
				value/=base;
		}
		fprintf(dumpTo, "%s",  buffer);
}
static void printUInt(FILE *dumpTo,int64_t value,int base,const char *chars) {
		__auto_type value2=value;
		long digits=0;
		do {
				digits++;
				value2/=base;
		} while(value2);
		char buffer[digits+1];
		buffer[digits]='\0';
		for(long d=0;d!=digits;d++) {
				buffer[digits-d-1]=chars[value%base];
				value/=base;
		}
		fprintf(dumpTo, "%s",  buffer);
}
static void insertTabs(FILE *f,long count) {
		while(count--)
				fputc('\t', f);
}
static long fileSize(FILE *f) {
		fseek(f, 0, SEEK_END);
		long end=ftell(f);
		fseek(f, 0, SEEK_SET);
		long start=ftell(f);

		return end-start;		
}
// https://algs4.cs.princeton.edu/34hash/
static long hash(FILE *f) {
		long size=fileSize(f);
		int retVal = 0;
	for (long i = 0; i != size; i++) {
		retVal = (31 * retVal + fgetc(f));
	}
	// printf("Hash:%i\n", retVal);
	return retVal;
}
static void copy(FILE *out,FILE *in) {
		fseek(in, 0, SEEK_SET);
		long size=fileSize(in);
		for(long c=0;c!=size;c++)
				fputc(fgetc(in),out);
}
MAP_TYPE_DEF(char *, CharP);
MAP_TYPE_FUNCS(char *, CharP);
static void free2(char **str) {
		free(*str);
}
/*
	* Types file has format 
 * name1:typestr
	* ...
	* namen:typestr
	*/
static void cacheCheckForTypeChanges() {
		long len=snprintf(NULL, 0,  "%s/%s.gVarTypes",cacheDirLocation,basefile);
		char buffer[len+1];
		snprintf(buffer, len+1,"%s/%s.gVarTypes",cacheDirLocation,basefile);
		FILE *f=fopen(buffer, "r");
		
		mapCharP curGVarsType=mapCharPCreate();
		long count;
		parserSymTableNames(NULL, &count);
		const char *keys[count];
		parserSymTableNames(keys,NULL);

		if(!f) goto writeOut;
		for(long k=0;k!=count;k++) {
				__auto_type find=parserGetGlobalSym(keys[k]);
				if(!find->var)
						if(find->type->type!=TYPE_FUNCTION) continue;
				mapCharPInsert(curGVarsType, keys[k],  object2Str(find->type));
		}
		
		size_t lineLen=1024;
		char *linebuffer=__builtin_alloca(lineLen);
		while(-1!=getline(&linebuffer, &lineLen, f)) {
				char *colon=strchr(linebuffer, ':');
				assert(colon);
				
				char gVarNm[colon-linebuffer+1];
				strncpy(gVarNm, linebuffer, colon-linebuffer);
				gVarNm[colon-linebuffer]='\0';

				if(!mapCharPGet(curGVarsType, gVarNm)) continue;
				//getline includes newlines
				const char *endlnChars="\n\r";
				for(long i=0;i!=3;i++)
						if(strchr(colon+1, endlnChars[i])) *strchr(colon+1, endlnChars[i])='\0';

				if(0==strcmp(*mapCharPGet(curGVarsType, gVarNm),colon+1)) continue;
				printf("Changed gvar:%s\n",gVarNm);
				mapChangedInsert(changedGVars, gVarNm, 1);
		}
		
		mapCharPDestroy(curGVarsType, (void(*)(void*))free2);
		fclose(f);
		remove(buffer);

	writeOut:
		f=fopen(buffer, "w");
		for(long k=0;k!=count;k++) {
				__auto_type find=parserGetGlobalSym(keys[k]);
				if(!find->var) continue;
				char *p=object2Str(find->type);
				fprintf(f, "%s:%s\n", keys[k],p);
		}
		fclose(f);
}
void sourceCacheInitAfterParse(const char *fn) {
		basefile=fnFromPath(fn);
		changedGVars=mapChangedCreate();
		cacheCheckForTypeChanges();
} 
char *hashSource(llLexerItem start,llLexerItem end,const char *name,long *fileExists) {
		if(fileExists)
				*fileExists=0;

		//If contains a changed global,delete the cached file. If a type changes a recompile must occur
		int containsChanged=0;
		
		long tabLevel=0;
		FILE *f=tmpfile();
		for(__auto_type cur=start;cur!=end;cur=llLexerItemNext(cur)) {
				__auto_type item=llLexerItemValuePtr(cur);
				if(item->template==&intTemplate) {
						struct lexerInt *i=lexerItemValuePtr(item);
						switch(i->base) {
						case 2: {
								if(i->type==INT_SLONG) printSInt(f, i->value.sLong, 2, "01");
								else printUInt(f, i->value.uLong, 2, "01");
								break;
						}
						case 8:
								if(i->type==INT_SLONG) printSInt(f, i->value.sLong, 8, "01234567");
								else printUInt(f, i->value.uLong, 8, "01234567");
								break;
						case 10:
								if(i->type==INT_SLONG) printSInt(f, i->value.sLong, 10, "0123456789");
								else printUInt(f, i->value.uLong, 10, "0123456789");
								break;
						case 16:
								if(i->type==INT_SLONG) printSInt(f, i->value.sLong, 10, "0123456789ABCDEF");
								else printUInt(f, i->value.uLong, 10, "0123456789ABCDEF");
								break;
								default:
										abort();
						}
				} else if(item->template==&strTemplate) {
						struct parsedString *str=lexerItemValuePtr(item);
						char *text=escapeString((char*)str->text,strlen((char*)str->text));
						if(str->isChar) fprintf(f, "\'%s\'", text);
						else fprintf(f, "\"%s\"", text);
						free(text);
				} else if(item->template==&floatTemplate) {
						struct lexerFloating *fl=lexerItemValuePtr(item);
						fprintf(f, "%lf", fl->value);
				} else if(item->template==&nameTemplate) {
						char *nm=lexerItemValuePtr(item);
						fprintf(f, "%s", nm);
						if(mapChangedGet(changedGVars, nm)) containsChanged=1;
				} else if(item->template==&opTemplate) {
						const char **op=lexerItemValuePtr(item);
						fprintf(f, "%s", *op);
				} else if(item->template==&kwTemplate) {
						const char **kw=lexerItemValuePtr(item);
						fprintf(f, "%s", *kw);

						if(0==strcmp(*kw, ";")) {
								fputc('\n', f);
								insertTabs(f,tabLevel);
						} else if(0==strcmp(*kw, "{")) {
								fputc('\n', f);
								tabLevel++;
								insertTabs(f,tabLevel);
						} else if(0==strcmp(*kw, "}")) {
								fputc('\n', f);
								tabLevel--;
								insertTabs(f,tabLevel);
						}
				}
		}
		long retVal=hash(f);
		
		const char *fmt="%s/%s.%li";
		long len=snprintf(NULL, 0, fmt, cacheDirLocation,name,retVal);
		char buffer[len+1];
		sprintf(buffer, fmt, cacheDirLocation,name,retVal);		
		{
				if(0==access(buffer,F_OK)&&!containsChanged) {
						//Check if files are the same
						FILE *existing=fopen(buffer, "r");
						long existingSize=fileSize(existing);
						long fSize=fileSize(f);
						fseek(f, 0, SEEK_SET);

						if(fSize!=existingSize) {
						} else {
								for(long c=0;c!=fSize;c++)
										if(fgetc(f)!=fgetc(existing))
												goto diff;
								if(fileExists)
										*fileExists=1;
						diff:;
						}
						fclose(existing);
				}

				//No need to rewrite out hashed value if the value is already in the hash
				if(fileExists)
						if(*fileExists) {
								fclose(f);
								return strcpy(calloc(strlen(buffer)+1, 1), buffer);
						}

				FILE *out=fopen(buffer, "w");
				copy(out,f);
				fclose(out);
		}

		fclose(f);
		return strcpy(calloc(strlen(buffer)+1, 1), buffer);
}
