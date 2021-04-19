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
