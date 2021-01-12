#include <basicBlocks.h>
#include <assert.h>
#include <IR.h>
static int varFilter(graphNodeIR node,const struct parserVar *exclude) {
		__auto_type value =graphNodeIRValuePtr(node);
		if(value->type==IR_VALUE) {
				__auto_type value2=(struct IRNodeValue*)value;
				if(value2->val.type==IR_VAL_VAR_REF)
						if(value2->val.value.var.value.var==exclude)
								return 0;
		}
		return 1;
} 
void basicBlockTests() {
		{
				initIR();
				struct parserVar *aVar,*bVar,*cVar,*dVar;
				__auto_type a=IRCreateVarRef(aVar=IRCreateVirtVar(&typeI64i));
				__auto_type b=IRCreateVarRef(bVar=IRCreateVirtVar(&typeI64i));
				__auto_type c=IRCreateVarRef(cVar=IRCreateVirtVar(&typeI64i));
				__auto_type d=IRCreateVarRef(dVar=IRCreateVirtVar(&typeI64i));
				//c=!d
				graphNodeIRConnect(IRCreateUnop(d,IR_LNOT),c,IR_CONN_DEST);
				//a=b+c
				graphNodeIRConnect( IRCreateBinop(b, c, IR_ADD),a, IR_CONN_DEST);

				__auto_type start=IRStmtStart(a);
				__auto_type map= graphNodeCreateMapping(a, 1);
				__auto_type blocks=IRGetBasicBlocksFromExpr(NULL, NULL, map, NULL, NULL);
				assert(strBasicBlockSize(blocks)==2);
				int foundCD=0,foundABC=0;
				for(long i=0;i!=2;i++) {
						foundCD|=blocks[i]->read[0]->value.var==dVar&&blocks[i]->define[0]->value.var==cVar;
						if(blocks[i]->define[0]->value.var==aVar) {
								foundABC|=
										(blocks[i]->read[0]->value.var==bVar&&blocks[i]->read[1]->value.var==cVar)||
										(blocks[i]->read[1]->value.var==bVar&&blocks[i]->read[0]->value.var==cVar);
						}
				}
				assert(foundCD&&foundABC);

				//Filter tests
				map= graphNodeCreateMapping(a, 1);
				blocks=IRGetBasicBlocksFromExpr(NULL, NULL, map, cVar, (int(*)(graphNodeIR,const void*))varFilter);
				assert(strBasicBlockSize(blocks)==1);
				assert(blocks[0]->define[0]->value.var==aVar);
				assert((blocks[0]->read[0]->value.var==bVar&&blocks[0]->read[1]->value.var==dVar)||
										(blocks[0]->read[1]->value.var==bVar&&blocks[0]->read[0]->value.var==dVar));
		}
}
