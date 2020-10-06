#pragma once
#include <str.h>
#include <stdio.h>
struct includeMacro {
	struct __vec *fileName;
};
struct defineMacro {
	struct __vec *name;
	struct __vec *text;
};
struct sourceMapping {
	long processedStart;
	long processedEnd;
};

STR_TYPE_DEF(struct sourceMapping, SourceMapping);
STR_TYPE_FUNCS(struct sourceMapping, SourceMapping);
long mappedPosition(const strSourceMapping, long processedPos);
struct __lexerItemTemplate *includeMacroTemplateCreate();
struct __lexerItemTemplate *createDefineMacroTemplate();
