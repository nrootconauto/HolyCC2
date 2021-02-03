#pragma once
#include <IR.h>
#include <stdio.h>
void IR2Asm(graphNodeIR start);
void IRCompile(graphNodeIR start);
void IR2AsmInit();
struct X86AddressingMode *IRNode2AddrMode(graphNodeIR start);
void asmAssign(struct X86AddressingMode *a, struct X86AddressingMode *b, long size);
void asmTypecastAssign(struct X86AddressingMode *outMode, struct X86AddressingMode *inMode);
