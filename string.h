
#include <utf8proc.h>

typedef struct string
{
    u32 len;
    u32 capacity;
    u8 *buffer;
} string;

static inline string allocate_string(u32 size)
{
    string result = 
    {
        .len = 0,
        .capacity = size,
        .buffer = (u8 *) malloc(sizeof(u8) * size)
    };
    return result;
}


typedef struct str
{
    u32 len;
    u8 *buffer;
} str;

static inline str Str(u8 *buffer, u32 size);

#define STR_LIT(s) Str((u8 *) (s), sizeof(s) - 1)


// #define str8_lit(S)  str8((U8*)(S), sizeof(S) - 1)
// #define str8_lit_comp(S) {(U8*)(S), sizeof(S) - 1,}
// #define str8_lit_cstr(S) str8((U8*)(S), sizeof(S))
// #define str8_varg(S) (int)((S).size), ((S).str)
// 
// #define str8_array(S,C) str8((U8*)(S), sizeof(*(S))*(C))
// #define str8_array_fixed(S) str8((U8*)(S), sizeof(S))
// #define str8_struct(S) str8((U8*)(S), sizeof(*(S)))
// 
// internal String8  str8(U8 *str, U64 size);
// internal String8  str8_range(U8 *first, U8 *one_past_last);
// internal String8  str8_zero(void);
// internal String16 str16(U16 *str, U64 size);
// internal String16 str16_range(U16 *first, U16 *one_past_last);
// internal String16 str16_zero(void);
// internal String32 str32(U32 *str, U64 size);
// internal String32 str32_range(U32 *first, U32 *one_past_last);
// internal String32 str32_zero(void);
// internal String8  str8_cstring(char *c);
// internal String16 str16_cstring(U16 *c);
// internal String32 str32_cstring(U32 *c);
// internal String8  str8_cstring_capped(void *cstr, void *cap);
// internal String16 str16_cstring_capped(void *cstr, void *cap);

static inline str from_string(string s)
{
    str result = { .buffer = s.buffer, .len = s.len };
    return result;
}

static inline u32 count_lines(str s)
{
    // TODO: Include all 'line' codepoints.
    // TODO: Use SIMD
    u32 result = 0;
    for (u32 i = 0; i < s.len; ++i)
    {
        if (s.buffer[i] == '\n')
        {
            result++;
        }
    }
    return result;
}

static inline i32 utf8_prev_codepoint(u8 *buffer, u32 pos, utf8proc_int32_t *cp) {

    if (pos == 0)
    {
        return -1;
    }

    size_t start = pos - 1;

    // Walk back over continuation bytes.
    while (start > 0 && (buffer[start] & 0xC0) == 0x80)
    {
        start--;
    }

    utf8proc_ssize_t len = utf8proc_iterate(buffer + start, pos - start, cp);

    if (len < 0)
    {
        Assert(!"Handle invalid unicode!");
    }

    return start;
}

static inline str get_grapheme_backward(u8 *buffer, u32 cursor)
{
    u32 end = cursor;
    u32 start = end;

    utf8proc_int32_t next_cp;
    utf8proc_int32_t cp;

    int state = 0;
    bool first = true;

    while (start > 0) 
    {
        i32 prev = utf8_prev_codepoint(buffer, start, &cp);

        if (prev < 0)
        {
            break;
        }

        if (!first) {
            if (utf8proc_grapheme_break_stateful(cp, next_cp, &state))
                break;
        }

        first = false;
        next_cp = cp;
        start = prev;
    }

    str result = { end - start, buffer + start };
    return result;
}


static inline u32 last_col(str s)
{
    u32 result = 0;
    while (s.len > 0)
    {
        str prev = get_grapheme_backward(s.buffer, s.len);

        if (prev.buffer[0] == '\n')
        {
            break;
        }

        result++;
        s.len -= prev.len;
    }
    return result;
}

