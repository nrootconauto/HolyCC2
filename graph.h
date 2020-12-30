#include <hashTable.h>
#include <linkedList.h>
#include <stdio.h>
#include <str.h>
#pragma once
struct __graphNode;
struct __graphEdge;
#define GRAPH_TYPE_DEF(type, edgeType, suffix)                                 \
	typedef struct __graphNode *graphNode##suffix;                               \
	typedef struct __graphEdge *graphEdge##suffix;
#define GRAPH_TYPE_FUNCS(type, edgeType, suffix)                               \
	STR_TYPE_DEF(graphNode##suffix, GraphNode##suffix##P);                       \
	STR_TYPE_FUNCS(graphNode##suffix, GraphNode##suffix##P);                     \
	STR_TYPE_DEF(graphEdge##suffix, GraphEdge##suffix##P);                       \
	STR_TYPE_FUNCS(graphEdge##suffix, GraphEdge##suffix##P);                     \
	inline void graphNode##suffix##KillGraph(                                    \
	    graphNode##suffix *node, void (*killNode)(void *),                       \
	    void (*killEdge)(void *)) __attribute__((always_inline));                \
	inline void graphNode##suffix##KillGraph(graphNode##suffix *node,            \
	                                         void (*killNode)(void *),           \
	                                         void (*killEdge)(void *)) {         \
		__graphKillAll((graphNode##suffix) * node, killNode, killEdge);            \
	}                                                                            \
	inline void graphNode##suffix##Kill(                                         \
	    graphNode##suffix *node, void (*killNode)(void *),                       \
	    void (*killEdge)(void *)) __attribute__((always_inline));                \
	inline void graphNode##suffix##Kill(graphNode##suffix *node,                 \
	                                    void (*killNode)(void *),                \
	                                    void (*killEdge)(void *)) {              \
		__graphNodeKill((graphNode##suffix) * node, killNode, killEdge);           \
	}                                                                            \
	inline void graphEdge##suffix##Kill(                                         \
	    graphNode##suffix node, graphNode##suffix node2, void *data,             \
	    int (*pred)(void *, void *), void (*killEdge)(void *))                   \
	    __attribute__((always_inline));                                          \
	inline void graphEdge##suffix##Kill(                                         \
	    graphNode##suffix node, graphNode##suffix node2, void *data,             \
	    int (*pred)(void *, void *), void (*killEdge)(void *)) {                 \
		return __graphEdgeKill((graphNode##suffix)node, (graphNode##suffix)node2,  \
		                       data, pred, killEdge);                              \
	}                                                                            \
	inline void graphNode##suffix##VisitForward(                                 \
	    graphNode##suffix node, void *data,                                      \
	    int (*pred)(const struct __graphNode *, const struct __graphEdge *,      \
	                const void *),                                               \
	    void (*visit)(struct __graphNode *, void *))                             \
	    __attribute__((always_inline));                                          \
	inline void graphNode##suffix##VisitForward(                                 \
	    graphNode##suffix node, void *data,                                      \
	    int (*pred)(const struct __graphNode *, const struct __graphEdge *,      \
	                const void *),                                               \
	    void (*visit)(struct __graphNode *, void *)) {                           \
		__graphNodeVisitForward((struct __graphNode *)node, data, pred, visit);    \
	}                                                                            \
	inline void graphNode##suffix##VisitBackward(                                \
	    graphNode##suffix node, void *data,                                      \
	    int(pred)(const struct __graphNode *, const struct __graphEdge *,        \
	              const void *),                                                 \
	    void (*visit)(struct __graphNode *, void *))                             \
	    __attribute__((always_inline));                                          \
	inline void graphNode##suffix##VisitBackward(                                \
	    graphNode##suffix node, void *data,                                      \
	    int(pred)(const struct __graphNode *, const struct __graphEdge *,        \
	              const void *),                                                 \
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
	inline graphEdge##suffix graphNode##suffix##Connect(                         \
	    graphNode##suffix node, graphNode##suffix node2, edgeType value)         \
	    __attribute__((always_inline));                                          \
	inline graphEdge##suffix graphNode##suffix##Connect(                         \
	    graphNode##suffix node, graphNode##suffix node2, edgeType value) {       \
		return __graphNodeConnect(node, node2, &value, sizeof(edgeType));          \
	}                                                                            \
	inline str##GraphEdge##suffix##P graphNode##suffix##Incoming(                \
	    graphNode##suffix node) __attribute__((always_inline));                  \
	inline str##GraphEdge##suffix##P graphNode##suffix##Incoming(                \
	    graphNode##suffix node) {                                                \
		return (str##GraphEdge##suffix##P)__graphNodeIncoming(node);               \
	}                                                                            \
	inline str##GraphEdge##suffix##P graphNode##suffix##Outgoing(                \
	    graphNode##suffix node) __attribute__((always_inline));                  \
	inline str##GraphEdge##suffix##P graphNode##suffix##Outgoing(                \
	    graphNode##suffix node) {                                                \
		return (str##GraphEdge##suffix##P)__graphNodeOutgoing(node);               \
	}                                                                            \
	inline edgeType *graphEdge##suffix##ValuePtr(const graphEdge##suffix edge)   \
	    __attribute__((always_inline));                                          \
	inline edgeType *graphEdge##suffix##ValuePtr(const graphEdge##suffix edge) { \
		return __graphEdgeValuePtr(edge);                                          \
	}                                                                            \
	inline graphNode##suffix graphEdge##suffix##Outgoing(                        \
	    const graphEdge##suffix edge) __attribute__((always_inline));            \
	inline graphNode##suffix graphEdge##suffix##Outgoing(                        \
	    const graphEdge##suffix edge) {                                          \
		return __graphEdgeOutgoing(edge);                                          \
	}                                                                            \
	inline graphNode##suffix graphEdge##suffix##Incoming(                        \
	    const graphEdge##suffix edge) __attribute__((always_inline));            \
	inline graphNode##suffix graphEdge##suffix##Incoming(                        \
	    const graphEdge##suffix edge) {                                          \
		return __graphEdgeIncoming(edge);                                          \
	}                                                                            \
	inline type *graphNode##suffix##ValuePtr(const graphNode##suffix edge)       \
	    __attribute__((always_inline));                                          \
	inline type *graphNode##suffix##ValuePtr(const graphNode##suffix edge) {     \
		return __graphNodeValuePtr(edge);                                          \
	}                                                                            \
	inline int graphNode##suffix##ConnectedTo(const graphNode##suffix from,      \
	                                          const graphNode##suffix to)        \
	    __attribute__((always_inline));                                          \
	inline int graphNode##suffix##ConnectedTo(const graphNode##suffix from,      \
	                                          const graphNode##suffix to) {      \
		return __graphIsConnectedTo(from, to);                                     \
	}                                                                            \
	inline strGraphNode##suffix##P graphNode##suffix##OutgoingNodes(             \
	    const graphNode##suffix node) __attribute__((always_inline));            \
	inline strGraphNode##suffix##P graphNode##suffix##OutgoingNodes(             \
	    const graphNode##suffix node) {                                          \
		return __graphNodeOutgoingNodes(node);                                     \
	}                                                                            \
	inline strGraphNode##suffix##P graphNode##suffix##IncomingNodes(             \
	    const graphNode##suffix node) __attribute__((always_inline));            \
	inline strGraphNode##suffix##P graphNode##suffix##IncomingNodes(             \
	    const graphNode##suffix node) {                                          \
		return __graphNodeIncomingNodes(node);                                     \
	}                                                                            \
	inline strGraphNode##suffix##P graphNode##suffix##AllNodes(                  \
	    const graphNode##suffix node) __attribute__((always_inline));            \
	inline strGraphNode##suffix##P graphNode##suffix##AllNodes(                  \
	    const graphNode##suffix node) {                                          \
		return __graphNodeVisitAll((struct __graphNode *)node);                    \
	}                                                                            \
	inline void graph##suffix##ReplaceNodes(                                     \
	    strGraphNode##suffix##P toReplace, graphNode##suffix node,               \
	    int (*edgeCmp)(const struct __graphEdge *, const struct __graphEdge *),  \
	    void (*killNodeData)(void *)) __attribute__((always_inline));            \
	inline void graph##suffix##ReplaceNodes(                                     \
	    strGraphNode##suffix##P toReplace, graphNode##suffix node,               \
	    int (*edgeCmp)(const struct __graphEdge *, const struct __graphEdge *),  \
	    void (*killNodeData)(void *)) {                                          \
		graphReplaceWithNode(toReplace, node, edgeCmp, killNodeData,               \
		                     sizeof(edgeType));                                    \
	}
