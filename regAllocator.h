#pragma once
#include <IR.h>
#include <IRLiveness.h>
void IRCoalesce(strGraphNodeIRP nodes, graphNodeIR start);
void IRRemoveRepeatAssigns(graphNodeIR enter);
typedef struct regSlice (*color2RegPredicate)(strRegSlice adjacent,strRegP avail,graphNodeIRLive live,int color,const void *data,long colorCount,const int *colors);
void IRRegisterAllocate(graphNodeIR start,color2RegPredicate colorFunc,void *colorData);
