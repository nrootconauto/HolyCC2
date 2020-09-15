#include <str.h>
#pragma once
struct __graphNode;
#define GRAPH_TYPE_DEF(type, suffix)                                           \
	typedef struct __graphNode *graphNode##suffix;
#define GRAPH_TYPE_FUNCS(type, suffix)                                         \
	STR_TYPE_DEF(graphNode##suffix, GraphNode##suffix##P);                       \
	STR_TYPE_FUNCS(graphNode##suffix, GraphNode##suffix##P);                     \
	inline void graphNode##suffix##KillGraph(graphNode##suffix *node,            \
	                                         void (*kill)(void *))               \
	    __attribute__((always_inline));                                          \
	inline void graphNode##suffix##KillGraph(graphNode##suffix *node,            \
	                                         void (*kill)(void *)) {             \
		__graphKillAll((graphNode##suffix) * node, kill);                          \
	}                                                                            \
	inline void graphNode##suffix##Kill(graphNode##suffix *node,                 \
	                                    void (*kill)(void *))                    \
	    __attribute__((always_inline));                                          \
	inline void graphNode##suffix##Kill(graphNode##suffix *node,                 \
	                                    void (*kill)(void *)) {                  \
		__graphNodeKill((graphNode##suffix) * node, kill);                         \
	}                                                                            \
	inline void graphNode##suffix##Detach(graphNode##suffix node,                \
	                                      graphNode##suffix node2)               \
	    __attribute__((always_inline));                                          \
	inline void graphNode##suffix##Detach(graphNode##suffix node,                \
	                                      graphNode##suffix node2) {             \
		return __graphNodeDetach((graphNode##suffix)node,                          \
		                         (graphNode##suffix)node2);                        \
	}                                                                            \
	inline void graphNode##suffix##VisitForward(                                 \
	    graphNode##suffix node, void *data,                                      \
	    int(pred)(struct __graphNode *, void *),                                 \
	    void (*visit)(struct __graphNode *, void *))                             \
	    __attribute__((always_inline));                                          \
	inline void graphNode##suffix##VisitForward(                                 \
	    graphNode##suffix node, void *data,                                      \
	    int(pred)(struct __graphNode *, void *),                                 \
	    void (*visit)(struct __graphNode *, void *)) {                           \
		__graphNodeVisitForward((struct __graphNode *)node, data, pred, visit);    \
	}                                                                            \
	inline void graphNode##suffix##VisitBackward(                                \
	    graphNode##suffix node, void *data,                                      \
	    int(pred)(struct __graphNode *, void *),                                 \
	    void (*visit)(struct __graphNode *, void *))                             \
	    __attribute__((always_inline));                                          \
	inline void graphNode##suffix##VisitBackward(                                \
	    graphNode##suffix node, void *data,                                      \
	    int(pred)(struct __graphNode *, void *),                                 \
	    void (*visit)(struct __graphNode *, void *)) {                           \
		__graphNodeVisitBackward((struct __graphNode *)node, data, pred, visit);   \
	}                                                                            \
	inline graphNode##suffix graphNode##suffix##Create(type value, int version)  \
	    __attribute__((always_inline));                                          \
	inline graphNode##suffix graphNode##suffix##Create(type value,               \
	                                                   int version) {            \
		return (graphNode##suffix)__graphNodeCreate(&value, sizeof(type),          \
		                                            version);                      \
	}                                                                            \
	inline void graphNode##suffix##ReadLock(graphNode##suffix node)              \
	    __attribute__((always_inline));                                          \
	inline void graphNode##suffix##ReadLock(graphNode##suffix node) {            \
		__graphNodeReadLock(node);                                                 \
	}                                                                            \
	inline void graphNode##suffix##ReadUnlock(graphNode##suffix node)            \
	    __attribute__((always_inline));                                          \
	inline void graphNode##suffix##ReadUnlock(graphNode##suffix node) {          \
		__graphNodeReadUnlock(node);                                               \
	}                                                                            \
	inline void graphNode##suffix##WriteLock(graphNode##suffix node)             \
	    __attribute__((always_inline));                                          \
	inline void graphNode##suffix##WriteLock(graphNode##suffix node) {           \
		__graphNodeWriteLock(node);                                                \
	}                                                                            \
	inline void graphNode##suffix##WriteUnlock(graphNode##suffix node)           \
	    __attribute__((always_inline));                                          \
	inline void graphNode##suffix##WriteUnlock(graphNode##suffix node) {         \
		__graphNodeWriteUnlock(node);                                              \
	}                                                                            \
	inline void graphNode##suffix##Connect(graphNode##suffix node,               \
	                                       graphNode##suffix node2)              \
	    __attribute__((always_inline));                                          \
	inline void graphNode##suffix##Connect(graphNode##suffix node,               \
	                                       graphNode##suffix node2) {            \
		__graphNodeConnect(node, node2);                                           \
	}                                                                            \
	inline const str##GraphNode##suffix##P graphNode##suffix##Incoming(          \
	    graphNode##suffix node) __attribute__((always_inline));                  \
	inline const str##GraphNode##suffix##P graphNode##suffix##Incoming(          \
	    graphNode##suffix node) {                                                \
		return (str##GraphNode##suffix##P)__graphNodeIncoming(node);               \
	}                                                                            \
	inline const str##GraphNode##suffix##P graphNode##suffix##Outgoing(          \
	    graphNode##suffix node) __attribute__((always_inline));                  \
	inline const str##GraphNode##suffix##P graphNode##suffix##Outgoing(          \
	    graphNode##suffix node) {                                                \
		return (str##GraphNode##suffix##P)__graphNodeOutgoing(node);               \
	}
STR_TYPE_DEF(struct __graphNode *, GraphNodeP);
STR_TYPE_FUNCS(struct __graphNode *, GraphNodeP);
void __graphKillAll(struct __graphNode *start, void (*killFunc)(void *));
void __graphNodeKill(struct __graphNode *node, void (*killItem)(void *item));
void __graphNodeDetach(struct __graphNode *in, struct __graphNode *out);
void __graphNodeVisitForward(struct __graphNode *node, void *data,
                             int(pred)(struct __graphNode *, void *),
                             void (*visit)(struct __graphNode *, void *));
void __graphNodeVisitBackward(struct __graphNode *node, void *data,
                              int(pred)(struct __graphNode *, void *),
                              void (*visit)(struct __graphNode *, void *));
struct __graphNode *__graphNodeCreate(void *value, long itemSize, int version);
void __graphNodeReadLock(struct __graphNode *node);
void __graphNodeReadUnlock(struct __graphNode *node);
void __graphNodeWriteLock(struct __graphNode *node);
void __graphNodeWriteUnlock(struct __graphNode *node);
void __graphNodeConnect(struct __graphNode *a, struct __graphNode *b);
const strGraphNodeP __graphNodeIncoming(struct __graphNode *node);
const strGraphNodeP __graphNodeOutgoing(struct __graphNode *node);
