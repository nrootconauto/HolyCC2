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
static void __vecDestroy2(struct __vec **vec) { __vecDestroy(*vec); }
static void fileDestroy(FILE **file) { fclose(*file); }
MAP_TYPE_DEF(struct defineMacro, DefineMacro);
MAP_TYPE_FUNCS(struct defineMacro, DefineMacro);
;
static __thread long lineStart;
static __thread strTextModify sourceMappings = NULL;
static __thread strFileMappings allFileMappings = NULL;
static FILE *createPreprocessedFileLine(mapDefineMacro defines,
                                        struct __vec *text_, int *err);
static void expandDefinesInRange(struct __vec **retVal, mapDefineMacro defines,
                                 long where, long end, int *expanded, int *err);
static void expandNextWord(struct __vec **retVal, FILE **prependLinesTo,
                           mapDefineMacro defines, long where, int *expanded,
                           int recur, int *err);
static void concatFile(FILE *a, FILE *b);
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
                              FILE **prependLinesTo, mapDefineMacro defines,
                              long pos, long *end, int *err) {
	if (err != NULL)
		*err = 0;

	const char *text2 = *(const char **)text + pos;
	if (*text2 != '#')
		return 0;

	pos = skipWhitespace(*text, pos + 1) - *(const void **)text;

	expandNextWord(text, prependLinesTo, defines, pos, NULL, 1, err);
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
static int includeMacroLex(struct __vec **text_, FILE **prependLinesTo,
                           mapDefineMacro defines, long pos, long *end,
                           struct includeMacro *result, int *err) {

	if (err != NULL)
		*err = 0;

	if (!expectMacroAndSkip("include", text_, prependLinesTo, defines, pos, &pos,
	                        err))
		return 0;

	expandNextWord(text_, prependLinesTo, defines, pos, NULL, 1, err);
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
	
	FILE *after=tmpfile();
	long lineLen=strlen(*(char**)text_);
	fwrite(*(char**)text_+*end, 1, lineLen-*end, after);
	concatFile(after,*prependLinesTo);
	*prependLinesTo=after;
	
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
static int defineMacroLex(struct __vec **text_, FILE **prependLinesTo,
                          mapDefineMacro defines, long pos, long *end,
                          struct defineMacro *result, int *err) {
	if (!expectMacroAndSkip("define", text_, prependLinesTo, defines, pos, &pos,
	                        err))
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
static void insertMacroText(struct __vec **text, const struct __vec *toInsert,
                            long insertAt, long deleteCount) {

	long insertLen = 0;
	if (toInsert != NULL)
		insertLen = strlen((const char *)toInsert);

	__auto_type sourceLen = __vecSize(*text);
	*text = __vecResize(*text, insertLen + sourceLen - deleteCount);

	assert(deleteCount <= sourceLen);

	if (deleteCount != 0) {
		struct textModify delete;
		delete.len = deleteCount;
		delete.where = insertAt + lineStart;
		delete.type = MODIFY_REMOVE;
		sourceMappings = strTextModifyAppendItem(sourceMappings, delete);
	}
	if (insertLen != 0) {
		struct textModify insert;
		insert.len = insertLen;
		insert.where = insertAt + lineStart;
		insert.type = MODIFY_INSERT;
		sourceMappings = strTextModifyAppendItem(sourceMappings, insert);
	}

	// Insert the text and delete requested amount of chars
	memmove(*(void **)text + insertAt + insertLen,
	        *(void **)text + insertAt + deleteCount,
	        sourceLen - insertAt - deleteCount);
	memmove(*(void **)text + insertAt, toInsert, insertLen);
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
static struct __vec *fileReadLine(FILE *file, long lineStart,
                                  long *nextLineStart, struct __vec **newLine);
static FILE *fileRemoveFirstLine(FILE *file, struct __vec **firstLine) {
	fseek(file, 0, SEEK_SET);

	__auto_type firstLine2 = fileReadLine(file, ftell(file), NULL, NULL);
	if (firstLine != NULL) {
		*firstLine = firstLine2;
	} else {
		__vecDestroy(firstLine2);
	}

	const long bufferSize = 65536;
	char buffer[bufferSize];

	/**
	 * Be sure to seek right to '\n',we want to include newline too
	 */
	fseek(file, 0, SEEK_SET);
	long startAt = fstreamSeekEndOfLine(file);

	__auto_type retVal = tmpfile();
	for (long pos = startAt;;) {
		fseek(file, pos - ftell(file), SEEK_CUR);

		__auto_type count = fread(buffer, 1, bufferSize, file);
		fwrite(buffer, 1, count, retVal);

		pos += count;
		if (count != bufferSize)
			break;
	}

	fclose(file);
	return retVal;
}
static void expandNextWord(struct __vec **retVal, FILE **prependLinesTo,
                           mapDefineMacro defines, long where, int *expanded,
                           int recur, int *err) {
	if (expanded != NULL)
		*expanded = 0;

	if (err != NULL)
		*err = 0;

	__auto_type macroFind = (void *)strchr(&where[*(char **)retVal], '#');
	__auto_type find = findNextWord(*retVal, where);

	if (find == NULL && macroFind == NULL)
		return;
	__auto_type endPtr = *(void **)retVal + __vecSize(*retVal);
	macroFind = (macroFind == NULL) ? endPtr : macroFind;
	find = (find == NULL) ? endPtr : find;

	if (find < macroFind) {
		long endPos = find - *(void **)retVal;
		for (; endPos < __vecSize(*retVal); endPos++)
			if (!isalnum(endPos[*(char **)retVal]))
				break;

		expandDefinesInRange(retVal, defines, where, endPos, expanded, err);
	} else if (macroFind < find) {
		__auto_type inputSize = __vecSize(*retVal);
		__auto_type macroFindIndex = macroFind - *(void **)retVal;
		struct __vec *slice __attribute((cleanup(__vecDestroy2)));
		slice = __vecAppendItem(NULL, macroFind, inputSize - macroFindIndex);

		assert(prependLinesTo != NULL);
		__auto_type result = createPreprocessedFileLine(defines, slice, err);

		struct __vec *firstLine __attribute((cleanup(__vecDestroy2)));
		result = fileRemoveFirstLine(result, &firstLine);
		/**
		 * Prepend following lines,so lines appearing after the macro will be placed
		 * before lines that were placed before
		 */
		if (prependLinesTo != NULL) {
			concatFile(result, *prependLinesTo);
			*prependLinesTo = result;
		}

		*retVal = __vecResize(*retVal, macroFindIndex);
		*retVal = __vecConcat(*retVal, firstLine);

		if (expanded != NULL)
			*expanded = 1;
	} else {
		// Will never reach here
		assert(0);
	}

	// Repeat expansions untill nothing left to expand
	if (expanded != NULL)
		if (*expanded)
			if (recur)
				for (int expanded2 = 1; expanded2 != 0;)
					expandNextWord(retVal, prependLinesTo, defines, where, &expanded2, 1,
					               err);
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
static void concatFile(FILE *a, FILE *b) {
	const long bufferSize = 65536;
	char buffer[bufferSize];
	fseek(a, 0, SEEK_END);
	fseek(b, 0, SEEK_SET);
	for (;;) {
		__auto_type count = fread(buffer, 1, bufferSize, b);
		fwrite(buffer, 1, count, a);
		if (count != bufferSize)
			break;
	}
	fclose(b);
}
static char *stringClone(const char *str) {
	char *retVal = malloc(strlen(str) + 1);
	strcpy(retVal, str);
	return retVal;
}
static FILE *includeFile(const char *fileName, mapDefineMacro defines,
                         struct __vec *textFollowingInclude, int *err) {
	FILE *readFrom = fopen(fileName, "r");
	fseek(readFrom, 0, SEEK_END);
	long fileEnd = ftell(readFrom);
	fseek(readFrom, 0, SEEK_SET);
	long fileStart = ftell(readFrom);
	long fileSize = fileEnd - fileStart;

	// Insert the file text
	struct textModify wholeFileInsert;
	wholeFileInsert.len = fileSize, wholeFileInsert.where = lineStart,
	wholeFileInsert.type = MODIFY_INSERT;
	sourceMappings = strTextModifyAppendItem(sourceMappings, wholeFileInsert);

	struct fileMapping __newFileMappping;
	__newFileMappping.fileOffset = lineStart;
	__newFileMappping.fileName = stringClone(fileName);
	__newFileMappping.mappingIndexStart = strTextModifySize(sourceMappings);
	__newFileMappping.mappingIndexEnd = -1;
	allFileMappings =
	    strFileMappingsAppendItem(allFileMappings, __newFileMappping);
	long t = sizeof(struct fileMapping);
	long newFileMappingPos = strFileMappingsSize(allFileMappings) - 1;

	__auto_type retValFile = tmpfile();
	long retValStart = ftell(retValFile);

	__auto_type lineStart2 = 0;
	for (int firstRun = 1;; firstRun = 0) {
		long nextLine;
		struct __vec *lineText __attribute__((cleanup(__vecDestroy2)));
		// newlineText is '\n' on linux,
		struct __vec *newLine __attribute__((cleanup(__vecDestroy2)));
		lineText = fileReadLine(readFrom, lineStart2, &nextLine, &newLine);
		lineStart2 = nextLine;

		/**!!!
		 * If last line(end of line is at end of file),append text following
		 * #include
		 */
		if (strlen((char *)newLine) == 0) {
			lineText = __vecAppendItem(lineText, textFollowingInclude,
			                           __vecSize(textFollowingInclude));
		}

		__auto_type lump = createPreprocessedFileLine(defines, lineText, err);
		if (err != NULL)
			if (*err)
				return NULL;
		concatFile(retValFile, lump);

		// Write out newline
		fwrite(newLine, 1, strlen((char *)newLine), retValFile);
		lineStart += strlen((char *)newLine);

		// Break if no next-line
		if (strlen((char *)newLine) == 0)
			break;
	}

	allFileMappings[newFileMappingPos].mappingIndexEnd =
	    strTextModifySize(sourceMappings);
	// Set end relative to included file size
	allFileMappings[newFileMappingPos].fileEndOffset =
	    ftell(retValFile) - retValStart +
	    allFileMappings[newFileMappingPos].fileOffset;

	fclose(readFrom);
	return retValFile;
}
static FILE *createPreprocessedFileLine(mapDefineMacro defines,
                                        struct __vec *text_, int *err) {
	__auto_type writeTo = tmpfile();
	FILE *afterLines;
	afterLines = tmpfile();

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

		where = nextMacro - (void *)retVal;
		long endPos;
		struct defineMacro define;
		struct includeMacro include;
		if (defineMacroLex(&retVal, &afterLines, defines,
		                   nextMacro - (void *)retVal, &endPos, &define, err)) {
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
		} else if (includeMacroLex(&retVal, &afterLines, defines,
		                           nextMacro - (void *)retVal, &endPos, &include,
		                           err)) {
			struct includeMacro includeClone
			    __attribute((cleanup(includeMacroDestroy)));
			includeClone = include;

			struct textModify destroy;
			destroy.where = where + lineStart;
			destroy.len = endPos - where + lineStart;
			destroy.type = MODIFY_REMOVE;
			sourceMappings = strTextModifyAppendItem(sourceMappings, destroy);

			struct __vec *fn __attribute__((cleanup(__vecDestroy2)));

			fn = createNullTerminated(include.fileName);
			assert(fn != NULL); // TODO whine about file not found

			struct __vec *textFollowingInclude
			    __attribute__((cleanup(__vecDestroy2)));
			textFollowingInclude = __vecAppendItem(NULL, (void *)retVal + endPos,
			                                       __vecSize(retVal) - endPos);

			retVal = __vecResize(retVal, where);
			fwrite(retVal, 1, __vecSize(retVal), writeTo);
			lineStart += __vecSize(retVal);

			__auto_type included = includeFile((char *)include.fileName, defines,
			                                   textFollowingInclude, err);
			if (err != NULL)
				if (*err)
					goto fail;

			concatFile(writeTo, included);
			concatFile(writeTo, afterLines);
			return writeTo;
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
	lineStart += strlen((char *)retVal);

	concatFile(writeTo, afterLines);
returnLabel:
	return writeTo;
fail:
	fclose(writeTo);
	fclose(afterLines);
	return NULL;
}

FILE *createPreprocessedFile(const char *fileName, strTextModify *mappings,
                             strFileMappings *fileMappings, int *err) {
	if (err != NULL)
		*err = 0;

	strTextModifyDestroy(&sourceMappings);
	strFileMappingsDestroy(&allFileMappings);
	sourceMappings = NULL;
	allFileMappings = NULL;
	lineStart = 0;

	mapDefineMacro defines = mapDefineMacroCreate();

	__auto_type preprocessedSource = includeFile(fileName, defines, NULL, err);

	if (mappings != NULL)
		*mappings = (strTextModify)__vecAppendItem(
		    NULL, sourceMappings, __vecSize((struct __vec *)sourceMappings));
	if (fileMappings != NULL)
		*fileMappings = (strFileMappings)__vecAppendItem(
		    NULL, allFileMappings, __vecSize((struct __vec *)allFileMappings));
returnLabel:
	mapDefineMacroDestroy(defines, defineMacroDestroy);

	if (err != NULL)
		if (*err)
			return NULL;

	return preprocessedSource;
}
void fileMappingsDestroy(strFileMappings *mappings) {
	for (long i = 0; i != strFileMappingsSize(*mappings); i++)
		free(mappings[0][i].fileName);
}
//
//File mappings can contain file mappings within them
//This find the innermost mapping which represents the included file at a point
//The inner most mapping's start is closest to pos beause nested #includes move forward
//The end is also closest to the position
//#include "a" #include "b"
//[  a text    [   pos    ] ]
static struct fileMapping *innerMostFileMapping(strFileMappings mappings,long lowerBoundI,long upperBoundI,const struct fileMapping *m,long pos) {
		long largestIndex=-1,smallestOffsetStart=-1,smallestOffsetEnd=-1;
		for (long i=lowerBoundI;i!=upperBoundI;i++) {
				if(largestIndex==-1)
						largestIndex=i;

				long offset=pos-mappings[i].fileOffset;
				//Start must be closest
				if(offset>=smallestOffsetStart||smallestOffsetStart==-1) {
						smallestOffsetStart=offset ;

						//End must be closest
						offset=mappings[i].fileEndOffset-pos;
						if(offset<=smallestOffsetEnd||smallestOffsetEnd==-1) {
								smallestOffsetEnd=offset;
								largestIndex=i;
						}
				}
		}
		
		assert(largestIndex!=-1);
		return &mappings[largestIndex];
}
const char *fileNameFromPos( strFileMappings mappings,long pos) {
		long lower=-1,upper=strFileMappingsSize(mappings);
		for(long i=0;i!=strFileMappingsSize(mappings);i++) {
				if(mappings[i].fileOffset<=pos&&lower==-1)
						lower=i;

				if(mappings[i].fileEndOffset>pos) {
						upper=i;
				} else break;
		}
		if(lower==-1)
				lower=strFileMappingsSize(mappings)-1;
		return innerMostFileMapping(mappings, lower, upper+1, mappings, pos)->fileName;
}
