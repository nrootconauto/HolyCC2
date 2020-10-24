#pragma once
#include <stdio.h>
#if DEBUG_PRINT_ENABLE
#define DEBUG_PRINT(text, ...) printf(text, __VA_ARGS__);
#else
#define DEBUG_PRINT(text, ...) ;
#endif
