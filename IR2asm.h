#pragma once
#include <IR.h>
#include <stdio.h>
void IR2Asm(graphNodeIR start);
void IRCompile(graphNodeIR start,int isFunc);
void IR2AsmInit();
struct X86AddressingMode *IRNode2AddrMode(graphNodeIR start);
enum asmAssignFlags {
		ASM_ASSIGN_X87FPU_POP=1,
};
void asmAssign(struct X86AddressingMode *a, struct X86AddressingMode *b, long size,enum asmAssignFlags flags);
void asmTypecastAssign(struct X86AddressingMode *outMode, struct X86AddressingMode *inMode,enum asmAssignFlags flags);
