#pragma once
#include <hashTable.h>
#include <parserA.h>
#include <str.h>
#include <linkedList.h>
#include <graph.h>
#include <lexer.h>
enum IRFlag {
		IR_FLAG_EQZ,
		IR_FLAG_NEQZ,
		IR_FLAG_GE,
		IR_FLAG_LE,
		IR_FLAG_GT,
		IR_FLAG_LT,
		IR_FLAG_EQ,
		IR_FLAG_NEQ,
		IR_FLAG_B,
		IR_FLAG_AE,
		IR_FLAG_BE,
		IR_FLAG_ABOVE,
		IR_FLAG_BELOW,
};
enum IRConnType {
		IR_CONN_SOURCE_A,
		IR_CONN_SOURCE_B,
		IR_CONN_DEST,
		IR_CONN_FLOW,
		IR_CONN_COND_TRUE,
		IR_CONN_COND_FALSE,
		IR_CONN_COND,
		IR_CONN_FUNC_ARG,
};
enum IRNodeType {
		IR_TYPECAST,
		//
		IR_STATEMENT_START,
		IR_STATEMENT_END,
		//
		IR_INC,
		IR_DEC,
		//
		IR_ADD,
		IR_SUB,
		//
		IR_POS,
		IR_NEG,
		//
		IR_MULT,
		IR_MOD,
		IR_DIV,
		IR_POW,
		//
		IR_LAND,
		IR_LXOR,
		IR_LOR,
		IR_LNOT,
		//
		IR_BNOT,
		IR_BAND,
		IR_BXOR,
		IR_BOR,
		IR_LSHIFT,
		IR_RSHIFT,
		//
		IR_ARRAY_ACCESS,
		//
		IR_SIMD,
		//
		IR_GT,
		IR_LT,
		IR_GE,
		IR_LE,
		IR_EQ,
		IR_NE,
		//
		IR_ASSIGN,
		//
		IR_CHOOSE,
		//
		IR_SPILL,
		IR_LOAD,
		//
		IR_JUMP,
		IR_COND_JUMP,
		IR_JUMP_TAB,
		//
		IR_VALUE,
		IR_LABEL,
		//
		IR_FUNC_CALL,
		//
		IR_SUB_SWITCH_START_LABEL,
};
struct IRNode;
MAP_TYPE_DEF(void*, IRNodeAttr);
MAP_TYPE_FUNCS(void*, IRNodeAttr);
struct IRNode {
		enum IRNodeType type;
		mapIRNodeAttr attrs; //NULL by defualt
};
GRAPH_TYPE_DEF(struct IRNode, enum IRConnType, IR);
GRAPH_TYPE_FUNCS(struct IRNode, enum IRConnType, IR);
enum IRValueType {
		__IR_VAL_MEM_FRAME,
		__IR_VAL_MEM_GLOBAL,
		__IR_VAL_LABEL,
		IR_VAL_MEM,
		IR_VAL_REG,
		IR_VAL_INDIRECT,
		IR_VAL_VAR_REF,
		IR_VAL_OPRESULT,
		IR_VAL_FUNC,
		IR_VAL_INT_LIT,
		IR_VAL_STR_LIT,
		IR_VAL_MEM_LABEL,
};
struct IRValue;
struct IRValOpResult {
		graphNodeIR  node;
};
struct IRValIndirect {
		struct IRValue *base;
		struct IRValue *index;
		long scale;
};
struct IRValReg {
		int num;
		int width;
		int size;
};
struct IRValMemFrame {
		long offset;
		int width;
};
struct IRValMemGlobal {
		struct variable *symbol;
};
enum IRVarType {
		IR_VAR_VAR,
		IR_VAR_MEMBER,
};
struct IRVar {
		enum IRVarType type;
		union {
				struct variable *var;
				struct parserNodeMemberAccess *member;
		} value;
};
struct IRVarRef {
		struct IRVar var;
		long SSANum;
};
struct IRValue {
		enum IRValueType type;
		union {
				struct IRValReg reg;
				struct IRVarRef var;
				struct IRValMemFrame __frame;
				struct IRValMemGlobal __global;
				graphNodeIR __label;
				struct IRValIndirect indir;
				struct IRValOpResult opRes;
				struct function *func;
				struct lexerInt intLit;
				const char *strLit;
				struct {
						struct object *type;
				} __virtVar;
				graphNodeIR memLabel;
		} value;
};
struct IRNodeAssign {
		struct IRNode base;
};
struct IRNodeBinop {
		struct IRNode base;
};
struct IRNodeUnop {
		struct IRNode base;
};
//Out-going connection is to where
struct IRNodeJump {
		struct IRNode base;
		int forward;		
};
//Out-going connection is to where
struct IRNodeCondJump {
		struct IRNode base;
		enum IRFlag cond;
};
struct IRNodeSpill {
		struct IRNode base;
		struct IRNode *item;
		struct IRValue where;
};
struct IRNodeLoad {
		struct IRNode base;
		struct IRValue from;
		struct IRValue to;
};
struct IRNodeSIMD {
		struct IRNode base;
		enum IRNodeType opType;
};
struct IRNodeValue {
		struct IRNode base;
		struct IRValue val; 
};
struct IRNodeInc {
		struct IRNode base;
		long isSuffix;
};
struct IRNodeDec {
		struct IRNode base;
		long isSuffix;
};
//"a"
//"b"
struct IRNodeArrayAccess {
		struct IRNode base;
		long scale;
};
struct IRNodeLabel {
		struct IRNode base;
};
struct IRNodeFuncCall {
		struct IRNode base;
		strGraphNodeIRP incomingArgs;
};
struct IRNodeJumpTable {
		struct IRNode base;
		long startIndex;
		strGraphNodeIRP labels;
};
struct IRNodeSubSwit {
		struct IRNode base;
};
struct IRNodeTypeCast {
		struct IRNode base;
		struct object *in;
		struct object *out;
};
struct IRNodeStatementStart {
		struct IRNode base;
};
struct IRNodeStatementEnd {
		struct IRNode base;
};
char *IR2Str();
graphNodeIR parserNode2IR(const struct parserNode *node) ;
