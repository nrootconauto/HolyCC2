#pragma once
#include <opcodesParser.h>
#include <parserA.h>
#include <stdio.h>
void X86EmitAsmInst(struct opcodeTemplate *template,strX86AddrMode args,int *err);
void X86EmitAsmParserInst(struct parserNodeAsmInstX86 *inst);
void X86EmitAsmInit();
char *X86EmitAsmLabel(const char *name);
struct X86AddressingMode *X86EmitAsmDU8(strX86AddrMode data,long len);
struct X86AddressingMode *X86EmitAsmDU16(strX86AddrMode data,long len);
struct X86AddressingMode *X86EmitAsmDU32(strX86AddrMode data,long len);
struct X86AddressingMode *X86EmitAsmDU64(strX86AddrMode data,long len);
struct X86AddressingMode *X86EmitAsmStrLit(const char *text);
