extern U8i *gets(U8i *buffer);;
class CFuncDebugger;
class CGlobalVarInfo {
						U0 *ptr;
						JsonNode *info;
};
class FILE;
extern FILE *fopen(U8i *fn,U8i *f);
extern U0 fclose(FILE *f);
extern I32i fread(U0 *ptr,I32i nmem,I32i sz,FILE *f);
extern U0 fwrite(U0 *ptr,I32i nmem,I32i sz,FILE *f);
#define FILE_NONEXIST -1
static U8i *ReadFile(U8i *file,I32i *size=NULL) {
							FILE *f=fopen(file,"r");
							if(!f) throw(FILE_NONEXIST);
							I32i bufferSz=1<<15;
							I32i written=0;
							U8i *retval=malloc(bufferSz);
							I32i succ;
							while(succ=fread(retval+written,1,1,f)) {
																																																	written+=succ;
																																																	if(written+1>=bufferSz) {
																																																																											retval=realloc(retval,written+bufferSz);																									
																																																	}
							}
							retval=realloc(retval,written+1);
							retval[written]='\0';
							if(size) *size=written;
							fclose(f);
							return retval;
}
#define DBG_SOURCE_CACHE_SIZE (16)
class CBreakpoint {
						U8i *fn;
						I32i ln;
};
class CFuncRange {
						U0 *start,*end;
						U8i *name;
};
class CFileDebugger {
						CHashTable *typeTable; //JsonNode *
						CHashTable *funcDebuggers; // CFuncDebugger *
						CHashTable *varTable; //CGlobalVarInfo *
						CHashTable *sourceFileCache; //U8i *
						JsonNode *bpData;
						I32i bpCount;
						CHashTable *bpByNum; //CBreakpoint *
						CFuncRange **funcNmByPtr; //NULL terminated
};
class CFuncDebugger {
						CHashTable *varTable; //JsonNode *
};
internal U8i HCC_DEBUG_SYMS;
internal U8i HCC_DEBUG_FUNC_DATAS;
internal U8i HCC_DEBUG_BREAKPOINTS_INFO;
internal U8i HCC_DEBUG_BREAKPOINTS_ARRAY;
internal U8i HCC_DEBUG_GLOBALS_INFO;
#define DEBUGGER_INIT_ERR -1
#define DEBUGGER_TYPE_NO_EXIST -2
CFuncDebugger *FuncDebuggerCreate(CFileDebugger *file,U0 *funcPtr,U0 *funcptrend,U8i *info);
CFileDebugger *FileDebuggerCreate() {
														JsonNode *bps=ParserJsonObject(&HCC_DEBUG_BREAKPOINTS_INFO,0,NULL,NULL);
														Print("%p\n",bps);
														JsonNode *parse=ParserJsonObject(&HCC_DEBUG_SYMS,0,NULL,NULL);
														JsonNode *types=JsonNodeEntry(parse->val.node,"Types");
														if(!types) throw(DEBUGGER_INIT_ERR);

														CHashTable *typeTable=HashTableCreate();
														for(I32i i=0;types->val.arr[i]!=NULL;i+=1) {
																							JsonNode *name=JsonNodeEntry(types->val.arr[i]->val.node,"name");
																							HashTableAdd(typeTable,types->val.arr[i],name->val.str);
														}

														CHashTable *glbls=HashTableCreate();
														I32i infoOff=0;
														JsonNode *infoJson=NULL;
														glblLoop:
														if(*(&HCC_DEBUG_GLOBALS_INFO+infoOff)(U0**)!=0) {
																																																															U0 *glblptr=*(&HCC_DEBUG_GLOBALS_INFO+infoOff)(U0**);
																																																															infoOff+=sizeof(glblptr);

																																																															infoJson=ParserJsonObject(&HCC_DEBUG_GLOBALS_INFO+infoOff,0,NULL,NULL);
																																																															name=JsonNodeEntry(infoJson->val.node,"name");

																																																															infoOff+=StrLen(&HCC_DEBUG_GLOBALS_INFO+infoOff)+1;

																																																															CGlobalVarInfo *global=malloc(sizeof(CGlobalVarInfo));
																																																															global->ptr=glblptr;
																																																															global->info=infoJson;
																																																															HashTableAdd(glbls,global,name->val.str);

																																																															goto glblLoop;
														}

														CFileDebugger *retval=malloc(sizeof(CFileDebugger));
														retval->typeTable=typeTable;
														retval->funcDebuggers=HashTableCreate();
														retval->bpData=bps;
														retval->varTable=glbls;
														retval->sourceFileCache=HashTableCreate();
														retval->bpCount=0;
														retval->bpByNum=HashTableCreate();
														retval->funcNmByPtr=malloc(sizeof(CFuncRange*));
														retval->funcNmByPtr[0]=NULL;
														
														U8i **list=(&HCC_DEBUG_FUNC_DATAS)(U8i**);
														while(*list) {
																											FuncDebuggerCreate(retval,list[0],list[1],list[2]);
																											list+=3;
														}
														return  retval;
}
CFuncDebugger *FuncDebuggerCreate(CFileDebugger *file,U0 *funcptr,U0 *funcptrend,U8i *info) {
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

																		CFuncRange *range=malloc(sizeof(CFuncRange));
																		range->start=funcptr,range->end=funcptrend,range->name=funcname;
																		
																		c=0;
																		I32i funccount=0;
																		for (;file->funcNmByPtr[c];c++) funccount++;
																		c=0;
																		for (;file->funcNmByPtr[c];c++) {
																						if(file->funcNmByPtr[c].start>funcptr) break;
																		}
																		I32i rev=funccount;
																		for (;rev>c;rev--) file->funcNmByPtr[rev]=file->funcNmByPtr[rev-1];
																		file->funcNmByPtr[c]=range;
																		file->funcNmByPtr=realloc(file->funcNmByPtr,(++funccount+1)*sizeof(CFuncRange));
																		file->funcNmByPtr[funccount]=NULL;
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
																	if(ptrLevel==1&&0==StrCmp(typeName,"U8i")) {
																																	I32i strlen=1024;
																																	U8i **strPtr=var(U8i**);
																																	try {
																																						strlen=StrLen(*strPtr); //Dummy run for segfualt check
																																		} catch {
																																				return MStrPrint("U8i*:UNACC");
																																		};
																																		U8i *str=StrDupLen(*strPtr,strlen);
																																		U8i *escaped=MStrPrint("\"%q\"",str);
																																		free(str);
																																		return escaped;
																	}						
																	U8i *name=StrDupLen(typeName,StrLen(typeName));
																	while((ptrLevel-=1)>=0) {
																																									name=StrCat(name,StrDupLen("*",1));
																	}
																	retval=MStrPrint("%q:%p\n",name,*var(U0**));
																	free(name);
																	return retval;
				}

				if(0==StrCmp(typeName,"I8i")) {
																																			return MStrPrint("%d",*var(I8i*));
				} else if(0==StrCmp(typeName,"I16i")) {
				                               return MStrPrint("%d",*var(I16i*));
				} else if(0==StrCmp(typeName,"I32i")) {
				                               return MStrPrint("%d",*var(I32i*));
				} else if(0==StrCmp(typeName,"U8i")) {
																																			return MStrPrint("%u",*var(U8i*));
				} else if(0==StrCmp(typeName,"U16i")) {
				                               return MStrPrint("%u",*var(U16i*));
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
static I32i LinumFromLoc(U8i *loc,U8i **end) {
							I32i pos=StrLen(loc)-1;
							for(;pos>=0;pos--) {
																										if('9'>=loc[pos]>='0') ; else break; 
							}
							I32i ln=Str2I32i(&loc[pos+1]);
							if(end) *end=&loc[pos];
							return ln;
}
static U8i *DbgMatchFile(U8i *fn) {
							JsonNode *root=JsonNodeEntry(HCC_ThisFileDebugger->bpData->val.node,"breakpoints");
							I32i c=0,chr=0,cmp;
							U8i *res;

							I32i dircount=0;
							for(c=0;;) {
															res=StrChr(&fn[c],'/');
															if(!res) break;
															c=res+1-fn;
															if(res!=fn) {
																												if(fn[res-1]=='\\') goto next;
															}
															dircount++;
															next:
							}
							I32i *matches=malloc(0);
							I32i matchcount=0;
							JsonNode *filename;
							I32i curdircount=0;
							for(curdircount=c=0;root->val.arr[c];c++) {
																																		curdircount=0;
																																		filename=JsonNodeEntry(root->val.arr[c]->val.node,"filename");
																																		U8i *str=filename->val.str;
																																		chr=StrLen(str);

																																		loop:
																																		for(;chr>=0;chr--) {
																																																					if(str[chr]=='/') break;
																																		}
																																		if(chr!=0) if(str[chr-1]=='\\') { chr--;goto loop;}

																																		if(chr==-1) res=NULL; else res=&str[chr];
																																		if(!res) {chr=0;goto foundFn;} else {
																																											if(curdircount++==dircount) {
																																																																							chr++; //Move past "/"
																																																																							goto foundFn;
																																											}
																																											res--;
																																		}
																																		chr=res-str;

																																		foundFn:
																																		if(0==StrCmp(fn,&str[chr])) {
																																																														matches=realloc(matches,(matchcount+1)*sizeof(I32i));
																																																														matches[matchcount++]=c;
																																		}
				  }
						U8i *retval=NULL;
						if(matchcount==0) goto ret;
						else if(matchcount==1) retval=JsonNodeEntry(root->val.arr[matches[0]]->val.node,"filename")->val.str;
						else {
											"Ambigious filename,canidates are:\n";
											while(--matchcount>=0) {
																																		filename=JsonNodeEntry(root->val.arr[matchcount]->val.node,"filename");
																																		"%q\n",filename;
											}
						}
						ret:
						free(matches);
						return StrDupLen(retval,StrLen(retval));
}
#define DBG_BREAKPOINT_NORM 1
#define DBG_BREAKPOINT_TMP 2
static U8i *FileDebuggerGetBreakPtr(CFileDebugger *dbg,U8i *fn,I32i line) {
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
			return NULL;
			found:
			I32i lastBpOff=-1; //See HCC_DEBUG_BREAKPOINTS_ARRAY
			for(c=0;node->val.arr[c];c++) {
																																	JsonNode *lineNode=JsonNodeEntry(node->val.arr[c]->val.node,"line");
																																	lastBpOff=JsonNodeEntry(node->val.arr[c]->val.node,"offset")->val.sint;
																																	if(lineNode->val.sint>=line) break;
			}
			if(lastBpOff==-1) return NULL;
			return &((&HCC_DEBUG_BREAKPOINTS_ARRAY)(U8i*)[lastBpOff]);
}
static U0 FileDebuggerBreak(CFileDebugger *dbg,U8i *fn,I32i line) {
							if(!FileDebuggerGetBreakPtr(dbg,fn,line)) throw (1);
							*FileDebuggerGetBreakPtr(dbg,fn,line)=DBG_BREAKPOINT_NORM;

							++HCC_ThisFileDebugger->bpCount;
							U8i *bptext=MStrPrint("%d",HCC_ThisFileDebugger->bpCount);
							"Added breakpoint %d\n",HCC_ThisFileDebugger->bpCount;

								CBreakpoint *bp=malloc(sizeof(CBreakpoint));
								bp->fn=StrDupLen(fn,StrLen(fn));
								bp->ln=line;
								HashTableAdd(HCC_ThisFileDebugger->bpByNum,bp,bptext);
								free(bptext);
}
static U0 FileDebuggerBreakTmp(CFileDebugger *dbg,U8i *fn,I32i line,U8i silent=0) {
							if(!FileDebuggerGetBreakPtr(dbg,fn,line)) throw (1);
							*FileDebuggerGetBreakPtr(dbg,fn,line)=DBG_BREAKPOINT_TMP;

							if(silent) {
																		++HCC_ThisFileDebugger->bpCount;
																		U8i *bptext=MStrPrint("%d",HCC_ThisFileDebugger->bpCount);
																		"Added tmp-breakpoint %d\n",HCC_ThisFileDebugger->bpCount;

																		CBreakpoint *bp=malloc(sizeof(CBreakpoint));
																		bp->fn=StrDupLen(fn,StrLen(fn));
																		bp->ln=line;
																		HashTableAdd(HCC_ThisFileDebugger->bpByNum,bp,bptext);
																		free(bptext);
							}
}
static U0 FileDebuggerRemoveBreak(CFileDebugger *dbg,I32i bpnum) {
			U8i *bptext=MStrPrint("%d",bpnum);
			CBreakpoint *bp=HashTableGet(HCC_ThisFileDebugger->bpByNum,bptext);
			if(!bp) {
											"Breakpoint %d doesn't exist\n",bpnum;
											free(bptext);
											return;
			}
			I32i line=bp->ln;
			U8i *fn=bp->fn;

			*FileDebuggerGetBreakPtr(dbg,fn,line)=0;

			"Removing breakpoint %d\n",bpnum;
			
			free(bp->fn);
			free(bp);
			free(bptext);
}
static U8i *__dbgSkipWhitespace(U8i *buffer) {
							I32i c;
							for(c=0;;c++) {
																					if(buffer[c]==' ') goto next;
																					if(buffer[c]=='\t') goto next;
																					break;
																					next:
						 }
							return &buffer[c];
}
static U8i *__dbgGetNextWord( U8i *buffer) {
							I32i c=__dbgSkipWhitespace(buffer)-buffer;
							I32i start=c;
							for(;;c++) {
																		if('z'>=buffer[c]>='a') goto valid;
																		if('9'>=buffer[c]>='0') goto valid;
																		if('Z'>=buffer[c]>='A') goto valid;
																		if(buffer[c]=='_') goto valid;
																		break;
																		valid:
							}
							//Ensure next charactor is end or whitespace
							if(buffer[c]=='\0'||buffer[c]==' '||buffer[c]=='\t') {
																																								U8i *retval=malloc(c-start+1);
																																								I32i off=0;
																																								for(;start!=c;start++) retval[off++]=buffer[start];
																																								retval[off]='\0';
																																								return retval;
							}
							return NULL;
}
static U8i **DbgListAdjLines(U8i *str,I32i startln,I32i hgt,I32i *read=NULL) {
							U8i **retval=NULL;
							
							I32i chr=0;
							U8i *endings="\n\r\c";
							I32i curln=0;
							I32i len=StrLen(str);

							U8i *startpos=NULL;
							U8i *endpos=NULL;
loop:
       I32i i;
							U8i *lnend=&str[len];
       for(i=0;i!=3;i++) {
																									U8i *find=StrChr(&str[chr],endings[i]);
																									if(lnend>find&&find)	lnend=find;
							}
							if(startln==curln) startpos=&str[chr];
							//Will be non-NULL if encontered first line
							if(startpos) {
																			U8i *slice=StrDupLen(&str[chr],lnend-&str[chr]);
																			retval=realloc(retval,(curln-startln+1)*sizeof(I32i));
																			retval[curln-startln]=slice;
							}
							if(startln+hgt==curln) {
																														endpos=&str[chr];
																														if(read) *read=curln-startln;
																														return retval;
							}
							curln++;
							chr=lnend-str;
							
							for(;StrChr(endings,str[chr]);chr++);
							if(str[chr]!='\0') goto loop;

							if(read) *read=curln-startln;
							return retval;
}
#define DBG_LIST_SIZE (16)
static U0 DbgFileList(CFileDebugger *dbg,U8i *fn,I32i line,I32i atln=-1) {
							loop:
							U8i *text=HashTableGet(dbg->sourceFileCache,fn);
							if(!text) {
																	if(DBG_SOURCE_CACHE_SIZE<=HashTableSize(dbg->sourceFileCache)) {
																			I32i count=HashTableSize(dbg->sourceFileCache);
																			U8i **keys=HashTableKeys(dbg->sourceFileCache);
																			I32i maxsize=0,maxsizeidx=0;
																			while ((count-=1)>=0) {
																									I32i size=StrLen(HashTableGet(dbg->sourceFileCache,keys[count]));
																									if (maxsize<size) maxsize=size,maxsizeidx=count;
																			}
																			free(HashTableGet(dbg->sourceFileCache,keys[maxsizeidx]));
																	}

																	HashTableAdd(dbg->sourceFileCache,ReadFile(fn),fn);
																	goto loop;
							}
							I32i origln=line;
							if(line-DBG_LIST_SIZE/2<0) line=0; else line-=DBG_LIST_SIZE/2;
							I32i read;
							U8i **lines=DbgListAdjLines(text,line,DBG_LIST_SIZE,&read);
							I32i maxlinumwidth=ceil(Log10((line+read)(F64)));
							I32i curln;
							for (curln=line;curln<line+read;curln++) {
											U8i *linetext=MStrPrint("%d",curln);
											I32i toadd=maxlinumwidth-StrLen(linetext);
											linetext=realloc(linetext,maxlinumwidth+1+1); 
											I32i c;
											for(c=StrLen(linetext);c!=maxlinumwidth;c++) linetext[c]=' ';
											linetext[maxlinumwidth]='|';
											linetext[maxlinumwidth+1]='\0';
											U8i chrbp=' ';
											U8i chratln=' ';

											U8i *bpptr=FileDebuggerGetBreakPtr(HCC_ThisFileDebugger,fn,curln);
											if(bpptr) if(*bpptr) chrbp='B';
											if(curln==atln) chratln='*';

											"%c%c%s%s\n",chratln,chrbp,linetext,lines[curln-line];
											free(lines[curln-line]);
							}
							free(lines);
}
static U8i *FileAndLine(U8i *str,I32i *ln,U8i *dftfn=NULL) {
							str=StrDupLen(str,StrLen(str));
							U8i *endpos;
							I32i linenum=LinumFromLoc(str,&endpos);
							if(ln) *ln=linenum;
							"%d\n",linenum;
							if(linenum==-1) {
																							invalidLoc:
																							free(str);
																							throw(-1);
							} else if(*endpos==':') {
									*endpos='\0';
									U8i *filename=DbgMatchFile(str);
									if(!filename) goto invalidLoc;
									free(str);
									return filename;
							} else if(dftfn) {
									return StrDupLen(dftfn,StrLen(dftfn));
							}
							goto invalidLoc;
}
static U0 *DebugGetFramePtr(U8i *bp,I32i lvl,U8i *fn);
static U0 DebugStackTrace(U8i *bp);

static U8i HCC_DebugAtLineDisable=0;
static U8i DebugStep=0;
static U8i *DebugNextFunc=NULL;
static U8i *DebugLastCom=NULL;
static U0 DbgEnter(U8i *func,U8i *funcBp,U8i *dftfn=NULL,I32i atln=0) {
							CFuncDebugger *fdbg=HashTableGet(HCC_ThisFileDebugger->funcDebuggers,func);

							U8i *currfuncbp=funcBp;
							U8i *currfuncnam=func;
							I32i currframei=1;
							CFuncDebugger *tmpdbg;
							currfuncbp=DebugGetFramePtr(funcBp,currframei,&currfuncnam);

							I32i i=0;
							U8i *buffer=malloc(1024);
							U8i *str;
							I32i c,offset,linum;
							U8i *printed,*endpos,*filename,*bpText;
							I32i listln=atln;
							U8i *listfn=HCC_DbgCurrentFunc;
							U8i **keys;

							loop:
							Print("BSAT >>>(%s):",func);
							gets(buffer);
							if (StrLen(buffer)!=0) {
										free(DebugLastCom);
										DebugLastCom=__dbgGetNextWord(buffer);
							}
							if(0==StrCmp(DebugLastCom,"help")) {
																																				help:
																																				Print("Commands:\n");
																																				Print("help -- display this message\n");
																																				Print("c -- continue\n");
																																				Print("p -- print a variable(not an expression)\n");
																																				Print("list -- Lists the area around the current line,or next. Can list a location.\n");
																																				Print("breaks -- lists breakpoints.\n");
																																				Print("lv -- list visible variables\n");
																																				Print("b -- break at a function,or file location.\n");
																																				Print("d -- remove a breakpoint.\n");
																																				Print("s -- step.\n");
																																				Print("bt -- backtrace.\n");
																																				Print("up -- go up a stack frame.\n");
																																				Print("down -- go up a stack frame.\n");
																																				goto loop;
							} else if(0==StrCmp(DebugLastCom,"down")) {
									if (currframei==1) {
												"At bottomost frame\n";
												goto loop;
									}
									currframei--;
									U8i *tmpfn;
									U0 *tmpbp;
									try {
													tmpbp=DebugGetFramePtr(funcBp,currframei,&tmpfn);
													currfuncnam=tmpfn;
													currfuncbp=tmpbp;
									} catch {
											"Cant move to frame %d\n",currframei++;
									}
									goto loop;
							} else if(0==StrCmp(DebugLastCom,"up")) {
									currframei++;
									tmpfn;
									tmpbp;
									try {
													tmpbp=DebugGetFramePtr(funcBp,currframei,&tmpfn);
													currfuncnam=tmpfn;
													currfuncbp=tmpbp;
													"%s:\n",tmpfn;
									} catch {
											"Cant move to frame %d\n",currframei--;
									}
									goto loop;
							} else if(0==StrCmp(DebugLastCom,"bt")) {
									DebugStackTrace(funcBp);
									goto loop;
							} else if(0==StrCmp(DebugLastCom,"n")) {
									DebugNextFunc=func;
									goto ret;
							} else if(0==StrCmp(DebugLastCom,"s")) {
									DebugStep=1;
									goto ret;
							} else if(0==StrCmp(DebugLastCom,"c")) {
									goto ret;
							} else if(0==StrCmp(DebugLastCom,"p")) {
									U8i *pVar=__dbgGetNextWord(buffer+1);

									"%s:\n",currfuncnam;
									tmpdbg=HashTableGet(HCC_ThisFileDebugger->funcDebuggers,currfuncnam);
									if(!tmpdbg) {
														free(pVar);
														"Unkown function at frame %d\n",currframei;
														goto loop;
									}

									JsonNode *pVarNode=HashTableGet(tmpdbg->varTable,pVar);
									if(!pVarNode) {
																							//Look for global
																							CGlobalVarInfo *ginfo=HashTableGet(HCC_ThisFileDebugger->varTable,pVar);
																							if(!ginfo) {
																																					Print("Var \"%s\" not found!\n",pVar);
																																					free(pVar);
																																					goto loop;
																							}

																							pVarNode=JsonNodeEntry(ginfo->info->val.node,"type")->val.node;
																							printed=DebuggerPrintTypeFromJson(HCC_ThisFileDebugger,ginfo->ptr,pVarNode);
																							"%s:%s\n",pVar,printed;
																							free(pVar),free(printed);
																							goto loop;
									}
									offset=JsonNodeEntry(pVarNode->val.node,"offset")->val.sint;
									pVarNode=JsonNodeEntry(pVarNode->val.node,"type")->val.node;

									printed=DebuggerPrintTypeFromJson(HCC_ThisFileDebugger,currfuncbp-offset,pVarNode);
									"%s:%s\n",pVar,printed;
									free(pVar),free(printed);
									goto loop;
							} else if(0==StrCmp(DebugLastCom,"lv")) {
														tmpdbg=HashTableGet(HCC_ThisFileDebugger->funcDebuggers,currfuncnam);
														if(!tmpdbg) {
																										"Unable to find function at frame %d\n",currframei;
																										goto loop;
														}
														I32i varCount=HashTableSize(tmpdbg->varTable);
														keys=HashTableKeys(tmpdbg->varTable);
														"=====[LOCALS]====\n";
														for(c=0;c!=varCount;c++) {
																																							"%s\n",keys[c];
														}
														free(keys);
														"=====[GLOBALS]====\n";
														varCount=HashTableSize(HCC_ThisFileDebugger->varTable);
														keys=HashTableKeys(HCC_ThisFileDebugger->varTable);
														for(c=0;c!=varCount;c++) {
																																							if(!HashTableGet(fdbg->varTable,keys[c])) "%s\n",keys[c];
														}
														free(keys);
														goto loop;
							} else if(0==StrCmp(DebugLastCom,"breaks")) {
									keys=HashTableKeys(HCC_ThisFileDebugger->bpByNum);
									c=HashTableSize(HCC_ThisFileDebugger->bpByNum);
									while (--c>=0) {
															CBreakpoint *bp=HashTableGet(HCC_ThisFileDebugger->bpByNum,keys[c]);
															"%s> %s:%d\n",keys[c],bp->fn,bp->ln;
									}
									free(keys);
									goto loop;
							} else if(0==StrCmp(DebugLastCom,"b")) {
									//__dbgGetNextWord Ensures result is a word and not a combination of alnums and others(will be a function name if not NULL)
									U8i *next=__dbgSkipWhitespace(buffer+1);
									try {
												filename=FileAndLine(next,&linum,dftfn);
												FileDebuggerBreak(HCC_ThisFileDebugger,filename,linum);
												free(filename);
									} catch {
											Print("Unable to make sense of provided location!!!\n");
									}
									goto loop;
							} else if(0==StrCmp("d",DebugLastCom)) {
									I32i num=Str2I32i(__dbgSkipWhitespace(buffer+1));
									FileDebuggerRemoveBreak(HCC_ThisFileDebugger,num);
									goto loop;
							} else if(0==StrCmp(DebugLastCom,"list")) {
									next=__dbgSkipWhitespace(buffer+StrLen("list"));
									if (*next=='\0') {
												DbgFileList(HCC_ThisFileDebugger,listfn,listln,atln);
												listln+=DBG_LIST_SIZE;
												goto loop;
									} else {
											try {
															filename=FileAndLine(next,&linum,dftfn);
															listln=linum;
															listfn=filename;
															DbgFileList(HCC_ThisFileDebugger,listfn,listln,atln);
															free(filename);
															listfn=dftfn;
															goto loop;
											} catch {
													Print("Unable to make sense of provided location!!!\n");
											}
									}
							}
							Print("Invalid command \"%q\"\n",buffer);
							goto help
							ret:
							free(buffer);
}
static U0 *DebugGetFramePtr(U8i *bp,I32i lvl,U8i **fn) {
							U0 *calledfrom=*((bp)(U8i*)+4)(U8i **);
							if(fn) *fn=NULL;
							while (--lvl>=0) {
													I32i c=0;
													for (;HCC_ThisFileDebugger->funcNmByPtr[c];c++) {
																	CFuncRange *r=HCC_ThisFileDebugger->funcNmByPtr[c];
																	if (r->start<=calledfrom<=r->end) {
																				if(fn) *fn=r->name;
																				break;
																	}
											  }
													bp=*(bp(U8i*))(U0**);
													calledfrom=*((bp(U8i*)+4)(U0**));
							}
							return bp;
}
static U0 DebugStackTrace(U8i *bp) {
							U0 *calledfrom=*((bp)(U8i*)+4)(U8i **);
							I32i frame=0;
							loop:
							try {
											I32i c=0;
											for (;HCC_ThisFileDebugger->funcNmByPtr[c];c++) {
															CFuncRange *r=HCC_ThisFileDebugger->funcNmByPtr[c];
															if (r->start<=calledfrom<=r->end) {
																		"%d:%s(bp:%p)\n",frame++,r->name,*(bp(U8i*))(U0**);
																		break;
															}
											}
											bp=*(bp(U8i*))(U0**);
											calledfrom=*((bp(U8i*)+4)(U0**));
							} catch {
									goto exit;
							}
							goto loop;
							exit:
}
U0 HCC_DebugAtLine(U8i *fn,U8i *func,I32i ln,I32i bpOff) {
			if(HCC_DebugAtLineDisable) return;
			U8i oldDisable=HCC_DebugAtLineDisable;
			HCC_DebugAtLineDisable=1;

			U8i *parBP=NULL;
			asm {
							IMPORT parBP;
							PUSH EDI
							MOV EDI, EBP
							MOV I32i [parBP], EDI
							POP EDI
			};

			HCC_DbgCurrentFunc=fn;

			if(DebugNextFunc) if(0==StrCmp(DebugNextFunc,func)) { DebugNextFunc=NULL;goto enter;}
			if(DebugStep) goto enter;
			if(!(&HCC_DEBUG_BREAKPOINTS_ARRAY)(U8i*)[bpOff]) goto ret;
			
			enter:
			DebugStep=0;
			
			"\nWelocme to %s:%d,%p\n",fn,ln,parBP;
			DbgEnter(func,parBP,fn,ln);

			ret:
			HCC_DebugAtLineDisable=oldDisable;
			return;
}