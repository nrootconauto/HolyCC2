#include "dumpDebugInfo.h"
#include "cleanup.h"
#include "diagMsg.h"
STR_TYPE_DEF(char,Char);
STR_TYPE_FUNCS(char,Char);
static int ptrPtrCmp(const void *a, const void *b) {
		if(*(void**)a>*(void**)b)
				return 1;
		else if(*(void**)a<*(void**)b)
				return -1;
		return 0;
}
static strChar fromFmt(const char *fmt,...) {
		va_list list,list2;
		va_start(list, fmt);
		va_copy(list2, list);
		long len=vsnprintf(NULL, 0, fmt, list);
		char buffer[len+1];
		vsprintf(buffer,fmt, list2);
		char *retVal=strcpy(calloc( len+1, 1),buffer);
		va_end(list);
		va_end(list2);

		__auto_type str=strCharAppendData(NULL, retVal, strlen(retVal));
		free(retVal);
		return str;
}
static char * emitTypeRef(struct object *obj) {
		strChar baseType CLEANUP(strCharDestroy)=NULL;
		long ptrLevel=0;
		strChar retVal CLEANUP(strCharDestroy)=NULL;
		for(;;) {
				switch(obj->type) {
				case TYPE_ARRAY:;
						//TODO implement dim
						struct objectArray *arr=(void*)obj;
						ptrLevel++;
						obj=arr->type;
						break;
				case TYPE_PTR:;
						struct objectPtr *ptr=(void*)obj;
						ptrLevel++;
						obj=ptr->type;
						break;
				case TYPE_Bool:
				case TYPE_CLASS:
				case TYPE_UNION:
				case TYPE_F64:
				case TYPE_FORWARD:
				case TYPE_FUNCTION:
				case TYPE_I16i:
				case TYPE_I32i:
				case TYPE_I64i:
				case TYPE_I8i:
				case TYPE_U0:
				case TYPE_U16i:
				case TYPE_U32i:
				case TYPE_U64i:
				case TYPE_U8i:;
						char *tn=object2Str(obj);
						baseType=fromFmt("%s",tn);
						baseType=strCharAppendItem(baseType, '\0');
						free(tn);
						goto emit;
				}
		}
	emit:
		retVal=fromFmt("{\n\t\"base\":\"%s\",\n",baseType);
		retVal=strCharConcat(retVal, fromFmt("\t\"ptrLevel\":%li,\n", ptrLevel));
		retVal=strCharConcat(retVal,fromFmt("}\n"));
		retVal=strCharAppendItem(retVal, '\0');
		
		return strcpy(calloc(1, strlen(retVal)+1), retVal);
}
STR_TYPE_DEF(struct parserVar *,PVar);
STR_TYPE_FUNCS(struct parserVar *,PVar);
typedef int(*varCmpType)(const struct parserVar **,const struct parserVar **);
static strChar emitVarFrameOffsets(ptrMapFrameOffset offsets) {
		long size=ptrMapFrameOffsetSize(offsets);
		struct parserVar *dumpTo[size];
		ptrMapFrameOffsetKeys(offsets,  dumpTo);

		strPVar named CLEANUP(strPVarDestroy)=NULL;
		for(long v=0;v!=size;v++) {
				if(!dumpTo[v]->name) continue;
				named=strPVarSortedInsert(named, dumpTo[v], (varCmpType)ptrPtrCmp);
		}

		strChar retVal=fromFmt("[\n");
		for(long v=0;v!=strPVarSize(named);v++) {
				retVal=strCharConcat(retVal, fromFmt("{\n\t\"name\":\"%s\",\n", named[v]->name));
				char *typeStr=emitTypeRef(named[v]->type);
				retVal=strCharConcat(retVal, fromFmt("\t\"type\":%s,\n",typeStr));
				free(typeStr);
				retVal=strCharConcat(retVal, fromFmt("\t\"offset\":%li,\n",*ptrMapFrameOffsetGet(offsets, named[v])));
				retVal=strCharConcat(retVal,fromFmt("},\n"));
		}
		retVal=strCharConcat(retVal,fromFmt("]\n"));

		return retVal;
}
STR_TYPE_DEF(long, Long);
STR_TYPE_FUNCS(long, Long);
static int longCmp(const long *a,const long *b) {
		if(*a>*b) return 1;
		if(*a<*b) return -1;
		return 0;
}
FILE *emitFuncInfo(graphNodeIR start,ptrMapFrameOffset offsets) {
		strGraphNodeIRP allNodes CLEANUP(strGraphNodeIRPDestroy)=graphNodeIRAllNodes(start);
		strLong breakpointLines CLEANUP(strLongDestroy)=NULL;
		for(long n=0;n!=strGraphNodeIRPSize(allNodes);n++) {
				if(graphNodeIRValuePtr(allNodes[n])->type!=IR_SOURCE_MAPPING) continue;
				struct IRNodeSourceMapping *mapping=(void*)graphNodeIRValuePtr(allNodes[n]);

				long line;
				const char *fn;
				diagLineCol(&fn, mapping->start, &line, NULL);
				if(!strLongSortedFind(breakpointLines, line, longCmp))
						breakpointLines=strLongSortedInsert(breakpointLines, line, longCmp);

				__auto_type bp=IRCreateDebug(fn,line);
				IRInsertAfter(allNodes[n], bp, bp, IR_CONN_FLOW);
		}
		strChar layoutJson CLEANUP(strCharDestroy)=emitVarFrameOffsets(offsets);
		return NULL;
} 
static strChar emitDebuggerTypeDefintion(struct object *obj) {
		if(obj->type==TYPE_FORWARD)
				return NULL;
  		strObjectMember members=NULL;
		if(obj->type==TYPE_CLASS) {
				struct objectClass *cls=(void*)obj;
				members=cls->members;
		} else if(obj->type==TYPE_UNION) {
				struct objectClass *un=(void*)obj;
				members=un->members;
		} else return NULL;
		char *tn=object2Str(obj);
		strChar retVal=fromFmt("{\n\"name\":\"%s\",\n", tn);
		free(tn);

		retVal=strCharConcat(retVal, fromFmt("\"size\":%li,\n",objectSize(obj, NULL)));
		retVal=strCharConcat(retVal, fromFmt("\"members\":[\n" ));
		for(long m=0;m!=strObjectMemberSize(members);m++) {
				retVal=strCharConcat(retVal, fromFmt("{\n\t\"name\":\"%s\",\n",members[m].name));
				retVal=strCharConcat(retVal, fromFmt("\t\"offset\":%li,\n",members[m].offset));
				char *typeStr =emitTypeRef(members[m].type);
				retVal=strCharConcat(retVal, fromFmt("\t\"type\":%s,\n",typeStr));
				free(typeStr);
				retVal=strCharConcat(retVal, fromFmt("},\n"));
		}
		retVal=strCharConcat(retVal, fromFmt("],\n}\n"));

		return retVal;
}
char *emitDebuggerTypeDefinitions() {
		long count;
		parserSymTableNames(NULL, &count);
		const char *keys[count];
		parserSymTableNames(keys, NULL);

		strChar total CLEANUP(strCharDestroy)=fromFmt("{\nTypes:[\n");
		for(long k=0;k!=count;k++) {
				__auto_type sym=parserGetGlobalSym(keys[k]);
				if(sym->var!=NULL) continue;
				if(sym->type->type==TYPE_CLASS||sym->type->type==TYPE_UNION) {
						total=strCharConcat(total, emitDebuggerTypeDefintion(sym->type));
						total=strCharAppendItem(total, ',');
				}
		}
		total=strCharConcat(total, fromFmt("],}\n"));
		total=strCharAppendItem(total, '\0');

		return strcpy(calloc(strlen(total)+1, 1), total);
}
char *emitDebuggerGlobalVarInfo(char *name) {
		__auto_type find=parserGetGlobalSym(name);
		if(!find->var) return NULL;
		char *ti=emitTypeRef(find->type);
		strChar str CLEANUP(strCharDestroy)=fromFmt("{name:\"%s\",\ntype:%s}", name,ti);
		str=strCharAppendItem(str, '\0');
		return strcpy(calloc(strlen(str)+1, 1), str);
}
char *emitDebufferFrameLayout(ptrMapFrameOffset offsets) {
		long size=ptrMapFrameOffsetSize(offsets);
		struct parserVar *keys[size];
		ptrMapFrameOffsetKeys(offsets, keys);

		strChar total CLEANUP(strCharDestroy)=fromFmt("[\n");
		for(long k=0;k!=size;k++) {
				if(!keys[k]->name) continue;
				total=strCharConcat(total, fromFmt("\t{"));
				char *typeRef =emitTypeRef(keys[k]->type);
				total=strCharConcat(total, fromFmt("\t\"name\":\"%s\",",keys[k]->name));
				total=strCharConcat(total, fromFmt("\t\"offset\":%li,",*ptrMapFrameOffsetGet(offsets, keys[k])));
				total=strCharConcat(total, fromFmt("\t\"type\":%s,",typeRef));
				free(typeRef);
				total=strCharConcat(total, fromFmt("\t},\n"));
		}
		total=strCharConcat(total, fromFmt("]"));
		total=strCharAppendItem(total, '\0');
		return strcpy(calloc(strlen(total)+1, 1), total);
}
