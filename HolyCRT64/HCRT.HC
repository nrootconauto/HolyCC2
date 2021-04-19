#define NULL (0)
_import pow F64 PowF64(F64 b,F64 e);
#include "Types.HC"
#include "ExceptLin.HC"
#include "String.HC"
#include "JSON.HC"
I32i main() {
					U8 *str=__JsonParseString("\"a\nbc\"");
					printf("%s\n",str);
					printf("%li\n", __JsonParseNumber("123"));
					printf("%li\n", __JsonParseNumber("0xFf"));
					printf("%li\n", __JsonParseNumber("0b1011"));
					printf("%s\n",__JsonParserSkipWhitespace("   abc"));
					printf("%s\n",__JsonParserSkipWhitespace("//   \nabc"));
					printf("%s\n",StrStr("123abc","abc"));
					printf("%s\n",__JsonParserSkipWhitespace("/*abc*/abc"));
					CJsonNode *arr=JsonNodeParse("[1,2,3]");
}