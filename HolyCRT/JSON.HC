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
						I32i type;
						JsonValue val;
};
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
							value.type=JSON_SINT;
							value.val.flt=flt;
							JsonNode *retval=malloc(sizeof(value));
							*retval=value;
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
							index+=1;
							str+=1;
							U8i toAddChr=0;
							if(*str=='\\') {
																						str+=1;
																						switch(*str) {
																						case 'e': {
																											toAddChr='\e';
																											break
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
																						str+=1;
							} else if(*str==startChr) {
									if(!run) {
											str=malloc(str-originalStr+1);
											str[str-originalStr]=0;
											run=1;
											start=originalStart;
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
static JsonNode *ParseJsonBody(U8i *str,U8i **end) {
							JsonNode *prevNode=NULL;
									I32i chrI=0;
									findName:
									for(;;chrI+=1) {
																					U8i chr=str[chrI];
																					if('z'>=chr>='a') goto findNameValid;
																					if('Z'>=chr>='A') goto findNameValid;
																					if('9'>=chr>='0') goto findNameValid;
																					if(chr=='_'||chr=='$') goto findNameValid;
																					goto findColon;
																					findNameValid:
									}
									findColon:
									if(str[chrI]!=':') return NULL;
									chrI+=1;
									I32i fltEnd=chrI,intEnd=chrI;
									F64 flt=Str2F64(&str[chrI],&fltEnd);
									I32i int=Str2I32i(&str[chrI],&intEnd);
									if(fltEnd!=chrI||intEnd!=chrI) {
																																								//Use largest item
																																								if(fltEnd>intEnd) {
																																																										prevNode=jsonNodeFltCreate(prevNode,flt);
																																								} else {
																																								                  prevNode=jsonNodeIntCreate(prevNode,int);
																																										}
									} else {
											
									}
}