#include <assert.h>
#include <ctype.h>
#include <hashTable.h>
#include <preprocessor.h>
#include <stdio.h>
#include <str.h>
#include <string.h>
#include <stringParser.h>
#include <unistd.h>
// TODO implement exe,if,ifdef,ifndef
static __thread FILE *preprocessedSource = NULL;
static void __vecDestroy2(struct __vec **vec) { __vecDestroy(*vec); }
static void fileDestroy(FILE **file) { fclose(*file); }
MAP_TYPE_DEF(struct defineMacro, DefineMacro);
MAP_TYPE_FUNCS(struct defineMacro, DefineMacro);
static __thread long sourceMappingStart;
static __thread long sourceMappingEnd;
static __thread int includeFileDepth = 0;
static __thread long lineStart;
static __thread long sourcePos;
static __thread strSourceMapping sourceMappings = NULL;
static void enterIncludeFile() { includeFileDepth++; }
static void leaveIncludeFile() { includeFileDepth--; }
static int isInChanged(long pos) {
	for (long i = 0; i != strSourceMappingSize(sourceMappings); i++) {
		__auto_type item = &sourceMappings[i];
		if (pos < item->processedEnd && pos >= item->processedStart)
			return 1;
		if (item->processedStart > pos)
			break;
	}
	return 0;
}
static void startSourceMapping(long col, long sourceSpan) {
	if (includeFileDepth || isInChanged(col + lineStart))
		return;
	sourceMappingStart = col + lineStart;

	sourcePos += sourceSpan;
}
static void endSourceMapping(long col) {
	if (includeFileDepth || isInChanged(col + lineStart))
		return;
	struct sourceMapping mapping;
	mapping.sourcePos = sourcePos;
	mapping.processedStart = sourceMappingStart;
	mapping.processedEnd = col + lineStart;
	sourceMappings = strSourceMappingAppendItem(sourceMappings, mapping);

	sourcePos += mapping.processedEnd - mapping.processedStart;
}

static void expandDefinesInRange(struct __vec **retVal, mapDefineMacro defines,
                                 long where, long end, int *expanded, int *err);
static void expandNextWord(struct __vec **retVal, mapDefineMacro defines,
                           long where, int *expanded, int *err);
