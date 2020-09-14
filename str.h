#pragma once
struct __vec;
#include <string.h>
#define STR_TYPE(type, suffix)                                                 \
	typedef type *str##suffix;                                                   \
	inline void str##suffix##Destroy(str##suffix vec)                            \
	    __attribute__((always_inline));                                          \
	inline void str##suffix##Destroy(str##suffix vec) {                          \
		__vecDestroy((struct __vec *)vec);                                         \
	};                                                                           \
	inline str##suffix str##suffix##AppendItem(str##suffix vec, type item)       \
	    __attribute__((always_inline));                                          \
	inline str##suffix str##suffix##AppendItem(str##suffix vec, type item) {     \
		return (str##suffix)__vecAppendItem((struct __vec *)vec, &item,            \
		                                    sizeof(type));                         \
	}                                                                            \
	inline str##suffix str##suffix##Reserve(str##suffix vec, long count)         \
	    __attribute__((always_inline));                                          \
	inline str##suffix str##suffix##Reserve(str##suffix vec, long count) {       \
		return (str##suffix)__vecReserve((struct __vec *)vec,                      \
		                                 count * sizeof(type));                    \
	}                                                                            \
	inline long str##suffix##Size(str##suffix vec)                               \
	    __attribute__((always_inline));                                          \
	inline long str##suffix##Size(str##suffix vec) {                             \
		return __vecSize((struct __vec *)vec) / sizeof(type);                      \
	}                                                                            \
	inline long str##suffix##Capacity(str##suffix vec)                           \
	    __attribute__((always_inline));                                          \
	inline long str##suffix##Capacity(str##suffix vec) {                         \
		return __vecCapacity((struct __vec *)vec) / sizeof(type);                  \
	}                                                                            \
	inline str##suffix str##suffix##Concat(str##suffix vec, str##suffix vec2)    \
	    __attribute__((always_inline));                                          \
	inline str##suffix str##suffix##Concat(str##suffix vec, str##suffix vec2) {  \
		return (str##suffix)__vecConcat((struct __vec *)vec,                       \
		                                (struct __vec *)vec2);                     \
	}                                                                            \
	inline str##suffix str##suffix##Resize(str##suffix vec, long size)           \
	    __attribute__((always_inline));                                          \
	inline str##suffix str##suffix##Resize(str##suffix vec, long size) {         \
		return (str##suffix)__vecResize((struct __vec *)vec, size * sizeof(type)); \
	}                                                                            \
	inline str##suffix str##suffix##SortedInsert(str##suffix vec, type item,     \
	                                             int (*pred)(void *, void *))    \
	    __attribute__((always_inline));                                          \
	inline str##suffix str##suffix##SortedInsert(str##suffix vec, type item,     \
	                                             int (*pred)(void *, void *)) {  \
		return (str##suffix)__vecSortedInsert((struct __vec *)vec, &item,          \
		                                      sizeof(item), pred);                 \
	}                                                                            \
	inline str##suffix str##suffix##AppendData(                                  \
	    str##suffix vec, type *data, long count) __attribute__((always_inline)); \
	inline str##suffix str##suffix##AppendData(str##suffix vec, type *data,      \
	                                           long count) {                     \
		__auto_type oldSize = __vecSize((struct __vec *)vec);                      \
		vec = (str##suffix)__vecResize((struct __vec *)vec, count * sizeof(type)); \
		memcpy(&vec[oldSize], data, count * sizeof(type));                         \
		return vec;                                                                \
	}
struct __vec *__vecAppendItem(struct __vec *a, void *item, long itemSize);
struct __vec *__vecReserve(struct __vec *a, long capacity);
struct __vec *__vecConcat(struct __vec *a, struct __vec *b);
long __vecCapacity(struct __vec *a);
long __vecSize(struct __vec *a);
struct __vec *__vecResize(struct __vec *a, long size);
void __vecDestroy(struct __vec *a);
struct __vec *__vecSortedInsert(struct __vec *a, void *item, long itemSize,
                                int predicate(void *, void *));
