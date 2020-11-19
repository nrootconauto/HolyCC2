#include <intExponet.h>
// https://en.wikipedia.org/wiki/Exponentiation_by_squaring
int64_t intExpS(int64_t x, int64_t n) {
	if (n < 0)
		return 0;
	else if (n == 0)
		return 1;
	int64_t y = 1;
	while (n > 1) {
		if (n % 2 == 0) {
			x = x * x;
			n >>= 1;
		} else {
			y = y * y;
			x = x * x;
			n = (n - 1) / 2;
		}
	}

	return x * y;
}
uint64_t intExpU(uint64_t x, uint64_t n) {
	if (n < 0)
		return 0;
	else if (n == 0)
		return 1;
	uint64_t y = 1;
	while (n > 1) {
		if (n % 2 == 0) {
			x = x * x;
			n >>= 1;
		} else {
			y = y * y;
			x = x * x;
			n = (n - 1) / 2;
		}
	}

	return x * y;
}
