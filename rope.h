#pragma once
struct rope;
void ropeDestroy(struct rope **r);
long ropeSize(struct rope *r);
struct rope *ropeConcat(struct rope *a,struct rope *b);
void ropeSplit(struct rope *r,long i,struct rope **a,struct rope **b);
struct rope *ropeFromText(const char *text);
char *ropeToText(struct rope *r);
struct rope *ropeInsertText(struct rope *r,const char *text,long i);
struct rope *ropeDeleteText(struct rope *r,long s,long e);
char *ropeSliceText(struct rope *r,long s,long e);
