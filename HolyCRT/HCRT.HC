#include "IntPow.HC"
extern U0 HCRT_ExceptRestoreState();
extern U0 HCRT_ExceptStoreState();
U0 throw(I32i msg);
_import pow F64 PowF64(F64 b,F64 e);
I32i PowI32i(I32i base,I32i exp);