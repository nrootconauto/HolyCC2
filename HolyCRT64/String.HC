I64 StrNCmp(U8 *a, U8 *b, I32i len) {
  if (!a)
    a = "";
  if (!b)
    b = "";
  while ((len -= 1) >= 0) {
    if (*a != *b)
      return *a(I8 *) - *b(I8 *);
    a += 1;
    b += 1;
  }
  return 0;
}
U8i *StrChr(U8i *str, U8i chr) {
  while (*str != '\0') {
    if (*str == chr)
      return str;
    str += 1;
  }
  return NULL;
}
I64 StrLen(U8 *str) {
				I64 len=0;
				for (;*str!='\0';len++,str++);
				return len;
}
//https://en.cppreference.com/w/cpp/algorithm/search
U8 *StrStr(U8 *a,U8 *b) {
			U8 *aend=a+StrLen(a);
			U8 *bend=b+StrLen(b);
			for (;;a++) {
							U8 *ait=a;
							for (U8 *bit=b,;;ait++,bit++) {
											if(bit==bend) return a;
											if(ait==aend) return NULL;
											if(*ait!=*bit) break;
							}
			}
}
U8i *StrDupLen(U8i *str, I64 len) {
  U8i *retVal = malloc(len + 1);
  retVal[len] = '\0';
  while ((len -= 1) >= 0)
    retVal[len] = str[len];
  return retVal;
}
