extern U0 printf(U8i *fmt,...);
I32i main() {
					U0 (*ptr)(U8i *fmt,...)=&printf;
					(*ptr)("Hello World\n");
					return 0;
}