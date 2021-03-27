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
														JsonNode *parse=ParserJsonObject(&HCC_DEBUG_SYMS,0,NULL,NULL);
														JsonNode *types=JsonNodeEntry(parse,"Types");
														if(!types) throw(DEBUGGER_INIT_ERR);

														CHashTable *typeTable=HashTableCreate();
														for(I32i i=0;types->val.arr[i]!=NULL;i+=1) {
																							JsonNode *name=JsonNodeEntry(types->val.arr[i],"name");
																							if(!types) throw(DEBUGGER_INIT_ERR);
																							HashTableAdd(typeTable,name->val.str,types->val.arr[i]);
														}
}
CFuncDebugger *FuncDebuggerCreate(CFileDebugger *file,JsonNode *vars) {
						
}
U8i *DebuggerPrintVar(CFileDebugger *f,U0 *var,U8i *typeName) {
				JsonNode *type=HashTableGet(f->typeTable,typeName);
				if(!type) throw(DEBUGGER_TYPE_NO_EXIST);

				if(0==StrCmp(typeName,"I8i")) {
																																			Print("I8i:%d\n",*var(I32i*));
				} else if(0==StrCmp(typeName,"I16i")) {
				                               Print("I16i:%d\n",*var(I32i*));
				} else if(0==StrCmp(typeName,"I32i")) {
				                               Print("I32i:%d\n",*var(I32i*));
				} else if(0==StrCmp(typeName,"U8i")) {
																																			Print("U8i:%u\n",*var(U32i*));
				} else if(0==StrCmp(typeName,"U16i")) {
				                               Print("U16i:%u\n",*var(U32i*));
				} else if(0==StrCmp(typeName,"U32i")) {
				                               Print("U32i:%u\n",*var(U32i*));
				} else if(0==StrCmp(typeName,"F64")) {
				                               Print("F64:%n\n",*var(F64*));  
				}
}