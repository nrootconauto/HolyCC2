#include <textMapper.h>
#include <stdio.h>
#include <preprocessor.h>
enum outputType {
		DIAG_ANSI_TERM,
		DIAG_DOL_DOC,
};
struct  diagInst;
void diagHighlight(long start,long end);
void diagEndMsg();
void diagErrorStart(long start,long end) ;
void diagPushText(const char *text);
void diagPushQoutedText(long start,long  end) ;
void diagWarnStart(long start,long end);
void diagNoteStart(long start,long end);
void diagErrorStart(long start,long end);
void diagInstCreate(enum outputType type,const strTextModify mappings,const char *fileName,FILE *sourceFile,FILE *dumpToFile);
