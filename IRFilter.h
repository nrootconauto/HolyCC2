#include <IR.h>
graphNodeIR IRFilter(graphNodeIR start, int (*pred)(graphNodeIR, const void *),
                     const void *data);
