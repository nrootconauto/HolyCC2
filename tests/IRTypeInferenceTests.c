#include <IRTypeInference.h>
#include <assert.h>
void IRTypeInferenceTests() {
		initIR();
		__auto_type var32=IRCreateVarRef(IRCreateVirtVar(&typeI32i));
		__auto_type var64=IRCreateVarRef(IRCreateVirtVar(&typeI64i));
		__auto_type one=IRCreateIntLit(1);
		__auto_type two=IRCreateIntLit(2);
		__auto_type three=IRCreateIntLit(3);
		__auto_type four=IRCreateIntLit(4);
		__auto_type binop1=IRCreateBinop(one ,two,IR_ADD);
		__auto_type binop2=IRCreateBinop(three,var32,IR_ADD);
		__auto_type binop3=IRCreateBinop(binop1, binop2, IR_ADD);
		__auto_type binop4=IRCreateBinop(var64, four, IR_ADD);
		__auto_type binop5=IRCreateBinop(binop3, binop4, IR_ADD);
		IRNodeType(binop5);
		assert(IRNodeType(binop5)==&typeI64i);
		assert(IRNodeType(binop4)==&typeI64i);
		assert(IRNodeType(four)==&typeI64i);
		assert(IRNodeType(three)==&typeI32i);
		assert(IRNodeType(two)==&typeI32i);
		assert(IRNodeType(one)==&typeI32i);
}
