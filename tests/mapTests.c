#include <assert.h>
#include <hashTable.h>
#include <string.h>
MAP_TYPE_DEF(int, Int);
MAP_TYPE_FUNCS(int, Int);
void mapTests() {
	__auto_type map = mapIntCreate();
	const char chars[] = "abcdefghijklmnopqrstuvwxyz";
	__auto_type count = strlen(chars);
	char buffer[2];
	;
	for (int i = 0; i != count; i++) {
		buffer[0] = chars[i];
		buffer[1] = '\0';
		mapIntInsert(map, buffer, chars[i]);

		long kcount;
		const char *keys[i + 1];
		mapIntKeys(map, keys, &kcount);
		assert(kcount == i + 1);
		for (long i2 = 0; i2 != i + 1; i2++) {
			assert(chars[i2] == keys[i2][0]);
		}
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
		__auto_type result = (int *)mapIntGet(map, buffer);
		__auto_type key = mapIntValueKey(result);
		assert(0 == strcmp(key, buffer));
	}
	//
	__auto_type newMap = mapIntClone(map, NULL);
	for (int i = 0; i != count; i++) {
		buffer[0] = chars[i];
		buffer[1] = '\0';
		__auto_type result = (int *)mapIntGet(newMap, buffer);
		__auto_type key = mapIntValueKey(result);
		assert(0 == strcmp(key, buffer));
	}
	//
	for (int i = 0; i != count; i++) {
		buffer[0] = chars[i];
		buffer[1] = '\0';
		mapIntRemove(map, buffer, NULL);
		assert(NULL == mapIntGet(map, buffer));

		long kcount;
		const char *keys[count - i - 1];
		mapIntKeys(map, keys, &kcount);
		assert(kcount == count - i - 1);
		for (long i2 = 0; i2 != count - i - 1; i2++) {
			assert(chars[i + 1 + i2] == keys[i2][0]);
		}
	}
	//
	mapIntDestroy(map, NULL);
	mapIntDestroy(newMap, NULL);
}
