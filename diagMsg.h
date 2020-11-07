#include <textMapper.h>
#include <stdio.h>
enum outputType {
		DIAG_ANSI_TERM,
		DIAG_DOL_DOC,
};
struct  diagInst;
void diagHighlight(struct diagInst *inst,long start,long end);
void diagEndMsg(struct diagInst *inst);
void diagErrorStart(struct diagInst *inst,long start,long end) ;
void diagPushText(struct diagInst *inst,const char *text);
void diagPushQoutedText(struct diagInst *inst,long start,long  end) ;
void diagWarnStart(struct diagInst *inst,long start,long end);
void diagNoteStart(struct diagInst *inst,long start,long end);
void diagErrorStart(struct diagInst *inst,long start,long end);
void diagInstDestroy(struct diagInst *inst);
struct diagInst *diagInstCreate(enum outputType type,const strTextModify mappings,const char *fileName,FILE *sourceFile,FILE *dumpToFile);
