U8i union U8 {
};
I8i union I8 {
};
U16i union U16 {
				I8i i8[2];
				U8i u8[2];
};
I16i union I16 {
				I8i i8[2];
				U8i u8[2];
};
U32i union U32 {
				I8i i8[4];
				U8i u8[4];
				I16i i16[2];
				U16i u16[2];
};
I32i union I32 {
				I8i i8[4];
				U8i u8[4];
				U16i u16[2];
				I16i i16[2];
};
U64i union U64 {
				I8i i8[8];
				U8i u8[8];
				I16i i16[4];
				U16i u16[4];
				I32i i32[8];
				U32i u32[8];
};
I64i union I64 {
				I8i i8[8];
				U8i u8[8];
				U16i u16[4];
				I16i i16[4];
				I32i i32[2];
				U32i u32[2];
};
