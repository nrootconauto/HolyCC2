#pragma once
struct __ll;
#define LL_TYPE_DEF(type, suffix) typedef struct __ll *ll##suffix;
#define LL_TYPE_FUNCS(type, suffix)                                            \
	inline ll##suffix ll##suffix##Create(type value)                             \
	    __attribute__((always_inline));                                          \
	inline ll##suffix ll##suffix##Create(type value) {                           \
		return __llCreate(&(value), sizeof(type));                                 \
	}                                                                            \
	inline ll##suffix ll##suffix##Empty() __attribute__((always_inline));        \
	inline ll##suffix ll##suffix##Empty() { return NULL; }                       \
	inline ll##suffix ll##suffix##Insert(                                        \
	    ll##suffix from, ll##suffix toInsert,                                    \
	    int (*pred)(const void *, const void *)) __attribute__((always_inline)); \
	inline ll##suffix ll##suffix##Insert(                                        \
	    ll##suffix from, ll##suffix toInsert,                                    \
	    int (*pred)(const void *, const void *)) {                               \
		return (ll##suffix){__llInsert(from, toInsert, pred)};                     \
	}                                                                            \
	inline ll##suffix ll##suffix##Remove(ll##suffix from)                        \
	    __attribute__((always_inline));                                          \
	inline ll##suffix ll##suffix##Remove(ll##suffix from) {                      \
		return (ll##suffix)__llRemoveNode(from);                                   \
	}                                                                            \
	inline type *ll##suffix##ValuePtr(const ll##suffix Node)                     \
	    __attribute__((always_inline));                                          \
	inline type *ll##suffix##ValuePtr(const ll##suffix Node) {                   \
		return (type *)__llValuePtr(Node);                                     \
	}                                                                            \
	inline ll##suffix ll##suffix##Next(const ll##suffix Node)                    \
	    __attribute__((always_inline));                                          \
	inline ll##suffix ll##suffix##Next(const ll##suffix Node) {                  \
		return (ll##suffix)__llNext(Node);                                         \
	}                                                                            \
	inline ll##suffix ll##suffix##Prev(const ll##suffix Node)                    \
	    __attribute__((always_inline));                                          \
	inline ll##suffix ll##suffix##Prev(const ll##suffix Node) {                  \
		return (ll##suffix)__llPrev(Node);                                         \
	}                                                                            \
	inline void ll##suffix##Destroy(ll##suffix *node, void (*killFunc)(void *))  \
	    __attribute__((always_inline));                                          \
	inline void ll##suffix##Destroy(ll##suffix *node,                            \
	                                void (*killFunc)(void *)) {                  \
		__llDestroy(*node, killFunc);                                              \
	}                                                                            \
	inline ll##suffix ll##suffix##FindLeft(                                      \
	    const ll##suffix node, const void *data,                                 \
	    int (*pred)(const void *, const void *)) __attribute__((always_inline)); \
	inline ll##suffix ll##suffix##FindLeft(                                      \
	    const ll##suffix node, const void *data,                                 \
	    int (*pred)(const void *, const void *)) {                               \
		return (ll##suffix)__llFindLeft(node, data, pred);                         \
	}                                                                            \
	inline ll##suffix ll##suffix##FindRight(                                     \
	    const ll##suffix node, const void *data,                                 \
	    int (*pred)(const void *, const void *)) __attribute__((always_inline)); \
	inline ll##suffix ll##suffix##FindRight(                                     \
	    const ll##suffix node, const void *data,                                 \
	    int (*pred)(const void *, const void *)) {                               \
		return (ll##suffix)__llFindRight(node, data, pred);                        \
	}                                                                            \
	inline ll##suffix ll##suffix##First(const ll##suffix node)                   \
	    __attribute__((always_inline));                                          \
	inline ll##suffix ll##suffix##First(const ll##suffix node) {                 \
		return (ll##suffix)__llGetFirst(node);                                     \
	}                                                                            \
	inline ll##suffix ll##suffix##Last(const ll##suffix node)                    \
	    __attribute__((always_inline));                                          \
	inline ll##suffix ll##suffix##Last(const ll##suffix node) {                  \
		return (ll##suffix)__llGetEnd(node);                                       \
	}                                                                            \
	inline long ll##suffix##Size(const ll##suffix node)                          \
	    __attribute__((always_inline));                                          \
	inline long ll##suffix##Size(const ll##suffix node) {                        \
		return __llSize(node);                                                     \
	}                                                                            \
	inline void ll##suffix##InsertListBefore(ll##suffix node, ll##suffix item)   \
	    __attribute__((always_inline));                                          \
	inline void ll##suffix##InsertListBefore(ll##suffix node, ll##suffix item) { \
		llInsertListBefore(node, item);                                            \
	}                                                                            \
	inline void ll##suffix##InsertListAfter(ll##suffix node, ll##suffix item)    \
	    __attribute__((always_inline));                                          \
	inline void ll##suffix##InsertListAfter(ll##suffix node, ll##suffix item) {  \
		llInsertListAfter(node, item);                                             \
	}
void __llDestroy(struct __ll *node, void (*killFunc)(void *));
void *__llValuePtr(const struct __ll *node);
struct __ll *__llRemoveNode(struct __ll *node);
struct __ll *__llCreate(const void *item, long size);
struct __ll *__llInsert(struct __ll *from, struct __ll *newItem,
                        int (*pred)(const void *, const void *));
int llLastPred(const void *a, const void *b);
int __llFirstPred(const void *a, const void *b);
struct __ll *__llPrev(const struct __ll *node);
struct __ll *__llNext(const struct __ll *node);
struct __ll *__llFindRight(const struct __ll *list, const void *data,
                           int (*pred)(const void *a, const void *b));
struct __ll *__llFindLeft(const struct __ll *list, const void *data,
                          int (*pred)(const void *a, const void *b));
struct __ll *__llGetEnd(const struct __ll *list);
struct __ll *__llGetFirst(const struct __ll *list);
long __llSize(const struct __ll *list);
void llInsertListAfter(struct __ll *a, struct __ll *b);
void llInsertListBefore(struct __ll *a, struct __ll *b);
struct __ll *__llValueResize(struct __ll *list, long newSize);
long __llItemSize(const struct __ll *list);
