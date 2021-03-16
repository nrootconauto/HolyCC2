_import pow F64 PowF64(F64 b,F64 e);
#include "IntPow.HC"
#include "ExceptLin.HC"
#include "String.HC"
#include "Print.HC"
static I32i foo() {
							Print("1\n");
							return 1;
}
U0 testNoDest() {
			Print("No-Dest\n");
			I32i a=10;
			I32i b=3;
			(foo()+a)`(b+b);
			(foo()+a)<<(b+b);
			(foo()+a)>>(b+b);
			(foo()+a)*(b+b);
			(foo()+a)/(b+b);
			(foo()+a)%(b+b);
			(foo()+a)^(b+b);
			(foo()+a)|(b+b);
			+(foo()+a);
			-(foo()+a);
			(foo()+a)<(b+b);
			(foo()+a)>(b+b);
			(foo()+a)>=(b+b);
			(foo()+a)<=(b+b);
			(foo()+a)==(b+b);
			(foo()+a)==(b+b);
}
U0 testRegsI32i() {
			printf("I32i\n");
			I32i reg EAX a=10;
			I32i reg EDX b=3;
			printf("%li\n",a`b==1000);
			printf("%li\n",a<<b==80);
			printf("%li\n",a>>b==1);
			printf("%li\n",a*b==30);
			printf("%li\n",a/b==3);
			printf("%li\n",a%b==1);
			printf("%li\n",a&b==2);
			printf("%li\n",a|b==11);
			printf("%li\n",+a==10);
			printf("%li\n",-a==-10);
			printf("%li\n",a<b==0);
			printf("%li\n",a>b==1);
			printf("%li\n",a<=b==0);
			printf("%li\n",a>=b==1);
			printf("%li\n",(a==b)==0);
			printf("%li\n",(a!=b)==1);

			for(I32i i=0;i!=2;i+=1) { 
												a=1;
												b=0;
												printf("%li\n",(a&&b)==0);
												printf("%li\n",(a||b)==1);
												printf("%li\n",(a^^b)==1);

												I32i tmp=a;
												a=b;
												b=tmp;
			}
			a=b=1;
			printf("%li\n",(a&&b)==1);
			printf("%li\n",(a||b)==1);
			printf("%li\n",(a^^b)==0);

			a=b=0;
			printf("%li\n",(a&&b)==0);
			printf("%li\n",(a||b)==0);
			printf("%li\n",(a^^b)==0);
}
U0 testRegsI16i() {
			printf("I16i\n");
			I16i reg AX a=10;
			I16i reg DX b=3;
			printf("%li\n",a`b==1000);
			printf("%li\n",a<<b==80);
			printf("%li\n",a>>b==1);
			printf("%li\n",a*b==30);
			printf("%li\n",a/b==3);
			printf("%li\n",a%b==1);
			printf("%li\n",a&b==2);
			printf("%li\n",a|b==11);
			printf("%li\n",+a==10);
			printf("%li\n",-a==-10);
			printf("%li\n",a<b==0);
			printf("%li\n",a>b==1);
			printf("%li\n",a<=b==0);
			printf("%li\n",a>=b==1);
			printf("%li\n",(a==b)==0);
			printf("%li\n",(a!=b)==1);

			for(I32i i=0;i!=2;i+=1) { 
												a=1;
												b=0;
												printf("%li\n",(a&&b)==0);
												printf("%li\n",(a||b)==1);
												printf("%li\n",(a^^b)==1);

												I32i tmp=a;
												a=b;
												b=tmp;
			}
			a=b=1;
			printf("%li\n",(a&&b)==1);
			printf("%li\n",(a||b)==1);
			printf("%li\n",(a^^b)==0);

			a=b=0;
			printf("%li\n",(a&&b)==0);
			printf("%li\n",(a||b)==0);
			printf("%li\n",(a^^b)==0);
}
U0 testRegsI8i() {
			printf("I8i\n");
			I8i reg AH a=2;
			I8i reg AL b=4;
			printf("%li\n",a`b==16);
			printf("%li\n",a<<b==32);
			printf("%li\n",a>>b==0);
			printf("%li\n",a*b==8);
			printf("%li\n",a/b==0);
			printf("%li\n",a%b==2);
			printf("%li\n",a&b==0);
			printf("%li\n",a|b==6);
			printf("%li\n",+a==2);
			printf("%li\n",-a==-2);
			printf("%li\n",a<b==1);
			printf("%li\n",a>b==0);
			printf("%li\n",a<=b==1);
			printf("%li\n",a>=b==0);
			printf("%li\n",(a==b)==0);
			printf("%li\n",(a!=b)==1);

			for(I32i i=0;i!=2;i+=1) { 
												a=1;
												b=0;
												printf("%li\n",(a&&b)==0);
												printf("%li\n",(a||b)==1);
												printf("%li\n",(a^^b)==1);

												I32i tmp=a;
												a=b;
												b=tmp;
			}
			a=b=1;
			printf("%li\n",(a&&b)==1);
			printf("%li\n",(a||b)==1);
			printf("%li\n",(a^^b)==0);

			a=b=0;
			printf("%li\n",(a&&b)==0);
			printf("%li\n",(a||b)==0);
			printf("%li\n",(a^^b)==0);
}
U0 testRegsCast() {
			I32i reg EAX reg32=1;
			I16i reg BX reg16=1;
			I8i reg CL reg8=1;
			do {
						I32i tmp32=reg16(I32i);
						printf("16->32 %li\n",tmp32==1);
						tmp32=reg8(I32i);
						printf("8->32 %li\n",tmp32==1);

						I16i tmp16=reg32(I32i);
						printf("32->16 %li\n",tmp16==1);
						tmp16=reg8(I16i);
						printf("8->16 %li\n",tmp16==1);

						I8i tmp8=reg32(I8i);
						printf("32->8 %li\n",tmp8==1);
						tmp8=reg16(I8i);
						printf("16->8 %li\n",tmp8==1);

						printf("8->F64 %li\n",tmp8(F64)==1.0);
						printf("16->F64 %li\n",tmp16(F64)==1.0);
						printf("32->F64 %li\n",tmp32(F64)==1.0);

						F64 f=2.3;
						printf("F64->8 %li\n",f(I8i)==2);
						printf("F64->16 %li\n",f(I16i)==2);
						printf("F64->32 %li\n",f(I32i)==2);
} while(0);
}
U0 testRange() {
			printf("3>2>1>0:%li\n",3>2>1>0);
}
I32i main() {
					U8i *str=__UnescapeStr("Hello World\n");
					printf("%s\n",str);
try {
				throw(1);
				} catch {
					testRegsI32i();
					testRegsI16i();
					testRegsI8i();
					testRegsCast();
					testNoDest();
					testRange();
					}

					"Hello Wolrd:%.6n\n",Str2F64("225.5e10",NULL);
					return 0;
}
Print("Hello World,%d\n",123);
