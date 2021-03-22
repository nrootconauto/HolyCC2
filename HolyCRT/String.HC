#define NULL (0)
U8i *StrChr(U8i *str,U8i chr) {
				 do {
																						if(*str==chr) return str;
																						str+=1;
				} while(*str!='\0');
				return NULL;
}
I32i StrLen(U8i *str) {
					I32i len=0;
				 for(;*str!='\0';str+=1) {
																							len+=1;
					}
				return len;
}
U0 StrCpy(U8i *dst,U8i *src) {
			I32i len=StrLen(src);
			dst[len]='\0';
			while((len-=1)>=0) {
																				dst[len]=src[len];
			}
}
U8i *StrDupLen(U8i *str,I32i len) {
				U8i *retVal=malloc(len+1);
				retVal[len]='\0';
				while((len-=1)>=0) retVal[len]=str[len];
				return retVal;
}
I32i StrNCmp(U8i *a,U8i *b,I32i len) {
					while((len-=1)>=0) {
																						if(*a!=*b) return *a-*b;
																						a+=1;
																						b+=1;
					}
					return 0;
} 
I32i ToUpper(I32i Chr) {
					I32i retVal=0;
					while(Chr!=0) {
																			I8i chr=Chr&0xff;
																if('z'>=chr&&chr>='a') {
																																							chr-='a';
																																							chr+='A';
																}
																retVal<<=8;
																retVal|=chr;
					}
					return retVal;
}
I32i Str2I32i(U8i *str,U8i **end=NULL) {
					I32i retVal=0;
					I32i mult=1;
					if(*str=='-') {
																			mult=-1;
																			str+=1;
					}
					loop:
					switch(*str) {
														case '0'...'9':
														retVal*=10;
														retVal+=*str-'0';
														str+=1;
														goto loop;
														default:
														break;
					}

					if(end) *end=str;
					return retVal*mult;
}
F64 Str2F64(U8i *str,U8i **end=NULL) {
				I32i expPresent=0,dotPresent=0;
				I32i exp=0;
				I32i decimalOffset=0;
				F64 current=0.0;
				I32i chr=0;
				for(;;chr+=1) {
													switch(str[chr]) {
																														case '0'...'9':
																														current*=10.0;
																														current+=(str[chr]-'0')(F64);
																														if(dotPresent) exp-=1;
																														break;
																														
																														case '.':
																														if(dotPresent) goto fail;
																														dotPresent=1;
																														break;

																														case 'e':
																														chr+=1;
																														if(expPresent) goto fail;
																														expPresent=1;
																														U8i *endPtr;
																														exp+=Str2I32i(&str[chr],&endPtr);
																														chr=endPtr-str;
																														goto end;

																														default:
																														goto end;
 												}
				}
				end:
				if(end) *end=&str[chr];
				current*=10.0`(exp)(F64);
				return current;

				fail:
				if(end) *end=&str[chr];
				return -0.0;
}