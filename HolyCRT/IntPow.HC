//https://stackoverflow.com/questions/101439/the-most-efficient-way-to-implement-an-integer-based-power-function-powint-int
I32i PowI32i(I32i base,I32i exp) {
					I32i result=1;
					for(;;) {
													if(exp&1) result*=base;
													exp>>=1;
													if(!exp) break;
														base*=base;
					}
					return result;
}