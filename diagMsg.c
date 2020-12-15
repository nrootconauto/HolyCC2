#include <assert.h>
#include <diagMsg.h>
#include <hashTable.h>
#include <parserA.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <str.h>
static char *strClone(const char *text) {
	long len = strlen(text);
	char *retVal = malloc(len + 1);
	strcpy(retVal, text);

	return retVal;
}
STR_TYPE_DEF(long, Long);
STR_TYPE_FUNCS(long, Long);
STR_TYPE_DEF(enum textAttr, TextAttr);
struct diagQoute {
	strTextAttr attrs;
	long start, end;
};
STR_TYPE_DEF(struct diagQoute, DiagQoute);
STR_TYPE_FUNCS(struct diagQoute, DiagQoute);

#define TEXT_WIDTH 60
enum textAttr {
	ATTR_END = 0,
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
	enum diagState state;
	long stateStart, stateEnd;
	FILE *dumpTo;
	FILE *sourceFile;
	char *fileName;
	strDiagQoute stateQoutes;
};
MAP_TYPE_DEF(struct diagInst, Inst);
MAP_TYPE_FUNCS(struct diagInst, Inst);
static mapInst insts = NULL;
// TODO implement file mappings
static strFileMappings fileMappings;
static strTextModify mappings;
static struct diagInst *currentInst = NULL;
static int errCount = 0;
static struct diagInst *diagInstByPos(long where) {
	if (where == diagInputSize()) {
		if (where - 1 >= 0) {
			where--;
		}
	}

	__auto_type name = fileNameFromPos(fileMappings, where);

	__auto_type find = mapInstGet(insts, name);
	assert(find != NULL);
	return find;
}
static void putAttrANSI(FILE *f, const strTextAttr attrs) {
	fprintf(f, "\e[");
	for (long i = 0; i != strTextAttrSize(attrs); i++) {
		if (i != 0)
			fprintf(f, ";");

		__auto_type a = attrs[i];
		switch (a) {
		case ATTR_REVERSE:
			fprintf(f, "7");
			break;
		case ATTR_HIDDEN:
			fprintf(f, "8");
			break;
		case ATTR_BLINK:
			fprintf(f, "5");
			break;
		case ATTR_UNDER:
			fprintf(f, "4");
			break;
		case ATTR_BOLD:
			fprintf(f, "1");
			break;
		case ATTR_NORMAL:
			fprintf(f, "0");
			break;
		case FG_COLOR_BLACK ... FG_COLOR_WHITE:
			fprintf(f, "%i", 30 + a - FG_COLOR_BLACK);
			break;
		case BG_COLOR_BLACK ... BG_COLOR_WHITE:
			fprintf(f, "%i", 40 + a - BG_COLOR_BLACK);
			break;
		case ATTR_END:
			break;
		}
	}
	fprintf(f, "m");
}
static void endAttrs(struct diagInst *inst) {
	if (inst->diagType == DIAG_ANSI_TERM)
		fprintf(inst->dumpTo, "\e[0m");
}
static void setAttrsVec(struct diagInst *inst, strTextAttr attrs) {
	if (inst->diagType == DIAG_ANSI_TERM) {
		putAttrANSI(inst->dumpTo, attrs);
	}
}
static void setAttrs(struct diagInst *inst, ...) {
	va_list list;
	va_start(list, inst);

	strTextAttr attrs = NULL;
	for (int firstRun = 1;; firstRun = 0) {
		__auto_type i = va_arg(list, enum textAttr);
		if (i == 0)
			break;
		attrs = strTextAttrAppendItem(attrs, i);
	}

	if (inst->diagType == DIAG_ANSI_TERM) {
		putAttrANSI(inst->dumpTo, attrs);
	}
	va_end(list);
}

