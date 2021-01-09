#include <stdint.h>
#include <math.h>
static long double digits[64];
__attribute__((constructor)) static void init()  {
		long double digit=1.0;
		for(long i=0;i!=64;i++){
				digits[i]=digit;
				digit/=2.0;
		}
}
void IEEE754Encode(double value,uint64_t *writeTo) {
		long double base2Exp=floorl(log2l(value));
		long double exponet2Removed=value/powl(2,base2Exp+1);// +1 makes it so exponet is before base-2 decimal point
		uint64_t fraction=0;
		for(long i=0;i!=53;i++) {
				if(exponet2Removed<=digits[i]) {
						exponet2Removed-=digits[i];
						uint64_t shifted=1;
						shifted<<=(53-i);
						fraction|=shifted;
				}
		}
		//1st bit is implicit,so clear
		fraction&=~(1ll<<53);
				
		uint64_t exponet2=(16383+floorl(base2Exp));
		int64_t sign=(value<0.0)?1:0;
		return (sign<<63)|((exponet2<<53)&0x7ff)|fraction;
}
