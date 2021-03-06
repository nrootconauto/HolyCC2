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
static U0 __PrintF64(F64 f,F64 e,F64 p) {
							F64 value=f*2.0`(e-p);
							if(value==0)
							{
								printf("0.0");
								return ;
							}

							F64 R=__PrintF64Shift(f,Max(0,e-p));
							F64 S=__PrintF64Shift(1.0,Max(0,-(e-p)));
							printf("%lf,%lf\n",R/S,value);

							F64 mA=__PrintF64Shift(1.0,Max(0,e-p));
							F64 mB=mA;

							printf("%lf,%lf,%lf\n",mA/S,mB/S,2.0`(e-p));
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
																											printf("%lf,%lf,%lf\n",R,mA,mB);
							}
							while((2.0*R)+mA>=2.0*S) {
																			k+=1;
																			S*=10;
																			printf("%lf,%lf,%li\n",R,S,k);
							}
							for(;;) {
															k-=1;
															F64 U=floor((R*10.0)/S);
															R=(R*10)%S;
															mA*=10.0;
															mB*=10.0;
															U8i low=2.0*R<mA;
															U8i high=2.0*R>=(2*S)-mB;
															if(!(!low&&!high))
																break;
															printf("%lf",U);
							}
}
I32i main() {
					__PrintF64(123.0,0.0,0.0);
					return 1;
}