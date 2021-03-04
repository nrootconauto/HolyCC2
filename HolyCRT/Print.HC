extern U0 printf(U8i *fmt,...);
extern I32i sscanf(U8i *fmt,...);
#define NULL (0)
static I32i Str2I32i(U8i *fmt,I32i *consumed) {
							I32i value=0;
							I32i *original=fmt;
							for(;;fmt=fmt+1) {
															switch(*fmt) {
																												default: goto b;
																												case '0'...'9':
																																	value*=10;
																																	value+=*fmt-'0';
																																	goto next;
}
															b: break;
															next:
							}
							if(consumed) *consumed=fmt-original;
							return value;
}
U0 PutChars(U32i chr) {
			while(chr!=0) {
																	printf("%c",chr&0xff);
																	chr>>=8;
			}
}
U32i ToUpper(U32i chr) {
   U32i retVal=0;
			while(chr!=0) {
																	U8i letter=chr&0xff;
																	if('a'<=letter<='z') letter+='A'-'a';
																	retVal<<=8;
																	retVal|=letter;
																	
																	chr>>=8;
			}
			return retVal;
}
extern U0 *malloc(I32i size); 
import U0 free(U0 *ptr);
extern U0 *realloc(U0 *ptr,I32i size);
U8i *I32_2Str(I32i value) {
				I32i digits=0;
				for(I32i value2=value;value2!=0;value2/=10) digits+=1;
				if(digits==0) digits=1;

				U8i *retVal=malloc(digits+1);
				retVal[digits]='\0';
				for(I32i value2=value;value2!=0;value2/=10) retVal[digits-=1]=(value2%10)+'0';

				return retVal;
}
U0 Print(U8i *fmt,...) {
			I32i chr=0;
			I32i argi=0;
			for(;fmt[chr]!='\0';chr=chr+1) {
											if(fmt[chr]!='%') goto next;
											
											chr=chr+1;
											
											U8i sign=fmt[chr]=='-';
											if(sign) chr=chr+1;
											
											I32i lPad=fmt[chr]=='0';
											if(lPad) chr=chr+1;

											I32i consumed;
											I32i padWidth=Str2I32i(&fmt[chr],&consumed);
											chr+=consumed;
											if(consumed) padWidth=0;

											I32i decimals=0;
											if(fmt[chr]=='.') {
																													chr=chr+1;
																													I32i consumed;
																													decimals=Str2I32i(&fmt[chr],&consumed);
																													chr+=consumed;
											}

											I32i aux=1;
											if(fmt[chr]=='h') {
																													chr=chr+1;
																													I32i consumed;
																													aux=Str2I32i(&fmt[chr],&consumed);
																													chr+=consumed;
											}

											switch(fmt[chr]) {
																												case 'n':
																												case 'd': {
																																	U8i *text=I32_2Str(argv[argi]);
																																	argi+=1;
																																	printf("%s",text);
																																	break;
																												}
																												case 'u':
																												case 'C': PutChars(ToUpper(argv[argi])); argi+=1; break;
																												case 'c': PutChars(argv[argi]); argi+=1; break;
																												case 'p': printf("%p",argv[argi]); argi+=1; break;
																												case 'q':
																												case 'Q':
											}
											next:
			}
}