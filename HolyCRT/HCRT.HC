_import pow F64 PowF64(F64 b,F64 e);
#include "IntPow.HC"
#include "ExceptLin.HC"
#include "String.HC"
#include "Print.HC"
#include "JSON.HC"
#include "HashTable.HC"
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
			I32i reg EBX a=10;
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
			I16i reg DX a=10;
			I16i reg BX b=3;
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
U0 testRegsCast() {
			I32i reg EDX reg32=1;
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
I32i end;
					ParserJsonLiteral("123",0,NULL,&end);
					printf("%li\n",end);
					U8i *str=__UnescapeStr("Hello World\n");
					printf("%s\n",str);
try {
				throw(1);
				} catch {
					testRegsI32i();
					testRegsI16i();
					//testRegsI8i();
					testRegsCast();
					testNoDest();
					testRange();
					}
					printf("%lf\n",52.0-1);
					printf("%lf\n",52-1.0);
					printf("%lf\n",(52+1.0)-(1.0+1.0));
					"Hello Wolrd:%.6n\n",15.500;
					"Hello Wolrd:%.6n\n",15.500;
					U8i *str="ABC:[1,\"2\\n2\",3]";
					I32i end;
					JsonNode *node=ParseJsonEntry(str,0,&end);
					JsonNodePrint(node);
					printf("%li\n",end);
					U8i *values[99+1]={
					"0",
					"1",
					"2",
					"3",
					"4",
					"5",
					"6",
					"7",
					"8",
					"9",
					"10",
					"11",
					"12",
					"13",
					"14",
					"15",
					"16",
					"17",
					"18",
					"19",
					"20",
					"21",
					"22",
					"23",
					"24",
					"25",
					"26",
					"27",
					"28",
					"29",
					"30",
					"31",
					"32",
					"33",
					"34",
					"35",
					"36",
					"37",
					"38",
					"39",
					"40",
					"41",
					"42",
					"43",
					"44",
					"45",
					"46",
					"47",
					"48",
					"49",
					"50",
					"51",
					"52",
					"53",
					"54",
					"55",
					"56",
					"57",
					"58",
					"59",
					"60",
					"61",
					"62",
					"63",
					"64",
					"65",
					"66",
					"67",
					"68",
					"69",
					"70",
					"71",
					"72",
					"73",
					"74",
					"75",
					"76",
					"77",
					"78",
					"79",
					"80",
					"81",
					"82",
					"83",
					"84",
					"85",
					"86",
					"87",
					"88",
					"89",
					"90",
					"91",
					"92",
					"93",
					"94",
					"95",
					"96",
					"97",
					"98",
					"99"
};
					I32i values2[100+1];
					CHashTable *table=HashTableCreate();
					for(I32i i=0;i!=100;i+=1) {
														values2[i]=i;
														HashTableAdd(table,&values2[i],values[i]);
					}
					for(I32i i=0;i!=100;i++) {
														Print("%d\n",*HashTableGet(table,values[i])(I32i*));
					}
					HashTableDestroy(table);
					return 0;
}