static long firstOf(const char *buffer, long index, const char *chrs) {
	int len = strlen(chrs);
	char *minFind = NULL;
	for (int i = 0; i != len; i++) {
		__auto_type find = strchr(buffer + index, chrs[i]);
		if (find != NULL)
			if (minFind > find || minFind == NULL)
				minFind = find;
	}

	if (minFind != NULL)
		return (minFind - buffer) / sizeof(char);

	return -1;
}
static strLong fileLinesIndexes(FILE *file) {
	fseek(file, 0, SEEK_END);
	long end = ftell(file);
	fseek(file, 0, SEEK_SET);
	long start = ftell(file);

	// Reserve by "average" chars per line
	strLong retVal = strLongReserve(NULL, (end - start) / 64);
	retVal = strLongAppendItem(retVal, 0);

	const long bufferSize = 1024;
	char buffer[bufferSize + 1];
	buffer[bufferSize] = '\0';
	fseek(file, 0, SEEK_SET);
	fread(buffer, 1, bufferSize, file);

	long bufferOffset = 0;
	long bufferFilePos = 0;
	long find;
	for (;;) {
		long find = firstOf(buffer, bufferOffset, "\n\r");

		if (find == -1) {
			long len = fread(buffer, 1, bufferSize, file);

			bufferFilePos += len;

			if (len != bufferSize)
				break;
		} else {
			retVal = strLongAppendItem(retVal, find + bufferFilePos + 1);
			bufferOffset = find + 1;
		}
	}

	return retVal;
}
static void getLineCol(struct diagInst *inst, long where, long *line,
                       long *col) {
	long line2 = 0;
	for (long i = 1; i != strLongSize(inst->lineStarts); i++) {
		if (inst->lineStarts[i] < where) {
			line2 = i;
			goto found;
		} else
			break;
	}

found : {
	if (line != NULL)
		*line = line2;
	if (col != NULL)
		*col = where - inst->lineStarts[line2];
}
}
static long fileSize(FILE *f) {
	fseek(f, 0, SEEK_END);
	long end = ftell(f);
	fseek(f, 0, SEEK_SET);
	long start = ftell(f);

	return end - start;
}
static int longCmp(long a, long b) {
	if (a > b)
		return 1;
	else if (b < a)
		return -1;
	else
		return 0;
}
static int qouteSort(const void *a, const void *b) {
	const struct diagQoute *A = a;
	const struct diagQoute *B = b;
	int res = longCmp(A->start, B->start);
	if (res)
		return res;

	res = longCmp(A->end, B->end);
	return res;
}
static int longPtrCmp(const long *a, const long *b) { return longCmp(*a, *b); }
static int textAttrCmp(const enum textAttr *a, const enum textAttr *b) {
	return *a - *b;
}
static long goBeforeNewLine(FILE *fp, long where) {
	long retVal = where;
	__auto_type originalOffset = ftell(fp);

	if (where - 1 > 0) {
		fseek(fp, where - 1, SEEK_SET);

		for (char chr = fgetc(fp); chr == '\n' || chr == '\r'; chr = fgetc(fp)) {
			// Move back -1 to compensate for moving forward on fgetc(fp)
			if (ftell(fp) - 2 >= 0) {
				fseek(fp, -2, SEEK_CUR);
				retVal = ftell(fp);
			} else
				break;
		}
	}
	// Measure from start of file
	fseek(fp, 0, SEEK_SET);
	long fileStart = ftell(fp);

	// Seek to original pos
	fseek(fp, originalOffset, SEEK_SET);

	return retVal - fileStart;
}
static void qouteLine(struct diagInst *inst, long start, long end,
                      strDiagQoute qoutes) {
	// Line end of line
	long line, col;
	getLineCol(inst, end, &line, &col);
	long lineEnd;
	if (line + 1 >= strLongSize(inst->lineStarts))
		lineEnd = fileSize(inst->sourceFile);
	else
		lineEnd = goBeforeNewLine(inst->sourceFile, inst->lineStarts[line + 1]) + 1;

	// Start of line
	getLineCol(inst, start, &line, &col);
	long lineStart = inst->lineStarts[line];

	// Sort qoutes
	__auto_type clone =
	    strDiagQouteAppendData(NULL, qoutes, strDiagQouteSize(qoutes));
	qsort(clone, strDiagQouteSize(clone), sizeof(*clone), qouteSort);

	char buffer[lineEnd - lineStart + 1];
	fseek(inst->sourceFile, lineStart, SEEK_SET);
	buffer[lineEnd - lineStart] = '\0';
	fread(buffer, 1, lineEnd - lineStart, inst->sourceFile);

	strTextAttr current = NULL;
	strTextAttr attrs = NULL;
	strLong currentlyIn = NULL;
	long oldPos = lineStart;
	for (long searchPos = lineStart; searchPos < lineEnd;) {
		long newPos;
		strLong hitStarts;
		hitStarts = NULL;
		strLong hitEnds;
		hitEnds = NULL;
		// Find next boundary(start/end) of qoute at pos
		// closesPos is made to point to end of buffer so all compares with initial
		// value will be lower
		long closestIdx = -1, closestPos = lineEnd + 1;
		for (long i = 0; i != strDiagQouteSize(qoutes); i++) {
			// starts
			if (qoutes[i].start >= searchPos)
				if (closestPos - searchPos >= qoutes[i].start - searchPos) {
					closestIdx = i;
					closestPos = qoutes[i].start;

					// If closer,will hit the closest so clear hitStarts
					if (closestPos - searchPos > qoutes[i].start - searchPos)
						hitStarts = strLongResize(hitStarts, 0);

					hitStarts = strLongAppendItem(hitStarts, i);
				}

			// ends
			if (qoutes[i].end >= searchPos)
				if (closestPos - searchPos >= qoutes[i].end - searchPos) {
					closestIdx = i;
					closestPos = qoutes[i].end;

					// If closer,will hit the closest so clear hitEnds
					if (closestPos - searchPos > qoutes[i].end - searchPos) {
						// End is closest than start as ends are checked after starts
						hitStarts = strLongResize(hitStarts, 0);
						hitEnds = strLongResize(hitEnds, 0);
					}

					hitEnds = strLongAppendItem(hitEnds, i);
				}
		}
		// If found no boundary ahead,break
		if (closestIdx == -1) {
			break;
		}

		// Write out text from old position to new position
		if (closestIdx != -1)
			for (long i = oldPos; i != closestPos; i++)
				fputc(buffer[i - lineStart], currentInst->dumpTo);

		// Add hit starts/remove hit ends
		currentlyIn = strLongSetDifference(currentlyIn, hitEnds, longPtrCmp);
		currentlyIn = strLongSetUnion(currentlyIn, hitStarts, longPtrCmp);

		// Remove attrs from items in hitEnds from attrs
		for (long i = 0; i != strLongSize(hitEnds); i++) {
			strTextAttr tmp = NULL;
			for (long i2 = 0; i2 != strTextAttrSize(qoutes[hitEnds[i]].attrs); i2++)
				tmp = strTextAttrSortedInsert(tmp, qoutes[hitEnds[i]].attrs[i2],
				                              textAttrCmp);

			attrs = strTextAttrSetDifference(attrs, tmp, textAttrCmp);
		}

		// Add attrs for starts to attrs
		for (long i = 0; i != strLongSize(hitStarts); i++) {
			strTextAttr tmp = NULL;
			for (long i2 = 0; i2 != strTextAttrSize(qoutes[hitStarts[i]].attrs); i2++)
				tmp = strTextAttrSortedInsert(tmp, qoutes[hitStarts[i]].attrs[i2],
				                              textAttrCmp);

			attrs = strTextAttrSetUnion(attrs, tmp, textAttrCmp);
		}

		// Set attrs
		setAttrsVec(inst, attrs);

		if (strTextAttrSize(attrs) == 0) {
			setAttrs(inst, ATTR_NORMAL, 0);
		}

		oldPos = closestPos;
		searchPos = closestPos + 1;
	}

	// Write out rest of text
	fprintf(inst->dumpTo, "%s", buffer + oldPos - lineStart);
	endAttrs(inst);

	start = end;
}
void diagPushText(const char *text) {
	assert(currentInst != NULL);
	setAttrs(currentInst, ATTR_BOLD, 0);
	fprintf(currentInst->dumpTo, "%s", text);
	endAttrs(currentInst);
}
void diagPushQoutedText(long start, long end) {
	assert(currentInst != NULL);

	setAttrs(currentInst, ATTR_BOLD, 0);

	char buffer[end - start + 1];
	buffer[end - start] = '\0';
	fseek(currentInst->sourceFile, start, SEEK_SET);
	fread(buffer, 1, end - start, currentInst->sourceFile);
	fprintf(currentInst->dumpTo, "'%s'", buffer);

	setAttrs(currentInst, ATTR_NORMAL, 0);
	endAttrs(currentInst);
}
void diagHighlight(long start, long end) {
	assert(NULL != currentInst);

	if (currentInst->state != DIAG_NONE) {
		// Chose color based on state
		enum textAttr other = ATTR_BOLD;
		enum textAttr color = ATTR_NORMAL;
		switch (currentInst->state) {
		case DIAG_ERR:
			color = FG_COLOR_RED;
			break;
		case DIAG_NOTE:
			color = FG_COLOR_BLUE;
		case DIAG_WARN:
			color = FG_COLOR_YELLOW;
			break;
		default:;
		}
		enum textAttr attrs[2] = {other, color};
		__auto_type count = sizeof(attrs) / sizeof(*attrs);
		strTextAttr attrsVec = strTextAttrAppendData(NULL, attrs, count);

		struct diagQoute qoute;
		qoute.attrs = attrsVec;
		qoute.start = start;
		qoute.end = end;

		currentInst->stateQoutes =
		    strDiagQouteAppendItem(currentInst->stateQoutes, qoute);
	}
}
// TODO implement included files
void diagEndMsg() {
	assert(currentInst != NULL);

	if (currentInst->state != DIAG_NONE) {
		fprintf(currentInst->dumpTo, "\n```\n");
		qouteLine(currentInst, currentInst->stateStart, currentInst->stateEnd,
		          currentInst->stateQoutes); // TODO
		fprintf(currentInst->dumpTo, "\n```");
	}
	currentInst->stateQoutes = NULL;
	currentInst->state = DIAG_NONE;
	endAttrs(currentInst);
	fprintf(currentInst->dumpTo, "\n");

	currentInst = NULL;
}
static void diagStateStart(long start, long end, enum diagState state,
                           const char *text, enum textAttr color) {
	currentInst = diagInstByPos(start);
	assert(currentInst != NULL);

	if (currentInst->state != DIAG_NONE)
		diagEndMsg();

	// 1st insert is the initial source so ignore initial source
	long where = mapToSource(start, mappings, 1);

	setAttrs(currentInst, color, ATTR_BOLD, 0);
	long ln, col;
	getLineCol(currentInst, start, &ln, &col);
	fprintf(currentInst->dumpTo, "%s:%li,%li: ", currentInst->fileName, ln + 1,
	        col + 1);
	endAttrs(currentInst);

	setAttrs(currentInst, currentInst->dumpTo, color, ATTR_BOLD, 0);
	fprintf(currentInst->dumpTo, "%s: ", text);
	endAttrs(currentInst);

	currentInst->state = state;
	currentInst->stateStart = start;
	currentInst->stateEnd = end;
}
void diagErrorStart(long start, long end) {
	errCount++;
	diagStateStart(start, end, DIAG_ERR, "error", FG_COLOR_RED);
}
void diagNoteStart(long start, long end) {
	diagStateStart(start, end, DIAG_NOTE, "note", FG_COLOR_BLUE);
}
void diagWarnStart(long start, long end) {
	diagStateStart(start, end, DIAG_WARN, "warning", FG_COLOR_YELLOW);
}
static void destroyDiags() __attribute__((destructor));
void diagInstCreate(enum outputType type, const strFileMappings __fileMappings,
                    const strTextModify __mappings, const char *fileName,
                    FILE *dumpToFile) {
	destroyDiags();
	mappings = __mappings;
	fileMappings = __fileMappings;

	insts = mapInstCreate();
	for (long i = 0; i != strFileMappingsSize(fileMappings); i++) {
		FILE *file = fopen(fileMappings[i].fileName, "r");

		struct diagInst retVal;
		retVal.dumpTo = dumpToFile;
		retVal.diagType = type;
		retVal.fileName = strClone(fileMappings[i].fileName);
		retVal.lineStarts = fileLinesIndexes(file);
		retVal.state = DIAG_NONE;
		retVal.stateQoutes = NULL;
		retVal.sourceFile = file;

		mapInstInsert(insts, fileName, retVal);
	}
}
static void diagInstDestroy(struct diagInst *inst) { fclose(inst->sourceFile); }
static void destroyDiags() {
	if (insts != NULL)
		mapInstDestroy(insts, (void (*)(void *))diagInstDestroy);
}
long diagInputSize() {
	long last = strFileMappingsSize(fileMappings) - 1;
	if (last >= 0)
		return fileMappings[last].fileEndOffset;

	return 0;
}
