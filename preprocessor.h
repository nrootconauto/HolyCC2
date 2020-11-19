#pragma once
#include <stdio.h>
#include <str.h>
#include <textMapper.h>
struct includeMacro {
	struct __vec *fileName;
};
struct defineMacro {
	struct __vec *name;
	struct __vec *text;
};
struct fileMapping {
	char *fileName;
	// Represents the index of the text mapping strTextModify
	long mappingIndexStart, mappingIndexEnd;
	// fileOffset/fileEndOffset is position in preprocessed text
	long fileOffset, fileEndOffset;
};
STR_TYPE_DEF(struct fileMapping, FileMappings);
STR_TYPE_FUNCS(struct fileMapping, FileMappings);
FILE *createPreprocessedFile(const char *fileName, strTextModify *mappings,
                             strFileMappings *fileMappings, int *err);
void fileMappingsDestroy(strFileMappings *mappings);
const char *fileNameFromPos(const strFileMappings mappings, long pos);
