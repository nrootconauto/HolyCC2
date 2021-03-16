#pragma once
#include "graph.h"
#include "hashTable.h"
#include "lexer.h"
#include "linkedList.h"
#include "opcodesParser.h"
#include "parserA.h"
#include "ptrMap.h"
#include "registers.h"
#include "str.h"
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
	IR_CONN_ASSIGN_FROM_PTR,
	IR_CONN_FLOW,
	IR_CONN_NEVER_FLOW,
	IR_CONN_COND_TRUE,
	IR_CONN_COND_FALSE,
	IR_CONN_COND,
	IR_CONN_CASE,
	IR_CONN_DFT,
	IR_CONN_SIMD_ARG,
	IR_CONN_FUNC,
	//
	IR_CONN_ARRAY_DIM_1,
	IR_CONN_ARRAY_DIM_2,
	IR_CONN_ARRAY_DIM_3,
	IR_CONN_ARRAY_DIM_4,
	IR_CONN_ARRAY_DIM_5,
	IR_CONN_ARRAY_DIM_6,
	IR_CONN_ARRAY_DIM_7,
	IR_CONN_ARRAY_DIM_8,
	IR_CONN_ARRAY_DIM_9,
	IR_CONN_ARRAY_DIM_10,
	IR_CONN_ARRAY_DIM_11,
	IR_CONN_ARRAY_DIM_12,
	IR_CONN_ARRAY_DIM_13,
	IR_CONN_ARRAY_DIM_14,
	IR_CONN_ARRAY_DIM_15,
	IR_CONN_ARRAY_DIM_16,
	//
	IR_CONN_FUNC_ARG_1,
	IR_CONN_FUNC_ARG_2,
	IR_CONN_FUNC_ARG_3,
	IR_CONN_FUNC_ARG_4,
	IR_CONN_FUNC_ARG_5,
	IR_CONN_FUNC_ARG_6,
	IR_CONN_FUNC_ARG_7,
	IR_CONN_FUNC_ARG_8,
	IR_CONN_FUNC_ARG_9,
	IR_CONN_FUNC_ARG_10,
	IR_CONN_FUNC_ARG_11,
	IR_CONN_FUNC_ARG_12,
	IR_CONN_FUNC_ARG_13,
	IR_CONN_FUNC_ARG_14,
	IR_CONN_FUNC_ARG_15,
	IR_CONN_FUNC_ARG_16,
	IR_CONN_FUNC_ARG_17,
	IR_CONN_FUNC_ARG_18,
	IR_CONN_FUNC_ARG_19,
	IR_CONN_FUNC_ARG_20,
	IR_CONN_FUNC_ARG_21,
	IR_CONN_FUNC_ARG_22,
	IR_CONN_FUNC_ARG_23,
	IR_CONN_FUNC_ARG_24,
	IR_CONN_FUNC_ARG_25,
	IR_CONN_FUNC_ARG_26,
	IR_CONN_FUNC_ARG_27,
	IR_CONN_FUNC_ARG_28,
	IR_CONN_FUNC_ARG_29,
	IR_CONN_FUNC_ARG_30,
	IR_CONN_FUNC_ARG_31,
	IR_CONN_FUNC_ARG_32,
	IR_CONN_FUNC_ARG_33,
	IR_CONN_FUNC_ARG_34,
	IR_CONN_FUNC_ARG_35,
	IR_CONN_FUNC_ARG_36,
	IR_CONN_FUNC_ARG_37,
	IR_CONN_FUNC_ARG_38,
	IR_CONN_FUNC_ARG_39,
	IR_CONN_FUNC_ARG_40,
	IR_CONN_FUNC_ARG_41,
	IR_CONN_FUNC_ARG_42,
	IR_CONN_FUNC_ARG_43,
	IR_CONN_FUNC_ARG_44,
	IR_CONN_FUNC_ARG_45,
	IR_CONN_FUNC_ARG_46,
	IR_CONN_FUNC_ARG_47,
	IR_CONN_FUNC_ARG_48,
	IR_CONN_FUNC_ARG_49,
	IR_CONN_FUNC_ARG_50,
	IR_CONN_FUNC_ARG_51,
	IR_CONN_FUNC_ARG_52,
	IR_CONN_FUNC_ARG_53,
	IR_CONN_FUNC_ARG_54,
	IR_CONN_FUNC_ARG_55,
	IR_CONN_FUNC_ARG_56,
	IR_CONN_FUNC_ARG_57,
	IR_CONN_FUNC_ARG_58,
	IR_CONN_FUNC_ARG_59,
	IR_CONN_FUNC_ARG_60,
	IR_CONN_FUNC_ARG_61,
	IR_CONN_FUNC_ARG_62,
	IR_CONN_FUNC_ARG_63,
	IR_CONN_FUNC_ARG_64,
	IR_CONN_FUNC_ARG_65,
	IR_CONN_FUNC_ARG_66,
	IR_CONN_FUNC_ARG_67,
	IR_CONN_FUNC_ARG_68,
	IR_CONN_FUNC_ARG_69,
	IR_CONN_FUNC_ARG_70,
	IR_CONN_FUNC_ARG_71,
	IR_CONN_FUNC_ARG_72,
	IR_CONN_FUNC_ARG_73,
	IR_CONN_FUNC_ARG_74,
	IR_CONN_FUNC_ARG_75,
	IR_CONN_FUNC_ARG_76,
	IR_CONN_FUNC_ARG_77,
	IR_CONN_FUNC_ARG_78,
	IR_CONN_FUNC_ARG_79,
	IR_CONN_FUNC_ARG_80,
	IR_CONN_FUNC_ARG_81,
	IR_CONN_FUNC_ARG_82,
	IR_CONN_FUNC_ARG_83,
	IR_CONN_FUNC_ARG_84,
	IR_CONN_FUNC_ARG_85,
	IR_CONN_FUNC_ARG_86,
	IR_CONN_FUNC_ARG_87,
	IR_CONN_FUNC_ARG_88,
	IR_CONN_FUNC_ARG_89,
	IR_CONN_FUNC_ARG_90,
	IR_CONN_FUNC_ARG_91,
	IR_CONN_FUNC_ARG_92,
	IR_CONN_FUNC_ARG_93,
	IR_CONN_FUNC_ARG_94,
	IR_CONN_FUNC_ARG_95,
	IR_CONN_FUNC_ARG_96,
	IR_CONN_FUNC_ARG_97,
	IR_CONN_FUNC_ARG_98,
	IR_CONN_FUNC_ARG_99,
	IR_CONN_FUNC_ARG_100,
	IR_CONN_FUNC_ARG_101,
	IR_CONN_FUNC_ARG_102,
	IR_CONN_FUNC_ARG_103,
	IR_CONN_FUNC_ARG_104,
	IR_CONN_FUNC_ARG_105,
	IR_CONN_FUNC_ARG_106,
	IR_CONN_FUNC_ARG_107,
	IR_CONN_FUNC_ARG_108,
	IR_CONN_FUNC_ARG_109,
	IR_CONN_FUNC_ARG_110,
	IR_CONN_FUNC_ARG_111,
	IR_CONN_FUNC_ARG_112,
	IR_CONN_FUNC_ARG_113,
	IR_CONN_FUNC_ARG_114,
	IR_CONN_FUNC_ARG_115,
	IR_CONN_FUNC_ARG_116,
	IR_CONN_FUNC_ARG_117,
	IR_CONN_FUNC_ARG_118,
	IR_CONN_FUNC_ARG_119,
	IR_CONN_FUNC_ARG_120,
	IR_CONN_FUNC_ARG_121,
	IR_CONN_FUNC_ARG_122,
	IR_CONN_FUNC_ARG_123,
	IR_CONN_FUNC_ARG_124,
	IR_CONN_FUNC_ARG_125,
	IR_CONN_FUNC_ARG_126,
	IR_CONN_FUNC_ARG_127,
	IR_CONN_FUNC_ARG_128,
};
enum IRNodeType {
		IR_SIZEOF,
		//
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
	IR_SIMD,
	//
	IR_GT,
	IR_LT,
	IR_GE,
	IR_LE,
	IR_EQ,
	IR_NE,
	//
	IR_COND_JUMP,
	IR_JUMP_TAB,
	//
	IR_VALUE,
	IR_LABEL,
	IR_LABEL_LOCAL,
	//
	IR_FUNC_ARG,
	IR_FUNC_CALL,
	IR_FUNC_RETURN,
	IR_FUNC_START,
	IR_FUNC_END,
	IR_FUNC_VAARG_ARGC,
	IR_FUNC_VAARG_ARGV,
	//
	IR_SUB_SWITCH_START_LABEL,
	//
	IR_ADDR_OF,
	IR_DERREF,
	//
	IR_SPILL_LOAD,
	//
	IR_MEMBERS,
	IR_MEMBERS_ADDR_OF,
	//
	IR_ARRAY_DECL,
	//
	IR_X86_INST,
	IR_ASM_DU8,
	IR_ASM_DU16,
	IR_ASM_DU32,
	IR_ASM_DU64,
	IR_ASM_IMPORT,
	//
	IR_SOURCE_MAPPING,
};
struct IRNode;
struct IRAttr {
	void *name;
	void (*destroy)(struct IRAttr *);
};
LL_TYPE_DEF(struct IRAttr, IRAttr);
LL_TYPE_FUNCS(struct IRAttr, IRAttr);
int IRAttrInsertPred(const struct IRAttr *a, const struct IRAttr *b);
int IRAttrGetPred(const void *key, const struct IRAttr *b);
struct IRNode {
	enum IRNodeType type;
	llIRAttr attrs; // NULL by defualt
};
GRAPH_TYPE_DEF(struct IRNode, enum IRConnType, IR);
GRAPH_TYPE_FUNCS(struct IRNode, enum IRConnType, IR);
extern void *IR_ATTR_VARIABLE;
enum IRValueType {
	__IR_VAL_MEM_FRAME,
	__IR_VAL_MEM_GLOBAL,
	__IR_VAL_LABEL,
	IR_VAL_REG,
	IR_VAL_VAR_REF,
	IR_VAL_FUNC,
	IR_VAL_INT_LIT,
	IR_VAL_STR_LIT,
	IR_VAL_FLT_LIT,
};
struct IRValue;
struct IRValOpResult {
	graphNodeIR node;
};
struct IRValIndirect {
	graphNodeIR base;
	graphNodeIR index;
	long scale;
};
struct IRValMemFrame {
	long offset;
	struct object *type;
};
struct IRValMemGlobal {
	struct parserVar *symbol;
};
struct IRVar {
		struct parserVar *var;
	long SSANum;
};
struct IRValue {
	enum IRValueType type;
	union {
		struct regSlice reg;
		struct IRVar var;
		struct IRValMemFrame __frame;
		struct IRValMemGlobal __global;
		graphNodeIR __label;
		struct IRValIndirect indir;
		struct IRValOpResult opRes;
		struct parserFunction *func;
		struct lexerInt intLit;
		double fltLit;
		struct __vec *strLit;
		graphNodeIR memLabel;
	} value;
};
struct IRNodeSizeof {
		struct IRNode base;
		struct object *type;
};
struct IRNodeArrayDecl {
		struct IRNode base;
		struct object *itemType;
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
// Out-going connection is to where
struct IRNodeJump {
	struct IRNode base;
	int forward;
};
// Out-going connection is to where
struct IRNodeCondJump {
	struct IRNode base;
	enum IRFlag cond;
};
struct IRNodeSpill {
	struct IRNode base;
	struct IRValue item;
};
struct IRNodeLoad {
	struct IRNode base;
	struct IRValue item;
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
struct IRNodeMembersAddrOf {
		struct IRNode base;
		strObjectMember members;
};
struct IRNodeMembers {
	struct IRNode base;
	strObjectMember members;
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
struct IRNodeLabelLocal {
	struct IRNode base;
};
struct IRNodePtrRef {
	struct IRNode base;
};
struct IRNodeFuncCall {
	struct IRNode base;
};
struct IRJumpTableRange {
	graphNodeIR to;
	long start, end;
};
STR_TYPE_DEF(struct IRJumpTableRange, IRTableRange);
STR_TYPE_FUNCS(struct IRJumpTableRange, IRTableRange);
struct IRNodeJumpTable {
	struct IRNode base;
	long startIndex;
	long count;
	strIRTableRange labels;
};
struct IRNodeSubSwit {
	struct IRNode base;
	graphNodeIR startCode;
};
struct IRNodeTypeCast {
	struct IRNode base;
	struct object *in;
	struct object *out;
};
struct IRNodeStatementStart {
	struct IRNode base;
	graphNodeIR end;
};
struct IRNodeStatementEnd {
	struct IRNode base;
	graphNodeIR start;
};
struct IRNodeFuncStart {
	struct IRNode base;
	graphNodeIR end;
	struct parserFunction *func;
};
struct IRNodeFuncArg {
	struct IRNode base;
	struct object *type;
	long argIndex;
};
struct IRNodeFuncEnd {
	struct IRNode base;
};
struct IRNodeFuncReturn {
	struct IRNode base;
	graphNodeIR exp;
};
struct IRNodeChoose {
	struct IRNode base;
	strGraphNodeIRP canidates;
};
struct IRNodeX86Inst {
	struct IRNode base;
	char *name;
	strX86AddrMode args;
};
struct IRNodeAsmImport {
	struct IRNode base;
	char *fileName;
};
struct IRNodeAsmDU8 {
	struct IRNode base;
	uint8_t *data;
	long count;
};
struct IRNodeAsmDU16 {
	struct IRNode base;
	uint16_t *data;
	long count;
};
struct IRNodeAsmDU32 {
	struct IRNode base;
	uint32_t *data;
	long count;
};
struct IRNodeSourceMapping {
		struct IRNode base;
		const char *fn;
		long start,len;
};
struct IRNodeAsmDU64 {
	struct IRNode base;
	uint64_t *data;
	long count;
};
char *IR2Str();
graphNodeIR IRCreateIntLit(int64_t lit);
graphNodeIR IRCreateBinop(graphNodeIR a, graphNodeIR b, enum IRNodeType type);
graphNodeIR IRCreateLabel();
graphNodeIR IRCreateVarRef(struct parserVar *var);
graphNodeIR IRCreateValueFromLabel(graphNodeIR lab);
struct parserVar *IRCreateVirtVar(struct object *type);
struct IRVarRefs {
	strGraphNodeIRP refs;
};
PTR_MAP_FUNCS(struct parserVar *, struct IRVarRefs, IRVarRefs);
strGraphNodeP IRStatementNodes(graphNodeIR stmtStart, graphNodeIR stmtEnd);
graphNodeIR IRCreateStmtEnd(graphNodeIR start);
graphNodeIR IRCreateStmtStart();
void initIR();
strGraphEdgeIRP IRGetConnsOfType(strGraphEdgeIRP conns, enum IRConnType type);
void IRInsertBefore(graphNodeIR insertBefore, graphNodeIR entry, graphNodeIR exit, enum IRConnType connType);
void IRInsertAfter(graphNodeIR insertAfter, graphNodeIR entry, graphNodeIR exit, enum IRConnType connType);
graphNodeIR IRCreateAssign(graphNodeIR in, graphNodeIR dst);
graphNodeIR IRCreateCondJmp(graphNodeIR cond, graphNodeIR t, graphNodeIR f);
void IRStmtBlockFromTailNode(graphNodeIR tail, graphNodeIR *enter, graphNodeIR *exit);
graphNodeIR IRStmtStart(graphNodeIR node);
int IRVarCmp(const struct IRVar *a, const struct IRVar *b);
char *graphEdgeIR2Str(struct __graphEdge *edge);
graphNodeIR IRCreateReturn(graphNodeIR exp, graphNodeIR func);
void IRGraphMap2GraphViz(graphNodeMapping graph, const char *title, const char *fn,
                         char *(*nodeLabelOverride)(graphNodeIR node, mapGraphVizAttr *attrs, const void *data),
                         char *(*edgeLabelOverride)(graphEdgeIR node, mapGraphVizAttr *attrs, const void *data), const void *dataNodes, const void *dataEdge);
graphNodeIR IRCreateStrLit(const char *text);
graphNodeIR IRCreateUnop(graphNodeIR a, enum IRNodeType type);
graphNodeIR IRCreateFuncCall(graphNodeIR func, ...);
graphNodeIR IRCreateTypecast(graphNodeIR in, struct object *inType, struct object *outType);
enum IRCloneMode {
	IR_CLONE_NODE,
	IR_CLONE_EXPR,
	IR_CLONE_EXPR_UNTIL_ASSIGN,
	IR_CLONE_UP_TO,
};
PTR_MAP_FUNCS(struct __graphNode *, struct __graphNode *, GraphNode);
graphNodeIR IRCloneNode(graphNodeIR node, enum IRCloneMode mode, ptrMapGraphNode *mappings);
int IRVarCmpIgnoreVersion(const struct IRVar *a, const struct IRVar *b);
int IRIsExprEdge(enum IRConnType type);
struct object *IRValueGetType(struct IRValue *node);
graphNodeIR IRCreateSpillLoad(struct IRVar *var);
graphNodeIR IRCreateRegRef(const struct regSlice *slice);
graphNodeIR IREndOfExpr(graphNodeIR node);
strGraphNodeIRP IRStmtNodes(graphNodeIR end);
void IRRemoveDeadExpression(graphNodeIR end, strGraphNodeP *removed);
int IRIsDeadExpression(graphNodeIR end);
struct object *IRNodeType(graphNodeIR node);
void IRNodeDestroy(struct IRNode *node);
graphNodeIR IRCloneUpTo(graphNodeIR node, strGraphNodeIRP to, ptrMapGraphNode *mappings);
int IRIsOperator(graphNodeIR node);
graphNodeIR IRCreateFuncArg(struct object *type, long funcIndex);
graphNodeIR IRCreateMemberAccess(graphNodeIR input, const char *name);
void IRRemoveNeedlessLabels(graphNodeIR start);
void IRInsertNodesBetweenExprs(graphNodeIR expr, int (*pred)(graphNodeIR, const void *), const void *predData);
void IRPrintMappedGraph(graphNodeMapping map);
graphNodeIR IRCreatePtrRef(graphNodeIR ptr);
void IRMarkPtrVars(graphNodeIR start);
strGraphNodeIRP IRVarRefs(struct parserVar *var, long *SSANum);
graphNodeIR IRCreateFloat(double value);
void IRAttrReplace(graphNodeIR node, llIRAttr attribute);
graphNodeIR IRCreateJumpTable();
graphNodeIR IRCreateFrameAddress(long offset, struct object *obj);
strGraphEdgeIRP IREdgesByPrec(graphNodeIR node);
void IRMoveAttrsTo(graphNodeIR from,graphNodeIR to);
graphNodeIR IRCreateAddrOf(graphNodeIR input);
graphNodeIR IRCreateDerref(graphNodeIR input);
struct IRAttrVariable {
	struct IRAttr base;
	struct IRVar var;
};
graphNodeIR IRCreateGlobalVarRef(struct parserVar *var);
graphNodeIR IRCreateArrayDecl(struct parserVar *assignInto,struct object *type,strGraphNodeIRP dims);
graphNodeIR IRGetArrayDimForVar(struct parserVar *arrVar,long i);
graphNodeIR IRObjectArrayScale(struct objectArray *arr);
graphNodeIR IRCreateArrayAccess(graphNodeIR arr,graphNodeIR index);
graphNodeIR IRCreateSourceMapping(const char *fileName,long start,long len);
void IRRemoveNeverFlows(graphNodeIR node);
graphNodeIR IRCreateFuncVaArgArgv();
graphNodeIR IRCreateFuncVaArgArgc();
graphNodeIR IRCreateSizeof(struct object *obj);