static struct __vec *createNullTerminated(struct __vec *vec) {
	__auto_type nameLen = __vecSize(vec);
	__auto_type retVal = __vecResize(NULL, nameLen + 1);
	memcpy(retVal, vec, nameLen);
	nameLen[(char *)retVal] = '\0';
	return retVal;
}
static void *findEndOfLine(const struct __vec *text, long pos) {
	void *retVal = (void *)text + pos;
	for (; retVal < (void *)text + __vecSize(text); retVal++) {
		__auto_type chr = *(char *)retVal;
		if (chr == '\r' || chr == '\n' || chr == '\0')
			break;
	}
	if (retVal == (void *)text + __vecSize(text))
		return retVal - 1;

	return retVal;
}
void *findNextLine(const struct __vec *text, long pos) {
	void *retVal = findEndOfLine(text, pos);
	for (; retVal < (void *)text + __vecSize(text); retVal++) {
		__auto_type chr = *(char *)retVal;
		if ((chr != '\r' && chr != '\n') || chr == '\0')
			break;
	}
	return retVal;
}
static void *findSkipStringAndComments(
    const struct __vec *text, long pos, const void *data,
    void *(*findFunc)(const void *, const struct __vec *text, long pos)) {
	__auto_type end = (void *)text + __vecSize(text);
loop:;
	void *find = findFunc(data, text, pos);

	void *findCommentS = strstr((void *)text + pos, "//");
	void *findCommentM = strstr((void *)text + pos, "/*");
	findCommentS = (findCommentS == NULL) ? end : findCommentS;
	findCommentM = (findCommentM == NULL) ? end : findCommentM;
	__auto_type lesserComment =
	    (findCommentS < findCommentM) ? findCommentS : findCommentM;

	void *findStr1 = strchr((void *)text + pos, '\'');
	void *findStr2 = strchr((void *)text + pos, '"');
	findStr1 = (findStr1 == NULL) ? end : findStr1;
	findStr2 = (findStr2 == NULL) ? end : findStr2;
	__auto_type lesserStr = (findStr1 < findStr2) ? findStr1 : findStr2;

	if (find < lesserStr && find < lesserComment)
		return find;

	if (lesserComment != end && lesserComment < lesserStr) {
		// Skip comments
		if (lesserComment == findCommentS) {
			// Single-line
			pos = findCommentS - (void *)text;
			pos = findNextLine(text, pos) - (void *)text;
		} else if (lesserComment == findCommentM) {
			// Multiline
			void *otherSide = strstr((void *)text, "*/");
			if (otherSide == NULL)
				return NULL;
			else
				return otherSide + strlen("*/");
		}
	} else if (lesserStr != end) {
		// Skip strings
		pos = findStr1 - (void *)text;
		long endOfString;
		int err;
		stringParse(text, pos, &endOfString, NULL, &err);
		if (err)
			return NULL;
	} else {
		return NULL; // Will never reach here
	}
	goto loop;
	return NULL;
}
static void *chrFind(const void *a, const struct __vec *b, long pos) {
	return strstr((void *)b + pos, a);
}
static void *skipNonMacro(const struct __vec *text, long pos) {
	return findSkipStringAndComments(text, pos, "#", chrFind);
}
static void *wordFind(const void *a, const struct __vec *text, long pos) {
	const mapDefineMacro map = (const mapDefineMacro)a;
	__auto_type b = (void *)text + pos;
	__auto_type textEnd = b + __vecSize(text);

	while (b != textEnd) {
		// Check if charactor before is a word charactor,if so quit search
		if (b != text) // Check if a start
			if (isalnum(-1 [(char *)b]))
				return NULL;
		if (isalpha(*(char *)b)) {
			__auto_type original = b;
			__auto_type end = original;
			while (isalnum(*(char *)end) && end < textEnd)
				end++;

			char buffer[end - original + 1];
			memcpy(buffer, original, end - original);
			buffer[end - original] = '\0';

			return b;
		}

	next:
		// Skip word then look for next word
		while (b < textEnd)
			if (isalnum(*(char *)b))
				b++;
			else
				break;
		while (b < textEnd)
			if (!isalnum(*(char *)b))
				b++;
			else
				break;

		continue;
	}
	return NULL;
}
static void *findNextWord(const struct __vec *text, long pos) {
	return findSkipStringAndComments(text, pos, NULL, wordFind);
}
static void *skipWhitespace(struct __vec *text, long pos) {
	__auto_type len = __vecSize(text);
	void *where;
	for (where = (void *)text + pos; where < (void *)text + len; where++) {
		__auto_type chr = *(char *)where;
		if (isblank(chr))
			continue;
		if (chr == '\n')
			continue;
		if (chr == '\r')
			continue;
		break;
	}
	return where;
}
static int expectMacroAndSkip(const char *macroText, struct __vec **text,
                              mapDefineMacro defines, long pos, long *end,
                              int *err) {
	if (err != NULL)
		*err = 0;

	const char *text2 = *(const char **)text;
	if (*text2 != '#')
		return 0;

	pos = skipWhitespace(*text, pos + 1) - *(const void **)text;

	expandNextWord(text, defines, pos, NULL, err);
	text2 = *(const char **)text;

	__auto_type len = strlen(macroText);
	if (__vecSize(*text) - pos < len)
		return 0;

	if (pos + len < __vecSize(*text))
		if (isalnum((pos + len)[*(const char **)text]))
			return 0;

	if (end != NULL)
		*end = pos + len;
	return 0 == strncmp(*(void **)text + pos, macroText, len);
}
static int includeMacroLex(struct __vec **text_, mapDefineMacro defines,
                           long pos, long *end, struct includeMacro *result,
                           int *err) {

	if (err != NULL)
		*err = 0;

	if (!expectMacroAndSkip("include", text_, defines, pos, &pos, err))
		return 0;

	expandNextWord(text_, defines, pos, NULL, err);
	struct __vec *text = *(text_);
	if (err != NULL)
		if (*err)
			return 0;

	pos = skipWhitespace(text, pos) - (void *)text;

	struct parsedString filename;
	if (!stringParse(text, pos, end, &filename, err))
		goto malformed;

	struct includeMacro retVal;
	retVal.fileName =
	    __vecAppendItem(NULL, (char *)filename.text, __vecSize(filename.text));
	if (result != NULL)
		*result = retVal;
	return 1;
malformed : {
	*err = 1;
	return 0;
}
}
static void defineMacroDestroy(void *macro) {
	struct defineMacro *macro2 = macro;
	__vecDestroy(macro2->name);
	__vecDestroy(macro2->text);
}
static int defineMacroLex(struct __vec **text_, mapDefineMacro defines,
                          long pos, long *end, struct defineMacro *result,
                          int *err) {
	if (!expectMacroAndSkip("define", text_, defines, pos, &pos, err))
		return 0;
	if (err != NULL)
		if (*err)
			return 0;

	struct __vec *text = *text_;
	pos = skipWhitespace(text, pos) - (void *)text;

	int len = 0;
	while (isalnum((pos + len)[(char *)text]))
		len++;

	if (isdigit(pos[(char *)text]))
		goto malformed;

	__auto_type name = __vecAppendItem(NULL, &pos[(char *)text], len);
	name = __vecAppendItem(name, "\0", 1);

	pos += len;
	pos = skipWhitespace(text, pos) - (void *)text;

	__auto_type end2 = findEndOfLine(text, pos) - (void *)text;
	// Ignore whitespace from end2
	for (end2--; isblank(end2[(char *)text]); end2--)
		;
	end2++; // end2 points to last non-blank,so move forard to last blank

	struct __vec *replacement =
	    __vecAppendItem(NULL, &pos[(char *)text], end2 - pos);
	if (end != NULL)
		*end = findEndOfLine(text, pos) - (void *)text;
	replacement = __vecAppendItem(replacement, "\0", 1);

	struct defineMacro retVal;
	retVal.name = name;
	retVal.text = replacement;
	if (result != NULL)
		*result = retVal;
	return 1;
malformed : {
	*err = 1;
	return 0;
}
}
void includeMacroDestroy(struct includeMacro *macro) {
	__vecDestroy(macro->fileName);
}
/**
 * This takes a processed text(result of preprocessor),then maps it to source
 */
