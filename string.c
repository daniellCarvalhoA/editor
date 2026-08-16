
static inline str Str(u8 *buffer, u32 size)
{
    str result = { .buffer = buffer, .len = size };
    return result;
}

static inline void copy_to(str src, str *dst)
{
    dst->len = src.len;
    dst->buffer = realloc(dst->buffer, sizeof(u8) * dst->len);
    memcpy(dst->buffer, src.buffer, sizeof(u8) * dst->len);
}


static const u8 utf8_len_table[] = {
    // 1  2  3  4  5  6  7  8  9  A  B  C  D  E  F
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // 0
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // 1
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // 2
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // 3
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // 4
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // 5
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // 6
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // 7
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 8
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 9
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // A
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // B
    0, 0, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, // C
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, // D
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, // E
    4, 4, 4, 4, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // F
};


// This calculates the grapheme length;
static inline u32 utf8_charlen_unchecked(const u8 *const str, u32 size)
{
    u8 c = (u8)(*str);
    if (c < 0x80)  //&& str[1] < 0x80)
    {
        return 1; // ASCII
    }

    utf8proc_int32_t state = 0;

    utf8proc_int32_t prev_codepoint;
    utf8proc_ssize_t len = utf8proc_iterate(str, size, &prev_codepoint);
    while (len < size)
    {
        utf8proc_int32_t next_codepoint;
        utf8proc_iterate(str + len, size - len, &next_codepoint);

        if (str[len] < 0x80 || 
            utf8proc_grapheme_break_stateful(prev_codepoint, next_codepoint, &state))
        {
            return len;
        }

        len += utf8_len_table[str[len]];
        prev_codepoint = next_codepoint;
    }
    return len;
}


