#include "escaper.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
char *escapeString(char *str,long len) {
	long retValCap = len + 256;
	char *retVal = calloc(retValCap,1);
	__auto_type where = retVal;

	while (*str != 0) {
		long offset = (where - retVal) / sizeof(char);
		if (offset >= retValCap) {
			// Be sure to allocate memory ahead so operations dont segfault
			retValCap += 10;
			retVal = realloc(retVal, retValCap);

			where = retVal + offset;
		}

		// sorry for long code
		if (*str == '\\') {
			memcpy(where, "\\\\", 2);
			where += 2;
			str++;
			continue;
		}
		if (*str == '\a') {
			memcpy(where, "\\a", 2);
			where += 2;
			str++;
			continue;
		}
		if (*str == '\b') {
			memcpy(where, "\\b", 2);
			where += 2;
			str++;
			continue;
		}
		if (*str == '\f') {
			memcpy(where, "\\f", 2);
			str++;
			where += 2;
			continue;
		}
		if (*str == '\n') {
			memcpy(where, "\\n", 2);
			str++;
			where += 2;
			continue;
		}
		if (*str == '\r') {
			memcpy(where, "\\r", 2);
			str++;
			where += 2;
			continue;
		}
		if (*str == '\t') {
			memcpy(where, "\\t", 2);
			str++;
			where += 2;
			continue;
		}
		if (*str == '\v') {
			memcpy(where, "\\v", 2);
			str++;
			where += 2;
			continue;
		}
		if (*str == '\?') {
			memcpy(where, "\\?", 2);
			where += 2;
			str++;
			continue;
		}
		if (*str == '\"') {
			memcpy(where, "\\\"", 2);
			where += 2;
			str++;
			continue;
		}
		//\uHHHH
		const uint8_t twoByteUTF8 = 0 | 0b11000000;
		const uint8_t threeByteUTF8 = 0 | 0b11100000;
		if ((str[0] & 0b11100000) == twoByteUTF8 || (str[0] & 0b11110000) == threeByteUTF8) {
			uint64_t value = 0;
			bool flag = false; // if passes 3 bytes
			if ((str[0] & 0b11110000) == threeByteUTF8) {
				value = str[0] & ~0b11110000;
				value <<= 6;
				str++;
				value |= str[0] & 0b00111111;
				value <<= 6;
				str++;
				value |= str[0] & 0b00111111;
				flag = true;
			} else {
				value |= str[0] & 0b00011111;
				str++;
				value <<= 6; // shift for next byte
				value = str[0] & 0b00111111;
			}
			char temp[7];
			sprintf(temp, "\\u%x%x%x%x", (uint32_t)((value >> 12) & 0xf), (uint32_t)((value >> 8) & 0xf), (uint32_t)((value >> 4) & 0xf), (uint32_t)((value >> 0) & 0xf));
			memcpy(where, temp, 6);
			where += 6;
			str++;
			continue;
		}
		//\UNNNNnnnn
		const uint8_t fourByteUTF8 = 0b11110000;
		if ((str[0] & 0b11111000) == fourByteUTF8) {
			uint64_t value = 0;
			value = str[0] & ~fourByteUTF8;
			for (int i = 1; i != 4; i++) {
				value <<= 6;
				value |= str[i] & 0b00111111;
			}
			char temp[2 + 8 + 1]; //[\][U][H]*8[null]
			sprintf(temp, "\\U%x%x%x%x%x%x%x%x", (uint32_t)((value >> 28) & 0xf), (uint32_t)((value >> 24) & 0xf), (uint32_t)((value >> 20) & 0xf),
			        (uint32_t)((value >> 16) & 0xf), // 4
			        (uint32_t)((value >> 12) & 0xf), (uint32_t)((value >> 8) & 0xf), (uint32_t)((value >> 3) & 0xf),
			        (uint32_t)((value >> 0) & 0xf) // 8
			);
			memcpy(where, temp, 2 + 8);
			where += 2 + 8;
			str += 4;
			continue;
		}
		// if cant be inputed with a (us) keyboard,escape

		const char *valids = " ~!@#$%^&*()_-+=|}{[]\\;':\",./<>?";
		bool isValid = false;
		if (*str >= 'a' && 'z' >= *str)
			isValid = true;
		else if (*str >= 'A' && 'Z' >= *str)
			isValid = true;
		else if (*str >= '0' && *str <= '9')
			isValid = true;
		else if (strchr(valids, *str) != NULL)
			isValid = true;
		if (!isValid) {
			char temp[32];
			sprintf(temp, "%o", str[0]);
			int len = strlen(temp);
			where[0] = '\\';
			memcpy(where + 1, temp, len);
			str++;
			where += len+1;
			continue;
		}
		*where = *str;
		str++;
		where++;
	}

	// Add a NULL byte
	long offset = (where - retVal) / sizeof(char);
	if (offset + 1 >= retValCap) {
		retVal = realloc(retVal, retValCap + 1);
		where = retVal + offset;
	}

	*where = '\0';
	return retVal;
}
