#include <IR.h>
#include <stdint.h>
enum IREvalValType {
	IREVAL_VAL_INT,
	IREVAL_VAL_PTR,
	IREVAL_VAL_DFT,
	IREVAL_VAL_FLT,
	IREVAL_VAL_VAR,
	IREVAL_VAL_REG,
};
struct IREvalVal {
	enum IREvalValType type;
	union {
		struct {
			long value;
			enum IREvalValType type;
		} ptr;
		double flt;
		int64_t i;
			struct reg *reg;
	} value;
};
struct IREvalVal IREvalNode(graphNodeIR node, int *success);
void IREvalSetVarVal(const struct variable *var, struct IREvalVal value);
struct IREvalVal IREvalValFltCreate(double f);
struct IREvalVal IREValValIntCreate(long i);
void IREvalInit();
struct IREvalVal IREvalPath(graphNodeIR start,int *success);
