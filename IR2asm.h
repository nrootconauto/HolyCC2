#pragma once
#include "IR.h"
#include <stdio.h>
void IR2Asm(graphNodeIR start);
void IRCompile(graphNodeIR start,int isFunc);
void IR2AsmInit();
struct X86AddressingMode *IRNode2AddrMode(graphNodeIR start);
enum asmAssignFlags {
		ASM_ASSIGN_X87FPU_POP=1,
};
void asmAssign(graphNodeIR atNode,struct X86AddressingMode *a, struct X86AddressingMode *b, long size,enum asmAssignFlags flags);
void asmTypecastAssign(graphNodeIR atNode,struct X86AddressingMode *outMode, struct X86AddressingMode *inMode,enum asmAssignFlags flags);
strRegP deadRegsAtPoint(graphNodeIR atNode,struct object *type);
struct reg *regForTypeExcludingConsumed(struct object *type);
void consumeRegister(struct reg *reg) ;
void unconsumeRegister(struct reg *reg);
void pushMode(struct X86AddressingMode *mode);
void asmAssignFromPtr(struct X86AddressingMode *a,struct X86AddressingMode *b,long size,enum asmAssignFlags flags);
void popMode(struct X86AddressingMode *mode);
void popReg(struct reg *r);
void pushReg(struct reg *r);
