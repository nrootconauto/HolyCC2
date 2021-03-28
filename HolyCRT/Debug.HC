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
														JsonNode *types=JsonNodeEntry(parse->val.node,"Types");
														printf("Types:%p\n",types);
														if(!types) throw(DEBUGGER_INIT_ERR);

														CHashTable *typeTable=HashTableCreate();
														printf("%li,\n",types->type);
														JsonNodePrint(parse);
														for(I32i i=0;types->val.arr[i]!=NULL;i+=1) {
																							JsonNode *name=JsonNodeEntry(types->val.arr[i]->val.node,"name");
																							printf("name:%p\n",name);
																							if(!name) throw(DEBUGGER_INIT_ERR);
																							printf("ADD:%s\n",name->val.str);
																							HashTableAdd(typeTable,types->val.arr[i],name->val.str);
														}
														
}
CFuncDebugger *FuncDebuggerCreate(CFileDebugger *file,JsonNode *vars) {
						
}
U8i *DebuggerPrintVar(CFileDebugger *f,U0 *var,U8i *typeName=lastclass) {
				Print("%q\n",typeName);
				if(0==StrCmp(typeName,"I8i")) {
																																			return MStrPrint("I8i:%d\n",*var(I32i*));
				} else if(0==StrCmp(typeName,"I16i")) {
				                               return MStrPrint("I16i:%d\n",*var(I32i*));
				} else if(0==StrCmp(typeName,"I32i")) {
				                               return MStrPrint("I32i:%d\n",*var(I32i*));
				} else if(0==StrCmp(typeName,"U8i")) {
																																			return MStrPrint("U8i:%u\n",*var(U32i*));
				} else if(0==StrCmp(typeName,"U16i")) {
				                               return MStrPrint("U16i:%u\n",*var(U32i*));
				} else if(0==StrCmp(typeName,"U32i")) {
				                               return MStrPrint("U32i:%u\n",*var(U32i*));
				} else if(0==StrCmp(typeName,"F64")) {
				                               return MStrPrint("F64:%n\n",*var(F64*));
				}

				JsonNode *type=HashTableGet(f->typeTable,typeName);
				if(!type) throw(DEBUGGER_TYPE_NO_EXIST);
}