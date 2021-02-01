#pragma once
#include <opcodesParser.h>
#include <parserA.h>
#include <stdio.h>
#include <frameLayout.h>
void X86EmitAsmInst(struct opcodeTemplate *template, strX86AddrMode args, int *err);
void X86EmitAsmParserInst(struct parserNodeAsmInstX86 *inst);
void X86EmitAsmInit();
void X86EmitAsm2File(const char *name);
char *X86EmitAsmLabel(const char *name);
struct X86AddressingMode *X86EmitAsmDU8(strX86AddrMode data, long len);
struct X86AddressingMode *X86EmitAsmDU16(strX86AddrMode data, long len);
struct X86AddressingMode *X86EmitAsmDU32(strX86AddrMode data, long len);
struct X86AddressingMode *X86EmitAsmDU64(strX86AddrMode data, long len);
struct X86AddressingMode *X86EmitAsmStrLit(const char *text);
void X86EmitAsmIncludeBinfile(const char *fileName);
void X86EmitAsmGlobalVar(struct parserVar *var);
