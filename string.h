
#include <utf8proc.h>

const u8 utf8_len_table[] = {
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

typedef struct string
{
    u32 len;
    u32 capacity;
    u8 *buffer;
} string;

typedef struct str
{
    u32 len;
    u8 *buffer;
} str;

static inline str from_string(string s)
{
    str result = { .buffer = s.buffer, .len = s.len };
    return result;
}

static inline u32 count_lines(str s)
{
    // TODO: Include all 'line' codpoints.
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


// This calculates the grapheme length;
static inline u32 utf8_charlen_unchecked(const u8 *const str, u32 size)
{
    u8 c = (u8)(*str);
    if (c < 0x80)  //&& str[1] < 0x80)
    {
        return 1; // ASCII
    }

    // u32 prev_len = 0;
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