long mappedPosition(const strSourceMapping mapping, long processedPos) {
	long processedStart = 0;
	long processedPrevEnd = 0;
	for (long i = 0; i != strSourceMappingSize(mapping); i++) {
		if (mapping[i].processedStart <= processedPos &&
		    mapping[i].processedEnd >= processedPos)
			return (processedPos - mapping[i].processedStart) + mapping[i].sourcePos;
		/**
		 * Check if between gap
		 * Source:
		 * #define x ["123"]
		 * ["a"] x "[b"]
		 *
		 * Processed:
		 * ["a"] ["123"] ["b"]
		 *
		 * Gap is where "123" is in processed text( "123" comes from x)
		 */
		__auto_type oldProcessedStart = processedPrevEnd;
		processedStart = mapping[i].processedStart;
		if (processedStart > processedPos && processedPos >= oldProcessedStart)
			return mapping[i].sourcePos;

		processedPrevEnd = mapping[i].processedEnd;
	}
	return -1;
}
static int sourceMappingCmp(const void *a, const void *b) {
	const struct sourceMapping *A = a;
	const struct sourceMapping *B = b;
	if (A->processedStart > B->processedStart)
		return 1;
	else if (A->processedStart == B->processedStart)
		return 0;
	else
		return -1;
}
static void insertMacroText(struct __vec **text, const struct __vec *toInsert,
                            long insertAt, long deleteCount) {

	long insertLen = 0;
	if (toInsert != NULL)
		insertLen = strlen((const char *)toInsert);

	__auto_type sourceLen = __vecSize(*text);
	*text = __vecResize(*text, insertLen + sourceLen - deleteCount);

	assert(deleteCount <= sourceLen);

	endSourceMapping(insertAt);

	// Insert the text and delete requested amount of chars
	memmove(*(void **)text + insertAt + insertLen,
	        *(void **)text + insertAt + deleteCount,
	        sourceLen - insertAt - deleteCount);
	memmove(*(void **)text + insertAt, toInsert, insertLen);

	startSourceMapping(insertAt + insertLen, deleteCount);
}
static long fstreamSeekEndOfLine(FILE *stream) {
	char buffer[1025];
	buffer[1024] = '\0';
	long traveled = 0;
	__auto_type oldPos = ftell(stream);
loop:;
	__auto_type count = fread(buffer, 1, 1024, stream);
	traveled += count;

	if (count == 0) { // EOF
		return ftell(stream);
	}

	char *finds[] = {
	    strchr(buffer, '\n'),
	    strchr(buffer, '\r'),
	};
	for (int i = 0; i != 2; i++) {
		if (finds[i] != NULL) {
			__auto_type at = ftell(stream);
			__auto_type offset = finds[i] - buffer;
			if (offset >= count)
				continue;
			// Seek to newline
			fseek(stream, -count, SEEK_CUR);
			fseek(stream, offset, SEEK_CUR);

			return ftell(stream);
		}
	}
	goto loop;
}
static long fstreamSeekPastEndOfLine(FILE *stream) {
	__auto_type res = fstreamSeekEndOfLine(stream);
	fseek(stream, res - ftell(stream), SEEK_CUR);
	do {
		__auto_type chr = fgetc(stream);
		if (chr == EOF)
			return ftell(stream);

		if (chr == '\n' || chr == '\r')
			continue;

		// rewind before current char
		fseek(stream, -1, SEEK_CUR);
		return ftell(stream);
		break;
	} while (1);
}
MAP_TYPE_DEF(void *, UsedDefines);
MAP_TYPE_FUNCS(void *, UsedDefines);
static void expandDefinesInRangeRecur(struct __vec **retVal,
                                      mapDefineMacro defines, long where,
                                      long end, int *expanded,
                                      mapUsedDefines used, int *err) {
	__auto_type prev = *(void **)retVal + where;

	for (;;) {
		void *nextReplacement = findNextWord(*retVal, where);
		void *nextMacroStart = strchr(where + *(char **)retVal, '#');
		long newEnd =
		    (nextMacroStart == NULL) ? end : nextMacroStart - *(void **)retVal;
		end = (newEnd < end) ? newEnd : end;

		if (nextReplacement == NULL) {
			break;
		} else if (nextReplacement - *(void **)retVal >= end) {
			break;
		} else {
			// Get replacement text
			__auto_type alnumCount = 0;
			for (__auto_type i = nextReplacement; isalnum(*(char *)i);
			     i++, alnumCount++)
				;
			struct __vec *slice __attribute__((cleanup(__vecDestroy2)));
			slice = __vecAppendItem(NULL, nextReplacement, alnumCount);
			slice = __vecAppendItem(slice, "\0", 1);
			__auto_type replacement = mapDefineMacroGet(defines, (void *)slice);

			if (replacement == NULL) {
				where = nextReplacement + alnumCount - *(void **)retVal;
				continue;
			}

			/** Check for infinite macro expansion
			 * For example:
			 * #define x y
			 * #define y x
			 * x //x will infinitly recur
			 */
			if (mapDefineMacroGet(used, (char *)slice) != NULL) {
				if (err != NULL)
					*err = 1;
				return;
			}
			mapUsedDefinesInsert(used, (char *)slice, NULL);

			// Add source mapping
			long insertAt = nextReplacement - *(void **)retVal;
			insertMacroText(retVal, replacement->text, insertAt, alnumCount);
			if (expanded != NULL)
				*expanded = 1;

			int expanded2 = 0;
			do {
				expandDefinesInRangeRecur(retVal, defines, where,
				                          where + __vecSize(replacement->text),
				                          &expanded2, used, err);
				if (err != NULL)
					if (*err)
						return;
			} while (expanded2);

			long oldWhere = where;
			where = where + __vecSize(replacement->text);

			// Check if macro was inserted,if so quit
			long nextMacroStart2 =
			    strchr(oldWhere + *(char **)retVal, '#') - *(char **)retVal;
			if (nextMacroStart2 < where)
				return;
		}
	}
}
static void expandDefinesInRange(struct __vec **retVal, mapDefineMacro defines,
                                 long where, long end, int *expanded,
                                 int *err) {
	if (expanded != NULL)
		*expanded = 0;

	__auto_type used = mapUsedDefinesCreate();
	expandDefinesInRangeRecur(retVal, defines, where, end, expanded, used, err);
	mapUsedDefinesDestroy(used, NULL);
}
static void expandNextWord(struct __vec **retVal, mapDefineMacro defines,
                           long where, int *expanded, int *err) {
	if (err != NULL)
		*err = 0;

	__auto_type find = findNextWord(*retVal, where);
	if (find == NULL)
		return;

	long endPos = find - *(void **)retVal;
	for (; endPos < __vecSize(*retVal); endPos++)
		if (!isalnum(endPos[*(char **)retVal]))
			break;

	expandDefinesInRange(retVal, defines, where, endPos, expanded, err);
}
static struct __vec *fileReadLine(FILE *file, long lineStart,
                                  long *nextLineStart, struct __vec **newLine) {
	__auto_type fileLineEnd = fstreamSeekEndOfLine(file);
	fseek(file, lineStart - ftell(file), SEEK_CUR);

	// Read line
	struct __vec *firstLine;
	firstLine = __vecResize(NULL, fileLineEnd - lineStart + 1);
	fseek(file, lineStart - ftell(file), SEEK_CUR);
	fread(firstLine, 1, fileLineEnd - lineStart, file);
	((char *)firstLine)[fileLineEnd - lineStart] = '\0';

	__auto_type oldPos = ftell(file);

	__auto_type nextLineStart2 = fstreamSeekPastEndOfLine(file);
	if (nextLineStart != NULL)
		*nextLineStart = nextLineStart2;

	if (newLine != NULL) {
		fseek(file, oldPos - ftell(file), SEEK_CUR);
		struct __vec *newLine2 = __vecResize(NULL, nextLineStart2 - oldPos + 1);
		fread((char *)newLine2, 1, nextLineStart2 - oldPos, file);
		(nextLineStart2 - oldPos)[(char *)newLine2] = '\0';

		*newLine = newLine2;
	}

	return firstLine;
}
static void createPreprocessedFileLine(mapDefineMacro defines,
                                       struct __vec *text_, FILE *writeTo,
                                       int *err) {
	struct __vec *retVal __attribute__((cleanup(__vecDestroy2)));
	retVal = __vecAppendItem(NULL, text_, __vecSize(text_));

	for (long where = 0; where != __vecSize(retVal);) {
	loop:;
		void *nextMacro = skipNonMacro(retVal, where);

		__auto_type expandEnd =
		    (nextMacro == NULL) ? __vecSize(retVal) : nextMacro - (void *)retVal;
		int expanded;
		for (;;) {
			expandDefinesInRange(&retVal, defines, where, expandEnd, &expanded, err);
			if (err != NULL)
				if (*err)
					goto returnLabel;

			if (expanded)
				goto loop;
			else
				break;
		}
		if (nextMacro == NULL)
			break;

		long endPos;
		struct defineMacro define;
		struct includeMacro include;
		if (defineMacroLex(&retVal, defines, nextMacro - (void *)retVal, &endPos,
		                   &define, err)) {
			// Make slice with null ending
			__auto_type nameStr = createNullTerminated(define.name);

			// Remove existing macro
			if (mapDefineMacroGet(defines, (char *)nameStr) != NULL)
				mapDefineMacroRemove(defines, (char *)nameStr, defineMacroDestroy);

			// Insert new macro
			mapDefineMacroInsert(defines, (char *)nameStr, define);
			__vecDestroy(nameStr);

			// Remove macro text from source
			__auto_type at = nextMacro - (void *)retVal;
			insertMacroText(&retVal, NULL, at, endPos - at);
		} else if (includeMacroLex(&retVal, defines, nextMacro - (void *)retVal,
		                           &endPos, &include, err)) {
			enterIncludeFile(); // TODO look here

			struct includeMacro includeClone
			    __attribute((cleanup(includeMacroDestroy)));
			includeClone = include;

			struct __vec *fn __attribute__((cleanup(__vecDestroy2)));

			fn = createNullTerminated(include.fileName);
			assert(fn != NULL); // TODO whine about file not found

			FILE *file __attribute__((cleanup(fileDestroy)));
			file = fopen((char *)fn, "r");
			long lineStart = ftell(file);

			/**
			 * Text included from file,AND(!!!) text following include statement will
			 * be handeled!!!
			 */
			struct __vec *textFollowingInclude
			    __attribute__((cleanup(__vecDestroy2)));
			textFollowingInclude = __vecAppendItem(NULL, (void *)retVal + endPos,
			                                       __vecSize(retVal) - endPos);
			retVal = __vecResize(retVal, where);
			fwrite(retVal, 1, __vecSize(retVal), writeTo);

			for (int firstRun = 1;; firstRun = 0) {
				long nextLine;
				struct __vec *lineText __attribute__((cleanup(__vecDestroy2)));
				// newlineText is '\n' on linux,
				struct __vec *newLine __attribute__((cleanup(__vecDestroy2)));
				lineText = fileReadLine(file, lineStart, &nextLine, &newLine);
				lineStart = nextLine;

				/**!!!
				 * If last line(end of line is at end of file),append text following
				 * #include
				 */
				if (strlen((char *)newLine) == 0)
					lineText = __vecAppendItem(lineText, textFollowingInclude,
					                           __vecSize(textFollowingInclude));

				createPreprocessedFileLine(defines, lineText, writeTo, err);
				if (err != NULL)
					if (*err)
						return;
				// Write out newline
				fwrite(newLine, 1, strlen((char *)newLine), writeTo);

				// Break if no next-line
				if (strlen((char *)newLine) == 0)
					break;
			}

			leaveIncludeFile(); // TODO look above for enterIncludeFile
			return;
		}
		if (err != NULL)
			if (*err)
				goto returnLabel;

		if (expanded == 0 && nextMacro == NULL)
			where = __vecSize(retVal);
		else
			where = endPos;
	}

	fwrite(retVal, 1, strlen((char *)retVal), writeTo);
returnLabel:
	return;
}

