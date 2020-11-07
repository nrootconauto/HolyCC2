#include <str.h>
#include <stdarg.h>
#include <stdio.h>
#include <diagMsg.h>
#include <stdlib.h>
#include <parserA.h>
static char *strClone(const char *text) {
		long len=strlen(text);
		char *retVal=malloc(len+1);
		strcpy(retVal, text);

		return retVal;
}
STR_TYPE_DEF(long,Long);
STR_TYPE_FUNCS(long,Long);
STR_TYPE_DEF(enum textAttr, TextAttr);
struct diagQoute {
		strTextAttr attrs;
		long start,end;
};
STR_TYPE_DEF(struct diagQoute, DiagQoute);
STR_TYPE_FUNCS(struct diagQoute, DiagQoute);

#define TEXT_WIDTH 60
enum textAttr {
		ATTR_END=0,
		FG_COLOR_BLACK,
		FG_COLOR_RED,
		FG_COLOR_GREEN,
		FG_COLOR_YELLOW,
		FG_COLOR_BLUE,
		FG_COLOR_MAGENTA,
		FG_COLOR_CYAN,
		FG_COLOR_WHITE,
		BG_COLOR_BLACK,
		BG_COLOR_RED,
		BG_COLOR_GREEN,
		BG_COLOR_YELLOW,
		BG_COLOR_BLUE,
		BG_COLOR_MAGENTA,
		BG_COLOR_CYAN,
		BG_COLOR_WHITE,
		ATTR_NORMAL,
		ATTR_BOLD,
		ATTR_UNDER,
		ATTR_BLINK,
		ATTR_REVERSE,
		ATTR_HIDDEN,
};
enum diagState {
		DIAG_ERR,
		DIAG_WARN,
		DIAG_NOTE,
		DIAG_NONE,
};
STR_TYPE_FUNCS(enum textAttr, TextAttr);

struct diagInst {
		strLong lineStarts;
		enum outputType diagType;
		strTextModify mappings;
		enum diagState state;
		long stateStart,stateEnd;
		FILE *dumpTo;
		FILE *sourceFile;
		char *fileName;
		strDiagQoute stateQoutes;
}; 

static void putAttrANSI(FILE *f,const strTextAttr attrs) {
		fprintf(f, "\e[");
		for(long i=0;i!=strTextAttrSize(attrs);i++) {
				__auto_type a=attrs[i];
				switch(a) {
				case ATTR_REVERSE:
						fprintf(f, "7;");break;
				case ATTR_HIDDEN:
						fprintf(f, "8;");break;
				case ATTR_BLINK:
								fprintf(f, "5;");break;
				case ATTR_UNDER:
						fprintf(f, "4;");break;
				case ATTR_BOLD:
						fprintf(f, "1;");break;
				case ATTR_NORMAL:
						fprintf(f, "0;");break;
				case FG_COLOR_BLACK...FG_COLOR_WHITE:
						fprintf(f, "%i", 30+a-FG_COLOR_BLACK);break;
				case BG_COLOR_BLACK...BG_COLOR_WHITE:
						fprintf(f, "%i", 40+a-BG_COLOR_BLACK);break;
				case ATTR_END:break;
				}
		}
		fprintf(f, "m");
}
static void endAttrs(struct diagInst *inst) {
		if(inst->diagType==DIAG_ANSI_TERM)
			;
}
static void setAttrs(struct diagInst *inst,...) {
		va_list list;
		va_start(list, inst);
		
		strTextAttr attrs=NULL;
		for(int firstRun=1;;firstRun=0) {
				attrs=strTextAttrAppendItem(attrs, va_arg(list,enum textAttr));
		}

		if(inst->diagType==DIAG_ANSI_TERM) {
				putAttrANSI(inst->dumpTo, attrs);
		}
		va_end(list);
}

static long firstOf(const char *buffer,long index,const char *chrs) {
		int len=strlen(chrs);
		char *minFind=NULL;
		for(int i=0;i!=len;i++) {
				__auto_type find=strchr(buffer+index,chrs[i]);
				if(find!=NULL)
						if(minFind>find||minFind==NULL)
						find=minFind;
						}

		if(minFind!=0)
				return (minFind-buffer)/sizeof(char);

		return -1;
}
static strLong fileLinesIndexes(FILE *file) {
		fseek(file, 0, SEEK_END);
		long end=ftell(file);
		fseek(file, 0, SEEK_SET);
		long start=ftell(file);

		//Reserve by "average" chars per line
		strLong retVal=strLongReserve(NULL, (end-start)/64);
		retVal=strLongAppendItem(retVal, 0);
		
		const long bufferSize=1024;
		char buffer[bufferSize+1];
		buffer[bufferSize]='\0';

		long bufferOffset=0;
		long bufferFilePos=0;
		int hitEnd=0;
		while(ftell(file)!=end) {
				long find=firstOf(buffer, bufferOffset, "\n\r");

				if(find==-1&&hitEnd)
						break;
				
				if(find==-1) {
						long len=fread(buffer, 1,bufferSize ,file);
						hitEnd=bufferSize!=len;

						bufferFilePos+=len;
				} else {
						retVal=strLongAppendItem(retVal, find+bufferFilePos);
				}
		}

		return retVal;
}
static void getLineCol(struct diagInst *inst,long where,long *line,long *col) {
		long line2=0;
		for(long i=1;i!=strLongSize(inst->lineStarts);i++) {
				if(inst->lineStarts[i]<=where) {
						line2=i;
						goto found;
				} else
						break;
		}
		
	found: {
		if(line !=NULL)
								*line=line2+1;
		if(col!=NULL)
				*col=where-inst->lineStarts[line2]+1;
		}
}
static long fileSize(FILE *f) {
		fseek(f, 0, SEEK_END);
		long end=ftell(f);
		fseek(f,0,SEEK_SET);
		long start=ftell(f);

		return  end-start;
}
static int longCmp(long a,long b) {
		if(a>b)
				return 1;
		else if(b<a)
				return -1;
		else
				return 0;
				}
