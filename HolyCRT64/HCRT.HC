#include "Types.HC"
#include "ExceptLin.HC"
#include "JSON.HC"
I32i main() {
					U8 *str=JsonParseString("\"abc\"");
					printf("%s\n",str);
}