extern U0 printf(U8i *fmt,...);
extern U0 puts(U8i *chr);
extern I32i sscanf(U8i *fmt,...);
#define NULL (0)

F64 Max(F64 a,F64 b)
{
		if(a>b)
			return a;
		return b;
}
extern F64 floor(F64 f);
extern F64 ceil(F64 f);
static F64 __PrintF64Shift(F64 a, F64 b) {
							return floor(a*2.0`b);
}
_extern putchar U0 PutC(U8i chr);
_import  log10 F64 Log10(F64 num);
static U8i *__PrintI32i(I32i value,I32i base,U8i *Digits) {
			I32i digits=0;
			I32i value2=value;
			do {
						digits+=1;
						value/=base;
			} while(value!=0);
			value=value2;
			
			U8i buffer[digits+1];
			buffer[digits]='\0';
			I32i bufferSize=digits;
			do {
						digits-=1;
						buffer[digits]=Digits[value%base];
						value/=base;
			} while(value!=0);

			return StrDupLen(buffer,bufferSize+1);
}

//http://kurtstephens.com/files/p372-steele.pdf
static U8i *__PrintF64(F64 f,F64 e,F64 p,I32i digits) {
							U8i * retval=calloc(128+digits,1);
							I32i retvalPos=0;
							F64 value=f*2.0`(e-p);
							if(value==0)
							{
								StrCpy(retval,"0.0");
								return retval;
							}

							//printf("%lf\n",e-p);
							F64 R=__PrintF64Shift(f,Max(0.0,e-p));
							//printf("%lf\n",R);
							F64 S=__PrintF64Shift(1.0,Max(0.0,-(e-p)));
							//printf("%lf\n",S);
							//printf("%lf,%lf\n",R/S,value);

							F64 mA=__PrintF64Shift(1.0,Max(0.0,e-p));
							F64 mB=mA;
							//printf("%lf,%lf,%lf\n",mA,mB,e-p);
							//printf("%lf,%lf,%lf\n",mA/S,mB/S,2.0`(e-p));
							//printf("p-1:%lf\n",p-1);
							//printf("FLOO:%lf,%lf\n",floor(f),ceil(__PrintF64Shift(1.0,p-1.0)));
							if(floor(f)==floor(__PrintF64Shift(1.0,p-1))) {
																																																					//																																					printf("HERE\n");
																											mA=__PrintF64Shift(mA,1.0);
																											R=__PrintF64Shift(R,1.0);
																											S=__PrintF64Shift(S,1.0);
							}
							I32i k=0;
							//printf("%lf\n",R);
							//printf("%lf,%lf\n",S,S/10.0);
							while(R<ceil(S/10)) {
																											//printf("%lf=%lf\n",(R/S)*10.0`(k(F64)),value);
																											k-=1;
																											R*=10.0;
																											mA*=10.0;
																											mB*=10.0;
																											//printf("%lf,%lf,%lf\n",R,mA,mB);
							}
							while((2.0*R)+mA>=2.0*S) {
																			k+=1;
																			S*=10;
																			//printf("%lf,%lf,%li\n",R,S,k);
							}
							for(I32i firstRun=1;;) {
															k-=1;
															F64 U=floor((R*10.0)/S);
															R=(R*10)%S;
															mA*=10.0;
															mB*=10.0;
															
															if((digits-=1)<0)
																break;


																U8i low=2.0*R<mA;
																U8i high=2.0*R>=(2*S)-mB;
																if(!(!low&&!high)) {
																																			if(low&&!high) {
																																																		retval[retvalPos]="0123456789"[U(I32i)];
																																																		retvalPos+=1;
																																			}
																																			else if(!low&&high) {
																																																		retval[retvalPos]="0123456789"[(U+1.0)(I32i)];
																																																		retvalPos+=1;
																																			}
																																			else if(low&&high) {
																																								if(2*R>=S) {
																																																		retval[retvalPos]="0123456789"[(U+1.0)(I32i)];
																																																		retvalPos+=1;
																																								} else {
																																																		retval[retvalPos]="0123456789"[U(I32i)];
																																																		retvalPos+=1;
																																								}
																																			}
																																			break;
																																			}																																		
																																			

																if(firstRun) {
																												retval[retvalPos]="0123456789"[U(I32i)];
																												retvalPos+=1;
																												retval[retvalPos]='.';
																												retvalPos+=1;
																												firstRun=0;
																} else {
																												retval[retvalPos]="0123456789"[U(I32i)];
																												retvalPos+=1;
																}

															
							}

							while((digits-=1)>=0) {
																													retval[retvalPos]='0';
																													retvalPos+=1;
							}
							retval[retvalPos]='e';
							retvalPos+=1;
							U8i *powStr=__PrintI32i(floor(Log10(value))(I32i),10,"0123456789");
							StrCpy(&retval[retvalPos],powStr);
							free(powStr);

							return retval;
}
class I64Pair {
						U32i a,b;
};
static F64 fild64(I64Pair pair) {
				asm {
								IMPORT pair;
								FILD U64[pair]
				}
}
static U8i *__callPrintF64(F64 f,I32i digits) {
							I64Pair buffer;
							*((&buffer)(F64*))=f;
							//0x7FF is 11 bits wide 
							I32i exp=0x7ff&(buffer.b>>(64-1-11-32));
							buffer.b|=1<<(52-32);
							//0x1FFFFF is 21 bits wide
							buffer.b&=0x1fffff;
							F64 base=fild64(buffer);
							exp-=1023;
							return __PrintF64(base,exp(F64),52.0,digits);
}
static U8i *__UnescapeStr(U8i *str) {
							I32i resSize=1;
							I32i len=StrLen(str);
							I32i run=0;

							U8i *retVal;
							loop2:
							I32i retValLen=0;
							for(I32i chr=0;chr!=len;chr+=1) {
																switch(str[chr]) {
																																	case '\n':
																																						if(run) {
																																														retVal[retValLen]='\\';
																																														retVal[retValLen+1]='n';
																																														}
																																						retValLen+=2;
																																						break;
																																	case '\'':
																																						if(run) {
																																														retVal[retValLen]='\\';
																																														retVal[retValLen+1]='\'';
																																														}
																																						retValLen+=2;
																																						break;
																																	case '\"':
																																						if(run) {
																																														retVal[retValLen]='\\';
																																														retVal[retValLen+1]='\"';
																																														}
																																						retValLen+=2;
																																						break;
																																	case '\e':
																																						if(run) {
																																														retVal[retValLen]='\\';
																																														retVal[retValLen+1]='e';
																																														}
																																						retValLen+=2;
																																						break;
																																	case '\f':
																																						if(run) {
																																														retVal[retValLen]='\\';
																																														retVal[retValLen+1]='f';
																																														}
																																						retValLen+=2;
																																						break;
																																	case '\r':
																																						if(run) {
																																														retVal[retValLen]='\\';
																																														retVal[retValLen+1]='r';
																																														}
																																						retValLen+=2;
																																						break;
																																	case '\t':
																																						if(run) {
																																														retVal[retValLen]='\\';
																																														retVal[retValLen+1]='t';
																																														}
																																						retValLen+=2;
																																						break;
																																	case '\v':
																																						if(run) {
																																														retVal[retValLen]='\\';
																																														retVal[retValLen+1]='v';
																																														}
																																						retValLen+=2;
																																						break;
																																	case '\\':
																																						if(run) {
																																														retVal[retValLen]='\\';
																																														retVal[retValLen+1]='\\';
																																														}
																																						retValLen+=2;
																																						break;
																																						default:
																																						if(run) {
																																														retVal[retValLen]=str[chr];
																																														}
																																						retValLen+=1;
																																						break;
																}
							}
							if(!run) {
																run=1;
																retVal=malloc(retValLen+1);
																retVal[retValLen]='\0';
																goto loop2;
							}
							return retVal;
}
U0 __Print(U0(*putChar)(U8i,U0 *),U0 *data,U8i *fmt,I32i *argv) {
			U8i *str=fmt;
			I32i argi=0;
			for(;;) {
											U8i *where=StrChr(str,'%');
											if(where==NULL) {
																											for(;*str!='\0';str+=1) putChar(*str,data);
																											break;
											}
											for(;str!=where;str+=1) putChar(*str,data);
											str+=1;
											I8i signFlag=0;
											if(*str=='-') {
																									signFlag=1;
																									str+=1;
											}
											I8i zeroFlag=0;
											if(*str=='-') {
																									zeroFlag=1;
																									str+=1;
											}
											I32i width=Str2I32i(str,&str);
											I32i decimals=0;
											if(*str=='.') {
																									str+=1;
																								decimals=Str2I32i(str,&str);
											}
											//
											// TODO implement flags and aux.
											//
											switch(*str) {
																								case 'q': {
																													U8i *str=__UnescapeStr(argv[argi]);
																													for(I32i c=0;c!=StrLen(str);c+=1) putChar(str[c],data);
																													free(str);
																													break;
																								}
																								case 'n': {
																													F64 *tmp=(&argv[argi])(F64);
																													F64 val=*tmp;
																													U8i *str=__callPrintF64(val,decimals);
																													I32i len=StrLen(str);
																													for(I32i c=0;c!=len;c+=1) {
																																						putChar(str[c],data);
																													}
																													free(str);

																													argi+=2;
																													break;
																								}
																								case 'C': {
																													I32i upper=ToUpper(argv[argi]);
																													while(upper) {
																																										putChar(upper&0xff,data);
																																										upper>>=8;
																													};
																													argi+=1;
																													break;
																								}
																								case 'c': {
																													I32i chr=argv[argi];
																													while(chr) {
																																										putChar(chr&0xff,data);
																																										chr>>=8;
																													};
																													argi+=1;
																													break;
																								}
																								case 'd': {
																													U8i *str=__PrintI32i(argv[argi],10,"0123456789");
																													I32i len=StrLen(str);
																													for(I32i c=0;c!=len;c+=1) {
																																						putChar(str[c],data);
																													}
																													free(str);
																													argi+=1;
																													break;
																								}
																								case 'x': {
																													U8i *str=__PrintI32i(argv[argi],16,"0123456789abcdef");
																													I32i len=StrLen(str);
																													for(I32i c=0;c!=len;c+=1) {
																																						putChar(str[c],data);
																													}
																													free(str);
																													argi+=1;
																													break;
																								}
																								case 'p':
																								case 'X': {
																													U8i *str=__PrintI32i(argv[argi],16,"0123456789ABCDEF");
																													I32i len=StrLen(str);
																													for(I32i c=0;c!=len;c+=1) {
																																						putChar(str[c],data);
																													}
																													free(str);
																													argi+=1;
																													break;
																								}
											}
											str+=1;
			}
}
static U0 __putChar2(U8i chr,U0 *data) {
							PutC(chr);
}
U0 Print(U8i *fmt,...) {
			__Print(&__putChar2,NULL,fmt,argv);
}
static U0 __printAdd1(U8i chr,U0 *data) {
							*(data(I32i*))+=1;
}
static U0 __printAddChr(U8i chr,U0 *data) {
							**(data(U8i**))=chr;
							*(data(U8i**))+=1;
}
U8i *MStrPrint(U8i *fmt,...) {
				I32i count=0;
				__Print(&__printAdd1,&count,fmt,argv);
				U8i *retval=malloc(count+1);
				retval[count]='\0';
				U8i *ptr=retval;
				__Print(&__printAddChr,&ptr,fmt,argv);
				return retval;
}