static int qouteSort(const void *a,const void *b) {
		const struct diagQoute *A=a;
		const struct diagQoute *B=b;
		int res=longCmp(A->start, B->start);
		if(res)
				return res;
		
		res=longCmp(A->end, B->end);
		return res;
}
static int longPtrCmp(const void *a,const void *b) {
		return longCmp(*(long*)a, *(long*)b);
}
static int textAttrCmp(const void *a,const void *b) {
		return *(enum textAttr*)a-*(enum textAttr*)b;
}
static void qouteLine(struct diagInst *inst,long start,long end,strDiagQoute qoutes) {
		//Line end of line
		long line,col;
		getLineCol(inst,start,&line,&col);
		long lineEnd;
		if(line+1>=strLongSize(inst->lineStarts))
				lineEnd=fileSize(inst->sourceFile);
		else
				lineEnd=inst->lineStarts[line+1];

		//Sort qoutes
		__auto_type clone= strDiagQouteAppendData(NULL, qoutes,strDiagQouteSize(qoutes) );
		qsort(clone, strDiagQouteSize(clone), sizeof(*clone), qouteSort);

		char buffer[lineEnd-start+1];
		fseek(inst->sourceFile,start,SEEK_SET);
		buffer[lineEnd-start]='\0';

		strTextAttr current=NULL;
		strTextAttr attrs=NULL;
		strLong currentlyIn=NULL;
		for(long pos=start;pos<lineEnd;) {
				long newPos;
				strLong hitStarts  __attribute__((cleanup(strLongDestroy)));hitStarts =NULL;
				strLong hitEnds  __attribute__((cleanup(strLongDestroy)));hitEnds =NULL;
				//Find next boundary(start/end) of qoute at pos
				long closestIdx=-1,closestPos=-1;
				for(long i=0;i!=strDiagQouteSize(qoutes);i++) {
						//starts
						if(qoutes[i].start>=pos)
								if(closestIdx-pos<qoutes[i].start-pos) {
										closestIdx=qoutes[i].start;
										closestPos=qoutes[i].start;
												
										if(pos==qoutes[i].start)
												hitStarts=strLongAppendItem(hitStarts, i);
								}

								
						//ends
						if(qoutes[i].end>=pos)
								if(closestIdx-pos<qoutes[i].end-pos) {
										closestIdx=qoutes[i].end;
										closestPos=qoutes[i].end;
												
										if(pos==qoutes[i].end)
												hitEnds=strLongAppendItem(hitEnds, i);
								}
				}
				//If found no boundary ahead,write out rest of line
				if(closestIdx==-1) {
						fprintf(inst->dumpTo, "%s", buffer+pos);
						break;
				}

				//Add hit starts/remove hit ends
				currentlyIn=strLongSetDifference(currentlyIn, hitEnds, longPtrCmp);
				currentlyIn=strLongSetUnion(currentlyIn, hitStarts, longPtrCmp);
						
				//Remove attrs from items in hitEnds from attrs
				for(long i=0;i!=strLongSize(hitEnds);i++) {
						strTextAttr tmp=NULL;
						for(long i2=0;i2!=strTextAttrSize(  qoutes[hitEnds[i]].attrs);i2++)
								tmp=strTextAttrSortedInsert(tmp, qoutes[hitEnds[i]].attrs[i2], textAttrCmp);

						attrs=strTextAttrSetDifference(attrs, tmp, textAttrCmp);
						strTextAttrDestroy(&tmp);
				}

				//Add attrs for starts to attrs
				for(long i=0;i!=strLongSize(hitStarts);i++) {
						strTextAttr tmp=NULL;
						for(long i2=0;i2!=strTextAttrSize(  qoutes[hitStarts[i]].attrs);i2++)
								tmp=strTextAttrSortedInsert(tmp, qoutes[hitStarts[i]].attrs[i2], textAttrCmp);

						attrs=strTextAttrSetUnion(attrs, tmp, textAttrCmp);
						strTextAttrDestroy(&tmp);
				}

				//Set attrs
				setAttrs(inst, inst->dumpTo, attrs,0);
				if(strTextAttrSize(attrs)==0) {
						setAttrs(inst, ATTR_NORMAL,0);
				}

				//Write text from pos to first bound
				for(long i=pos;i!=closestPos;i++) {
						if(i>=lineEnd)
								break;

						fputc(buffer[pos-start], inst->dumpTo);
				}

				pos=closestPos;
		}

		start=end;
		strDiagQouteDestroy(&clone);		
}
void diagPushText(struct diagInst *inst,const char *text) {
		fprintf(inst->dumpTo, "%s",text );
}
void diagPushQoutedText(struct diagInst *inst,long start,long  end) {
		setAttrs(inst,ATTR_BOLD, 0);

		char buffer[end-start+1];
		buffer[end-start]='\0';
		fread(buffer, 1, end-start, inst->sourceFile);
		fprintf(inst->dumpTo, "'%s'", buffer);

		setAttrs(inst,ATTR_NORMAL, 0);
		endAttrs(inst);
}
void diagHighlight(struct diagInst *inst,long start,long end) {
		if(inst->state!=DIAG_NONE) {
				//Chose color based on state
				enum textAttr other=ATTR_BOLD;
				enum textAttr color=ATTR_NORMAL;
				switch(inst->state) {
				case DIAG_ERR:
						color=FG_COLOR_RED;break;
				case DIAG_NOTE:
						color=FG_COLOR_BLUE;
				case DIAG_WARN:
						color=FG_COLOR_YELLOW;break;
				default:;
				}
				enum textAttr attrs[2]={other,color};
				__auto_type count=sizeof(attrs)/sizeof(*attrs);
				strTextAttr attrsVec=strTextAttrAppendData(NULL, attrs, count);

				struct diagQoute qoute;
				qoute.attrs=attrsVec;
				qoute.start=start;
				qoute.end=end;

				inst->stateQoutes=strDiagQouteAppendItem(inst->stateQoutes, qoute);
		}		
}
static void strDiagQouteDestroy2(strDiagQoute *str) {
		for(long i=0;i!=strDiagQouteSize(*str);i++)
				strTextAttrDestroy(&str[0][i].attrs);

		strDiagQouteDestroy(str);
}
//TODO implement included files
void diagEndMsg(struct diagInst *inst) {
		if(inst->state!=DIAG_NONE) {
				qouteLine(inst, inst->stateStart, inst->stateEnd, inst->stateQoutes); //TODO

				strDiagQouteDestroy2(&inst->stateQoutes);
		}
		inst->stateQoutes=NULL;
		inst->state=DIAG_NONE;
}
static void diagStateStart(struct diagInst *inst,long start,long end,const char *text,enum textAttr color) {
if(inst->state!=DIAG_NONE)
				diagEndMsg(inst);
		
		long where=mapToSource(start, inst->mappings, 0);
	
		setAttrs(inst, inst->dumpTo, ATTR_BOLD,0);
		long ln,col;
		getLineCol(inst, start, &ln, &col);
		fprintf(inst->dumpTo, "%s:%li,%li: ", inst->fileName, ln+1,col+1);
		endAttrs(inst);

		setAttrs(inst, inst->dumpTo, color,ATTR_BOLD,0);
		fprintf(inst->dumpTo, "%s: ",text);
		endAttrs(inst);

		inst->state=DIAG_ERR;
		inst->stateStart=start;
		inst->stateEnd=end;
}
void diagErrorStart(struct diagInst *inst,long start,long end) {
		diagStateStart(inst,start,end,"error",FG_COLOR_RED);
}
void diagNoteStart(struct diagInst *inst,long start,long end) {
		diagStateStart(inst,start,end,"note",FG_COLOR_BLUE);
}
void diagWarnStart(struct diagInst *inst,long start,long end) {
		diagStateStart(inst,start,end,"warning",FG_COLOR_YELLOW);
}
struct diagInst *diagInstCreate(enum outputType type,const strTextModify mappings,const char *fileName,FILE *sourceFile,FILE *dumpToFile) {
		struct diagInst retVal;
		retVal.dumpTo=dumpToFile;
		retVal.diagType=type;
		retVal.fileName=strClone(fileName);
		retVal.lineStarts=fileLinesIndexes(sourceFile);
		retVal.mappings=strTextModifyAppendData(NULL, mappings, strTextModifySize(mappings));
		retVal.state=DIAG_NONE;
		retVal.stateQoutes=NULL;
		retVal.sourceFile=sourceFile;

		struct diagInst *r=malloc(sizeof(retVal));
		memcpy(r, &retVal, sizeof(retVal));
		return r;
}
void diagInstDestroy(struct diagInst *inst) {
		free(inst->fileName);
		strTextModifyDestroy(&inst->mappings);
		strLongDestroy(&inst->lineStarts);
		strDiagQouteDestroy2(&inst->stateQoutes);
		free(inst);
}
