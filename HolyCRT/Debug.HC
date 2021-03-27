extern U8i *gets(U8i *buffer);;
class CFuncDebugger;
class CFileDebugger {
						CHashTable *typeTable; //JsonNode *
};
class CFuncDebugger {
						CHashTable *varTable; //JsonNode *
						I32i lineStart,lineEnd;
};
internal  U8i HCC_DEBUG_SYMS;
#define DEBUGGER_INIT_ERR -1
#define DEBUGGER_TYPE_NO_EXIST -2
CFileDebugger *FileDebuggerCreate() {
														JSON *parse=ParserJsonObject(&HCC_DEBUG_SYMS,0,NULL,NULL);
														JSON *types=JsonNodeEntry(parse,"Types");
														if(!types) throw(DEBUGGER_INIT_ERR);

														CHashTable *typeTable=HashTableCreate();
														for(I32i i=0;types->val.arr[i]!=NULL;i+=1) {
																							JsonNode *name=JsonNodeEntry(types->val.arr[i],"name");
																							if(!types) throw(DEBUGGER_INIT_ERR);
																							HashTableAdd(typeTable,name->val.str,types->val.arr[i]);
														}
}
CFuncDebugger *FuncDebuggerCreate(CFileDebugger *file,JSON *vars) {
						
}
U8i *DebuggerPrintVar(CFileDebugger *f,U0 *var,U8i *typeName) {
				JsonNode *type=HashTableGet(f->typeTable,type);
				if(!type) throw(DEBUGGER_TYPE_NO_EXIST);

				if(0==StrNCmp(typeName,"I8i")) {
																																			Print("I8i:%d\n",*var(I32i*));
				} else if(0==StrNCmp(typeName,"I16i")) {
				                               Print("I16i:%d\n",*var(I32i*));
				} else if(0==StrNCmp(typeName,"I32i")) {
				                               Print("I32i:%d\n",*var(I32i*));
				} else if(0==StrNCmp(typeName,"U8i")) {
				} else if(0==StrNCmp(typeName,"U16i")) {
				} else if(0==StrNCmp(typeName,"U32i")) {
				} else if(0==StrNCmp(typeName,"F64")) {
				}
}