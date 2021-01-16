#include <asmEmitter.c>
#include <assert.h>
#include <parserA.h>
#include <lexer.h>
#include <str.h>
void asmEmitterTests() {
		const char *text=
				"asm {"
				"    MOV EAX,10\n"
				"    MOV_CR0_RAX\n"
				"    MOV EAX,ES:0xff[EAX]\n"
				"};";
		__auto_type str=strCharAppendData(NULL, text, strlen(text)+1);
		int err=0;
		__auto_type items=lexText((struct __vec*)str, &err);
		assert(!err);
		__auto_type block=parseStatement(items, NULL);
}
