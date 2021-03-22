class JsonNode;
#define JSON_STR 0
#define JSON_SINT 1
#define JSON_NODE 2
#define JSON_FLT 3
#define JSON_ARR 4
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
static JsonNode *JsonNodeStrCreate(JsonNode *prev,U8i *str) {
							JsonNode value;
							value.prev=prev;
							value.par=NULL;
							value.next=NULL;
							value.type=JSON_STR;
							value.val.str=str;
							JsonNode *retval=malloc(sizeof(value));
							*retval=value;
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
							
							printf("T:%li,%li\n",retval->type,value.type);
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
static JsonNode *ParserJsonLiteral(U8i *str,I32i chrI,JsonNode *prevNode,I32i *endI) {
							  U8i *fltEnd=&str[chrI],*intEnd=&str[chrI];
									F64 flt=Str2F64(&str[chrI],&fltEnd);
									I32i int=Str2I32i(&str[chrI],&intEnd);
									printf("%li\n",fltEnd-&str[chrI]);
									printf("%li\n",intEnd-&str[chrI]);
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
									I32i strEnd=chrI;
									U8i *string=JsonParseString(&str[chrI],&strEnd);
									if(string) {
																				if(endI) *endI=strEnd;
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
																																											if(str[chrI]==']') break;
																																											if(!firstRun) {
																																																									if(str[chrI]==',')
																																																																								chrI+=1;				
																																																									else
																																																													Print("Expected \',\'\n");
																																											}
																																											JsonNode *tmp=ParserJsonLiteral(str,chrI,NULL,&chrI);
																																											if(!tmp) {
																																																				Print("Expected item.\n");
																																																				break;
																																											}
																																											printf("f:%li\n",chrI);
																																											arr=realloc(arr,sizeof(JsonNode*)*(arrLen+=1));
																																											arr[arrLen-1]=tmp;

																																											firstRun=0;
																												}

																												chrI+=1;
																												if(endI) *endI=chrI;
																												JsonNode *y= JsonNodeArrCreate(prevNode,arrLen,arr);
																												printf("t:%li\n",y->type);;
																												return y;
}

									if(str[chrI]=='{') {																												
																												for(;str[chrI]!='}';) {
																																		ParseJsonEntry(str,chrI,&chrI);
																																		}
																												chrI+=1;						
									}
}
static U0 tabPrintN(I32i tabLen) {
							while((tabLen-=1)>=0) printf("\t");
}
U0 JsonNodePrint(JsonNode *root,I32i tabLen=0) {
			tabPrintN(tabLen);
			printf("t:%li\n",root->type);
			switch(root->type) {
																						case JSON_STR: {
																											Print("\"%q\";\n",root->val.str);
																											break;
																						}
																						case JSON_SINT: {
																											Print("%d;\n",root->val.sint);
																											break;
																						}
																						case JSON_NODE: {
																											break;
																						}
																						case JSON_FLT: {
																											Print("%.6n",root->val.flt);
																											break;
																						}
																						case JSON_ARR: {
																											printf("[\n");
																											for(I32i i=0;root->val.arr[i]!=NULL;i+=1)
																																				JsonNodePrint(root->val.arr[i],tabLen+1);
																											tabPrintN(tabLen);
																											printf("]\n");
																											break;
																						}
			}
}
static I32i __JsonSkipWhitespace(U8i *str,I32i chrI) {
							loop:
							if(str[chrI]==' '||str[chrI]=='\t'||str[chrI]=='\r') {
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
							JsonNode *prevNode=NULL;
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
						printf("JSON_NAME:%s\n",name);

						chrI=__JsonSkipWhitespace(str,chrI);
						if(str[chrI]!=':') return NULL;
						chrI=__JsonSkipWhitespace(str,chrI+1);

						JsonNode *node=ParserJsonLiteral(str,chrI,prevNode,&chrI);
						node.name=name;
						if(end) *end=chrI;
						return node;
}