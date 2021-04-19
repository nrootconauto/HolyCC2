extern U0 *calloc(I64 len,I64 n);
class JsonNode;
#define JSON_STR 0
#define JSON_SINT 1
#define JSON_OBJ 2
#define JSON_FLT 3
#define JSON_ARR 4
#define JSON_SYN_ERR -1
class CJsonNode;
union UJsonValue {
			U8i *str;
			I64i sint;
			F64 flt;
			CJsonNode **arr;
			CJsonNode *obj;
};
class CJsonNode {
			CJsonNode *prev,*next;
			U8 *name;
			I64 type;
			UJsonValue value;
};
static I64 __JsonParseNumber(U8 *str,U8 **end=0) {
							I64 mult=1;
							if (*str=='-') {
										mult=-1;
										str++;
							}
							U8 *digits="0123456789";
							U8 *digits2="0123456789";
							I64 base=10;
							if (0==StrNCmp(str,"0x",2)) {
										str+=2;
										digits="0123456789ABCDEF";
										digits2="0123456789abcdef";
 										base=16;
							} else if(0==StrNCmp(str,"0b",2)) {
									str+=2;
									digits="01";
									digits2="01";
									base=2;
							} else if(str[0]=='0') {
									digits="01234567";
									digits2="01234567";
									base=8;
							} 
							I64 retval=0;
							for (;;) {
											I64 digitvalue=0;
											U8i *digit=StrChr(digits,*str);
											if(digit) {digitvalue=digit-digits;goto find;}

											if(!digit) digit=StrChr(digits2,*str);
											if(digit) {digitvalue=digit-digits2;goto find;}
											
											if(!digit) break;
											find:
											retval*=base;
											retval+=digitvalue;
											str++;
							}
							if(end) *end=str;
							return retval*mult;
}
static CJsonNode *JsonParseNumber(U8 *str,U8 **end=0) {
							U8 *end2=NULL;
							I64 num=__JsonParseNumber(str,&end2);
							if(str==end2) return NULL;
							CJsonNode *retval=calloc(sizeof(CJsonNode),1);
							retval->type=JSON_SINT;
							retval->value.sint=num;
							if(end) *end=end2;
							return retval;
}
static U8 *__JsonParseString(U8 *str,U8 **end=0) {
							U8 run=0;
							U8 firstchr=*str;
							if (firstchr!='\''&&firstchr!='"') {
										if(end) *end=str;
										return 0
							}
							U8 *origstr=++str;
							U8 *retval=0;
							I64 len=0;
							loop:
							U8 toaddchr=0;
							if(*str=='\0') throw(JSON_SYN_ERR);
							if (*str=='\\') {
										str++;
										switch (*str) {
																	case 'a': toaddchr='\a'; goto add;
																	case 'b': toaddchr='\b'; goto add;
																	case 'f': toaddchr='\f'; goto add;
																	case 'n': toaddchr='\n'; goto add;
																	case 't': toaddchr='\t'; goto add;
																	case 'v': toaddchr='\v'; goto add;
																	case '\\': toaddchr='\\'; goto add;
																	case '\'': toaddchr='\''; goto add;
																	case '\"': toaddchr='"'; goto add;
										}
							} else if(*str==firstchr) {
									if (run) {
												if(end) *end=str+1;
												return retval;
									}
									retval=calloc(len+1,1);
									run=1;
									len=0;
									str=origstr;
									goto loop;
							}
							toaddchr=*str;
							str++;
							add:
							if (run) {
										retval[len++]=toaddchr;
							} else len++;
							goto loop;
}
static CJsonNode *JsonParseString(U8 *str,U8 **end=0) {
							U8 *end2=NULL;
							U8 *str2=__JsonParseString(str,&end2);
							if(str==end2) return NULL;
							CJsonNode *retval=calloc(sizeof(CJsonNode),1);
							retval->type=JSON_STR;
							retval->value.str=str2;

							if(end) *end=end2;
							return retval;
}
static U8 *__JsonParserSkipWhitespace(U8 *str) {
							loop:
							U8 *white=" \t\n\r";
							if (StrChr(white,*str)) {
										str++;
										goto loop;
							}
							if (0==StrNCmp(str,"//",2))  {
										str=StrChr(str+2,'\n');
										if(!str) return str+StrLen(str);
										goto loop;
							}
							if (0==StrNCmp(str,"/*",2)) {
										str=StrStr(str,"*/");
										if(!str) throw(JSON_SYN_ERR);
										str+=2;
										goto loop;
							}
							return str;
}
static CJsonNode *JsonNodeParse(U8 *str,U8 **end);
static CJsonNode *JsonParseArray(U8 *str,U8 **end) {
							str=__JsonParserSkipWhitespace(str);
							if (*str!='[') {
										if(end) *end=str;
										return NULL;
							}
							I64 arrlen=0;
							str++;
							CJsonNode **arr=NULL;
							loop:
							str=__JsonParserSkipWhitespace(str);
							if (*str==']') {
										if(end) *end=str+1;
										arr=realloc(arr,++arrlen*sizeof(CJsonNode*));
										arr[arrlen-1]=NULL;
										CJsonNode *retval=calloc(sizeof(CJsonNode),1);
										retval->type=JSON_ARR;
										retval->value.arr=arr;
										return retval;
							}
							CJsonNode *node=JsonNodeParse(str,&str);
							if(!node) throw (JSON_SYN_ERR);
							arr=realloc(arr,++arrlen*sizeof(CJsonNode*));
							arr[arrlen-1]=node;
							str=__JsonParserSkipWhitespace(str);
							if (*str==',') {
										str++;
										goto loop;
							} else if(*str!=']') {
									throw(JSON_SYN_ERR);
							} else goto loop;
}
static U8 *__JsonParseName(U8 *str,U8 **end=NULL) {
							U8 *orig=NULL;
							loop:
							switch (str) {
														case 'a'...'z':
														case 'A'...'Z':
														case '0'...'9':
														case '_':
														str++;
														goto loop;
														default: break;
							}
							if(str==orig) return NULL;
							if(end) *end=str;
							return StrDupLen(orig,str-orig);
}
static CJsonNode *JsonParseObject(U8 *str,U8 **end=NULL) {
							str=__JsonParserSkipWhitespace(str);
							if (*str!='{') {
										if(end) *end=str;
										return NULL;
							} else str++;
							CJsonNode *curr=NULL;
							loop:
							str=__JsonParserSkipWhitespace(str);
							if (*str=='}') {
										CJsonNode *retval=malloc(sizeof(CJsonNode));
										retval->type=JSON_OBJ;
										retval->value.obj=curr;
										return retval;
							}
							U8 *name=__JsonParseString(str,&str);
							if(name) goto found;
							name=__JsonParseName(str,&str);
							if(!name) throw(JSON_SYN_ERR);
							found:
							CJsonNode *find=JsonNodeParse(str,&str);
							if(!find) throw(JSON_SYN_ERR);
							find->name=name;
							find->prev=curr;
							curr->next=find;
							curr=find;
							str=__JsonParserSkipWhitespace(str);
							if(*str==',') str++;
							goto loop;
}
static CJsonNode *JsonNodeParse(U8 *str,U8 **end=NULL) {
							CJsonNode *retval=NULL;
							retval=JsonParseArray(str,end);
							if(retval) return retval;
							retval=JsonParseString(str,end);
							if(retval) return retval;
							retval=JsonParseNumber(str,end);
							if(retval) return retval;
							return retval;
}
static U0 PrintNTabs(I64 tabs) {
							U8 fill[tabs+1];
							fill[tabs]='\0';
							while(--tabs>=0) fill[tabs]='\t';
							printf("%s",fill);
}
static U0 JsonPrint(CJsonNode *node,I64 tabc=0) {
							if(node->name) printf("%s:",node->name);
							I64 i=0;
							switch (node->type) {
														case JSON_STR:
																			PrintNTabs(tabc);
																			printf("\"%s\"",node->value.str);
																			break;
														case JSON_SINT:
																			PrintNTabs(tabc);
																			printf("%li",node->value.sint);
																			break;
														case JSON_OBJ:
																			PrintNTabs(tabc);
																			printf("{");
																			CJsonNode *currnode=node->value.obj;
																			if (currnode) {
																						for(currnode=node->value.obj->prev;currnode;currnode=currnode->prev) JsonPrint(currnode,tabc+1);
																						JsonPrint(currnode,tabc+1);
																						for(currnode=node->value.obj->next;currnode;currnode=currnode->next) JsonPrint(currnode,tabc+1);
																			}
																			PrintNTabs(tabc);
																			printf("}");
																			break;
														case JSON_FLT:
																			PrintNTabs(tabc);
																			printf("%lf",node->value.flt);
																			break;
														case JSON_ARR:
																				PrintNTabs(tabc);
																				printf("[\n");
																				for (i=0;node->value.arr[i];i++) {
																								JsonPrint(node->value.arr[i],tabc+1);
																								printf(",\n");
																				}
																				PrintNTabs(tabc);
																				printf("]\n");
																				break;
							}
}