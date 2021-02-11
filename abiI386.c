#include <IR.h>
#include <IRLiveness.h>
#include <abi.h>
#include <assert.h>
#include <cleanup.h>
#include <ptrMap.h>
#include <registers.h>

/*****
for (long i = 0; i != strGraphNodeIRPSize(allNodes); i++) {
    struct IRNodeFuncCall *call = (void *)graphNodeIRValuePtr(allNodes[i]);
    if (call->base.type != IR_FUNC_CALL)
      continue;
    strGraphEdgeIRP incoming CLEANUP(strGraphEdgeIRPDestroy) = graphNodeIRIncoming(allNodes[i]);
    strGraphEdgeIRP func CLEANUP(strGraphEdgeIRPDestroy) = IRGetConnsOfType(incoming, IR_CONN_FUNC);
    // ABI fun time
    // http://www.sco.com/developers/devspecs/abi386-4.pdf
    struct reg *toPreserve[] = {&regX86EBX, &regX86ESI, &regX86EDI, &regX86EBP, &regX86ESP};
    qsort(toPreserve, sizeof(toPreserve) / sizeof(*toPreserve), sizeof(toPreserve), ptrPtrCmp);
    //
    // ecx,edx are destroyed,so backup
    //
    struct reg *destroyed[] = {&regX86ECX, &regX86EDX};
    qsort(destroyed, sizeof(destroyed) / sizeof(*destroyed), sizeof(destroyed), ptrPtrCmp);
    // x87 "barrel stack" needs to be emptied,
    struct reg *toEmpty[] = {&regX86ST0, &regX86ST1, &regX86ST2, &regX86ST3, &regX86ST4, &regX86ST5, &regX86ST6, &regX86ST7};
    qsort(toEmpty, sizeof(toEmpty) / sizeof(*toEmpty), sizeof(toEmpty), ptrPtrCmp);

    // Handle spills if we are in a block that consumes  registers
    __auto_type attr = llIRAttrFind(call->base.attrs, IR_ATTR_BASIC_BLOCK, IRAttrGetPred);
    if (attr) {
      struct IRAttrBasicBlock *block = (void *)llIRAttrValuePtr(attr);
      for (long i = 0; i != strVarSize(block->block->in); i++) {
        __auto_type reg = ptrMapVar2RegGet(var2Reg, block->block->in[i].value.var);
        if (!reg)
          continue;
        struct reg *find = bsearch(reg, destroyed, ARRAY_SIZE(destroyed), sizeof(*destroyed), ptrPtrCmp);
        if (find) {
        }
        find = bsearch(reg, destroyed, ARRAY_SIZE(destroyed), sizeof(*destroyed), ptrPtrCmp);
      }
    }
  }
  *****/
