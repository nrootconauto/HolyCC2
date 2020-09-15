#pragma once
struct __ll;
#define LL_TYPE_DEF(type,suffix) typedef struct __ll *ll##suffix;
#define LL_TYPE_FUNCS(type, suffix)                                            \
	inline ll##suffix ll##suffix##Create(type value)                             \
	    __attribute__((always_inline));                                          \
	inline ll##suffix ll##suffix##Create(type value) {                           \
		return __llCreate(&(value), sizeof(type));                                 \
	}                                                                            \
	inline ll##suffix ll##suffix##Empty() __attribute__((always_inline));        \
	inline ll##suffix ll##suffix##Empty() { return NULL; }                       \
	inline ll##suffix ll##suffix##Insert(ll##suffix from, ll##suffix toInsert,   \
	                                     int (*pred)(void *, void *))            \
	    __attribute__((always_inline));                                          \
	inline ll##suffix ll##suffix##Insert(ll##suffix from, ll##suffix toInsert,   \
	                                     int (*pred)(void *, void *)) {          \
		return (ll##suffix){__llInsert(from, toInsert, pred)};                     \
	}                                                                            \
	inline ll##suffix ll##suffix##Remove(ll##suffix from)                        \
	    __attribute__((always_inline));                                          \
	inline ll##suffix ll##suffix##Remove(ll##suffix from) {                      \
		__llRemoveNode(from);                                                      \
		return from;                                                               \
	}                                                                            \
	inline type *ll##suffix##ValuePtr(ll##suffix Node)                           \
	    __attribute__((always_inline));                                          \
	inline type *ll##suffix##ValuePtr(ll##suffix Node) {                         \
		return __llValuePtr(Node);                                                 \
	}                                                                            \
	inline ll##suffix ll##suffix##Next(ll##suffix Node)                          \
	    __attribute__((always_inline));                                          \
	inline ll##suffix ll##suffix##Next(ll##suffix Node) {                        \
		return __llNext(Node);                                                     \
	}                                                                            \
	inline ll##suffix ll##suffix##Prev(ll##suffix Node)                          \
	    __attribute__((always_inline));                                          \
	inline ll##suffix ll##suffix##Prev(ll##suffix Node) {                        \
		return (ll##suffix)__llPrev(Node);                                         \
	}                                                                            \
	inline void ll##suffix##Destroy(ll##suffix *node)                             \
	    __attribute__((always_inline));                                          \
	inline void ll##suffix##Destroy(ll##suffix *node) { __llDestroy(*node, NULL); }
void __llDestroy(struct __ll *node, void (*killFunc)(void *));
void *__llValuePtr(struct __ll *node);
void __llRemoveNode(struct __ll *node);
struct __ll *__llCreate(void *item, long size);
struct __ll *__llInsert(struct __ll *from, struct __ll *newItem,
                        int (*pred)(void *, void *));
int llLastPred(void *a, void *b);
int llFirstPred(void *a, void *b);
struct __ll *__llPrev(struct __ll *node);
struct __ll *__llNext(struct __ll *node);
