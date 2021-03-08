extern U0 printf(U8i *fmt,...);
extern U0 puts(U8i *chr);
extern I32i sscanf(U8i *fmt,...);
#define NULL (0)
extern U0 *malloc(I32i size); 
import U0 free(U0 *ptr);
extern U0 *realloc(U0 *ptr,I32i size);

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
_import  log10 F64 Log10(F64 num);
static U0 __PrintF64(F64 f,F64 e,F64 p,I32i digits) {
							F64 value=f*2.0`(e-p);
							if(value==0)
							{
								printf("0.0");
								return ;
							}

							F64 R=__PrintF64Shift(f,Max(0.0,e-p));
							F64 S=__PrintF64Shift(1.0,Max(0.0,-(e-p)));
							printf("%lf,%lf\n",R/S,value);

							F64 mA=__PrintF64Shift(1.0,Max(0.0,e-p));
							F64 mB=mA;

							//printf("%lf,%lf,%lf\n",mA/S,mB/S,2.0`(e-p));
							if(floor(f)==__PrintF64Shift(1.0,p-1)) {
																											mA=__PrintF64Shift(mA,1.0);
																											R=__PrintF64Shift(R,1.0);
																											S=__PrintF64Shift(S,1.0);
							}
							I32i k=0;
							while(R<ceil(S/10)) {
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
															R=(R*10.0)%S;
															mA*=10.0;
															mB*=10.0;

															printf("S:%lf\n",S);
															printf("R:%lf\n",R);
															printf("mA:%lf\n",mA);
															
															if(firstRun) {
																												printf("%lf\n",U);
																												printf(".\n");
																												firstRun=0;
															} else {
																	printf("%lf\n",U);
															}


															if((digits-=1)<0)
																break;
							}
							printf("e%lf\n",floor(Log10(value)));
}
class I64Pair {
						U32i a,b;
};
F64 fild64(I64Pair pair) {
				asm {
								IMPORT pair;
								FILD U64[pair]
								LEAVE
								RET
				}
}
static U0 __callPrintF64(F64 f,I32i digits) {
							I64Pair buffer;
							*((&buffer)(F64*))=f;
							//0x7FF is 11 bits wide 
							I32i exp=0x7ff&(buffer.b>>(64-1-11-32));
							buffer.b|=1<<(52-32);
							//0x1FFFFF is 21 bits wide
							buffer.b&=0x1fffff;
							F64 base=fild64(buffer);
							exp-=1023;
							__PrintF64(base,exp(F64),52.0,digits);
}
I32i main() {
					__callPrintF64(123.0,2);
					return 1;
}