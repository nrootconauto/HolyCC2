#include <ieee754.h>
#include <assert.h>
void IEEE754Tests() {
		uint64_t enc=IEEE754Encode(8.125);
		assert(enc==0x4020400000000000ll);
}
