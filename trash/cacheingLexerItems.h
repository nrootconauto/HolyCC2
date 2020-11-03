#pragma once
#include <stringParser.h>

const void *skipWhitespace(struct __vec *text, long from);
void *findNextLine(struct __vec *text, long pos);
void *findEndOfLine(struct __vec *text, long pos);
struct __lexerItemTemplate keywordTemplateCreate(const char **keywords,
                                                 long keywordCount);
struct __lexerItemTemplate nameTemplateCreate(const char **keywords,
                                              long keywordCount);
struct __lexerItemTemplate stringTemplateCreate();
struct __lexerItemTemplate intTemplateCreate();
struct __lexerItemTemplate floatingTemplateCreate();
