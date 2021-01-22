#include <IR.h>
#include <ptrMap.h>
#include <registers.h>
#include <cleanup.h>
#include <ptrMap.h>
#include <assert.h>
static void *IR_ATTR_NODE_TYPE="NODE_TYPE";
struct IRAttrNodeType {
		struct IRAttr base;
		struct object *type;
};
static struct object **getType(graphNodeIR node) {
	loop:;
		__auto_type find=llIRAttrFind(graphNodeIRValuePtr(node)->attrs,IR_ATTR_NODE_TYPE,IRAttrGetPred);
		if(!find) {
				struct IRAttrNodeType dummy;
				dummy.base.name=IR_ATTR_NODE_TYPE;
				dummy.type=NULL;
				dummy.base.destroy=NULL;
				graphNodeIRValuePtr(node)->attrs=llIRAttrInsert(graphNodeIRValuePtr(node)->attrs,__llCreate(&dummy, sizeof(dummy)),IRAttrInsertPred);
				goto loop;
		}
		return &((struct IRAttrNodeType*) llIRAttrValuePtr(find))->type;
}
static int isUndetirminedType(const struct __graphNode *node,const struct __graphEdge *edge,const void *data) {
		if(!IRIsExprEdge(*graphEdgeIRValuePtr((graphEdgeIR)edge)))
				return 0;
		if(NULL==*getType((graphNodeIR)node))
				return 1;
		return 0;
}
static void assignType(struct __graphNode * node,void *data) {
		*getType((graphNodeIR)node)=data;
}
static void unifiyUpwards(graphNodeIR node,struct object *type) {
		graphNodeIRVisitBackward(node, type, isUndetirminedType, assignType);
}
static int objIndex(const struct object **objs, long count,
                    const struct object *obj) {
	for (long i = 0; i != count; i++)
		if (obj == objs[i])
			return i;

	assert(0);
	return -1;
}

static struct object *getHigherType(struct object *a,struct object *b) {
		const struct object *ranks[] = {
	    &typeU0,   &typeI8i,  &typeU8i,  &typeI16i, &typeU16i,
	    &typeI32i, &typeU32i, &typeI64i, &typeU64i, &typeF64,
	};
		long count = sizeof(ranks) / sizeof(*ranks);
		__auto_type aIndex= objIndex(ranks, count, a);
		__auto_type bIndex= objIndex(ranks, count, b);
		return (aIndex>bIndex)?a:b;
}
struct object *IRNodeType(graphNodeIR node);
struct object *__IRNodeType(graphNodeIR node) {
		if(NULL!=*getType(node))
				return *getType(node);
		
		struct IRNodeValue *nodeVal=(void*)graphNodeIRValuePtr(node);
		if(nodeVal->base.type==IR_VALUE) {
				if(nodeVal->val.type==IR_VAL_REG) {
						return nodeVal->val.value.reg.type;
				} else if(nodeVal->val.type==IR_VAL_VAR_REF) {
						if(nodeVal->val.value.var.type==IR_VAR_VAR)
								return nodeVal->val.value.var.value.var->type;
				} else if(nodeVal->val.type==__IR_VAL_MEM_FRAME) {
						return nodeVal->val.value.__frame.type;
				}
				return NULL;
		}

		//Cant find type directly so intfer
		strGraphEdgeIRP in CLEANUP(strGraphEdgeIRPDestroy)=graphNodeIRIncoming(node);
		strGraphEdgeIRP sourceA CLEANUP(strGraphEdgeIRPDestroy)=IRGetConnsOfType(in, IR_CONN_SOURCE_A);
		strGraphEdgeIRP sourceB CLEANUP(strGraphEdgeIRPDestroy)=IRGetConnsOfType(in, IR_CONN_SOURCE_B);
				
		if(strGraphEdgeIRPSize(sourceA)&&strGraphEdgeIRPSize(sourceB)) {
				//Binop
				__auto_type aType=IRNodeType(graphEdgeIRIncoming(sourceA[0]));
				__auto_type bType=IRNodeType(graphEdgeIRIncoming(sourceB[0]));
				int aTypeDefined=NULL!=aType;
				int bTypeDefined=NULL!=bType;
				if(aTypeDefined^bTypeDefined) {
						struct object *type=(aTypeDefined)?aType:bType;
						unifiyUpwards(node, type);
						return type;
				} else if(aTypeDefined&&bTypeDefined) {
						return getHigherType(aType,bType);
				} else
						return NULL;
		} else if(strGraphEdgeIRPSize(sourceA)||strGraphEdgeIRPSize(sourceB)) {
				//Unop
				__auto_type aType=IRNodeType(graphEdgeIRIncoming(sourceA[0]));
				return aType;
		} 
		return NULL;
}
struct object *IRNodeType(graphNodeIR node) {
		__auto_type type=__IRNodeType(node);
		*getType(node)=type;
		return type;
}
