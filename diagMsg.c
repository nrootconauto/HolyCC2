#include <str.h>
#include <stdarg.h>
#include <stdio.h>
#include <diagMsg.h>
#include <ncurses.h>
#include <stdlib.h>
STR_TYPE_DEF(long,Long);
STR_TYPE_FUNCS(long,Long);
#define TEXT_WIDTH 60
enum outputType {
		DIAG_ANSI_TERM,
		DIAG_HTML,
};
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

STR_TYPE_DEF(enum textAttr, TextAttr);
STR_TYPE_FUNCS(enum textAttr, TextAttr);
STR_TYPE_DEF(strTextAttr, StrTextAttr);
STR_TYPE_FUNCS(strTextAttr, StrTextAttr);

struct diagInst {
		strLong lineStarts;
		enum outputType diagType;strTextModify mappings;
		enum diagState state;
		long stateStart,stateEnd;
		FILE *dumpTo;
		FILE *sourceFile;
		char *fileName;
}; 

static void putAttrANSI(FILE *f,const strTextAttr attrs) {
		
		for(long i=0;i!=strTextAttrSize(attrs);i++) {
				__auto_type a=attrs[i];
				switch(a) {
				case ATTR_HIDDEN:
						attrset(A_INVIS);break;
						case ATTR_BLINK:
								attrset(A_BLINK);break;
case ATTR_UNDER:
		attrset(A_UNDERLINE);break;
				case ATTR_BOLD:
						attrset(A_BOLD);break;
				case ATTR_NORMAL:
						attrset(A_NORMAL);break;
				case FG_COLOR_BLACK:
						attrset(COLOR_BLACK);break;
				case FG_COLOR_RED:
						attrset(COLOR_RED);break;
				case FG_COLOR_GREEN:
						attrset(COLOR_GREEN);break;
				case FG_COLOR_YELLOW:
						attrset(COLOR_YELLOW);break;
				case FG_COLOR_BLUE:
						attrset(COLOR_BLUE);break;
				case FG_COLOR_MAGENTA:
						attrset(COLOR_MAGENTA);break;
				case FG_COLOR_WHITE:
						attrset(COLOR_WHITE);break;
				case ATTR_END:break;
				}
		}
}
static void endAttrANSI(struct diagInst *inst,FILE *f)  {
		long newSize=strStrTextAttrSize(inst->attrsStack);
		if(!newSize)
				return ;

		strTextAttrDestroy(&inst->attrsStack[newSize-1]);
		
		inst->attrsStack=strStrTextAttrResize(inst->attrsStack, --newSize);
		if(newSize>0) {
				putAttrANSI(f,inst->attrsStack[ newSize-1]);
		} else {
				attrset(A_NORMAL);
		}
}
static void endAttrs(struct diagInst *inst) {
		if(inst->diagType==DIAG_ANSI_TERM)
				endAttrANSI(inst, inst->dumpTo);
}
static void setAttrs(struct diagInst *inst,FILE *f,...) {
		va_list list;
		va_start(list, f);
		
		strTextAttr attrs=NULL;
		for(int firstRun=1;;firstRun=0) {
				attrs=strTextAttrAppendItem(attrs, va_arg(list,enum textAttr));
		}

		if(inst->diagType==DIAG_ANSI_TERM) {
				putAttrANSI(f, attrs);
		}
		inst->attrsStack=strStrTextAttrAppendItem(inst->attrsStack,attrs);
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
struct diagQoute {
		strTextAttr attrs;
		long start,end;
};
STR_TYPE_DEF(struct diagQoute, DiagQoute);
STR_TYPE_FUNCS(struct diagQoute, DiagQoute);
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
static void qouteLines(struct diagInst *inst,long start,long end,strDiagQoute qoutes) {
		long curPos=start;
		for(;curPos<end;) {
		long line,col;
		getLineCol(inst,start,&line,&col);
		long lineEnd;
		if(line+1>=strLongSize(inst->lineStarts))
				lineEnd=fileSize(inst->sourceFile);
		else
				lineEnd=inst->lineStarts[line+1];

		__auto_type clone= strDiagQouteAppendData(NULL, qoutes,strDiagQouteSize(qoutes) );
		qsort(clone, strDiagQouteSize(clone), sizeof(*clone), qouteSort);

		char buffer[lineEnd-start+1];
		fseek(inst->sourceFile,start,SEEK_SET);
		buffer[lineEnd-start]='\0';

		strTextAttr current=NULL;
		long qouteIndex=0; //inc'd ahead
		strTextAttr attrs=NULL;
		strLong currentlyIn=NULL;
		for(long pos=start;pos<lineEnd;) {
				if(qouteIndex+1>=strDiagQouteSize(qoutes)) {
						//Write rest of file
						fwrite(buffer+pos, 1, lineEnd-pos, inst->dumpTo) ;
						break;
				}

				long newPos;
				if(strLongSize(currentlyIn)==0) {
						newPos=qoutes[qouteIndex].start;
						
						fwrite(buffer+pos, 1, newPos-pos, inst->dumpTo) ;
				} else if() {
						strLong hitStarts  __attribute__((cleanup(strLongDestroy)));hitStarts =NULL;
						strLong hitEnds  __attribute__((cleanup(strLongDestroy)));hitEnds =NULL;
						//Find next boundary(start/end) of qoute at pos
						long closestIdx=-1;
						for(long i=0;i!=strDiagQouteSize(qoutes);i++) {
								//starts
								if(qoutes[i].start>=pos)
										if(closestIdx-pos<qoutes[i].start-pos) {
												closestIdx=qoutes[i].start;
												
												if(pos==qoutes[i].start)
														hitStarts=strLongAppendItem(hitStarts, i);
										}

								
								//ends
								if(qoutes[i].end>=pos)
										if(closestIdx-pos<qoutes[i].end-pos) {
												closestIdx=qoutes[i].end;
												
												if(pos==qoutes[i].end)
														hitEnds=strLongAppendItem(hitEnds, i);
										}
						}

						//Add hit starts/remove hit ends
						currentlyIn=strLongSetDifference(currentlyIn, hitEnds, longPtrCmp);
						
						//Remove attrs from items in hitEnds from attrs
						for(long i=0;i!=strLongSize(hitEnds);i++) {
								strTextAttr tmp=NULL;
								for(long i2=0;i2!=strTextAttrSize(  qoutes[hitStarts[i]].attrs);i2++)
										tmp=strTextAttrSortedInsert(tmp, qoutes[hitStarts[i]].attrs[i2], textAttrCmp);

								attrs=strTextAttrSetDifference(attrs, tmp, textAttrCmp);
						}
				}
		}

		start=end;
		strDiagQouteDestroy(&clone);
		}
}
//TODO implement included files
void diagEndMsg(struct diagInst *inst,long start,long *end) {
		switch(inst->state) {
		case DIAG_ERR:
				setAttrs(inst, inst->dumpTo, FG_COLOR_RED);break;
		case DIAG_NOTE:
				setAttrs(inst, inst->dumpTo, FG_COLOR_BLUE);break;
				case DIAG_WARN:
				setAttrs(inst, inst->dumpTo, FG_COLOR_YELLOW);break;
				default:
						;
		}
		inst->state=DIAG_NONE;
}
static void diagErrorStart(struct diagInst *inst,long start,long end) {
		long where=mapToSource(start, inst->mappings, 0);
	
		setAttrs(inst, inst->dumpTo, ATTR_BOLD,0);
		long ln,col;
		getLineCol(inst, start, &ln, &col);
		fprintf(inst->dumpTo, "%s:%li,%li: ", inst->fileName, ln+1,col+1);
		endAttrs(inst);

		setAttrs(inst, inst->dumpTo, FG_COLOR_RED,0);
		fprintf(inst->dumpTo, "error: ");
		endAttrs(inst);

		inst->state=DIAG_ERR;
		inst->stateStart=start;
		inst->stateEnd=end;
}
