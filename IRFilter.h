#include <IR.h>
graphNodeIR filterForVar(graphNodeIR start,int(*pred)(graphNodeIR,const void *),const void* data);
