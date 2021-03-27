class JsonNode;
#define JSON_STR 0
#define JSON_SINT 1
#define JSON_OBJ 2
#define JSON_FLT 3
#define JSON_ARR 4
#define JSON_SYN_ERR -1
union JsonValue {
						U8i *str;
						I32i sint;
						F64 flt;
						JsonNode **arr; //NULL terminated
						JsonNode *node;
};
class JsonNode {
						JsonNode *prev,*next,*par;
						U8i *name;
						I32i type;
						JsonValue val;
};
static I32i __JsonSkipWhitespace(U8i *str,I32i chrI);
static JsonNode *JsonNodeStrCreate(JsonNode *prev,U8i *str) {
							JsonNode value;
							value.prev=prev;
							value.par=NULL;
							value.next=NULL;
							value.type=JSON_STR;
							value.val.str=StrDupLen(str,StrLen(str));
							JsonNode *retval=malloc(sizeof(value));
							*retval=value;
							if(prev) prev->next=retval;
							return retval;
}
static JsonNode *JsonNodeIntCreate(JsonNode *prev,I32i sint) {
							JsonNode value;
							value.prev=prev;
							value.par=NULL;
							value.next=NULL;
							value.type=JSON_SINT ;
							value.val.sint=sint;
							JsonNode *retval=malloc(sizeof(value));
							*retval=value;

							if(prev) prev->next=retval;
							return retval;
}
static JsonNode *JsonNodeFltCreate(JsonNode *prev,F64 flt) {
							JsonNode value;
							value.prev=prev;
							value.par=NULL;
							value.next=NULL;
							value.type=JSON_FLT;
							value.val.flt=flt;
							JsonNode *retval=malloc(sizeof(value));
							*retval=value;

							if(prev) prev->next=retval;
							return retval;
}
static JsonNode *JsonNodeArrCreate(JsonNode *prev,I32i len,JsonNode **nodes) {
							JsonNode value;
							value.prev=prev;
							value.par=NULL;
							value.next=NULL;
							value.type=JSON_ARR;
							value.val.arr=malloc(sizeof(JsonNode*)*(len+1));
							value.val.arr[len]=NULL;
							while((len-=1)>=0)
								value.val.arr[len]=nodes[len];
							JsonNode *retval=malloc(sizeof(value));
							*retval=value;

							if(prev) prev->next=retval;
							return retval;
}
static JsonNode *JsonNodeObjCreate(JsonNode *prev,JsonNode *first) {
							JsonNode value;
							value.prev=prev;
							value.par=NULL;
							value.next=NULL;
							value.type=JSON_OBJ;
							value.val.node=first;
							JsonNode *retval=malloc(sizeof(value));
							*retval=value;

							if(prev) prev->next=retval;
							return retval;
}
static U8i *JsonParseString(U8i *str,U8i **end) {
							if(*str=='\''||*str=='\"') {
							} else {
									return NULL;
							}
							U8i *retVal=NULL;
							U8i *originalStr=str;
							U8i startChr=*str;
							I32i run=0;
							I32i index=-1;
							loop:
							if(run) index+=1;
							str+=1;
							U8i toAddChr=0;
							if(*str=='\\') {
																						str+=1;
																						switch(*str) {
																						case 'e': {
																											toAddChr='\e';
																											break;
																						}
																							case 'n': {
																												toAddChr='\n';
																												break;
																							}
																							case 'r': {
																												toAddChr='\r';
																												break;
																							}
																							case 't': {
																												toAddChr='\t';
																												break;
																							}
																							case 'v': {
																												toAddChr='\v';
																												break;
																							}
																							case '\\': {
																												toAddChr='\\';
																												break;
																							}
																							case '\'': {
																												toAddChr='\'';
																												break;
																							}
																							case '\"': {
																												toAddChr='\"';
																												break;
																							}					
																						}
							} else if(*str==startChr) {
									if(!run) {
											retVal=malloc(str-originalStr+1);
											retVal[str-originalStr]=0;
											run=1;
											
											if(end) *end=str+1;

											str=originalStr;
											goto loop;
									} else {
											goto end;
									}
							} else {
									toAddChr=*str;
							}
							
							if(run) {
															retVal[index]=toAddChr;
							}
							goto loop;
							end:

							return retVal; 							
}
static JsonNode *ParseJsonEntry(U8i *str,I32i chrI,I32i *end,JsonNode *prevNode=NULL);
static JsonNode *ParserJsonObject(U8i *str,I32i chrI,JsonNode *prevNode,I32i *end) {
							chrI=__JsonSkipWhitespace(str,chrI);
							if(0!=StrNCmp(&str[chrI],"{",1)) return NULL;
							chrI+=1;
							JsonNode *cur=NULL;
							loop:
							chrI=__JsonSkipWhitespace(str,chrI);
							if(0==StrNCmp(&str[chrI],"}",1)) {
							     chrI+=1;
												if(end) *end=chrI;
												JsonNode retval;
												retval.type=JSON_OBJ;
												retval.prev=retval.next=retval.par=NULL;
												retval.val.node=cur;
												JsonNode *alloced=malloc(sizeof(JsonNode));
												*alloced=retval;
												//printf("HELLO:%p\n",prevNode);
												alloced->prev=prevNode;
												return alloced;
							} else {
							    //printf("CUR2:%p\n",cur);
  					    JsonNode *find=ParseJsonEntry(str,chrI,&chrI,cur);
											if(!find) throw(JSON_SYN_ERR);
											cur=find;
											//printf("CUR:%p\n",cur);

											chrI=__JsonSkipWhitespace(str,chrI);
											if(str[chrI]==',') chrI+=1;
											goto loop;
       }
}
static JsonNode *ParserJsonLiteral(U8i *str,I32i chrI,JsonNode *prevNode,I32i *endI) {
							  U8i *fltEnd=&str[chrI],*intEnd=&str[chrI];
									F64 flt=Str2F64(&str[chrI],&fltEnd);
									I32i int=Str2I32i(&str[chrI],&intEnd);
									if(fltEnd!=&str[chrI]||intEnd!=&str[chrI]) {
																																								//Use largest item
																																								if(fltEnd>intEnd) {
																																																										if(endI) *endI=fltEnd-str;
																																																										return JsonNodeFltCreate(prevNode,flt);
																																								} else {
																																																										if(endI) *endI=intEnd-str;
																																								                  return  JsonNodeIntCreate(prevNode,int);
																																								}
									}
									U8i *strEnd;
									U8i *string=JsonParseString(&str[chrI],&strEnd);
									if(string) {
																				if(endI) *endI=strEnd-str;
																				prevNode=JsonNodeStrCreate(prevNode,string);
																				free(string);
																				return prevNode;
									}

									if(str[chrI]=='[') {
																												chrI+=1;
																												I32i arrLen=0;
																												JsonNode **arr=NULL;
																												I8i firstRun=1;
																												for(;;) {
																																				chrI=__JsonSkipWhitespace(str,chrI);
																																											if(str[chrI]==']') break;
																																											if(!firstRun) {
																																																									if(str[chrI]==',') {
																																																																								chrI=__JsonSkipWhitespace(str,chrI+1);
																																																																								if(str[chrI]==']') break;
																																																																								
																																																									} else
																																																													Print("Expected \',\'\n");
																																											}
																																											JsonNode *tmp=ParserJsonLiteral(str,chrI,NULL,&chrI);
																																											if(!tmp) {
																																																				Print("Expected item.\n");
																																																				break;
																																											}
																																											arr=realloc(arr,sizeof(JsonNode*)*(arrLen+=1));
																																											arr[arrLen-1]=tmp;

																																											firstRun=0;
																												}

																												chrI+=1;
																												if(endI) *endI=chrI;
																												JsonNode *y= JsonNodeArrCreate(prevNode,arrLen,arr);
																												return y;
         }
	        //Print("%p\n",prevNode);
									return ParserJsonObject(str,chrI,prevNode,endI);
}
static U0 tabPrintN(I32i tabLen) {
							while((tabLen-=1)>=0) Print("\t");
}
JsonNode *JsonNodeEntry(JsonNode *root,U8i *str) {
									if(!root) return NULL;
									for(;
									root->prev;
									root=root->prev);
									I32i strLen=StrLen(str);
									for(;root;root=root->next) {
																																				if(!root->name) goto next;
																																				I32i curLen=StrLen(root->name);
																																				printf("%li\n",curLen);
																																				if(strLen!=curLen) goto next;
																																				if(0==StrNCmp(root->name,str,curLen)) return root;
																																				next:
									}
									return NULL;
}
U0 JsonNodePrint(JsonNode *root,I32i tabLen=0) {
			switch(root->type) {
																						case JSON_STR: {
																											Print("\"%q\"",root->val.str);
																											break;
																						}
																						case JSON_SINT: {
																											Print("%d",root->val.sint);
																											break;
																						}
																						case JSON_OBJ: {
																											tabPrintN(tabLen);
																											Print("{\n");
																											JsonNode *node=root->val.node;
																											if(!node) goto objEnd;
																											for(;node->prev;node=node->prev);
																											for(;node;node=node->next) {
																																																						tabPrintN(tabLen);
																																																						Print("\"%q\":",node->name);
																																																						JsonNodePrint(node,tabLen+1);
																																																						Print(",\n");
																											}
																											objEnd:
																											tabPrintN(tabLen);
																											Print("}\n");
																											break;
																						}
																						case JSON_FLT: {
																											Print("%.6n",root->val.flt);
																											break;
																						}
																						case JSON_ARR: {
																						 				tabPrintN(tabLen);
																											Print("[\n");
																											for(I32i i=0;root->val.arr[i]!=NULL;i+=1) {
																																				JsonNodePrint(root->val.arr[i],tabLen+1);
																																				Print(",\n");
																											}
																											tabPrintN(tabLen);
																											Print("]\n");
																											break;
																						}
			}
}
static I32i __JsonSkipWhitespace(U8i *str,I32i chrI) {
							loop:
							if(str[chrI]==' '||str[chrI]=='\t'||str[chrI]=='\r'||str[chrI]=='\n') {
																		chrI+=1;
																		goto loop;
							}
							if(0==StrNCmp(str+chrI,"//",2)) {
							loop2:
																																if(str[chrI]=='\t'||str[chrI]=='\r') {
																																																									chrI+=1;
																																																									goto loop;
																																} else {
																																		chrI+=1;
																																		goto loop2;
																																}
							}
							if(0==StrNCmp(str+chrI,"/*",2)) {
																																					loop3:
																																					if(0==StrNCmp(str+chrI,"*/",2)) {
																																																									chrI+=2;
																																																									goto loop;
																																					} else {
																																												chrI+=1;
																																												goto loop3;
																																					}
							}
							return chrI;
}
static JsonNode *ParseJsonEntry(U8i *str,I32i chrI,I32i *end,JsonNode *prevNode=NULL) {
							chrI=__JsonSkipWhitespace(str,chrI);
							findName:
							I32i nameStart=chrI;
							U8i *name=NULL;
							for(;;chrI+=1) {
																						U8i chr=str[chrI];
																						if('z'>=chr>='a') goto findNameValid;
																						if('Z'>=chr>='A') goto findNameValid;
																						if('9'>=chr>='0') goto findNameValid;
																						if(chr=='_'||chr=='$') goto findNameValid;

																						name=StrDupLen(&str[nameStart],chrI-nameStart);
																						goto findColon;
																						findNameValid:
																						}
						findColon:
						if(nameStart==chrI) {
																										U8i *strEnd=NULL;
																										name=JsonParseString(&str[chrI],&strEnd);
																										chrI=strEnd-str;
						}
						//printf("name:%s\n",name);
						
						chrI=__JsonSkipWhitespace(str,chrI);
						if(str[chrI]!=':') return NULL;
						chrI=__JsonSkipWhitespace(str,chrI+1);

						JsonNode *node=ParserJsonLiteral(str,chrI,prevNode,&chrI);
						//Print("%d\n",node->prev==prevNode);
						node.name=name;
						if(end) *end=chrI;
						return node;
}