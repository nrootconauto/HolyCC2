#define DBG_BREAKPOINT_NORM 1
#define DBG_BREAKPOINT_TMP 2
#define DBG_BREAKPOINT_FUNC 4

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
						U8i *fn,*funcNm;  //const,dont free
						I32i ln;
};
class CFuncRange {
						U0 *start,*end;
						U8i *name; //const,dont free
						U8i *funcName;
};
class __CSourceFile {
  U8i *source;
		I32i *lineStarts;
		I32i lineCount;
};
static __CSourceFile *SourceFileCreate(U8 *fn) {
							U8 *src=ReadFile(fn);
							I32 len=StrLen(src);
							I32 run=0;
							I32 linecount=1;
							I32 *linestarts=NULL;
							loop:
							for(I32i c=0;;) {
																U8 *find=StrChr(&src[c],'\n');
																if	(!find) {
																			if (run==0) {
																						linestarts=malloc(sizeof(I32)*linecount);
																						run=1;
																						linecount=0;
																						goto loop;
																			} else break;
																}
																if (!run)
																			linecount++;
																else
																	linestarts[linecount++]=c;

															c=find+1-src;		
							}
							__CSourceFile *retval=malloc(sizeof(__CSourceFile));
							retval->source=src;
							retval->lineStarts=linestarts;
							retval->lineCount=linecount;
							return retval;
}
class CFileDebugger {
						CHashTable *typeTable; //JsonNode *
						CHashTable *funcDebuggers; // CFuncDebugger *
						CHashTable *varTable; //CGlobalVarInfo *
						CHashTable *sourceFileCache; //__CSourceFile *
						CHashTable *bpByNum; //CBreakpoint *
						CFuncRange **funcNmByPtr; //NULL terminated
						CHashTable *breakAtFuncs; //1  as dummy;
						JsonNode *bpData;
						U8i *bpArray;
						I32i bpCount;
};
class CFuncDebugger {
						CHashTable *varTable; //JsonNode *
};
static CHashTable *HCC_FileDebuggers=HashTableCreate();
CFileDebugger *HCC_ThisFileDebugger=NULL;
extern U8i HCC_DEBUG_SYMS;
extern U8i HCC_DEBUG_FUNC_DATAS;
extern U8i HCC_DEBUG_BREAKPOINTS_INFO;
extern U8i HCC_DEBUG_BREAKPOINTS_ARRAY;
extern U8i HCC_DEBUG_GLOBALS_INFO;
#define DEBUGGER_INIT_ERR -1
#define DEBUGGER_TYPE_NO_EXIST -2
CFuncDebugger *FuncDebuggerCreate(CFileDebugger *file,U0 *funcPtr,U0 *funcptrend,U8i *info);
CFileDebugger *FileDebuggerCreate(
														U8i *HCC_DEBUGGER_FILE,
														U8i *HCC_DEBUG_SYMS,
														U8i *HCC_DEBUG_FUNC_DATAS,
														U8i *HCC_DEBUG_BREAKPOINTS_INFO,
														U8i *HCC_DEBUG_BREAKPOINTS_ARRAY,
														U8i *HCC_DEBUG_GLOBALS_INFO
														) {
														JsonNode *bps=ParserJsonObject(HCC_DEBUG_BREAKPOINTS_INFO,0,NULL,NULL);
														JsonNode *parse=ParserJsonObject(HCC_DEBUG_SYMS,0,NULL,NULL);
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
														if(*(HCC_DEBUG_GLOBALS_INFO+infoOff)(U0**)!=0) {
																																																															U0 *glblptr=*(HCC_DEBUG_GLOBALS_INFO+infoOff)(U0**);
																																																															infoOff+=sizeof(glblptr);

																																																															infoJson=ParserJsonObject(HCC_DEBUG_GLOBALS_INFO+infoOff,0,NULL,NULL);
																																																															name=JsonNodeEntry(infoJson->val.node,"name");

																																																															infoOff+=StrLen(HCC_DEBUG_GLOBALS_INFO+infoOff)+1;

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
														retval->breakAtFuncs=HashTableCreate();
														retval->bpArray=HCC_DEBUG_BREAKPOINTS_ARRAY;

														U8i **list=(HCC_DEBUG_FUNC_DATAS)(U8i**);
														while (*list) {
																				FuncDebuggerCreate(retval,list[0],list[1],list[2]);
																				list+=3;
														}

														try {
																		if(HCC_FileDebuggers) HashTableAdd(HCC_FileDebuggers,retval,HCC_DEBUGGER_FILE);
														} catch {
																//Silent fail
														};
														return retval;
}
extern I32i getchar();
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
																																																																							JsonNode *tmpnode=framelayout->val.arr[c];
																																																																							name=JsonNodeEntry(tmpnode->val.node	,"name");
																																																																							HashTableAdd(retval->varTable,tmpnode,name->val.str);
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
																		file->funcNmByPtr=realloc(file->funcNmByPtr,(++funccount+1)*sizeof(CFuncRange*));
																		I32i rev=funccount-1;
																		for (;rev>c;rev--) file->funcNmByPtr[rev]=file->funcNmByPtr[rev-1];
																		file->funcNmByPtr[c]=range;
																		file->funcNmByPtr[funccount]=NULL;
														} catch {
																"info:%p\n",info;
																Print("Failed to parse json for debugger");
																
																getchar();
														}
}
U8i *DebuggerPrintVar(CFileDebugger *f,U0 *var,U8i *typeName=lastclass,I32i ptrLevel=0);
static U8i *DebuggerPrintTypeFromJson(CFileDebugger *f,U0 *var,JsonNode *node) {
							U8i *typename=JsonNodeEntry(node,"base")->val.str;
							I32 ptrLevel=JsonNodeEntry(node,"ptrLevel")->val.sint;
							return DebuggerPrintVar(f,var,typename,ptrLevel);
}
U8i *DebuggerPrintVar(CFileDebugger *f,U0 *var,U8i *typeName=lastclass,I32i ptrLevel=0) {
				U8i *errret=NULL;
				U8i *retval=NULL;
				if(ptrLevel) {
																	if(ptrLevel==1&&(0==StrCmp(typeName,"U8i")||0==StrCmp(typeName,"U8"))) {
																																	I32i strlen=1024;
																																	U8i **strPtr=var(U8i**);
																																	try {
																																						strlen=StrLen(*strPtr); //Dummy run for segfualt check
																																		} catch {
																																				errret=MStrPrint("U8i*:UNACC");
																																		};
																																		if(errret) return errret;
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

				if(0==StrCmp(typeName,"I8i")||0==StrCmp(typeName,"I8")) {
																																			return MStrPrint("%d",*var(I8i*));
				} else if(0==StrCmp(typeName,"I16i")||0==StrCmp(typeName,"I16")) {
				                               return MStrPrint("%d",*var(I16i*));
				} else if(0==StrCmp(typeName,"I32i")||0==StrCmp(typeName,"I32")) {
				                               return MStrPrint("%d",*var(I32i*));
				} else if(0==StrCmp(typeName,"U8i")||0==StrCmp(typeName,"U8")) {
																																			return MStrPrint("%u",*var(U8i*));
				} else if(0==StrCmp(typeName,"U16i"||0==StrCmp(typeName,"U16"))) {
				                               return MStrPrint("%u",*var(U16i*));
				} else if(0==StrCmp(typeName,"U32i")||0==StrCmp(typeName,"U32")) {
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
}static U8i *HCC_DbgCurrentFunc=NULL; 
static I32i LinumFromLoc(U8i *loc,U8i **end) {
							I32i pos=StrLen(loc)-1;
							for(;pos>=0;pos--) {
																										if('9'>=loc[pos]>='0') ; else break; 
							}
							I32i ln=Str2I32i(&loc[pos+1]);
							//Ensure is ':' or whitespace
							if (pos>=0) {
										if(loc[pos]==':') goto succ;
										if(loc[pos]==' '||loc[pos]=='\t') goto succ;
										ln=-1;
										 succ:
							}
							if(pos==StrLen(loc)-1) ln=-1;
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
			return &((HCC_ThisFileDebugger->bpArray)(U8i*)[lastBpOff]);
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
U0 DebugBreakAtFunc(U8i *funcnm) {
			JsonNode *node=JsonNodeEntry(HCC_ThisFileDebugger->bpData->val.node,"funcLines");
			if(!node) throw(1);
			node=JsonNodeEntry(node->val.node,funcnm);
			if (!node) {
						"Function %s not found\n.",funcnm;
						throw(1);
			}
			U8i *fn=JsonNodeEntry(node->val.node,"filename")->val.str;
			I32i ln=JsonNodeEntry(node->val.node,"line")->val.sint;
			U0 *find=HashTableGet(HCC_ThisFileDebugger->breakAtFuncs,funcnm);
			if (!find)
						HashTableAdd(HCC_ThisFileDebugger->breakAtFuncs,1,funcnm);	
			*FileDebuggerGetBreakPtr(HCC_ThisFileDebugger,fn,ln)|=DBG_BREAKPOINT_FUNC;

			++HCC_ThisFileDebugger->bpCount;
			U8i *bptext=MStrPrint("%d",HCC_ThisFileDebugger->bpCount);
			"Added breakpoint %d\n",HCC_ThisFileDebugger->bpCount;

			CBreakpoint *bp=malloc(sizeof(CBreakpoint));
			bp->fn=StrDupLen(fn,StrLen(fn));
			bp->ln=ln;
			bp->funcNm=funcnm;
			HashTableAdd(HCC_ThisFileDebugger->bpByNum,bp,bptext);
			free(bptext);
}
static U0 FileDebuggerBreakTmp(CFileDebugger *dbg,U8i *fn,I32i line,U8i silent=0) {
							if(!FileDebuggerGetBreakPtr(dbg,fn,line)) throw (1);
							*FileDebuggerGetBreakPtr(dbg,fn,line)|=DBG_BREAKPOINT_TMP;
							
							++HCC_ThisFileDebugger->bpCount;
							U8i *bptext=MStrPrint("%d",HCC_ThisFileDebugger->bpCount);
							"Added tmp-breakpoint %d\n",HCC_ThisFileDebugger->bpCount;

							CBreakpoint *bp=malloc(sizeof(CBreakpoint));
							bp->fn=StrDupLen(fn,StrLen(fn));
							bp->ln=line;
							bp->funcNm=NULL;
							HashTableAdd(HCC_ThisFileDebugger->bpByNum,bp,bptext);
							free(bptext);
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
#define DBG_LIST_SIZE (16)
static U0 DbgFileList(CFileDebugger *dbg,U8i *fn,I32i line,I32i atln=-1) {
							loop:
							 __CSourceFile *text=HashTableGet(dbg->sourceFileCache,fn);
							if(!text) {
																	HashTableAdd(dbg->sourceFileCache,SourceFileCreate(fn),fn);
																	goto loop;
							}
							I32i origln=line;
							if(line-DBG_LIST_SIZE/2<0) line=0; else line-=DBG_LIST_SIZE/2;
							I32i read;
							
							U8i **lines=malloc(sizeof(U8*)*(text->lineCount+DBG_LIST_SIZE));
							I32 endline=line+DBG_LIST_SIZE;
							for (read=0;line!=endline&&line<text->lineCount;++read,line++) {
											U8 *linestartpos=&text->source[text->lineStarts[line]];
											U8 *lineendpos=NULL;
											if(line+1>=text->lineCount) {lineendpos=StrLen(linestartpos)+linestartpos;}
											else lineendpos=&text->source[text->lineStarts[line+1]]-1; //-1 ignores newline
											
											lines[read]=StrDupLen(linestartpos,lineendpos-linestartpos);
							}

							line=origln;
							if(line-DBG_LIST_SIZE/2<0) line=0; else line-=DBG_LIST_SIZE/2;

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
							U8i *filename=NULL;
							if(*endpos==':') {
									*endpos='\0';
									filename=DbgMatchFile(str);
									if(!filename) goto invalidLoc;
									if(0==StrLen(filename)) goto invalidLoc;
									return filename;
							} else if(dftfn&&linenum!=-1) {
									return StrDupLen(dftfn,StrLen(dftfn));
							}
							goto invalidLoc;
							invalidLoc:
							free(str);
							free(filename);
							throw(-1);
}
static U0 *DebugGetFramePtr(U8i *bp,I32i lvl,U8i **fn);
static U0 DebugStackTrace(U8i *bp,U8i *currFn) {
							I32 fin=0;
							"0:%s\n",currFn;
							U0 *calledfrom=*((bp)(U8i*)+4)(U8i **);
							I32i frame=1;
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
									fin=1;
							}
							if(!fin)
							goto loop;
							exit:
}
static U8i HCC_DebugAtLineDisable=0;
U8i DebugStep=0;
static U8i DebugAutolist=0;
static U8i *DebugNextFunc=NULL;
static U8i *DebugNextFramePtr=0xffffffff;
static U8i *DebugLastCom=NULL;
static U0 DbgEnter(U8i *func,U8i *funcBp,U8i *dftfn=NULL,I32i atln=0) {
							CFuncDebugger *fdbg=HashTableGet(HCC_ThisFileDebugger->funcDebuggers,func);

							U8i *currfuncbp=funcBp;
							U8i *currfuncnam=func;
							I32i currframei=0;
							CFuncDebugger *tmpdbg;
							currfuncbp=DebugGetFramePtr(funcBp,currframei,&currfuncnam);
							if(currfuncnam==NULL) {currfuncnam=func;currfuncbp=funcBp;}

							I32i i=0;
							U8i *buffer=malloc(1024);
							U8i *str;
							I32i c,offset,linum;
							U8i *printed,*endpos,*filename,*bpText;
							I32i listln=atln;
							U8i *listfn=HCC_DbgCurrentFunc;
							U8i **keys;
							U8i *next;

							if (DebugAutolist) {
										DbgFileList(HCC_ThisFileDebugger,dftfn,atln,atln);
							}

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
																																				Print("down -- go down a stack frame.\n");
																																				Print("tautolist -- toggle auto-list.\n");
																																				Print("lfdebuggers -- list the file debuggers.\n");
																																				Print("fswitch -- switch the file debugger.\n");
																																				Print("o -- step out of function.\n");
																																				goto loop;
							} else if(0==StrCmp(DebugLastCom,"o")) {
							if (!DebugGetFramePtr(currfuncbp,1,NULL)) {
												"Can't find frame pointer from here\n";
									} else {
											DebugNextFramePtr=DebugGetFramePtr(currfuncbp,1,NULL);
											goto ret;
									}
									goto loop;
							} else if(0==StrCmp(DebugLastCom,"lfdebuggers")) {
									c=HashTableSize(HCC_FileDebuggers);
									keys=HashTableKeys(HCC_FileDebuggers);
									"=====[FILE DEBUGGERS]=====\n";
									while(--c>=0) {
																							"%s\n",keys[c];
																							free(keys[c]);
									}
									free(keys);
									goto loop;
							}  else if(0==StrCmp(DebugLastCom,"fswitch")) {
											next=__dbgSkipWhitespace(buffer+StrLen("fswitch"));
											CFileDebugger *fdbgfind=HashTableGet(HCC_FileDebuggers,next);
											if (!fdbgfind) {
														"File \"%s\" not found.\n",next;
											} else {
													HCC_ThisFileDebugger=fdbgfind;
											}
											goto loop;
							} else if(0==StrCmp(DebugLastCom,"tautolist")) {
									DebugAutolist=!DebugAutolist;
									"%d",DebugAutolist;
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
									DebugStackTrace(funcBp,func);
									goto loop;
							} else if(0==StrCmp(DebugLastCom,"n")) {
									DebugNextFunc=func;
									DebugNextFramePtr=funcBp;
									goto ret;
							} else if(0==StrCmp(DebugLastCom,"s")) {
									DebugNextFunc=NULL; //Set to NULL to disable next 
									DebugNextFramePtr=0xffffffff;
									DebugStep=1;
									goto ret;
							} else if(0==StrCmp(DebugLastCom,"c")) {
									DebugNextFunc=NULL; //Set to NULL to disable next
									DebugNextFramePtr=0xffffffff;
									goto ret;
							} else if(0==StrCmp(DebugLastCom,"p")) {
									U8i *pVar=__dbgGetNextWord(buffer+1);

									tmpdbg=HashTableGet(HCC_ThisFileDebugger->funcDebuggers,currfuncnam);
									if(!tmpdbg) {
														goto lookForGlobal;
									}

									JsonNode *pVarNode=HashTableGet(tmpdbg->varTable,pVar);
									if (!pVarNode) {
												lookForGlobal:
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
														"=====[GLOBALS]====\n";
														I32i varCount=HashTableSize(HCC_ThisFileDebugger->varTable);
														keys=HashTableKeys(HCC_ThisFileDebugger->varTable);
														for (c=0;c!=varCount;c++) {
																																							if (fdbg) {
																																							 		if (!HashTableGet(fdbg->varTable,keys[c]))
																																													"%s\n",keys[c];
																																							} else 	"%s\n",keys[c];
														}
														free(keys);
														if(!tmpdbg) {
																										"Unable to find function at frame %d\n",currframei;
																										goto loop;
														}
														 varCount=HashTableSize(tmpdbg->varTable);
														keys=HashTableKeys(tmpdbg->varTable);
														"=====[LOCALS]====\n";
														for(c=0;c!=varCount;c++) {
																																							"%s\n",keys[c];
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
									next=__dbgSkipWhitespace(buffer+1);
									try {
												filename=FileAndLine(next,&linum,dftfn);
												"filename:%s\n",filename;
												FileDebuggerBreak(HCC_ThisFileDebugger,filename,linum);
												free(filename);
									} catch {
											next=__dbgGetNextWord(next);
											if (!next) {
														bInvalidLoc:
														free(next);
														Print("Unable to make sense of provided location!!!\n");
											} else {
													tmpdbg=HashTableGet(HCC_ThisFileDebugger->funcDebuggers,next);
													if(!tmpdbg) goto bInvalidLoc;
													DebugBreakAtFunc(next);
													free(next);
											}
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
U0 HCC_DebugAtLine(CFileDebugger **dbg,U8i *fn,U8i *func,I32i ln,I32i bpOff) {
			if(HCC_DebugAtLineDisable) return;
			HCC_ThisFileDebugger=*dbg;
			if(!HCC_ThisFileDebugger) return;
			U8i oldDisable=HCC_DebugAtLineDisable;
			HCC_DebugAtLineDisable=1;

			U8i *parBP=NULL;
			asm {
							IMPORT parBP;
							PUSH EDI
							MOV EDI, [EBP]
							MOV I32i [parBP], EDI
							POP EDI
			};

			HCC_DbgCurrentFunc=fn;

			if(DebugNextFunc) if(0==StrCmp(DebugNextFunc,func)&&DebugNextFramePtr==parBP) {goto enter;}
			if ((DebugNextFramePtr<=parBP)&&DebugNextFramePtr) {
						DebugNextFramePtr=parBP;
						goto enter;
			}
			if(DebugStep) {DebugStep=0; goto enter;}
			if(!(HCC_ThisFileDebugger->bpArray)(U8i*)[bpOff]) goto ret;
			U8i bpval=(HCC_ThisFileDebugger->bpArray)(U8i*)[bpOff];
			if(bpval&DBG_BREAKPOINT_NORM) goto enter;
			if(bpval&DBG_BREAKPOINT_TMP) {
																																bpval&=~DBG_BREAKPOINT_TMP;
																																goto enter;
			}
			if (bpval&DBG_BREAKPOINT_FUNC) {
			 		if (HashTableGet(HCC_ThisFileDebugger->breakAtFuncs,func)) goto enter;
						goto enter;
			}
			return;
			enter:
			DebugStep=0;
			"\nWelocme to %s:%d,%p\n",fn,ln,parBP;
			DbgEnter(func,parBP,fn,ln);

			ret:
			HCC_DebugAtLineDisable=oldDisable;
			return;
}