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

														CFileDebugger *retval=malloc(sizeof(CFileDebugger));
														retval->typeTable=typeTable;
														return  retval;
}
CFuncDebugger *FuncDebuggerCreate(CFileDebugger *file,JsonNode *vars) {
						
}
U8i *DebuggerPrintVar(CFileDebugger *f,U0 *var,U8i *typeName=lastclass,I32i ptrLevel=0);
static U8i *DebuggerPrintTypeFromJson(CFileDebugger *f,U0 *var,JsonNode *node) {
							U8i *typename=JsonNodeEntry(node,"base")->val.str;
							U8i *ptrLevel=JsonNodeEntry(node,"ptrLevel")->val.sint;
							return DebuggerPrintVar(f,var,typename,ptrLevel);
}
U8i *DebuggerPrintVar(CFileDebugger *f,U0 *var,U8i *typeName=lastclass,I32i ptrLevel=0) {
				if(ptrLevel) {
																	U8i *name=StrDupLen(typeName,StrLen(typeName));
																	while((ptrLevel-=1)>=0) {
																																									name=StrCat(name,StrDupLen("*",1));
																	}
																	U8i *retval=MStrPrint("%q:%p\n",name,*var(U0**));
																	free(name);
																	return retval;
				}

				if(0==StrCmp(typeName,"I8i")) {
																																			return MStrPrint("%d",*var(I32i*));
				} else if(0==StrCmp(typeName,"I16i")) {
				                               return MStrPrint("%d",*var(I32i*));
				} else if(0==StrCmp(typeName,"I32i")) {
				                               return MStrPrint("%d",*var(I32i*));
				} else if(0==StrCmp(typeName,"U8i")) {
																																			return MStrPrint("%u",*var(U32i*));
				} else if(0==StrCmp(typeName,"U16i")) {
				                               return MStrPrint("%u",*var(U32i*));
				} else if(0==StrCmp(typeName,"U32i")) {
				                               return MStrPrint("%u",*var(U32i*));
				} else if(0==StrCmp(typeName,"F64")) {
				                               return MStrPrint("%n",*var(F64*));
				}

				printf("Getting\n");
				JsonNode *type=HashTableGet(f->typeTable,typeName);
				if(!type) throw(DEBUGGER_TYPE_NO_EXIST);
				type=type->val.node;

				U8i *retval=MStrPrint("{");
				JsonNode *members=JsonNodeEntry(type,"members");
				if(!members) throw(DEBUGGER_TYPE_NO_EXIST);
				for(I32i m=0;members->val.arr[m]!=NULL;m+=1) {
													printf("%p\n\n\n",members->val.arr[m]);
													JsonNode *node=members->val.arr[m]->val.node;
													JsonNode *name=JsonNodeEntry(node,"name");
													JsonNode *offset=JsonNodeEntry(node,"offset");
													U0 *ptr2=var(U8i*)+offset->val.sint;
													JsonNode *type=JsonNodeEntry(node,"type");
													
													U8i *valstr=DebuggerPrintTypeFromJson(f,ptr2,type->val.node);
													retval=StrCat(retval,MStrPrint("%s=%s,",name->val.str,valstr));
													free(valstr);
				}
				retval=StrCat(retval,StrDupLen("}",1));
				return retval;
}