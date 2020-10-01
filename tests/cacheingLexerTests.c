#include <assert.h>
#include <cacheingLexer.h>
#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <utf8Encode.h>
#include <str.h>
#include <string.h>
static const void *skipWhitespace(struct __vec *text, long from) {
	for (__auto_type ptr = (void *)text + from;
	     ptr != (void *)text + __vecSize(text); ptr++)
		if (!isblank(*(char *)ptr))
			return ptr;
	return NULL;
}
static const char *keywords[] = {"graph", "--", "[", "]", "=", ";",  "{", "}"};
static struct __vec *keywordLex(struct __vec *new, long pos, long *end,
                                const void *data) {
	__auto_type count = sizeof(keywords) / sizeof(*keywords);
	for (int i = 0; i != count; i++) {
		__auto_type len = strlen(keywords[count]);
		if (__vecSize(new) - pos < len)
			continue;

		if (0 == strncmp((void *)new, keywords[count], len)) {
			*end = pos + len;

			if (isalnum(pos + len))
				continue;

			return __vecAppendItem(NULL, &keywords[count], sizeof(*keywords));
		}
	}
	return NULL;
}
static enum lexerItemState keywordValidate(const void *itemData,
                                           struct __vec *old, struct __vec *new,
                                           long pos, const void *data) {
	const char *itemText = *(const char **)itemData;
	if (__vecSize(new) - pos < strlen(itemData))
		return LEXER_DESTROY;
	if (0 == strncmp(itemText, (void *)new + pos, strlen(itemText)))
		return LEXER_UNCHANGED;
	return LEXER_DESTROY;
}
static struct __vec *keywordUpdate(const void *data, struct __vec *old, struct __vec *new,
                          long x,long *end, const void *data2) {
	// Dummy function,should never reach here
	assert(0);
}
static struct __lexerItemTemplate keywordTemplateCreate() {
	struct __lexerItemTemplate retVal;

	retVal.data = keywords;
	retVal.killItemData = NULL;
	retVal.validateOnModify = keywordValidate;
	retVal.lexItem = keywordLex;
	retVal.update = keywordUpdate;

