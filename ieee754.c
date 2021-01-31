#include <math.h>
#include <stdint.h>
static long double digits[64];
__attribute__((constructor)) static void init() {
	long double digit = 1.0;
	for (long i = 0; i != 64; i++) {
		digit /= 2.0;
		digits[i] = digit;
	}
}
uint64_t IEEE754Encode(double value) {
	long double base2Exp = floorl(log2l(value));
	long double exponet2Removed = value / powl(2, base2Exp + 1); // +1 makes it so exponet is before base-2 decimal point
	uint64_t fraction = 0;
	for (long i = 0; i != 53; i++) {
		if (exponet2Removed >= digits[i]) {
			exponet2Removed -= digits[i];
			uint64_t shifted = 1;
			shifted <<= (53 - i - 1);
			fraction |= shifted;
		}
	}
	// 1st bit is implicit,so clear
	fraction &= ~(1ll << 52);

	uint64_t exponet2 = (((1 << 10) - 1) + floorl(base2Exp));
	uint64_t sign = (value < 0.0) ? 1 : 0;
	return (sign << 63) | ((exponet2 & 0x7ff) << 52) | fraction;
}
