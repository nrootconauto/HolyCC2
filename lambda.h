#ifdef __clang__
#define LAMBDA_CAPTURE __block
#define LAMBDA(retType, name, ...) __auto_type name = ^retType(__VA_ARGS__)
#elif __GNUC__
#define LAMBDA_CAPTURE
#define LAMBDA(retType, name, ...) retType name(__VA_ARGS__)
#endif