void __graphKillAll(struct __graphNode *start, void (*killFunc)(void *),
                    void (*killEdge)(void *));
void __graphNodeKill(struct __graphNode *node, void (*killNode)(void *item),
                     void (*killEdge)(void *item));
void __graphNodeVisitForward(struct __graphNode *node, void *data,
                             int(pred)(const struct __graphNode *,
                                       const struct __graphEdge *,
                                       const void *),
                             void (*visit)(struct __graphNode *, void *));
void __graphNodeVisitBackward(struct __graphNode *node, void *data,
                              int(pred)(const struct __graphNode *,
                                        const struct __graphEdge *,
                                        const void *),
                              void (*visit)(struct __graphNode *, void *));
struct __graphNode *__graphNodeCreate(void *value, long itemSize, int version);
void __graphNodeReadLock(struct __graphNode *node);
void __graphNodeReadUnlock(struct __graphNode *node);
void __graphNodeWriteLock(struct __graphNode *node);
void __graphNodeWriteUnlock(struct __graphNode *node);
struct __graphEdge *__graphNodeConnect(struct __graphNode *a,
                                       struct __graphNode *b, void *data,
                                       long itemSize);
STR_TYPE_DEF(struct __graphNode *, GraphNodeP);
STR_TYPE_FUNCS(struct __graphNode *, GraphNodeP);
STR_TYPE_DEF(struct __graphEdge *, GraphEdgeP);
STR_TYPE_FUNCS(struct __graphEdge *, GraphEdgeP);
strGraphEdgeP __graphNodeIncoming(const struct __graphNode *node);
strGraphEdgeP __graphNodeOutgoing(const struct __graphNode *node);
void __graphEdgeKill(struct __graphNode *in, struct __graphNode *out,
                     void *data, int (*pred)(void *, void *),
                     void (*kill)(void *));
