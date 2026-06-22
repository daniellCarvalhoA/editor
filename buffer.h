
typedef struct buffer
{
    u8 *text;
    u32 *lines;

    u32 text_len;
    u32 num_lines;

    u32 lines_capacity;
    u32 text_capacity;
} buffer;
