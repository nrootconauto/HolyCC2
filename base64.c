#include <base64.h>
#include <stdint.h>
#include <stdlib.h>
#include <str.h>
STR_TYPE_DEF(char, Char);
STR_TYPE_FUNCS(char, Char);
static unsigned char getByteAt(const char *buffer, long bit) {
	int masked1 = (((unsigned char *)buffer)[bit / 8] >> (bit % 8));
	int masked2 = (((unsigned char *)buffer)[(bit + 8) / 8] >> ((bit + 8) % 8));
	int masked = (masked1 | (masked2 << (8 - bit % 8))) & 0xff;

	return masked;
}
char *base64Enc(const char *buffer, long count) {
	//+1 for rounding up
	//+2 for padding
	//+1 for nullByte
	strChar retVal = strCharReserve(NULL, (double)count * 6. / 8. + 4);
	long bit = 0;
	unsigned char current = 0;
	const int sextetsPerRun = 4;
	for (; bit < count * 8;) {
		//
		// Bytes holds the text from buffer,byteCount is the number of bytes sotred
		// in bytes
		//
		uint64_t bytes = 0;
		int byteCount = sextetsPerRun * 6 / 8;
		long remaining = count - bit / 8;
		if (remaining > 3)
			remaining = 3;
		memcpy(&bytes, &buffer[bit / 8], remaining);

		uint64_t bytesSwapped = 0;
		((char *)&bytesSwapped)[0] = ((char *)&bytes)[2];
		((char *)&bytesSwapped)[1] = ((char *)&bytes)[1];
		((char *)&bytesSwapped)[2] = ((char *)&bytes)[0];

		bytes = bytesSwapped;
		// SH
		for (int i2 = sextetsPerRun - 1; i2 >= 0; i2--) {
			if (!(count > bit / 8))
				break;
			bit += 6;

			unsigned char atBit = (bytes >> (i2 * 6)) & 0b00111111;
			if (atBit >= 0 && atBit < 26) {
				retVal = strCharAppendItem(retVal, 'A' + atBit);
			} else if (atBit >= 26 && atBit < 52) {
				retVal = strCharAppendItem(retVal, 'a' + atBit - 26);
			} else if (atBit >= 52 && atBit < 62) {
				retVal = strCharAppendItem(retVal, '0' + atBit - 52);
			} else if (atBit == 62)
				retVal = strCharAppendItem(retVal, '+');
			else if (atBit == 63)
				retVal = strCharAppendItem(retVal, '/');
		}
	}

	// Add padding
	while (bit % 8 != 0) {
		retVal = strCharAppendItem(retVal, '=');
		bit += 6;
	}

	retVal = strCharAppendItem(retVal, '\0');

	char *retVal2 = malloc(strCharSize(retVal));
	strcpy(retVal2, retVal);

	strCharDestroy(&retVal);
	return retVal2;
}
