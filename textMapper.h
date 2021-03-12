#pragma once
#include "str.h"
enum textModifyType {
	MODIFY_INSERT,
	MODIFY_REMOVE,
};
struct textModify {
	long where, len;
	enum textModifyType type;
};
STR_TYPE_DEF(struct textModify, TextModify);
STR_TYPE_FUNCS(struct textModify, TextModify);
long mapToSource(long resultPos, const strTextModify edits, long startEdit);
