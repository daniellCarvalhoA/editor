
#define Paste_(A,B) A##B
#define Paste(A,B) Paste_(A,B)
#define StaticAssert(C, ID) static U8 Paste(ID, __LINE__)[(C)?1:-1]
