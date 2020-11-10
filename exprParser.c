#include <parserA.h>
#include <assert.h>
#include <stdlib.h>
#include <diagMsg.h>
#include <stdio.h>
struct object * assignTypeToOp(struct parserNode *node);
static int isArith(struct object *type) {
		if(
					type==&typeU8i
					||type==&typeU16i
					||type==&typeU32i
					||type==&typeU64i
					||type==&typeI8i
					||type==&typeI16i
					||type==&typeI32i
					||type==&typeI64i
					||type==&typeF64
					||type->type==TYPE_PTR
					||type->type==TYPE_ARRAY
					) {
				return 1;
		}
		return 0;
}
static int objIndex(const struct object **objs,long count,const struct object *obj) {
		for(long i=0;i!=count;i++)
				if(obj==objs[i])
						return i;

		assert(0);
		return -1;
}
static struct object *promotionType(const struct object *a,const struct object *b) {
		const struct object *ranks[]={
				&typeU0,
				&typeI8i,
				&typeU8i,
				&typeI16i,
				&typeU16i,
				&typeI32i,
				&typeU32i,
				&typeI64i,
				&typeU64i,
				&typeF64,
		};
		long count=sizeof(ranks)/sizeof(*ranks);
		long I64Rank=objIndex(ranks, count, &typeI64i);

		long aRank=objIndex(ranks,count,a);
		long bRank=objIndex(ranks,count,b);
		if(aRank<I64Rank)
				aRank=I64Rank;
		if(bRank<I64Rank)
				bRank=I64Rank;

		if(aRank==bRank)
				return (struct object *)ranks[aRank];
		else if(aRank>bRank)
				return (struct object *)ranks[aRank];
		else if(aRank<bRank)
				return (struct object *)ranks[bRank];

		return NULL;
}
static struct parserNode *promoteIfNeeded(struct parserNode *node,struct object *toType) {
		if(assignTypeToOp(node)!=toType) {
				struct parserNodeTypeCast *cast=malloc(sizeof(struct parserNodeTypeCast));
				cast->base.type=NODE_TYPE_CAST;
				cast->exp=node;
				cast->type=toType;

				return (void *)cast;
		}

		return node;
}
struct object *assignTypeToOp(struct parserNode *node) {
		if(node->type==NODE_BINOP) {
				struct parserNodeBinop *binop=(void*)node;
				if(binop->type!=NULL)
						return binop->type;

				__auto_type aType=assignTypeToOp(binop->a);
				__auto_type bType=assignTypeToOp(binop->b);

				int aArih=isArith(aType);
				int bArih=isArith(bType);
				if(aArih&&bArih) {
						__auto_type resType= promotionType(aType, bType);
						if(aType!=resType) {
								binop->a= promoteIfNeeded(binop->a, resType);
								binop->b= promoteIfNeeded(binop->b, resType);
						}

						binop->type=resType;
						return resType;
				}else if(aArih||bArih) {
				binopInvalid:;
						//Both are no compatible with arithmetic operators,so if one isnt,whine
						struct parserNodeOpTerm *op=(void*) binop->op;
						assert(op->base.type==NODE_OP);

						diagErrorStart(op->pos.start, op->pos.end);
						diagPushText("Invalid operands to operator ");
						diagPushQoutedText(op->pos.start, op->pos.end);
						char buffer[1024];
						char *aName=object2Str(aType),*bName=object2Str(bType);
						sprintf(buffer, ". Operands are type '%s' and '%s'.", aName,bName);
						diagPushText(buffer);
						diagHighlight(op->pos.start, op->pos.end);

						free(aName),free(bName);
						
						//Dummy value
						binop->type=&typeI64i;
						return &typeI64i;
				} else {
						//Both are non-arithmetic
						goto binopInvalid;
				}
		} else if(node->type==NODE_UNOP) {
				struct parserNodeUnop *unop=(void*)node;

				__auto_type aType=assignTypeToOp(unop->a);
				if(!isArith(aType)) {
				invalidUnop:;
						struct parserNodeOpTerm *op=(void*) unop->op;
						assert(op->base.type==NODE_OP);

						diagErrorStart(op->pos.start, op->pos.end);
						diagPushText("Invalid operands to operator.");
						diagHighlight( op->pos.start, op->pos.end);

						char buffer[1024];
						char *name=object2Str(aType);
						sprintf(buffer,"Operand is of type '%s'.",name);
						free(name);

						diagPushText(buffer);
						diagEndMsg();

						//Dummy value
						unop->type=&typeI64i;
						return &typeI64i;
				}
		} else if(node->type==NODE_FUNC_CALL) {
				struct parserNodeFuncCall *call=(void*)node;
				__auto_type funcType=assignTypeToOp(call->func);
				
				//TODO add method support
				if(funcType->type!=TYPE_FUNCTION) {
						struct parserNodeOpTerm *op=(void*)call->lP;
						diagErrorStart(op->pos.start,op->pos. end);
						char buffer[1024];
						char *typeName=object2Str(funcType);
						sprintf(buffer, "Type '%s' isn't callable.", typeName);
						diagEndMsg();

						free(typeName);
				}
		} else if(node->type==NODE_TYPE_CAST) {
				struct parserNodeTypeCast *cast=(void*)node;
		} else {
				assert(0);
		}
}