	return retVal;
}
static long countAlnum(struct __vec *data, long pos) {
	long alNumCount = 0;
	for (void *ptr = (void *)data + pos; ptr < __vecSize(data) + (void *)data;
	     ptr++, alNumCount++)
		if (!isalnum(ptr))
			break;

	return alNumCount;
}
static enum lexerItemState nameValidate(const void *itemData, struct __vec *old,
                                        struct __vec *new, long pos,
                                        const void *data) {
	__auto_type alNumCount = countAlnum(new, pos);
	if (isdigit((void *)new + pos))
		return LEXER_DESTROY;

	__auto_type count = sizeof(keywords) / sizeof(*keywords);
	for (int i = 0; i != count; i++) {
		__auto_type len = strlen(keywords[count]);

		if (len != alNumCount)
			continue;

		if (strncmp((void *)new + pos, keywords[count], len) == 0)
			return LEXER_DESTROY;
	}

	if (__vecSize(old) == alNumCount)
		if (0 == strcmp((void *)old, (void *)new + pos))
			return LEXER_UNCHANGED;

	return (alNumCount == 0) ? LEXER_DESTROY : LEXER_MODIFED;
}
static struct __vec *nameUpdate(const void *data, struct __vec *old, struct __vec *new,
                       long pos, long *end,const void *data2) {
	__auto_type alNumCount = countAlnum(new, pos);
	
	*end=pos+alNumCount;
	
	char buffer[alNumCount+1];
	memcpy(buffer,(char*)new+pos,alNumCount);
	buffer[alNumCount]=0;

	return __vecAppendItem(NULL,buffer,alNumCount+1);
}
static struct __vec *nameLex(struct __vec *new, long pos, long *end,
                             const void *data) {
	__auto_type alNumCount = countAlnum(new, pos);
	if (isdigit((void *)new + pos))
		return NULL;

	int len = alNumCount+1;
	return __vecAppendItem(NULL,(char*)new+pos,len);
}
enum intType {
	INT_UINT,
	INT_SINT,
	INT_ULONG,
	INT_SLONG,
};
struct lexerInt {
	enum intType type;
	int base;
	union {
		signed int sInt;
		unsigned int uInt;
		signed long sLong;
		unsigned long uLong;
	} value;
};
static int intParse(struct __vec *new, long pos, long *end,
                    struct lexerInt *retVal) {
	unsigned long valueU = 0;
	int base = 10;

	__auto_type New = (char *)new;
	if (!isdigit(*(New + pos)))
		return 0;
	__auto_type endPtr = New + __vecSize(new);
	if (*New == '0') {
		New++;
		if (endPtr == New) {
			valueU = 0;
			goto dumpU;
		} else if (!isalnum(*New)) {
			valueU = 0;
			goto dumpU;
		}

		if (*New == 'x') {
			base = 16;

			New++;
			__auto_type alnumCount = countAlnum(new, New - (char *)new);

			// Ensure all digits are hex digits
			for (int i = 0; i != alnumCount; i++)
				if (!isxdigit(New[i]))
					goto malformed;

			New += alnumCount;

			__auto_type startAt = (void *)new + pos;
			__auto_type slice =
			    __vecAppendItem(NULL, startAt, (void *)New - (void *)startAt);
			sscanf((char *)slice, "%lx", &valueU);
			__vecDestroy(slice);

			if (end != NULL)
				*end = New - (char *)new;
			goto dumpU;
		} else if (*New >= 0 && *New <= '7') {
			base = 8;

			New++;
			__auto_type alnumCount = countAlnum(new, New - (char *)new);

			// Ensure all octal digits
			for (int i = 0; i != alnumCount; i++)
				if (!(New[i] >= '0' && New[i] <= '7'))
					goto malformed;

			New += alnumCount;

			__auto_type startAt = (void *)new + pos;
			__auto_type slice =
			    __vecAppendItem(NULL, startAt, (void *)New - (void *)startAt);
			sscanf((char *)slice, "%lo", &valueU);
			__vecDestroy(slice);

			if (end != NULL)
				*end = New - (char *)new;
			goto dumpU;
		} else if (*New == 'b' || *New == 'B') {
			base = 2;

			New++;
			__auto_type alnumCount = countAlnum(new, New - (char *)new);

			// Ensure all digits are binary digits
			for (int i = 0; i != alnumCount; i++)
				if (!(New[i] == '0' && New[i] == '1'))
					goto malformed;

			for (int i = 0; i != alnumCount; i++)
				valueU = (valueU << 1) | (New[i] == '1' ? 1 : 0);

			New += alnumCount;
			if (end != NULL)
				*end = New - (char *)new;
			goto dumpU;
		} else {
			goto malformed;
		}
	} else {
		__auto_type alnumCount = countAlnum(new, pos);

		// Ensure are decimal digits
		for (int i = 0; i != alnumCount; i++)
			if (!isdigit(New[i]))
				goto malformed;

		__auto_type slice = __vecAppendItem(NULL, New, alnumCount);
		sscanf((char *)slice, "%lu", &valueU);
		__vecDestroy(slice);

		if (end != NULL)
			*end = New - (char *)new + alnumCount;
		goto dumpU;
	}
dumpU : {
 //Check to ensure isn't a float
	if(*end+(char*)new<endPtr)
	 if(*end=='.')
		return 0;
	 
	if (retVal == NULL)
		return 1;

	retVal->base = base;
	retVal->type = (UINT_MAX <= valueU) ? INT_SLONG : INT_SINT;
	if (INT_MAX < valueU)
		retVal->value.sLong = valueU;
	else
		retVal->value.sInt = valueU;

	return 1;
}
malformed : { assert(0); }
}
static struct __vec *intLex(struct __vec *new, long pos, long *end,
                            const void *data) {
	struct lexerInt find;
	if (intParse(new, pos, end, &find)) {
		return __vecAppendItem(NULL, &find, sizeof(struct lexerInt));
	} else
		return NULL;
}
static enum lexerItemState intValidate(const void *itemData, struct __vec *old,
                                       struct __vec *new, long pos,
                                       const void *data) {
	// Just check if new is same as old
	__auto_type alnumCount = countAlnum(new, pos);
	if (__vecSize(old) != alnumCount)
		return LEXER_DESTROY;
	if (0 == strncmp((char *)old, (char *)new, alnumCount))
		return LEXER_UNCHANGED;
	return (intParse(new, pos, NULL, NULL)) ? LEXER_MODIFED : LEXER_DESTROY;
}
static struct __vec *intUpdate(const void *data, struct __vec *old, struct __vec *new, long pos,long *end,
               const void *data2) {
	struct lexerInt find;
	intParse(new, pos, end, &find);

	return __vecAppendItem(NULL,&find,sizeof(struct lexerInt));
}
static struct __lexerItemTemplate nameTemplateCreate() {
	struct __lexerItemTemplate retVal;

