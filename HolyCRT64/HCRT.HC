#define NULL (0)
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
}