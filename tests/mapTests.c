#include <assert.h>
#include <hashTable.h>
MAP_TYPE_DEF(int, Int);
MAP_TYPE_FUNCS(int, Int);
void mapTests() {
	__auto_type map = mapIntCreate();
	const char chars[] = "abcdefghijklmnopqrstuvwxyz";
	__auto_type count = sizeof(chars) / sizeof(*chars);
	char buffer[2];
	;
	for (int i = 0; i != count; i++) {
		buffer[0] = chars[i];
		buffer[1] = '\0';
		mapIntInsert(map, buffer, chars[i]);
	}
	//
	for (int i = 0; i != count; i++) {
		buffer[0] = chars[i];
		buffer[1] = '\0';
		__auto_type result = (int *)mapIntGet(map, buffer);
		assert(result != NULL);
		assert(*result == chars[i]);
	}
	//
	for (int i = 0; i != count; i++) {
		buffer[0] = chars[i];
		buffer[1] = '\0';
		mapIntRemove(map, buffer, NULL);
		assert(NULL == mapIntGet(map, buffer));
	}
	//
	mapIntDestroy(map, NULL);
}
