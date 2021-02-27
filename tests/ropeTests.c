#include <rope.h>
#include <string.h>
#include <assert.h>
void ropeTests() {
		const char *text="Hello World";
		struct rope *helloWorld=ropeFromText(text);
		char *result=ropeToText(helloWorld);
		assert(0==strcmp(text,result));
}
