#include <assert.h>
#include <subGraph.h>
typedef int (*geCmpType)(const struct __graphEdge **,
                         const struct __graphEdge **b);
// https://github.com/sdiemert/subgraph-isomorphism/blob/master/index.js
#define INT_BITS (sizeof(int) * 8)
STR_TYPE_DEF(unsigned int, Bits);
STR_TYPE_FUNCS(unsigned int, Bits);
STR_TYPE_DEF(strBits, Bits2D);
STR_TYPE_FUNCS(strBits, Bits2D);
STR_TYPE_DEF(int, Int);
STR_TYPE_FUNCS(int, Int);
int bitSearch(strBits bits, int startAt) {
	int firstRun = 1;
	for (int i = startAt / INT_BITS; i != strBitsSize(bits); i++) {
		__auto_type copy = bits[i];
		if (copy == 0)
			continue;

		if (firstRun) {
			copy >>= (startAt % INT_BITS);
			copy <<= (startAt % INT_BITS);
		}

		__auto_type find = __builtin_ffs(copy);
		if (find != 0)
			return i * INT_BITS + find - 1;

		firstRun = 0;
	}
	return -1;
}
struct __mat {
	strBits2D data;
	int h;
	int w;
};
STR_TYPE_DEF(struct __mat, Mat);
STR_TYPE_FUNCS(struct __mat, Mat);
struct __mat __adjMatrixCreate(strGraphNodeP nodes) {
	__auto_type size = strGraphNodePSize(nodes);
	strBits2D data = strBits2DResize(NULL, size);
	for (int i = 0; i != size; i++) {
		__auto_type ints = size / INT_BITS + (size % INT_BITS != 0 ? 1 : 0);
		data[i] = strBitsResize(NULL, ints);
		memset(data[i], 0, ints * sizeof(int));
	}
	for (int i1 = 0; i1 != size; i1++) {
		for (int i2 = 0; i2 != size; i2++) {
			if (__graphIsConnectedTo(nodes[i1], nodes[i2])) {
				data[i1][i2 / INT_BITS] |= 1u << (i2 % INT_BITS);
			}
		}
	}
	return (struct __mat){data, size, size};
}
static int degree(strBits row) {
	int retVal = 0;
	__auto_type size = strBitsSize(row);
	for (int i = 0; i != size; i++) {
		retVal += __builtin_popcount(row[i]);
	}
	return retVal;
}
static struct __mat initMorphism(struct __mat *graph, struct __mat *sub,
                                 strGraphNodeP graphNodes,
                                 strGraphNodeP subNodes,
                                 int (*nodePred)(const struct __graphNode *,
                                                 const struct __graphNode *)) {
	strBits2D data = strBits2DResize(NULL, sub->h);
	for (int i = 0; i != sub->h; i++) {
		__auto_type ints = graph->h / INT_BITS + (graph->h % INT_BITS != 0 ? 1 : 0);
		data[i] = strBitsResize(NULL, ints);
		memset(data[i], 0, ints * sizeof(int));
	}
	for (int i = 0; i != sub->h; i++) {
		for (int j = 0; j != graph->h; j++) {
			if (nodePred != NULL)
				if (!nodePred(subNodes[i], graphNodes[j]))
					continue;

			__auto_type subDegreee = degree(sub->data[i]);
			__auto_type graphDegree = degree(graph->data[j]);

			if (subDegreee <= graphDegree) {
				data[i][j / INT_BITS] |= 1u << (j % INT_BITS);
			}
		}
	}
	return (struct __mat){data, sub->h, graph->h};
}
static int morph(struct __mat *m, int p) {
	__auto_type res = bitSearch(m->data[p], 0);
	if (res != -1) {
		return res;
	}
	assert(0);
}
static void __matDestroy(struct __mat *mat) {
	for (int i = 0; i != strBits2DSize(mat->data); i++)
		strBitsDestroy(&mat->data[i]);
	strBits2DDestroy(&mat->data);
}
static int isIso(struct __mat *m, struct __mat *graph, struct __mat *sub) {
	__auto_type rows = sub->h;
	for (int r1 = 0; r1 != rows; r1++) {
		for (int r2 = bitSearch(sub->data[r1], 0); r2 != -1;
		     r2 = bitSearch(sub->data[r1], r2 + 1)) {
			int c1 = morph(m, r1);
			int c2 = morph(m, r2);
			if (!(graph->data[c1][c2 / INT_BITS] & (1u << (c2 % INT_BITS)))) {
				return 0;
			}
		}
	}
	return 1;
}
static struct __mat matClone(struct __mat *toClone) {
	struct __mat retVal = {NULL, toClone->h, toClone->w};
	retVal.data = strBits2DResize(NULL, toClone->h);
	for (int i1 = 0; i1 != toClone->h; i1++) {
		__auto_type width = toClone->w / INT_BITS + (toClone->w % INT_BITS ? 1 : 0);
		retVal.data[i1] = strBitsResize(NULL, width);
		memcpy(retVal.data[i1], toClone->data[i1], width * sizeof(int));
	}
	return retVal;
}
static void prune(struct __mat *mat, struct __mat *sub, struct __mat *graph) {
	assert(mat->w == graph->w);
	for (int i = 0; i != mat->h; i++) {
		for (int j = bitSearch(mat->data[i], 0); j != -1;
		     j = bitSearch(mat->data[i], j + 1)) {
			for (int x = bitSearch(sub->data[i], 0); x != -1;
			     x = bitSearch(sub->data[i], x + 1)) {

				int hasYNeighbor = 0;
				__auto_type n = bitSearch(graph->data[j], 0);
				if (n != -1) {
					hasYNeighbor = 1;
				}

				if (!hasYNeighbor) {
					mat->data[i][j / INT_BITS] &= ~(1u << (j % INT_BITS));
				}
			}
		}
	}
}
static void recurse(strInt usedCols, int curRow, struct __mat *graph,
                    struct __mat *sub, struct __mat *m, strMat *result) {
	if (m->h == curRow) {
		if (isIso(m, graph, sub)) {
			*result = strMatAppendItem(*result, matClone(m));
		}
	} else {
		__auto_type mp = matClone(m);

		prune(&mp, sub, graph);

		for (int c = bitSearch(m->data[curRow], 0); c != -1;
		     c = bitSearch(m->data[curRow], c + 1)) {
			if (usedCols[c] == 0) {
				for (int i = 0; i != m->w; i++) {
					if (i == c) {
						mp.data[curRow][i / INT_BITS] |= 1u << (i % INT_BITS);
					} else {
						mp.data[curRow][i / INT_BITS] &= ~(1u << (i % INT_BITS));
					}
				}

				usedCols[c] = 1;
				recurse(usedCols, curRow + 1, graph, sub, &mp, result);
				usedCols[c] = 0;
			}
		}

		__matDestroy(&mp);
	}
}
int edgeComp(const void *a, const void *b) { return *(void **)a - *(void **)b; }
static strGraphEdgeP edgesConnectedToNode(struct __graphNode *from,
                                          struct __graphNode *to) {
	__auto_type outgoing = graphNodeSubOutgoing(from);
	__auto_type size = strGraphEdgePSize(outgoing);
	strGraphEdgeP toRemove = strGraphEdgePReserve(NULL, size);

	for (int i = 0; i != size; i++)
		if (to != __graphEdgeOutgoing(outgoing[i]))
			toRemove =
			    strGraphEdgePSortedInsert(toRemove, outgoing[i], (geCmpType)edgeComp);

	outgoing =
	    strGraphEdgePSetDifference(outgoing, toRemove, (geCmpType)edgeComp);
	strGraphEdgePDestroy(&toRemove);
	return outgoing;
}
static strGraphNodeSubP reconstructFromAdj(
    struct __mat *mat, strGraphNodeP graphNodes, strGraphNodeP subGraph,
    int (*edgePred)(const struct __graphEdge *, const struct __graphEdge *)) {
	__auto_type subGraphSize = strGraphNodePSize(subGraph);
	strGraphNodeSubP nodes = strGraphNodeSubPResize(NULL, subGraphSize);
	assert(subGraphSize == mat->h);
	for (int i = 0; i != subGraphSize; i++) {
		__auto_type g = morph(mat, i);
		nodes[i] = graphNodeSubCreate(graphNodes[g], 0);
	}

	for (int i1 = 0; i1 != subGraphSize; i1++) {
		for (int i2 = 0; i2 != subGraphSize; i2++) {
			if (graphNodeSubConnectedTo(subGraph[i1], subGraph[i2])) {
				__auto_type graphNodeFrom = *graphNodeSubValuePtr(nodes[i1]);
				__auto_type graphNodeTo = *graphNodeSubValuePtr(nodes[i2]);

				strGraphEdgeP edgesGraph __attribute__((cleanup(strGraphEdgePDestroy)));
				edgesGraph = edgesConnectedToNode(graphNodeFrom, graphNodeTo);
				if (edgePred != NULL) {
					strGraphEdgeP edgesSub __attribute__((cleanup(strGraphEdgePDestroy)));
					edgesSub = edgesConnectedToNode(subGraph[i1], subGraph[i2]);
					int connectionCount = 0;

					for (int i1 = 0; i1 != strGraphEdgePSize(edgesGraph); i1++) {
						int edgeFits = 1;
						for (int i2 = 0; i2 != strGraphEdgePSize(edgesSub); i2++) {
							if (!edgePred(edgesGraph[i1], edgesSub[i1])) {
								edgeFits = 0;
								break;
							}
						}

						if (edgeFits) {
							connectionCount++;
							graphNodeSubConnect(nodes[i1], nodes[i2], edgesGraph[i1]);
						}
					}

					if (strGraphEdgePSize(edgesSub) > connectionCount)
						goto fail;
				} else {
					for (int i = 0; i != strGraphEdgePSize(edgesGraph); i++)
						graphNodeSubConnect(nodes[i1], nodes[i2], edgesGraph[i]);
				}
			}
		}
	}

success:
	return nodes;
fail:;
	__auto_type size = strGraphNodeSubPSize(nodes);
	for (int i = 0; i != size; i++)
		graphNodeSubKill(&nodes[i], NULL, NULL);

	return NULL;
}
strSub isolateSubGraph(strGraphNodeP graph, strGraphNodeP sub,
                       int (*nodePred)(const struct __graphNode *,
                                       const struct __graphNode *),
                       int (*edgePred)(const struct __graphEdge *,
                                       const struct __graphEdge *)) {
	struct __mat graphAdjMat;
	struct __mat subAdjMat;
	graphAdjMat = __adjMatrixCreate(graph);
	subAdjMat = __adjMatrixCreate(sub);

	__auto_type graphSize = strGraphNodePSize(graph);
	__auto_type subSize = strGraphNodePSize(sub);

	strInt unused __attribute__((cleanup(strIntDestroy)));
	unused = strIntResize(NULL, graphSize);
	memset(unused, 0, sizeof(int) * graphSize);

	__auto_type m = initMorphism(&graphAdjMat, &subAdjMat, graph, sub, nodePred);

	strMat results = NULL;
	recurse(unused, 0, &graphAdjMat, &subAdjMat, &m, &results);

	strSub retVal = NULL;
	for (int i = 0; i != strMatSize(results); i++) {
		__auto_type result = reconstructFromAdj(&results[i], graph, sub, edgePred);
		if (result == NULL)
			continue;

		retVal = strSubAppendItem(retVal, result);
	}
	for (int i = 0; i != strMatSize(results); i++)
		__matDestroy(&results[i]);
	__matDestroy(&graphAdjMat);
	__matDestroy(&subAdjMat);
	__matDestroy(&m);

	return retVal;
}
void killSubgraphs(strSub *subgraphs) {
	for (int i1 = 0; i1 != strSubSize(*subgraphs); i1++)
		for (int i2 = 0; i2 != strGraphNodeSubPSize(subgraphs[0][i1]); i2++)
			graphNodeSubKill(&subgraphs[0][i1][i2], NULL, NULL);
}
