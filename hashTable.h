#include <stdlib.h>
#pragma once
struct __map;
#define MAP_TYPE_DEF(type, suffix) typedef struct __map *map##suffix;
#define MAP_TYPE_FUNCS(type, suffix)                                           \
	inline map##suffix map##suffix##Create() __attribute__((always_inline));     \
	inline map##suffix map##suffix##Create() {                                   \
		return (map##suffix)__mapCreate();                                         \
	};                                                                           \
	inline int map##suffix##Insert(map##suffix map, const char *key, type item)  \
	    __attribute__((always_inline));                                          \
	inline int map##suffix##Insert(map##suffix map, const char *key,             \
	                               type item) {                                  \
		return __mapInsert(map, key, &item, sizeof(type));                         \
	};                                                                           \
	inline void *map##suffix##Get(map##suffix map, const char *key)              \
	    __attribute__((always_inline));                                          \
	inline void *map##suffix##Get(map##suffix map, const char *key) {            \
		return __mapGet(map, key);                                                 \
	};                                                                           \
	inline void map##suffix##Remove(map##suffix map, const char *key,            \
	                                void (*kill)(void *))                        \
	    __attribute__((always_inline));                                          \
	inline void map##suffix##Remove(map##suffix map, const char *key,            \
	                                void (*kill)(void *)) {                      \
		return __mapRemove(map, key, kill);                                        \
	}                                                                            \
	inline void map##suffix##Destroy(map##suffix map, void (*kill)(void *))      \
	    __attribute__((always_inline));                                          \
	inline void map##suffix##Destroy(map##suffix map, void (*kill)(void *)) {    \
		__mapDestroy(map, kill);                                                   \
	}
void __mapDestroy(struct __map *map, void (*kill)(void *));
int __mapInsert(struct __map *map, const char *key, const void *item,
                const long itemSize);
void *__mapGet(struct __map *map, const char *key);
struct __map *__mapCreate();
void __mapRemove(struct __map *map, const char *key, void (*kill)(void *));
