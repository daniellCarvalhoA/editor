
static b32 buffers_are_equal(const buffer *a, const buffer *b)
{
    // TODO: See disaseembly to see if this gets vecotrized.
    b32 result = (a->text_len == b->text_len) && (a->num_lines == b->num_lines);

    for (u32 i = 0; i < a->text_len; ++i)
    {
        result = result && (a->text[i] == b->text[i]);
    }
    for (u32 i = 0; i < a->num_lines; ++i)
    {
        result = result && (a->lines[i] == b->lines[i]);
    }
    return result;
}

static void copy_buffer(const buffer *src, buffer *dst)
{
    memcpy(&dst->text,  &src->text,  src->text_len  * sizeof(u8));
    memcpy(&dst->lines, &src->lines, src->num_lines * sizeof(u32));
}

static b32 strictly_increasing(const u32 *buffer, const u32 count)
{
    u32 result = true;

    if (count > 1)
    {
        for (u32 i = 1, prev = 0; i < count; ++i)
        {
            u32 value = buffer[i];
            result &= (value > prev);
            prev = value;
        }
    }
    return result;
}

// static buffer *get_buffer(buffer_type type, const segment

// static buffer *get_buffer(piece_list *list, const segmented_node *node, const u32 piece_index)
// {
//     switch (node->b_types[piece_index])
//     {
//         case BufferType_Append:
//         {
//             return &list->append;
//         } break;
//         case BufferType_Original:
//         {
//             return  &list->original;
//         } break;
//     }
//     return NULL;
// }
//
static inline const buffer *get_buffer(const piece_list *list, const buffer_type type)
{
    // TODO: This might be called loads of times while rendering or writing to a file,
    // Turn this if into an addition, put the Append buffer right after Original in memory, 
    // and just add the buffer_type to the original to get the buffer you want.
    //
    // Morever, this branch is not predictable at all while rendering. Fow
    switch (type)
    {
        case BufferType_Append:
        {
            return &list->append;

        } break;
        case BufferType_Original:
        {
            return &list->original;
        } break;
    }
    return NULL;
}

static inline u32 line_offset(const buffer *buffer, const piece piece, const u32 line)
{
    u32 result = 0;
    if ((piece.lcnt != 0) && (line > 0))
    {
        result = buffer->lines[piece.off.row + line] - (buffer->lines[piece.off.row] + piece.off.col);
    }
    return result;
}

static offset search_piece(const buffer *buffer, const piece piece, const u32 delta)
{
    u32 start = buffer->lines[piece.off.row] + piece.off.col;

    u32 off = start + delta;
    u32 low = piece.off.row;
    u32 high = piece.off.row + piece.lcnt;

    u32 mid_line = 0;
    u32 mid_abs = 0;

    while (low <= high)
    {
        mid_line = low + ((high - low) / 2);
        mid_abs = buffer->lines[mid_line];

        if (mid_line == high)
        {
            break;
        }

        u32 mid_abs_next = buffer->lines[mid_line + 1];

        if (off < mid_abs)
        {
            high = mid_line - 1;
        }
        else if (off >= mid_abs_next)
        {
            low = mid_line + 1;
        }
        else
        {
            break;
        }
    }

    offset result = { .row = mid_line, .col = off - mid_abs };
    return result;
}

#if TESTS
static buffer rand_ascii_buffer(memory_arena *arena, prng *prng)
{
    buffer buf = {};
    buf.text_len = rand_u8_inclusive(prng, 40);
    buf.text     = PushArray(arena, buf.text_len, u8, default_arena_params());

    u32 lines[40] = {};
    u32 num_lines = 1;

    ratio ratio = init_ratio(1, 10);

    for (u32 i = 0; i < buf.text_len; ++i) 
    {
        if (chance(prng, ratio))
        {
            buf.text[i] = '\n';
            lines[num_lines++] = i + 1;
        }
        else
        {
            u8 c = rand_range_u8_inclusive(prng, 32, 126);
            buf.text[i] = c;
        }
    }

    buf.num_lines = num_lines;
    buf.lines = PushArray(arena, buf.num_lines, u32, default_arena_params());
    memcpy(buf.lines, lines, sizeof(u32) * num_lines);

    return buf;
}
#endif

