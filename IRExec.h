#include <IR.h>
#include <stdint.h>
enum IREvalValType {
		IREVAL_VAL_INT,
		IREVAL_VAL_PTR,
		IREVAL_VAL_DFT,
		IREVAL_VAL_FLT
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
		}value;
};
struct IREvalVal evalIRNode(graphNodeIR node,int *success);