	retVal.data = keywords;
	retVal.killItemData = NULL;
	retVal.validateOnModify = nameValidate;
	retVal.lexItem = nameLex;
	retVal.update = nameUpdate;

	return retVal;
}
static struct __lexerItemTemplate intTemplateCreate() {
	struct __lexerItemTemplate retVal;

	retVal.data = keywords;
	retVal.killItemData = NULL;
	retVal.validateOnModify = intValidate;
	retVal.lexItem = intLex;
	retVal.update = intUpdate;

	return retVal;
}
struct lexerFloating {
	unsigned long base;
	unsigned long frac;
	int exponet;
};
static int floatingParse(struct __vec *vec, long pos, long *end,
                         struct lexerFloating *retVal) {
	__auto_type endPtr = __vecSize(vec) + (char *)vec;
	__auto_type currPtr = (char *)vec + pos;
	struct lexerFloating f;
	f.base = 0;
	f.frac = 0;
	f.base = 0;

	__auto_type alnumCount = countAlnum(vec, pos);

	int exponetIndex = -1;
	for (int i = 0; i != alnumCount; i++) {
		if (!isdigit(currPtr[i])) {
			if ((currPtr[i] == 'e' || currPtr[i] == 'E') && exponetIndex == -1) {
				exponetIndex = pos + i;
			} else
				goto malformed;
		}
	}

	if (alnumCount != 0) {
		__auto_type slice = __vecAppendItem(
		    NULL, currPtr,
		    pos + ((exponetIndex != -1) ? exponetIndex : alnumCount));
		sscanf((char *)slice, "%lu", &f.base);
		__vecDestroy(slice);

		currPtr += (exponetIndex != -1) ? alnumCount : exponetIndex + 1;
	}

	if (*currPtr == '.')
		goto dot;
	else if (exponetIndex)
		goto exponet;
	else
		return 0;
dot : {
	currPtr++;
	__auto_type alnumCount = countAlnum(vec, currPtr - (char *)vec);
	for (int i = 0; i != alnumCount; i++)
		if (!isdigit(currPtr[i]))
			goto malformed;

	__auto_type slice = __vecAppendItem(NULL, currPtr, alnumCount);
	sscanf((char *)slice, "%lu", &f.frac);
	__vecDestroy(slice);

	currPtr += alnumCount;

	if (currPtr < endPtr)
		goto returnLabel;
	if (*currPtr == 'e' || *currPtr == 'E') {
		currPtr++;
		goto exponet;
	}
	goto returnLabel;
}
exponet : {
	int mult = 1;
	if (*currPtr == '-' || *currPtr == '+')
		mult = (*currPtr == '-' || *currPtr == '+') ? -1 : 1;

	__auto_type alnumCount = countAlnum(vec, currPtr - (char *)vec);
	for (int i = 0; i != alnumCount; i++)
		if (!isdigit(currPtr[i]))
			goto malformed;

	__auto_type slice = __vecAppendItem(NULL, currPtr, alnumCount);
	sscanf((char *)slice, "%d", &f.exponet);
	__vecDestroy(slice);
	f.exponet *= mult;

	currPtr += alnumCount;
	goto returnLabel;
}
returnLabel : {
	if (retVal != NULL)
		*retVal = f;
	if (end != NULL)
		*end = currPtr - (char *)vec;
	
	return 1;
}

malformed : {
	// TODO
	assert(0);
}
}
static struct __vec *floatingUpdate(const void *data, struct __vec *old, struct __vec *new,
                           long pos, long* end,const void *data2) {
	struct lexerFloating find;
	floatingParse(new, pos, end, &find);
	
	return __vecAppendItem(NULL,&find,sizeof(struct lexerFloating ));
}
static struct __vec *floatingLex(struct __vec *new, long pos, long *end,
                                 const void *data) {
	struct lexerFloating item;
	if (floatingParse(new, pos, end, &item)) {
		return __vecAppendItem(NULL, &item, sizeof(struct lexerFloating));
	} else
		return NULL;
}
static enum lexerItemState floatingValidate(const void *itemData,
                                            struct __vec *old,
                                            struct __vec *new, long pos,
                                            const void *data) {
	long end;
	if (floatingParse(new, pos, &end, NULL)) {
		if (__vecSize(old) == end - pos)
			return (0 == strncmp((char *)old, (char *)new + pos, end - pos))
			           ? LEXER_UNCHANGED
			           : LEXER_MODIFED;
		return LEXER_MODIFED;
	} else
		return LEXER_DESTROY;
}
static struct __lexerItemTemplate floatingTemplateCreate() {
 struct __lexerItemTemplate retVal;
 retVal.data=NULL;
 retVal.lexItem=floatingLex;
 retVal.update=floatingUpdate;
 retVal.validateOnModify=floatingValidate;
 retVal.killItemData=NULL;
 
