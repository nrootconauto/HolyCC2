#include <registers.h>
#include <parserA.h>
#include <opcodesParser.h>
#include <stdio.h>
#include <cleanup.h>
#include <assert.h>
void X86EmitAsmInst(struct opcodeTemplate *template,strX86AddrMode args,FILE *dumpTo) {
		//TODO map TempleOS opcode name to gas opcode name
		fprintf(dumpTo, "%s ", opcodeTemplateName(template));
		for(long i=0;i!=strX86AddrModeSize(args);i++) {
				switch(args[i].type) {
				case X86ADDRMODE_FLT: {
						fprintf(dumpTo, "%ld", args->)	
				}
				case X86ADDRMODE_ITEM_ADDR: {
				}
				case X86ADDRMODE_LABEL: {
				}
				case X86ADDRMODE_REG: {
						
				}
				case X86ADDRMODE_SINT: {
				}
				case X86ADDRMODE_UINT: {
				}
				case X86ADDRMODE_MEM: {
				}
				}
		}
}
void X86EmitAsmParserInst(struct parserNodeAsmInstX86 *inst,FILE *dumpTo) {
		struct parserNodeName *name=(void*)inst->name;
		strX86AddrMode args CLEANUP(strX86AddrModeDestroy);
		for(long i=0;i!=strParserNodeSize(inst->args);i++)
				args=strX86AddrModeAppendItem(args,parserNode2X86AddrMode(inst->args[i]));
		strOpcodeTemplate templates CLEANUP(strOpcodeTemplateDestroy)=X86OpcodesByArgs(name->text, args, NULL);
		assert(strOpcodeTemplateSize(templates)!=0);
		
}