FILE *createPreprocessedFile(struct __vec *text, strSourceMapping *mappings,
                             int *err) {
	if (err != NULL)
		*err = 0;

	strSourceMappingDestroy(&sourceMappings);
	sourceMappings = NULL;
	preprocessedSource = tmpfile();

	mapDefineMacro defines = mapDefineMacroCreate();

	__auto_type processedPos = 0;

	includeFileDepth = 0;
	__auto_type lineStart2 = 0;
	startSourceMapping(0, 0);

	for (long lineEnd = findEndOfLine(text, lineStart2) - (void *)text;;
	     lineEnd = findEndOfLine(text, lineStart2) - (void *)text) {
		if (lineEnd == lineStart2)
			break;

		__auto_type oldFPos = ftell(preprocessedSource);

		struct __vec *lineText __attribute__((cleanup(__vecDestroy2)));
		lineText = __vecResize(NULL, lineEnd - lineStart2 + 1);
		memcpy(lineText, (void *)text + lineStart2, lineEnd - lineStart2);
		(lineEnd - lineStart2)[(char *)lineText] = '\0';

		createPreprocessedFileLine(defines, lineText, preprocessedSource, err);
		if (err != NULL)
			if (*err)
				goto returnLabel;

		// Append newline to preprocessed source
		__auto_type nextLine = findNextLine(text, lineStart2) - (void *)text;
		__auto_type lineEnd2 = findEndOfLine(text, lineStart2) - (void *)text;
		fwrite((void *)text + lineEnd2, 1, nextLine - lineEnd2, preprocessedSource);

		// Update the start of the processed source for next line
		__auto_type added = ftell(preprocessedSource) - oldFPos;
		processedPos += added;
		lineStart = processedPos;

		lineStart2 = nextLine;
	}
	endSourceMapping(0);

	if (mappings != NULL)
		*mappings = (strSourceMapping)__vecAppendItem(
		    NULL, sourceMappings, __vecSize((struct __vec *)sourceMappings));
returnLabel:
	mapDefineMacroDestroy(defines, defineMacroDestroy);

	return preprocessedSource;
}