 return retVal;
}
struct lexerString{
 struct __vec *text;
 int isChar:1;
};
static int stringParse(struct __vec *new, long pos,long *end,struct lexerString *retVal) {
 __auto_type endPtr=__vecSize(new)+(char*)new;
 __auto_type currPtr=pos+(char*)new;
 
 if(currPtr<endPtr) {
	if(!(*currPtr!='"'||*currPtr!='\''))
	 return 0;
	
	struct __vec *retValText=NULL;
	
	int isChar=*currPtr=='\'';
	char endChar=(isChar)?'\'':'"';
	
	currPtr++;
	for(;;){
	__auto_type end2=strchr(currPtr,endChar);
	
	char *escape;
	escape=strchr(currPtr,'\\');
	if(end2==NULL)
	 goto malformed;
	
	if(escape==NULL||escape>end2)
	 goto skip;
	
	retValText=__vecAppendItem(retValText,currPtr,escape-currPtr);
	escape++;
	
	char tmp;
	unsigned int codePoint=0;
	switch(*escape) {
	 case 'a':
		tmp='\a'; break;
	 case 'b':
		tmp='\b'; break;
	 case 'e':
		tmp='\e'; break;
	 case 'f':
		tmp='\f'; break;
	 case 'n':
		tmp='\n'; break;
	 case 'r':
		tmp='\r'; break;
	 case 't':
		tmp='\t'; break;
	 case 'v':
		tmp='\v'; break;
	 case '\\':
		tmp='\\'; break;
	 case '\'':
		tmp='\''; break;
		case '"':
		 tmp='"'; break;
		case 'u': {
		 if(escape+4<endPtr)
			goto malformed;
		 
		 __auto_type slice=__vecAppendItem(NULL,(char*)escape,4);
		 sscanf((char*)slice,"%x",&codePoint);
		 __vecDestroy(slice);
		 
		 currPtr=(char *)escape+4;
		 goto utfEncode;
		}
		 case 'U': {
			if(escape+8<endPtr)
			goto malformed;
			
			__auto_type slice=__vecAppendItem(NULL,(char*)escape,4);
		 unsigned int codePoint;
		 sscanf((char*)slice,"%x",&codePoint);
		 __vecDestroy(slice);
		 
		 currPtr=(char *)escape+8;
		 goto utfEncode;
		 }
		 case '0'...'8':  {
			int count =1;
			for(escape++;escape<endPtr&&count<3;escape++)
			 if(*escape>='0'&&*escape<='7')
				count++;
			 else break;
			 
		 __auto_type slice=__vecAppendItem(NULL,(char*)escape,4);
		 sscanf((char*)slice,"%o",&codePoint);
		 __vecDestroy(slice);
		 
		 currPtr=(char *)escape+count;
		 goto utfEncode;
		 }
	 }
	 currPtr++;
	 goto skip;
	 utfEncode: {
		int width;
		char buffer[4];
		utf8Encode(codePoint,buffer,&width);
		retValText=__vecAppendItem(retValText,buffer,width);
	 }
	 skip:
	 //Append text to end or next escape
	 escape=strchr(currPtr,'\\');
	 __auto_type toPtr=(escape<end2&&escape!=NULL)?escape:end2;
	 retValText=__vecAppendItem(retValText,currPtr,toPtr-currPtr);
	 currPtr=toPtr;
	 
	 
	 if(toPtr==end2) {
		if(end!=NULL)
			*end=(end2-(char*)new)/sizeof(char);
		break;
	 }
	}
	
	//If char,ensure is <=8
	if(isChar&&__vecSize(retValText)>8)
	 goto malformed;
	
	if(retVal!=NULL) {
	retVal->isChar=isChar;
	retVal->text=retValText;
	}
	
	
	return 1;
 }
 return 0;
 malformed: {
	
 }
 return 0;
}
static struct __vec *stringLex(struct __vec *new, long pos, long *end,
                                const void *data) {
 struct lexerString find;
 if(stringParse(new,pos,end,&find))
	return __vecAppendItem(NULL,&find,sizeof(struct lexerString ));
 return NULL;
}
static struct __vec *stringUpdate(const void *data, struct __vec *old, struct __vec *new,
                           long pos, long* end,const void *data2) {
 
 struct lexerString find;
 if(0==strcmp((char*)old,(char*)new+pos))
	
 if(stringParse(new,pos,end,&find))
	return __vecAppendItem(NULL,&find,sizeof(struct lexerString ));
 return NULL;
}