void *__graphEdgeValuePtr(const struct __graphEdge *edge);
struct __graphNode *__graphEdgeIncoming(const struct __graphEdge *edge);
struct __graphNode *__graphEdgeOutgoing(const struct __graphEdge *edge);
void *__graphNodeValuePtr(const struct __graphNode *node);
int __graphIsConnectedTo(const struct __graphNode *from,
                         const struct __graphNode *to);
strGraphNodeP __graphNodeIncomingNodes(const struct __graphNode *node);
strGraphNodeP __graphNodeOutgoingNodes(const struct __graphNode *node);
strGraphNodeP __graphNodeVisitAll(const struct __graphNode *start);
STR_TYPE_DEF(strGraphEdgeP, GraphPath);
STR_TYPE_FUNCS(strGraphEdgeP, GraphPath);
strGraphPath graphAllPathsTo(struct __graphNode *from, struct __graphNode *to);
void graphPrint(struct __graphNode *node, char *(*toStr)(struct __graphNode *),
                char *(*toStrEdge)(struct __graphEdge *));
void graphReplaceWithNode(strGraphNodeP toReplace,
                          struct __graphNode *replaceWith,
                          int (*edgeCmp)(const struct __graphEdge *,
                                         const struct __graphEdge *),
                          void (*killNodeData)(void *), long edgeSize);

GRAPH_TYPE_DEF(struct __graphNode *, struct __graphEdge *, Mapping);
GRAPH_TYPE_FUNCS(struct __graphNode *, struct __graphEdge *, Mapping);
graphNodeMapping
createFilteredGraph(struct __graphNode *start, strGraphNodeP nodes, void *data,
                    int (*pred)(struct __graphNode *, void *data));
MAP_TYPE_DEF(char *, GraphVizAttr);
MAP_TYPE_FUNCS(char *, GraphVizAttr);
void graph2GraphViz(FILE *dumpTo, graphNodeMapping graph, const char *title,
                    char *(*nodeToLabel)(const struct __graphNode *node,
                                         mapGraphVizAttr *attrs,
                                         const void *data),
                    char *(*edgeToLabel)(const struct __graphEdge *node,
                                         mapGraphVizAttr *attrs,
                                         const void *data),
                    const void *nodeData, const void *edgeData);
long graphNodeValueSize(const struct __graphNode *node);
long graphEdgeValueSize(const struct __graphEdge *edge);
graphNodeMapping graphNodeCreateMapping(const struct __graphNode *node,
                                        int preserveConnections);
strGraphPath graphAllPathsToPredicate(
    struct __graphNode *from, const void *data,
    int (*predicate)(const struct __graphNode *node, const void *data));
void graph2GraphVizUndir(FILE *dumpTo, graphNodeMapping graph,
                         const char *title,
                         char *(*nodeToLabel)(const struct __graphNode *node,
                                              mapGraphVizAttr *attrs,
                                              const void *data),
                         const void *nodeData);
strGraphEdgeP graphAllEdgesBetween(const struct __graphNode *node,const void *data,
                              int (*predicate)(const struct __graphNode *node,
                                               const void *data));
