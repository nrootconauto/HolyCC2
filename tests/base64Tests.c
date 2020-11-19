#include <assert.h>
#include <base64.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
void base64Tests() {
	// https://en.wikipedia.org/wiki/Base64
	//
	const char *text =
	    "Man is distinguished, not only by his reason, but by this singular "
	    "passion from other animals, which is a lust of the mind, that by a "
	    "perseverance of delight in the continued and indefatigable generation "
	    "of knowledge, exceeds the short vehemence of any carnal pleasure.";
	const char *expected =
	    "TWFuIGlzIGRpc3Rpbmd1aXNoZWQsIG5vdCBvbmx5IGJ5IGhpcyByZWFzb24sIGJ1dCBieSB0"
	    "aGlz"
	    "IHNpbmd1bGFyIHBhc3Npb24gZnJvbSBvdGhlciBhbmltYWxzLCB3aGljaCBpcyBhIGx1c3Qg"
	    "b2Yg"
	    "dGhlIG1pbmQsIHRoYXQgYnkgYSBwZXJzZXZlcmFuY2Ugb2YgZGVsaWdodCBpbiB0aGUgY29u"
	    "dGlu"
	    "dWVkIGFuZCBpbmRlZmF0aWdhYmxlIGdlbmVyYXRpb24gb2Yga25vd2xlZGdlLCBleGNlZWRz"
	    "IHRo"
	    "ZSBzaG9ydCB2ZWhlbWVuY2Ugb2YgYW55IGNhcm5hbCBwbGVhc3VyZS4=";
	char *res = base64Enc(text, strlen(text));
	assert(0 == strcmp(expected, res));
	free(res);
}