STR_TYPE_DEF(char,Char);
STR_TYPE_FUNCS(char,Char);
static int charEq(const void *a,const void *b) {
 return a==b;
}
static llLexerItem expectKeyword(llLexerItem node,struct __lexerItemTemplate *template,const char *spelling) {
 __auto_type lexerItem=llLexerItemValuePtr(node);
 assert(lexerItem->template==template);
 __auto_type value=(const char *)lexerItemValuePtr(lexerItem);
 assert(0==strcmp(value,spelling));
 return llLexerItemNext(node);
}
static llLexerItem expectName(llLexerItem node,struct __lexerItemTemplate *template,const char *spelling) {
 __auto_type lexerItem=llLexerItemValuePtr(node);
 assert(lexerItem->template==template);
 __auto_type value=(const char *)lexerItemValuePtr(lexerItem);
 assert(0==strcmp(value,spelling));
 return llLexerItemNext(node);
}
static llLexerItem expectInt(llLexerItem node,struct __lexerItemTemplate *template,unsigned long value) {
 __auto_type lexerItem=llLexerItemValuePtr(node);
 assert(lexerItem->template==template);
 __auto_type value2=(struct lexerInt*)lexerItemValuePtr(lexerItem);
 assert(value2->value.uLong==value);
 return llLexerItemNext(node);
}
void cachingLexerTests() {
	const char *text = "graph h2O {\n"
	                   "    h [Label=\"H\"];\n"
	                   "    h -- O1;\n"
	                   "    h -- O2;\n"
	                   "}";
	__auto_type nameTemplate = nameTemplateCreate();
	__auto_type keywordTemplate = keywordTemplateCreate();
	__auto_type intTemplate=intTemplateCreate();
	__auto_type floatingTemplate=floatingTemplateCreate();
	__auto_type templates=strLexerItemTemplateResize(NULL,4);
	templates[0]=&nameTemplate;
	templates[1]=&keywordTemplate;
	templates[2]=&intTemplate;
	templates[3]=&floatingTemplate;
	
	__auto_type str=strCharAppendData(NULL,(char*)text,strlen(text));
	__auto_type lexer=lexerCreate((struct __vec*)str,templates,charEq,skipWhitespace);
	__auto_type items=lexerGetItems(lexer);
	
	__auto_type node=__llGetFirst(items);
	node=expectKeyword(node,&keywordTemplate,"graph");
	node=expectKeyword(node,&nameTemplate,"h2O");
	node=expectKeyword(node,&keywordTemplate,"{");
	node=expectKeyword(node,&nameTemplate,"h");
	node=expectKeyword(node,&keywordTemplate,"[");
	node=expectKeyword(node,&nameTemplate,"Label");
	node=expectKeyword(node,&keywordTemplate,"=");
}
