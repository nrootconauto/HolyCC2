extern U8i *gets(U8i *buffer);;
class CFuncDebugger;
class CFileDebugger {
						CHashTable *typeTable; //JsonNode *
						CHashTable *funcDebuggers; // CFuncDebugger *
						JsonNode *bpData;
};
class CFuncDebugger {
						CHashTable *varTable; //JsonNode *
};
internal U8i HCC_DEBUG_SYMS;
internal U8i HCC_DEBUG_FUNC_DATAS;
internal U8i HCC_DEBUG_BREAKPOINTS_INFO;
internal U8i HCC_DEBUG_BREAKPOINTS_ARRAY;
#define DEBUGGER_INIT_ERR -1
#define DEBUGGER_TYPE_NO_EXIST -2
CFuncDebugger *FuncDebuggerCreate(CFileDebugger *file,U8i *info);
CFileDebugger *FileDebuggerCreate() {
														JsonNode *bps=ParserJsonObject(&HCC_DEBUG_BREAKPOINTS_INFO,0,NULL,NULL);
														Print("%p\n",bps);
														JsonNode *parse=ParserJsonObject(&HCC_DEBUG_SYMS,0,NULL,NULL);
														JsonNode *types=JsonNodeEntry(parse->val.node,"Types");
														if(!types) throw(DEBUGGER_INIT_ERR);

														CHashTable *typeTable=HashTableCreate();
														for(I32i i=0;types->val.arr[i]!=NULL;i+=1) {
																							JsonNode *name=JsonNodeEntry(types->val.arr[i]->val.node,"name");
																							if(!name) throw(DEBUGGER_INIT_ERR);
																							HashTableAdd(typeTable,types->val.arr[i],name->val.str);
														}

														CFileDebugger *retval=malloc(sizeof(CFileDebugger));
														retval->typeTable=typeTable;
														retval->funcDebuggers=HashTableCreate();
														retval->bpData=bps;
														
														U8i **list=(&HCC_DEBUG_FUNC_DATAS)(U8i**);
														while(*list) {
																											FuncDebuggerCreate(retval,*list);
																											list++;
														}
														return  retval;
}
CFuncDebugger *FuncDebuggerCreate(CFileDebugger *file,U8i *info) {
														JsonNode *root=NULL;
														JsonNode *framelayout=NULL;
														JsonNode *name=NULL;
														I32i c=0;
														U8i *funcname=NULL;
														CFuncDebugger *retval=malloc(sizeof(CFuncDebugger));
														retval->varTable=HashTableCreate();
														try {
																		root=ParserJsonObject(info,0,NULL,NULL);
																		framelayout=JsonNodeEntry(root->val.node,"frameLayout");
																		if(framelayout) {
																																		for(c=0;framelayout->val.arr[c];c++) {
																																																																							JsonNode *tmp=framelayout->val.arr[c];
																																																																							name=JsonNodeEntry(tmp->val.node	,"name");
																																																																							HashTableAdd(retval->varTable,tmp,name->val.str);
																																		}
																		}

																		name=JsonNodeEntry(root->val.node	,"name");
																		if(name) funcname=name->val.str;
																		HashTableAdd(file->funcDebuggers,retval,funcname);
														} catch {
																Print("Failed to parse json for debugger");
														}
}
U8i *DebuggerPrintVar(CFileDebugger *f,U0 *var,U8i *typeName=lastclass,I32i ptrLevel=0);
static U8i *DebuggerPrintTypeFromJson(CFileDebugger *f,U0 *var,JsonNode *node) {
							U8i *typename=JsonNodeEntry(node,"base")->val.str;
							U8i *ptrLevel=JsonNodeEntry(node,"ptrLevel")->val.sint;
							return DebuggerPrintVar(f,var,typename,ptrLevel);
}
U8i *DebuggerPrintVar(CFileDebugger *f,U0 *var,U8i *typeName=lastclass,I32i ptrLevel=0) {
				U8i *retval=NULL;
				if(ptrLevel) {
																	U8i *name=StrDupLen(typeName,StrLen(typeName));
																	while((ptrLevel-=1)>=0) {
																																									name=StrCat(name,StrDupLen("*",1));
																	}
																	retval=MStrPrint("%q:%p\n",name,*var(U0**));
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

				JsonNode *type=HashTableGet(f->typeTable,typeName);
				if(!type) throw(DEBUGGER_TYPE_NO_EXIST);
				type=type->val.node;

				retval=MStrPrint("{");
				JsonNode *members=JsonNodeEntry(type,"members");
				if(!members) throw(DEBUGGER_TYPE_NO_EXIST);
				for(I32i m=0;members->val.arr[m]!=NULL;m+=1) {
													JsonNode *node=members->val.arr[m]->val.node;
													JsonNode *nameNode=JsonNodeEntry(node,"name");
													JsonNode *offsetName=JsonNodeEntry(node,"offset");
													U0 *ptr2=var(U8i*)+offsetName->val.sint;
													type=JsonNodeEntry(node,"type");
													
													U8i *valstr=DebuggerPrintTypeFromJson(f,ptr2,type->val.node);
													retval=StrCat(retval,MStrPrint("%s=%s,",nameNode->val.str,valstr));
													free(valstr);
				}
				retval=StrCat(retval,StrDupLen("}",1));
				return retval;
}
static CFileDebugger *HCC_ThisFileDebugger=NULL;
static U8i *HCC_DbgCurrentFunc=NULL; 
U0 FileDebuggerBreak(CFileDebugger *dbg,U8i *fn,I32i line) {
			if(!fn) fn=HCC_DbgCurrentFunc;
			loop:
			JsonNode *bps=JsonNodeEntry(HCC_ThisFileDebugger->bpData->val.node,"breakpoints");
			I32i c=0;
			JsonNode *node;
			for(;bps->val.arr[c];c++) {
																														node=JsonNodeEntry(bps->val.arr[c]->val.node,"filename");
																														if(0!=StrCmp(node->val.str,fn)) goto next;
																														node=JsonNodeEntry(bps->val.arr[c]->val.node,"lines");
																														goto found;
																														next:
			}
			Print("File \"%q\" not found!!!\n",fn);
			return;
			found:
			I32i lastBpOff=-1; //See HCC_DEBUG_BREAKPOINTS_ARRAY
			for(c=0;node->val.arr[c];c++) {
																																	JsonNode *lineNode=JsonNodeEntry(node->val.arr[c]->val.node,"line");
																																	lastBpOff=JsonNodeEntry(node->val.arr[c]->val.node,"offset")->val.sint;
																																	if(lineNode->val.sint>=line) break;
			}
			if(lastBpOff==-1) return;
			(&HCC_DEBUG_BREAKPOINTS_ARRAY)(U8i*)[lastBpOff]=1;
}
static U0 DbgEnter() {
							
							U8i *buffer=malloc(1024);
							loop:
							Print("BSAT >>>:");
							gets(buffer);
							if(0==StrCmp(buffer,"help")) {
																																				help:
																																				Print("Commands:\n");
																																				Print("help -- display this message\n");
																																				Print("c -- continue\n");
																																				goto loop;
							} else if(0==StrCmp(buffer,"c")) {
									goto ret;
							}
							Print("Invalid command \"%q\"",buffer);
							goto help
							ret:
							free(buffer);
}
static U8i HCC_DebugAtLineDisable=0;
U0 HCC_DebugAtLine(U8i *fn,I32i ln,I32i bpOff) {
			if(HCC_DebugAtLineDisable) return;
			
			HCC_DbgCurrentFunc=fn;
			if(!(&HCC_DEBUG_BREAKPOINTS_ARRAY)(U8i*)[bpOff]) return;
			if(!HCC_ThisFileDebugger) return;
		
			U8i oldDisable=HCC_DebugAtLineDisable;
			HCC_DebugAtLineDisable=1;
			
			DbgEnter();

			ret:
			HCC_DebugAtLineDisable=oldDisable;
			return;
}