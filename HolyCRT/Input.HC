extern I32i scanf(U8i *str);
#define INPUT_BUFF_SIZE (1024)
I32i GetI32i() {
					U8 buffer[INPUT_BUFF_SIZE];
					scanf("%s",buffer);
					return Str2I32i(buffer);
}