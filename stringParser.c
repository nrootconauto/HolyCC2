#include <stdio.h>
#include <str.h>
#include <stringParser.h>
#include <utf8Encode.h>
int stringParse(const struct __vec *new, long pos, long *end, struct parsedString *retVal, int *err) {
	if (err != NULL)
		*err = 0;

	__auto_type endPtr = strlen((char *)new) + (char *)new;
	__auto_type currPtr = pos + (char *)new;

	if (currPtr < endPtr) {
		if (!(*currPtr == '"' || *currPtr == '\''))
			return 0;

		struct __vec *retValText = NULL;

		int isChar = *currPtr == '\'';
		char endChar = (isChar) ? '\'' : '"';

		currPtr++;
		for (;;) {
			__auto_type end2 = strchr(currPtr, endChar);

			char *escape;
			escape = strchr(currPtr, '\\');
			if (end2 == NULL)
				goto malformed;

			if (escape == NULL || escape > end2)
				goto skip;

			if (retVal != NULL)
				retValText = __vecAppendItem(retValText, currPtr, escape - currPtr);
			escape++;

			char tmp;
			unsigned int codePoint = 0;
			switch (*escape) {
			case 'a':
				tmp = '\a';
				break;
			case 'b':
				tmp = '\b';
				break;
			case 'e':
				tmp = '\e';
				break;
			case 'f':
				tmp = '\f';
				break;
			case 'n':
				tmp = '\n';
				break;
			case 'r':
				tmp = '\r';
				break;
			case 't':
				tmp = '\t';
				break;
			case 'v':
				tmp = '\v';
				break;
			case '\\':
				tmp = '\\';
				break;
			case '\'':
				tmp = '\'';
				break;
			case '"':
				tmp = '"';
				break;
			case 'u': {
				if (escape + 4 < endPtr)
					goto malformed;

				__auto_type slice = __vecAppendItem(NULL, (char *)escape, 4);
				sscanf((char *)slice, "%x", &codePoint);
				__vecDestroy(&slice);
				
				currPtr = (char *)escape + 4;
				goto utfEncode;
			}
			case 'U': {
				if (escape + 8 < endPtr)
					goto malformed;

				__auto_type slice = __vecAppendItem(NULL, (char *)escape, 4);
				unsigned int codePoint;
				sscanf((char *)slice, "%x", &codePoint);

				currPtr = (char *)escape + 8;
				goto utfEncode;
			}
			case '0' ... '8': {
					__auto_type originalPos=escape;
					int count = 1;
				for (escape++; escape < endPtr && count < 3; escape++)
					if (*escape >= '0' && *escape <= '7')
						count++;
					else
						break;

				__auto_type slice = __vecAppendItem(NULL, (char *)originalPos, count);
				slice=__vecAppendItem(slice, "\0", 1);
				sscanf((char *)slice, "%o", &codePoint);

				currPtr = escape;
				goto utfEncode;
			}
			}
			currPtr++;
			goto skip;
		utfEncode : {
			int width;
			char buffer[4];
			utf8Encode(codePoint, buffer, &width);
			if (retVal != NULL)
				retValText = __vecAppendItem(retValText, buffer, width);
		}
		skip:
			// Append text to end or next escape
			escape = strchr(currPtr, '\\');
			__auto_type toPtr = (escape < end2 && escape != NULL) ? escape : end2;
			if (retVal != NULL)
				retValText = __vecAppendItem(retValText, currPtr, toPtr - currPtr);
			currPtr = toPtr;

			if (toPtr == end2) {
				// Append NULL byte
					retValText = __vecAppendItem(retValText, "\0", 1);

				if (end != NULL)
					*end = (end2 + 1 - (char *)new) / sizeof(char);
				break;
			}
		}

		// If char,ensure is <=8
		if (isChar && __vecSize(retValText) > 8)
			goto malformed;

		if (retVal != NULL) {
			retVal->isChar = isChar;
			retVal->text = retValText;
		}

		return 1;
	}
	return 0;
malformed : {
	if (err != NULL)
		*err = 1;
	return 0;
}
}
