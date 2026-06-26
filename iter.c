// API.
//
//  type: base_iter
//      - set_position()/set_line() => sets iteration mode, for now we have by line or by position.
//      - base_init() initializes the iterator.
//      - base_next()/base_prev() moves the iterator by a single character forwards/backwards,
//      - base_advance_by/base_advance_rev(count) moves the iterator by *count* characters forwards/backwards.
//      - base_next_line/base_prev_line() moves the iterator by a single line_forwardsbackwards.
//      - base_advance_by_line/base_advance_rev_by_line(count) moves the iterator by *count* lines 
//      forwards/backwards.
//      - get_char() gets the character at the current position.
//      - get_position() returns the current position.
//      - get_line() returns the current line.
//
//


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


// static i32 prev_codepoint(const u8 *s, size_t len, size_t pos, size_t *start)
// {
//     Assert(pos > 0);
//     ssize_t i = (ssize_t) pos - 1;
//
//     while ((i >= 0) && (s[i] & 0xC0) == 0x80)
//     {
//         i--;
//     }
//
//     i32 codepoint;
//     i32 rc = utf8proc_iterate(s + i, (i32) (len - i), &codepoint);
//
//     *start = (size_t) i;
//     return codepoint;
//
//
// }
//
static u32 utf8_charlen_unchecked(const u8 *const str, u32 size)
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


static inline b32 base_init_(piece_list *list, iter_type type, base_iter *iter)
{
    iter->list = list;
    iter->node = list->root_sentinel.next;
    iter->node_pos = 0;
    iter->node_line = 0;
    iter->piece_pos = 0;
    iter->piece_line = 0;
    iter->pos_in_piece = 0;
    iter->line_in_piece = 0;
    iter->piece_idx = 0;
    iter->abs_idx = 0;
    iter->type = type;
    b32 result = iter->list->size > 0; 
    return result;
}


static inline piece *get_piece_(base_iter *iter)
{
    piece *result = (iter->piece_idx < iter->node->count) ? iter->node->pieces + iter->piece_idx : 0;
    return result;
}

static inline buffer_type *get_type_(base_iter *iter) 
{
    buffer_type *type = iter->node->b_types + iter->piece_idx;
    return type;
}

static inline u32 get_position_from_line_unsafe(base_iter *iter, piece piece)
{
    buffer_type *type = get_type_(iter);
    const buffer *buffer = get_buffer_2(iter->list, *type);
    u32 result = line_offset(buffer, piece, iter->line_in_piece);
    return result;
}

static inline u32 get_position_from_line(base_iter *iter)
{
    u32 result = 0;
    piece *piece = get_piece_(iter);
    if (piece)
    {
        result = get_position_from_line_unsafe(iter, *piece);
    }
    return result;
}

static inline u32 get_line_from_position(base_iter *iter)
{
    u32 result = 0;
    piece *piece = get_piece_(iter);
    if (piece)
    {
        buffer_type *type = get_type_(iter);
        const buffer *buffer = get_buffer_2(iter->list, *type);
        result = search_piece(buffer, *piece, iter->pos_in_piece).row - piece->off.row;
    }
    return result;
}

static inline offset get_offset_from_position(base_iter *iter)
{
    buffer_type *type    = get_type_(iter);
    piece *piece         = get_piece_(iter);
    Assert(piece);
    const buffer *buffer = get_buffer_2(iter->list, *type);
    offset result        = search_piece(buffer, *piece, iter->pos_in_piece);
    return result;
}

static inline u32 position(base_iter *iter)
{
    // Assert(iter->type & Position);
    u32 result = iter->node_pos + iter->piece_pos + iter->pos_in_piece;
    return result;
}

static inline u32 line_number(base_iter *iter)
{
    Assert(iter->type & LineNumber);
    u32 result = iter->node_line + iter->piece_line + iter->line_in_piece;
    return result;
}

static inline base_iter base_init(piece_list *list, iter_type type)
{
    base_iter result = { 
        .list = list,
        .node = &list->root_sentinel,
        .pos_in_piece = UINT32_MAX,
        .line_in_piece = UINT32_MAX,
        .type = type,
    };
    return result;
}

static inline b32 base_prev_pos(base_iter *iter)
{
    iter->type &= ~LineNumber;
    iter->type |= ~Position;
    if (position(iter) == 0)
    {
        return false;
    }

    if (iter->piece_pos + iter->pos_in_piece == 0)
    {
        iter->node = iter->node->prev;
        iter->abs_idx      -= iter->node->count;
        iter->node_line    -= iter->node->lcnt;
        iter->node_pos     -= iter->node->size;
        iter->piece_idx     = iter->node->count - 1;
        iter->piece_line    = iter->node->lcnt - iter->node->pieces[iter->piece_idx].lcnt;
        iter->piece_pos     = iter->node->size - iter->node->pieces[iter->piece_idx].size;
        iter->pos_in_piece  = iter->node->pieces[iter->piece_idx].size;
    }

    if (iter->pos_in_piece == 0)
    {
        iter->piece_idx--;
        iter->abs_idx--;
        iter->piece_line   -= iter->node->pieces[iter->piece_idx].lcnt;
        iter->piece_pos    -= iter->node->pieces[iter->piece_idx].size;
        iter->pos_in_piece  = iter->node->pieces[iter->piece_idx].size;
    }

    iter->pos_in_piece--;
    return true;
}

static inline b32 base_prev_line(base_iter *iter)
{
    iter->type &= ~Position;
    iter->type |= LineNumber;
    if (line_number(iter) == 0)
    {
        return false;
    }

    iter->line_in_piece--;
    while (iter->node->prev != &iter->list->root_sentinel && iter->piece_line + iter->line_in_piece == 0) 
    {
        iter->node = iter->node->prev;
        iter->abs_idx      -= iter->node->count;
        iter->node_line    -= iter->node->lcnt;
        iter->node_pos     -= iter->node->size;
        iter->piece_idx     = iter->node->count - 1;
        iter->piece_line    = iter->node->lcnt - iter->node->pieces[iter->piece_idx].lcnt;
        iter->piece_pos     = iter->node->size - iter->node->pieces[iter->piece_idx].size;
        iter->line_in_piece = iter->node->pieces[iter->piece_idx].lcnt;
    }

    while (iter->piece_idx > 0 && iter->line_in_piece == 0)
    {
        iter->piece_idx--;
        iter->abs_idx--;
        iter->piece_line   -= iter->node->pieces[iter->piece_idx].lcnt;
        iter->piece_pos    -= iter->node->pieces[iter->piece_idx].size;
        iter->line_in_piece = iter->node->pieces[iter->piece_idx].lcnt;
    }

    return true;
}


static inline b32 base_init_rev_(piece_list *list, iter_type type, base_iter *iter)
{
    b32 result = list->size > 0;
    if (result)
    {
        iter->list = list;
        iter->node          = list->root_sentinel.prev;
        iter->abs_idx       = list->num_pieces - 1;
        iter->node_line     = list->lcnt - iter->node->lcnt;
        iter->node_pos      = list->size - iter->node->size;
        iter->piece_idx     = iter->node->count - 1;
        iter->piece_line    = iter->node->lcnt - iter->node->pieces[iter->piece_idx].lcnt;
        iter->piece_pos     = iter->node->size - iter->node->pieces[iter->piece_idx].size;
        iter->type = type;

        if (type & Position)
        {
            iter->pos_in_piece = iter->node->pieces[iter->piece_idx].size - 1;
        }
        else if (type & LineNumber)
        {
            while (iter->node->prev != &list->root_sentinel && iter->node->lcnt == 0)
            {
                iter->node       = iter->node->prev;
                iter->abs_idx   -= iter->node->count;
                iter->piece_idx  = iter->node->count - 1;
                iter->node_line -= iter->node->lcnt;
                iter->node_pos  -= iter->node->size;
                iter->piece_line = iter->node->lcnt - iter->node->pieces[iter->piece_idx].lcnt;
                iter->piece_pos  = iter->node->size - iter->node->pieces[iter->piece_idx].size;
            }

            while (iter->piece_idx > 0 && iter->node->pieces[iter->piece_idx].lcnt == 0)
            {
                iter->piece_idx--;
                iter->abs_idx--;
                iter->piece_line -= iter->node->pieces[iter->piece_idx].lcnt;
                iter->piece_pos  -= iter->node->pieces[iter->piece_idx].size;
            }
            iter->line_in_piece = iter->node->pieces[iter->piece_idx].lcnt;
        }
    }
    return result;
}


static inline b32 base_advance_by(base_iter *iter, u32 count)
{
    if (iter->abs_idx == iter->list->num_pieces)
    {
        return false;
    }

    while (count > iter->node->count - iter->piece_idx)
    {
        count -= iter->node->count - iter->piece_idx;
        iter->abs_idx   += iter->node->count;
        iter->node_pos  += iter->node->size;
        iter->node_line += iter->node->lcnt;
        iter->node = iter->node->next;
        iter->piece_idx = 0;
    }

    while (count > 0)
    {
        count--;
        iter->piece_pos  += iter->node->pieces[iter->piece_idx].size;
        iter->piece_line += iter->node->pieces[iter->piece_idx].lcnt;
        iter->piece_idx++;
        iter->abs_idx++;
    }

    return true;
}


static inline u32 get_row_from_line(base_iter *iter)
{
    piece *piece = get_piece_(iter);
    u32 result = piece->off.row + iter->line_in_piece;
    return result;
}

static inline u32 get_position(base_iter *iter)
{
    if (iter->type & Position)
    {
        u32 result = iter->node_pos + iter->piece_pos + iter->pos_in_piece;
        return result;
    }

    if (iter->type & LineNumber)
    {
        iter->type |= Position;
        iter->pos_in_piece = get_position_from_line(iter);
    }

    u32 result = iter->node_pos + iter->piece_pos + iter->pos_in_piece;
    return result;
}

static inline u32 get_line_number(base_iter *iter)
{
    if (iter->type & LineNumber)
    {
        u32 result = iter->node_line + iter->piece_line + iter->line_in_piece;
        return result;
    }

    if (iter->type & Position)
    {
        iter->type |= LineNumber;
        iter->line_in_piece = get_line_from_position(iter);
    } 
    
    u32 result = iter->node_line + iter->piece_line + iter->line_in_piece;
    return result;
}

static inline offset get_offset(base_iter *iter)
{
    offset result;
    if (iter->type & LineNumber)
    {
        piece *piece = get_piece_(iter);
        u32 row = piece->off.row + iter->line_in_piece;
        result.row = row;
        if (iter->type & Position)
        {
            buffer_type *type    = get_type_(iter);
            const buffer *buffer = get_buffer_2(iter->list, *type);
            result.col = (iter->line_in_piece) ?
                iter->pos_in_piece - line_offset(buffer, *piece, iter->line_in_piece) :
                iter->pos_in_piece + piece->off.col;
        }
    }
    else if (iter->type & Position)
    {
        result = get_offset_from_position(iter);
    }

    return result;
}

typedef enum
{
    Over,
    Row,
    NewLine
} row_result;

static inline row_result base_next_row(base_iter *iter, u8 *row, u32 *row_size)
{
    Assert(iter->type & LineNumber);
    Assert(iter->type & Position);

    row_result result = Row;

    if (position(iter) >= iter->list->size)
    {
        return Over;
    }

    u32 size = 0;

    for(;;)
    {
        piece piece = iter->node->pieces[iter->piece_idx];
        buffer_type type = iter->node->b_types[iter->piece_idx];
        const buffer *buffer = get_buffer_2(iter->list, type);
        u32 piece_offset = buffer->lines[piece.off.row] + piece.off.col;
        u32 start_offset = piece_offset + iter->pos_in_piece;

        if (iter->line_in_piece < piece.lcnt)
        {
            u32 end_offset       = piece_offset + line_offset(buffer, piece, iter->line_in_piece + 1);
            u32 line_len         = end_offset - start_offset - 1;

            if (line_len > *row_size)
            {
                memcpy(row, buffer->text + start_offset, *row_size);
                size += *row_size;
                iter->pos_in_piece += *row_size;
                result = Row;
            }
            else
            {
                memcpy(row, buffer->text + start_offset, line_len);
                size += line_len;
                iter->pos_in_piece += line_len + 1;
                iter->line_in_piece++;
                result = NewLine;
            }

            break;
        }

        u32 remaining = piece.size - iter->pos_in_piece;

        if (remaining > *row_size || position(iter) + remaining == iter->list->size)
        {
            u32 amount = Minimum(*row_size, remaining);
            memcpy(row, buffer->text + start_offset, amount);
            size += amount;
            iter->pos_in_piece += amount;
            result = Row;
            break;
        }

        memcpy(row, buffer->text + start_offset, remaining);
        size += remaining;
        iter->pos_in_piece  = 0;
        iter->line_in_piece = 0;
        iter->abs_idx++;

        if (iter->piece_idx + 1 == iter->node->count)
        {
            iter->piece_idx = 0;
            iter->node_pos += iter->node->size;
            iter->node_line += iter->node->lcnt;
            iter->piece_pos  = 0;
            iter->piece_line = 0;
            iter->node = iter->node->next;
        }
        else
        {
            iter->piece_idx++;
            iter->piece_pos  += piece.size;
            iter->piece_line += piece.lcnt;
        }
        *row_size -= remaining;
        row += remaining;
    }

    *row_size = size;
    return result;
}

static inline void fix_iter(base_iter *iter) 
{
    if (iter->pos_in_piece + iter->piece_pos >= iter->node->size)
    {
        iter->abs_idx   += iter->node->count - iter->piece_idx;
        iter->node_line += iter->node->lcnt;
        iter->node_pos  += iter->node->size;
        iter->piece_idx  = iter->piece_line = iter->piece_pos = iter->pos_in_piece = iter->line_in_piece = 0 ;
        iter->node       = iter->node->next;
    } 
    else if (iter->pos_in_piece >= iter->node->pieces[iter->piece_idx].size)
    {
        iter->piece_pos  += iter->node->pieces[iter->piece_idx].size;
        iter->piece_line += iter->node->pieces[iter->piece_idx].lcnt;
        iter->pos_in_piece = iter->line_in_piece = 0;
        iter->piece_idx++;
        iter->abs_idx++;
    }
}
static inline cell_item base_next_cell(base_iter *iter)
{
    cell_item result = {};

    if (get_position(iter) == iter->list->size)
    {
        result.valid = false;
    } 
    else
    {
        if (iter->pos_in_piece + iter->piece_pos >= iter->node->size)
        {
            iter->abs_idx   += iter->node->count;
            iter->node_line += iter->node->lcnt;
            iter->node_pos  += iter->node->size;
            iter->piece_idx  = iter->piece_line = iter->piece_pos = iter->pos_in_piece = 0;
            iter->node       = iter->node->next;
        } 
        else if (iter->pos_in_piece >= iter->node->pieces[iter->piece_idx].size)
        {
            iter->piece_pos  += iter->node->pieces[iter->piece_idx].size;
            iter->piece_line += iter->node->pieces[iter->piece_idx].lcnt;
            iter->pos_in_piece = iter->line_in_piece = 0;
            iter->piece_idx++;
            iter->abs_idx++;
        }

        piece piece = iter->node->pieces[iter->piece_idx];
        buffer_type type = iter->node->b_types[iter->piece_idx];

        const buffer *buffer = get_buffer_2(iter->list, type);
        u32 piece_offset = buffer->lines[piece.off.row] + piece.off.col;
        const u8 *cell_start   = buffer->text + piece_offset + iter->pos_in_piece;
        u32 cell_len = utf8_charlen_unchecked(cell_start, piece.size - iter->piece_pos);
        
        result.valid = true;  
        result.len   = cell_len;
        if (cell_len <= 4)
        {
            memcpy(&result.cell, cell_start, cell_len);
        }
        else
        {
            result.cell = cell_from_buffer_index(piece_offset, type).value;
        }
        iter->line_in_piece += (*cell_start == '\n');
        iter->pos_in_piece += cell_len;
    }
    return result;
}


static inline void get_grid(
    base_iter *iter,
    u32 *grid,
    u32 width,
    u32 height,
    line *line)
{
    // if (get_position(iter) >= iter->list->size)
    // {
    //     return;
    // }

    u32 *cursor = grid;
    u32 row = 0;
    u32 row_size = 0;
    line->num_rows = 1;
    u32 start_height = line->start;

    while (row < height)
    {
        cell_item item = base_next_cell(iter);
        if (!item.valid)
        {
            break;
        }
        if ((u8) item.cell == '\n')
        {
            row++;
            cursor = grid + row * width;
            row_size = 0;
            start_height += line->num_rows;
            ++line;
            line->num_rows = 1;
            line->start = start_height;
        } 
        else if (row_size == width)
        {
            ++line->num_rows;
            cursor = grid + row * width;
            row++;
            row_size = 0;
        }
        else
        {
            *cursor++ = item.cell; 
            row_size++;
        }
    }
}


static inline b32 base_advance_pos_by(base_iter *iter, u32 count)
{
    iter->type &= ~LineNumber;
    if (position(iter) >= iter->list->size)
    {
        return false;
    }

    while (count > iter->node->size - (iter->pos_in_piece + iter->piece_pos))
    {
        count -= iter->node->size - (iter->pos_in_piece + iter->piece_pos);
        iter->abs_idx   += iter->node->count - iter->piece_idx;
        iter->node_line += iter->node->lcnt;
        iter->node_pos  += iter->node->size;
        iter->piece_idx = iter->piece_line = iter->piece_pos = iter->pos_in_piece = 0;
        iter->node  = iter->node->next;
    }

    while (count > iter->node->pieces[iter->piece_idx].size - iter->pos_in_piece)
    {
        count -= iter->node->pieces[iter->piece_idx].size - iter->pos_in_piece;
        iter->piece_pos    += iter->node->pieces[iter->piece_idx].size;
        iter->piece_line   += iter->node->pieces[iter->piece_idx].lcnt;
        iter->pos_in_piece = 0;
        iter->piece_idx++; 
        iter->abs_idx++;
    }

    iter->pos_in_piece += count;

    return true;
}

static inline b32 base_advance_by_line(base_iter *iter, u32 count)
{
    iter->type &= ~Position;
    if (line_number(iter) + count >= iter->list->lcnt + 1)
    {
        return false;
    }

    while (count > iter->node->lcnt - (iter->line_in_piece + iter->piece_line))
    {
        count -= iter->node->lcnt - (iter->line_in_piece + iter->piece_line );
        iter->abs_idx   += iter->node->count - iter->piece_idx;
        iter->node_line += iter->node->lcnt;
        iter->node_pos  += iter->node->size;
        iter->piece_pos = iter->piece_line = iter->line_in_piece = iter->piece_idx = 0;
        iter->node  = iter->node->next;
    }

    while (count > iter->node->pieces[iter->piece_idx].lcnt - iter->line_in_piece)
    {
        count -= iter->node->pieces[iter->piece_idx].lcnt - iter->line_in_piece;
        iter->piece_pos  += iter->node->pieces[iter->piece_idx].size;
        iter->piece_line += iter->node->pieces[iter->piece_idx].lcnt;
        iter->line_in_piece = 0;
        iter->piece_idx++;
        iter->abs_idx++;
    }

    iter->line_in_piece += count;
    return true;
}

static inline b32 base_advance_rev_by_line(base_iter *iter, u32 count)
{
    iter->type &= ~Position;
    if (line_number(iter) == 0 && position(iter) == 0)
    {
        return false;
    }

    while (iter->node->prev != &iter->list->root_sentinel && count >= iter->piece_line + iter->line_in_piece)
    {
        count -= iter->piece_line + iter->line_in_piece;
        iter->node          = iter->node->prev;
        iter->abs_idx      -= (iter->piece_idx + 1);
        iter->node_line    -= iter->node->lcnt;
        iter->node_pos     -= iter->node->size;
        iter->piece_idx     = iter->node->count - 1;
        iter->piece_line    = iter->node->lcnt - iter->node->pieces[iter->piece_idx].lcnt;
        iter->piece_pos     = iter->node->size - iter->node->pieces[iter->piece_idx].size;
        iter->line_in_piece = iter->node->pieces[iter->piece_idx].lcnt;
    }

    while (iter->piece_idx > 0 && count >= iter->line_in_piece)
    {
        count -= iter->line_in_piece;
        iter->piece_idx--;
        iter->abs_idx--;
        iter->piece_line   -= iter->node->pieces[iter->piece_idx].lcnt;
        iter->piece_pos    -= iter->node->pieces[iter->piece_idx].size;
        iter->line_in_piece = iter->node->pieces[iter->piece_idx].lcnt;
    }

    iter->line_in_piece -= count;

    return true;
}

static inline void reset_cursor_(base_iter *iter)
{
     base_init_(iter->list, Position | LineNumber, iter);
}

static inline void normalize(base_iter *iter)
{
    if (!(iter->type & LineNumber))
    {
        Assert(iter->type & Position);
        iter->type |= LineNumber;
        iter->line_in_piece = get_line_from_position(iter);
    }

    if (!(iter->type & Position))
    {
        Assert(iter->type & LineNumber);
        iter->type |= Position;
        iter->pos_in_piece = get_position_from_line(iter);
    }
}

static inline base_iter base_init_rev(piece_list *list, iter_type type)
{
    base_iter result = {
        .list = list,
        .node = &list->root_sentinel,
        .node_pos = list->size,
        .node_line = list->lcnt,
        .type = type,
    };
    return result;
}

static inline b32 base_next(base_iter *iter)
{
    if (iter->abs_idx == iter->list->num_pieces)
    {
        return false;
    }

    if (iter->piece_idx == MAX_PIECES_PER_NODE)
    {
        iter->abs_idx   += iter->node->count;
        iter->node_pos  += iter->node->size;
        iter->node_line += iter->node->lcnt;
        iter->piece_pos = iter->piece_line = 0;
        iter->node = iter->node->next;
        iter->piece_idx = 0;
    }
    else
    {
        iter->piece_pos  += iter->node->pieces[iter->piece_idx].size;
        iter->piece_line += iter->node->pieces[iter->piece_idx].lcnt;
    }
    iter->pos_in_piece = iter->line_in_piece = 0;
    iter->piece_idx++;
    iter->abs_idx++;
    return true;

}

// ◆ utf8proc_iterate()
// utf8proc_ssize_t utf8proc_iterate 
//  (const utf8proc_uint8_t str, utf8proc_ssize_tstrlen, utf8proc_int32_t *codepoint_ref )
//  Reads a single codepoint from the UTF-8 sequence being pointed to by str.
//  The maximum number of bytes read is strlen, unless strlen is negative (in which case up to 4 bytes are read).
//  If a valid codepoint could be read, it is stored in the variable pointed to by codepoint_ref,
//  otherwise that variable will be set to -1.
//  In case of success, the number of bytes read is returned; otherwise, a negative error code is returned.
//






static inline b32 base_next_cell_(base_iter *iter)
{

    if (get_position(iter) == iter->list->size)
    {
        return false;
    } 
    else
    {
        if (iter->pos_in_piece + iter->piece_pos >= iter->node->size)
        {
            iter->abs_idx   += iter->node->count - iter->piece_idx;
            iter->node_line += iter->node->lcnt;
            iter->node_pos  += iter->node->size;
            iter->piece_idx  = iter->piece_line = iter->piece_pos = iter->pos_in_piece = iter->line_in_piece = 0;
            iter->node       = iter->node->next;
        } 
        else if (iter->pos_in_piece >= iter->node->pieces[iter->piece_idx].size)
        {
            iter->piece_pos  += iter->node->pieces[iter->piece_idx].size;
            iter->piece_line += iter->node->pieces[iter->piece_idx].lcnt;
            iter->pos_in_piece = iter->line_in_piece = 0;
            iter->piece_idx++;
            iter->abs_idx++;
        }

        piece piece = iter->node->pieces[iter->piece_idx];
        buffer_type type = iter->node->b_types[iter->piece_idx];

        const buffer *buffer = get_buffer_2(iter->list, type);
        u32 piece_offset = buffer->lines[piece.off.row] + piece.off.col;
        const u8 *cell_start   = buffer->text + piece_offset + iter->pos_in_piece;
        u32 cell_len = utf8_charlen_unchecked(cell_start, piece.size - iter->piece_pos);
        
        iter->pos_in_piece += cell_len;
        iter->line_in_piece += (*cell_start == '\n');
        return true;
    }
}

static inline b32 base_advance_by_cell(base_iter *iter, u32 count)
{
    b32 result = true;

    while (count-- > 0 && result)
    {
        result = base_next_cell_(iter);
    } 

    return result;
}

static inline b32 base_next_piece(base_iter *iter)
{
    if (iter->abs_idx == iter->list->num_pieces)
    {
        return false;
    }

    if (iter->piece_idx == iter->node->count)
    {
        iter->abs_idx   += iter->node->count;
        iter->node_pos  += iter->node->size;
        iter->node_line += iter->node->lcnt;
        iter->piece_pos = iter->piece_line = 0;
        iter->node = iter->node->next;
        iter->piece_idx = 0;
    }
    else
    {
        iter->piece_pos  += iter->node->pieces[iter->piece_idx].size;
        iter->piece_line += iter->node->pieces[iter->piece_idx].lcnt;
    }
    iter->pos_in_piece = iter->line_in_piece = 0;
    iter->piece_idx++;
    iter->abs_idx++;
    return true;
}

static inline b32 base_next_pos(base_iter *iter)
{
    if (get_position(iter) + 1 >= iter->list->size)
    {
        return false;
    }
    iter->type &= ~LineNumber;

    if (iter->pos_in_piece + iter->piece_pos >= iter->node->size)
    {
        iter->abs_idx   += iter->node->count;
        iter->node_line += iter->node->lcnt;
        iter->node_pos  += iter->node->size;
        iter->piece_idx  = iter->piece_line = iter->piece_pos = iter->pos_in_piece = 0;
        iter->node       = iter->node->next;
    }

    if (iter->pos_in_piece >= iter->node->pieces[iter->piece_idx].size)
    {
        iter->piece_pos    += iter->node->pieces[iter->piece_idx].size;
        iter->piece_line   += iter->node->pieces[iter->piece_idx].lcnt;
        iter->pos_in_piece = 0;
        iter->piece_idx++;
        iter->abs_idx++;
    }

    iter->pos_in_piece++;
    return true;
}


static inline b32 base_advance_pos_rev_by(base_iter *iter, u32 count)
{
    iter->type &= ~LineNumber;
    if (position(iter) == 0)
    {
        return false;
    }

    while (count > iter->piece_pos + iter->pos_in_piece)
    {
        count -= iter->piece_pos + iter->pos_in_piece;
        iter->node = iter->node->prev;
        // iter->abs_idx      -= iter->node-count;
        iter->abs_idx      -= (iter->piece_idx + 1);
        iter->node_line    -= iter->node->lcnt;
        iter->node_pos     -= iter->node->size;
        iter->piece_idx     = iter->node->count - 1;
        iter->piece_line    = iter->node->lcnt - iter->node->pieces[iter->piece_idx].lcnt;
        iter->piece_pos     = iter->node->size - iter->node->pieces[iter->piece_idx].size;
        iter->pos_in_piece  = iter->node->pieces[iter->piece_idx].size;
    }

    while (count > iter->pos_in_piece)
    {
        count -= iter->pos_in_piece;
        iter->piece_idx--;
        iter->abs_idx--;
        iter->piece_line   -= iter->node->pieces[iter->piece_idx].lcnt;
        iter->piece_pos    -= iter->node->pieces[iter->piece_idx].size;
        iter->pos_in_piece  = iter->node->pieces[iter->piece_idx].size;
    }

    iter->pos_in_piece -= count;

    return true;
}




static inline b32 base_next_line_until(base_iter *iter, u32 end)
{
    // iter->typegcc
    if (get_line_number(iter) >=  end)
    {
        iter->type &= ~Position;
        return false;
    }
    iter->type &= ~Position;

    while (0 >= iter->node->lcnt - (iter->piece_line + iter->line_in_piece) )
    {
        iter->abs_idx   += iter->node->count;
        iter->node_line += iter->node->lcnt;
        iter->node_pos  += iter->node->size;
        iter->piece_idx = iter->piece_line = iter->piece_pos = 0;
        iter->line_in_piece = 0;
        iter->node  = iter->node->next;
    }

    while (iter->node->pieces[iter->piece_idx].lcnt == iter->line_in_piece)
    {
        iter->piece_pos  += iter->node->pieces[iter->piece_idx].size;
        iter->piece_line += iter->node->pieces[iter->piece_idx].lcnt;
        iter->line_in_piece = 0;
        iter->piece_idx++;
        iter->abs_idx++;
    }

    iter->line_in_piece++;

    return true;
}

static inline b32 base_next_line(base_iter *iter)
{
    u32 result = base_next_line_until(iter, iter->list->lcnt);
    return result;
}

static inline u8 get_char(base_iter *iter) 
{
    if (!(iter->type & Position))
    {
        Assert(iter->type & LineNumber);
        iter->type |= Position;
        iter->pos_in_piece = get_position_from_line(iter);
    }

    if (iter->node->pieces[iter->piece_idx].size == iter->pos_in_piece)
    {
        piece piece;
        buffer_type type;
        if (iter->piece_idx + 1 == iter->node->count)
        {
            segmented_node *node = iter->node->next;
            Assert(node != &iter->list->root_sentinel);
            piece = *node->pieces;
            type  = *node->b_types;
        }
        else
        {
            piece = *(iter->node->pieces  + iter->piece_idx + 1);
            type  = *(iter->node->b_types + iter->piece_idx + 1);
        }
        const buffer *buffer = get_buffer_2(iter->list, type);
        u32 piece_offset = buffer->lines[piece.off.row] + piece.off.col;
        u8 result        = buffer->text[piece_offset];

        return result;
    }
    buffer_type type = iter->node->b_types[iter->piece_idx];
    const buffer *buffer = get_buffer_2(iter->list, type);

    piece piece = iter->node->pieces[iter->piece_idx];
    u32 piece_offset = buffer->lines[piece.off.row] + piece.off.col;

    return buffer->text[piece_offset + iter->pos_in_piece];
}





































static inline node_iter node_iter_init_from(segmented_node *from, const u32 pos, const u32 line)
{
    node_iter result = { .node = from, .pos = pos, .line = line };
    return result;
}

static inline node_iter node_iter_init(const piece_list *list)
{
    node_iter result = node_iter_init_from(list->root_sentinel.next, 0, 0);
    return result;
}

static inline void node_iter_next(node_iter *iter)
{
    iter->pos  += iter->node->size;
    iter->line += iter->node->lcnt;
    iter->node  = iter->node->next;
}

static inline b32 node_iter_valid(const piece_list *list, const node_iter *iter)
{
    b32 result = iter->node != &list->root_sentinel;
    return result;
}

static inline node_iter_rev node_iter_init_rev(const piece_list *list)
{
    node_iter_rev result = node_iter_init_from(list->root_sentinel.prev, list->size, list->lcnt);
    return result;
}

static inline void node_iter_prev(node_iter_rev *iter)
{
    iter->pos  -= iter->node->size;
    iter->line -= iter->node->lcnt;
    iter->node  = iter->node->prev;
}

static inline node_iter node_iter_init_from_pos(const piece_list *list, const u32 pos)
{
    node_iter result = node_iter_init(list);
    for (; pos > result.pos + result.node->size; node_iter_next(&result));
    return result;
}

static inline node_iter node_iter_init_from_pos_rev(const piece_list *list, const u32 pos)
{
    node_iter_rev result = node_iter_init_rev(list);
    for (; pos < result.pos - result.node->size; node_iter_prev(&result));
    return result;
}

static inline node_iter node_iter_init_from_line(const piece_list *list, const u32 line)
{
    node_iter result = node_iter_init(list);
    for (; line > result.line + result.node->lcnt; node_iter_next(&result));
    return result;
}

static inline node_iter node_iter_init_from_line_rev(const piece_list *list, const u32 line)
{
    node_iter_rev result = node_iter_init_rev(list);
    for (; line < result.line - result.node->lcnt; node_iter_prev(&result));
    return result;
}

static inline line_starts line_starts_init_alt(const piece_list *list)
{
    line_starts result = { .list = list, .node = &list->root_sentinel };
    return result;
}

static inline line_starts line_starts_init(const piece_list *list)
{
    segmented_node *node = list->root_sentinel.next;
    line_starts result = { .list = list, .node = node, .piece = node->pieces };
    return result;
}

static inline b32 line_starts_prev(line_starts *iter)
{
    if (current_line(iter) == 0)
    {
        return false;
    }

    iter->line_in_piece--;
    while (iter->piece_line + iter->line_in_piece == 0)
    {
        iter->node = iter->node->prev;
        if (iter->node == &iter->list->root_sentinel)
        {
            iter->piece_pos = iter->pos_in_piece = 0;
            return true;
        }
        iter->node_line    -= iter->node->lcnt;
        iter->node_pos     -= iter->node->size;
        iter->piece         = iter->node->pieces + iter->node->count - 1;
        iter->piece_line    = iter->node->lcnt - iter->piece->lcnt;
        iter->piece_pos     = iter->node->size - iter->piece->size;
        iter->line_in_piece = iter->piece->lcnt;
        iter->pos_in_piece  = iter->piece->size;
    }

    while (iter->line_in_piece == 0 )
    {
        iter->piece--;
        iter->piece_line   -= iter->piece->lcnt;
        iter->piece_pos    -= iter->piece->size;
        iter->line_in_piece = iter->piece->lcnt;
        iter->pos_in_piece  = iter->piece->size;
    }

    u32 piece_index = iter->piece - iter->node->pieces;
    buffer_type type = iter->node->b_types[piece_index];
    const buffer *buffer = get_buffer_2(iter->list, type);

    iter->pos_in_piece = 
        buffer->lines[iter->piece->off.row + iter->line_in_piece] -
        (buffer->lines[iter->piece->off.row] + iter->piece->off.col);
    return true;
}

static inline line_starts line_starts_init_rev(const piece_list *list)
{
    segmented_node *node = list->root_sentinel.prev;
    piece *piece = node->pieces + node->count - 1;
    line_starts result = {
        .list          = list,
        .node          = node,
        .piece         = piece,
        .node_pos      = list->size - node->size,
        .node_line     = list->lcnt - node->lcnt,
        .piece_pos     = node->size - piece->size,
        .piece_line    = node->lcnt - piece->lcnt,
        .pos_in_piece  = piece->size,
        .line_in_piece = piece->lcnt,
    };
    while (result.piece_line + result.line_in_piece == 0 && result.node->prev != &list->root_sentinel)
    {
        result.node          = result.node->prev;
        result.node_line    -= result.node->lcnt;
        result.node_pos     -= result.node->size;
        result.piece         = result.node->pieces + result.node->count - 1;
        result.piece_line    = result.node->lcnt - result.piece->lcnt;
        result.piece_pos     = result.node->size - result.piece->size;
        result.line_in_piece = result.piece->lcnt;
        result.pos_in_piece  = result.piece->size;
    }
    while (result.line_in_piece == 0 && result.piece > result.node->pieces)
    {
        result.piece--;
        result.piece_line   -= result.piece->lcnt;
        result.piece_pos    -= result.piece->size;
        result.line_in_piece = result.piece->lcnt;
        result.pos_in_piece  = result.piece->size;
    }

    if (result.line_in_piece == 0)
    {
        result.pos_in_piece = 0;
    }
    else
    {
        u32 piece_index = result.piece - result.node->pieces;
        buffer_type type = result.node->b_types[piece_index];
        const buffer *buffer = get_buffer_2(list, type);

        u32 l_offset = buffer->lines[result.piece->off.row + result.line_in_piece] -
            (buffer->lines[result.piece->off.row] + result.piece->off.col);
        result.pos_in_piece = l_offset;
    }
    return result;
}

static inline b32 line_starts_next(line_starts *iter)
{
    if (iter->node == &iter->list->root_sentinel)
    {
        return false;
    }
    // SKIP nodes with no lines
    while (iter->piece_line + iter->line_in_piece == iter->node->lcnt)
    {
        iter->node_line += iter->node->lcnt;
        iter->node_pos  += iter->node->size;
        iter->piece_line = iter->line_in_piece = iter->piece_pos  = iter->pos_in_piece = 0;
        iter->node  = iter->node->next;
        iter->piece = iter->node->pieces;
        if (iter->node == &iter->list->root_sentinel)
        {
            // NOTE: For the purpose of iterating over the lengths of lines within a range,
            // the last line of a file may not terminate in a new line character, 
            // thus we pretend there is one and assume the position of the last 
            // line is one character beyond the last character. 
            // This avoids spetial casing the last line.
            iter->node_pos++;
            return true;
        }
    }

    // SKIP pieces with no lines
    while (iter->piece->lcnt == iter->line_in_piece)
    {
        iter->piece_pos    += iter->piece->size;
        iter->piece_line   += iter->piece->lcnt;
        iter->line_in_piece = iter->pos_in_piece = 0;
        iter->piece++;
    }

    iter->line_in_piece++;

    u32 piece_index  = iter->piece - iter->node->pieces;
    buffer_type type = iter->node->b_types[piece_index];
    const buffer *buffer = get_buffer_2(iter->list, type);

    iter->pos_in_piece = buffer->lines[iter->piece->off.row + iter->line_in_piece] -
        (buffer->lines[iter->piece->off.row] + iter->piece->off.col);
    return true;
}


static inline line_lengths line_lengths_init(const piece_list *list)
{
    line_lengths result = { .l_starts = line_starts_init(list) };
    return result;
}

static inline lengths_item line_lengths_next(line_lengths *lens)
{
    lengths_item result = {
        .valid = line_starts_next(&lens->l_starts),
        .len   = current_position(&lens->l_starts) - lens->last_line_pos - 1,
    };
    lens->last_line_pos = current_position(&lens->l_starts);
    return result;
}

#define LINE_LENGTHS(list, len, body)                         \
    for (line_lengths lens = line_lengths_init((list));;)     \
    {                                                         \
        lengths_item item = line_lengths_next(&lens);         \
        if (!item.valid)                                      \
        {                                                     \
            break;                                            \
        }                                                     \
        (len) = item.len;                                     \
        body                                                  \
    }                                                         \

static inline line_starts line_starts_init_from(const piece_list *list, const u32 from)
{
    line_starts result = line_starts_init(list);
    for (; current_line(&result) < from; line_starts_next(&result));
    return result;
}

// static inline render_iter render_iter_init_from(const piece_list *list, const u32 from)
// {
//     render_iter result = render_iter_init(list);
//     for (; current
// }


static inline line_lengths line_lengths_init_from(const piece_list *list, const u32 from)
{
    line_starts l_starts = line_starts_init_from(list, from);
    line_lengths result = {
        .l_starts = l_starts,
        .last_line_pos = current_position(&l_starts),
    };
    return result;
}

static inline lengths_item line_lengths_next_until(line_lengths *lens, const u32 end)
{
    lengths_item result = line_lengths_next(lens);
    result.valid = result.valid && current_line(&lens->l_starts) <= end;
    return result;
}

#define LINE_LENGTHS_RANGE(list, Len, start, end, body)                    \
    for (line_lengths lens = line_lengths_init_from((list), (start));;)    \
    {                                                                      \
        lengths_item item = line_lengths_next_until(&lens, (end));         \
        if (!item.valid)                                                   \
        {                                                                  \
            break;                                                         \
        }                                                                  \
        (Len) = item.len;                                                  \
        body                                                               \
    }

static inline line_lengths line_lengths_init_rev(const piece_list *list)
{
    line_lengths result = { .l_starts = line_starts_init_rev(list), .last_line_pos = list->size + 1 };
    return result;
}

static inline lengths_item line_lengths_prev(line_lengths *lens)
{
    u32 len = lens->last_line_pos - current_position(&lens->l_starts) - 1;
    lens->last_line_pos = current_position(&lens->l_starts);

    lengths_item result = {
        .valid = line_starts_prev(&lens->l_starts),
        .len = len,
    };
    return result;
}

static inline b32 line_starts_next_until(line_starts *iter, const u32 end)
{
    b32 result = line_starts_next(iter) && (current_line(iter) <= end);
    return result;
}

#define LINE_LENGTHS_REV(list, Len, body)                               \
    for(line_lengths lens = line_lengths_init_rev((list));;)            \
    {                                                                   \
        lengths_item item = line_lengths_prev(&lens);                   \
        (Len) = item.len;                                               \
        body                                                            \
        if (!item.valid)                                                \
        {                                                               \
            break;                                                      \
        }                                                               \
    }                                                                   \

static inline line_starts line_starts_init_from_rev(const piece_list *list, const u32 from)
{
    line_starts result = line_starts_init_rev(list);
    for (; current_line(&result) > from; line_starts_prev(&result));
    return result;
}

static inline line_lengths line_lengths_init_from_rev(const piece_list *list, const u32 from)
{
    line_lengths result = line_lengths_init_rev(list); 
    for (; current_line(&result.l_starts) >= from; line_lengths_prev(&result));
    return result;
            
}

static inline lengths_item line_lengths_prev_until(line_lengths *lens, const u32 end)
{
    lengths_item result = line_lengths_prev(lens); 
    result.valid = result.valid && current_line(&lens->l_starts) >= end;
    return result;
}

#define LINE_LENGTHS_REV_RANGE(list, Len, start, end, body)             \
    for(line_lengths lens = line_lengths_init_from_rev((list),(end));;) \
    {                                                                   \
        lengths_item item = line_lengths_prev_until(&lens, (start));    \
        (Len) = item.len;                                               \
        body                                                            \
        if (!item.valid)                                                \
        {                                                               \
            break;                                                      \
        }                                                               \
    }                                                                   \

static inline b32 line_starts_prev_until(line_starts *iter, const u32 end)
{
    b32 result = line_starts_prev(iter) && (current_line(iter) >= end);
    return result;
}

static inline piece_iter piece_iter_init(const node_iter n_iter)
{
    piece_iter result = { 
        .node  = n_iter.node,
        .pos   = n_iter.pos,
        .line  = n_iter.line,
        .piece = n_iter.node->pieces
    };
    return result;
}

static inline piece_iter piece_iter_init_rev(const node_iter n_iter)
{
    piece_iter_rev result = {
        .node = n_iter.node,
        .pos  = n_iter.pos,
        .line = n_iter.line,
        .piece = n_iter.node->pieces + n_iter.node->count - 1
    };
    return result;
}

static inline void piece_iter_next(piece_iter *iter)
{
    iter->pos  += iter->piece->size;
    iter->line += iter->piece->lcnt;
    iter->piece++;
}

static inline void piece_iter_prev(piece_iter_rev *iter)
{
    iter->pos  -= iter->piece->size;
    iter->line -= iter->piece->lcnt;
    iter->piece--;
}

static inline b32 piece_iter_valid(const piece_iter *iter)
{
    b32 result = iter->piece < iter->node->pieces + iter->node->count;
    return result;
}

static inline b32 piece_iter_rev_valid(const piece_iter_rev *iter)
{
    b32 result = iter->piece >= iter->node->pieces;
    return result;
}

static inline b32 piece_iter_pos_valid(const piece_iter *iter, const u32 pos)
{
    b32 result = piece_iter_valid(iter) && (pos > iter->pos);
    return result;
}

static inline b32 piece_iter_pos_valid_rev(const piece_iter_rev *iter, const u32 pos)
{
    b32 result = piece_iter_rev_valid(iter) && (pos <= iter->pos);
    return result;
}

static inline b32 piece_iter_line_valid(const piece_iter *iter, const u32 line)
{
    b32 result = piece_iter_valid(iter) && (line > iter->line);
    return result;
}

static inline b32 piece_iter_line_valid_rev(const piece_iter_rev *iter, const u32 line)
{
    b32 result = piece_iter_rev_valid(iter) && (line <= iter->line);
    return result;
}

static inline piece_iter piece_iter_init_from_pos(const node_iter iter, const u32 pos)
{
    piece_iter result = piece_iter_init(iter);
    for (; pos > result.pos + result.piece->size; piece_iter_next(&result));
    return result;
}

static inline piece_iter piece_iter_init_from_pos_rev(const node_iter_rev iter, const u32 pos)
{
    piece_iter_rev result = piece_iter_init_rev(iter);
    for (; pos < result.pos - result.piece->size; piece_iter_prev(&result));
    return result;
}

static inline piece_iter piece_iter_init_from_line(const node_iter iter, const u32 line)
{
    piece_iter result = piece_iter_init(iter);
    for (; line > result.line + result.piece->lcnt; piece_iter_next(&result));
    return result;
}

static inline piece_iter piece_iter_init_from_line_rev(const node_iter_rev iter, const u32 line)
{
    piece_iter_rev result = piece_iter_init_rev(iter);
    for (; 
        line <= result.line - result.piece->lcnt && piece_iter_rev_valid(&result);
        piece_iter_prev(&result));
    return result;
}

static void write_to_buffer_rev(const piece_list *list, u8 *buf, const u32 len)
{
    Assert(len >= list->size);
    u32 cursor = len - 1;
    for (node_iter_rev n_iter = node_iter_init_rev(list);
        node_iter_valid(list, &n_iter);
        node_iter_prev(&n_iter))
    {
        for (piece_iter_rev p_iter = piece_iter_init_rev(n_iter);
            piece_iter_rev_valid(&p_iter);
            piece_iter_prev(&p_iter))
        {
            u32 piece_index  = p_iter.piece - p_iter.node->pieces;
            buffer_type type = p_iter.node->b_types[piece_index];
            const buffer *buffer = get_buffer_2(list, type);

            u32 start = buffer->lines[p_iter.piece->off.row] + p_iter.piece->off.col;
            u32 end   = start + p_iter.piece->size;
            cursor -= end - start;
            memcpy(buf + cursor, (buffer->text + start), end - start);
        }
    }
}

static void write_range_to_buffer(const piece_list *list, const u32 start, u8 *buf, const u32 len)
{
    Assert(start + len <= list->size);
    u32 end = start + len;
    u32 cursor = 0;

    for (node_iter n_iter = node_iter_init_from_pos(list, start);
        node_iter_valid(list, &n_iter);
        node_iter_next(&n_iter))
    {
        for (piece_iter p_iter = piece_iter_init_from_pos(n_iter, start);
            piece_iter_pos_valid(&p_iter, end);
            piece_iter_next(&p_iter))
        {
            u32 piece_index  = p_iter.piece - p_iter.node->pieces;
            buffer_type type = p_iter.node->b_types[piece_index];
            const buffer *buffer = get_buffer_2(list, type);

            u32 begin  = buffer->lines[p_iter.piece->off.row] + p_iter.piece->off.col;
            u32 finish = begin + p_iter.piece->size;

            if (start > p_iter.pos)
            {
                begin += start - p_iter.pos;
            }

            if (end < p_iter.pos + p_iter.piece->size)
            {
                finish -= p_iter.piece->size - (end - p_iter.pos);
            }
            memcpy(buf + cursor, buffer->text + begin, finish - begin);
            cursor += finish - begin;
        }
    }
}

static void write_range_to_buffer_rev(
    const piece_list *list,
    const u32 start,
    u8 *buf,
    const u32 len)
{
    Assert(start + len <= list->size);
    u32 end = start + len;
    u32 cursor = len;

    for (node_iter_rev n_iter = node_iter_init_from_pos_rev(list, end);
        node_iter_valid(list, &n_iter);
        node_iter_prev(&n_iter))
    {
        for (piece_iter_rev p_iter = piece_iter_init_from_pos_rev(n_iter, end);
            piece_iter_pos_valid_rev(&p_iter, start);
            piece_iter_prev(&p_iter))
        {
            u32 piece_index  = p_iter.piece - p_iter.node->pieces;
            buffer_type type = p_iter.node->b_types[piece_index];
            const buffer *buffer = get_buffer_2(list, type);

            u32 begin  = buffer->lines[p_iter.piece->off.row] + p_iter.piece->off.col;
            u32 finish = begin + p_iter.piece->size;

            if (start > p_iter.pos - p_iter.piece->size)
            {
                begin += start - (p_iter.pos - p_iter.piece->size);
            }

            if (end < p_iter.pos)
            {
                finish -= p_iter.pos - end; 
            }
            cursor -= finish - begin;
            memcpy(buf + cursor, buffer->text + begin, finish - begin);
        }
    }
}

static u32 write_line_range_to_buffer(
    const piece_list *list,
    const u32 start,
    u8 *buf,
    const u32 len)
{
    Assert(start + len <= list->lcnt);
    u32 end = start + len;
    u32 cursor = 0;

    for (node_iter n_iter = node_iter_init_from_line(list, start);
        node_iter_valid(list, &n_iter);
        node_iter_next(&n_iter))
    {
        for (piece_iter p_iter = piece_iter_init_from_line(n_iter, start);
            piece_iter_line_valid(&p_iter, end);
            piece_iter_next(&p_iter))
        {
            u32 piece_index  = p_iter.piece - p_iter.node->pieces;
            buffer_type type = p_iter.node->b_types[piece_index];
            const buffer *buffer = get_buffer_2(list, type);

            u32 begin  = buffer->lines[p_iter.piece->off.row] + p_iter.piece->off.col;
            u32 finish = begin + p_iter.piece->size;

            if (start > p_iter.line)
            {
                begin = buffer->lines[p_iter.piece->off.row + start - p_iter.line];
            }

            if (end < p_iter.line + p_iter.piece->lcnt)
            {
                finish = buffer->lines[p_iter.piece->off.row + end - p_iter.line];
            }
            memcpy(buf + cursor, buffer->text + begin, finish - begin);
            cursor += finish - begin;
        }
    }
    return cursor;
}

static void write_line_range_to_buffer_rev(
    const piece_list *list,
    const u32 start,
    u8 *buf,
    const u32 num_lines,
    const u32 len)
{
    Assert(start + len <= list->size);
    u32 end = start + num_lines;
    u32 cursor = len;

    for (node_iter_rev n_iter = node_iter_init_from_line_rev(list, end);
        node_iter_valid(list, &n_iter);
        node_iter_prev(&n_iter))
    {
        for (piece_iter_rev p_iter = piece_iter_init_from_line_rev(n_iter, end);
            piece_iter_line_valid_rev(&p_iter, start);
            piece_iter_prev(&p_iter))
        {
            u32 piece_index  = p_iter.piece - p_iter.node->pieces;
            buffer_type type = p_iter.node->b_types[piece_index];
            const buffer *buffer = get_buffer_2(list, type);

            u32 begin  = buffer->lines[p_iter.piece->off.row] + p_iter.piece->off.col;
            u32 finish = begin + p_iter.piece->size;

            if (start > p_iter.line - p_iter.piece->lcnt)
            {
                begin = buffer->lines[p_iter.piece->off.row + start - (p_iter.line - p_iter.piece->lcnt)];
            }

            if (end < p_iter.line)
            {
                finish = buffer->lines[p_iter.piece->off.row + end - (p_iter.line - p_iter.piece->lcnt)];
            }

            cursor -= finish - begin;
            memcpy(buf + cursor, buffer->text + begin, finish - begin);
        }
    }
}

#define LINE_STARTS(list, line_pos, body)                 \
    do                                                    \
    {                                                     \
        line_starts iter = line_starts_init((list));      \
        do                                                \
        {                                                 \
            (line_pos) = current_position(&iter);              \
            body                                          \
        } while (line_starts_next(&iter));                \
    } while (0)

#define LINE_STARTS_REV(list, line_pos, body)             \
    do                                                    \
    {                                                     \
        line_starts iter = line_starts_init_rev((list));  \
        do                                                \
        {                                                 \
            (line_pos) = current_position(&iter);              \
            body                                          \
        } while (line_starts_prev(&iter));                \
    } while (0)

#define LINE_STARTS_RANGE(list, line_pos, start, end, body)         \
    do                                                              \
    {                                                               \
        line_starts iter = line_starts_init_from((list), (start));  \
        do                                                          \
        {                                                           \
            (line_pos) = current_position(&iter);                        \
            body                                                    \
        } while (line_starts_next_until(&iter, (end)));             \
    } while (0)

#define LINE_STARTS_REV_RANGE(list, line_pos, start, end, body)         \
    do                                                                  \
    {                                                                   \
        line_starts iter = line_starts_init_from_rev((list), (end));    \
        do                                                              \
        {                                                               \
            (line_pos) = current_position(&iter);                            \
            body                                                        \
        } while (line_starts_prev_until(&iter, (start)));               \
    } while (0)

// TODO! find a better name
// This function takes a starting line and calculates,
// given a certain number of rows with a given size, 
// the number of lines that encompass said number of rows.
// If a some part of the range of rows is partially inside 
// the last line, rows_in denotes the number of rows 
// inside the last line, and rows_out denotes the number of 
// rows outside the last line.
static inline lines_result num_lines_from(
    const piece_list *list,
    const u32 start,
    const u32 row_size,
    u32 num_rows)
{
    u32 len = 0;
    lines_result result = {};

    LINE_LENGTHS_RANGE(list, len, start, list->lcnt + 1,
    {
        u32 rows_size = num_rows * row_size;
        if (len > num_rows * row_size)
        {
            result.rows_in = rows_size;
            result.rows_out  = len - result.rows_in;
            break;
        }
        result.num_lines++;

        num_rows -= 1 + len / row_size;
    });

    return  result;
}
//
static inline lines_result num_lines_from_rev(
    const piece_list *list,
    const u32 end,
    const u32 row_size,
    u32 num_rows)
{
    u32 len = 0;
    lines_result result = {};

    LINE_LENGTHS_RANGE(list, len, 0, end,
    {
        u32 rows_size = num_rows * row_size;
        if (len > num_rows * row_size)
        {
            result.rows_in = rows_size;
            result.rows_out  = len - result.rows_in;
            break;
        }
        result.num_lines++;
        num_rows -= 1 + len / row_size;
    });
    return  result;
}

static inline render_iter render_iter_init(const piece_list *list)
{
    line_starts starts = line_starts_init(list);
    render_iter result = { 
        .starts = starts,
        .buffer = get_buffer_2(list, list->root_sentinel.next->b_types[0])
    };
    return result;
}

static inline render_iter render_iter_init_alt(const piece_list *list)
{
    line_starts starts = line_starts_init(list);
    starts.line_in_piece = UINT32_MAX;
    starts.pos_in_piece = UINT32_MAX;
    render_iter result = { 
        .starts = starts,
        .buffer = get_buffer_2(list, list->root_sentinel.next->b_types[0])
    };
    return result;
}

static inline render_iter render_iter_init_from(const piece_list *list, u32 start)
{
    line_starts starts = line_starts_init_from(list, start);
    u32 piece_index  = starts.piece - starts.node->pieces;
    buffer_type type = starts.node->b_types[piece_index];
    render_iter result = {
        .starts = starts,
        .buffer = get_buffer_2(list, type)
    };
    return  result;
}

static inline render_item next_char_alt(render_iter *iter)
{
    render_item item = {};
    line_starts *starts = &iter->starts;
    b32 changed_buffer = false;

    starts->pos_in_piece++;
    if (starts->pos_in_piece + starts->piece_pos == starts->node->size)
    {
        starts->node_line += starts->node->lcnt;
        starts->node_pos  += starts->node->size;
        starts->piece_line = starts->line_in_piece = starts->piece_pos = starts->pos_in_piece = 0;
        starts->node  = starts->node->next;
        starts->piece = starts->node->pieces;
        if (starts->node == &starts->list->root_sentinel)
        {
            return item;
        }
        changed_buffer = true;
    }

    if (starts->pos_in_piece == starts->piece->size)
    {
        starts->piece_pos    += starts->piece->size;
        starts->piece_line   += starts->piece->lcnt;
        starts->line_in_piece = starts->pos_in_piece = 0;
        starts->piece++;
        changed_buffer = true;
    }

    if (changed_buffer)
    {
        u32 piece_index  = iter->starts.piece - iter->starts.node->pieces;
        buffer_type type = iter->starts.node->b_types[piece_index];
        iter->buffer = get_buffer_2(iter->starts.list, type);
    }
    u32 piece_offset = iter->buffer->lines[starts->piece->off.row] + starts->piece->off.col;
    item.item = iter->buffer->text[piece_offset + starts->pos_in_piece];
    item.valid = true;
    return item;
}

static inline render_item next_char(render_iter *iter) 
{
    render_item item = {};
    if (iter->starts.node == &iter->starts.list->root_sentinel)
    {
        return item;
    }

    item.valid = true;
    // cache this value, indirection is unnecessary.
    u32 piece_offset  = iter->buffer->lines[iter->starts.piece->off.row] + iter->starts.piece->off.col;
    item.item = iter->buffer->text[piece_offset + iter->starts.pos_in_piece] ;
    b32 changed_buffer = false;

    iter->starts.pos_in_piece++;
    if (iter->starts.node->count == 0 || 
        iter->starts.pos_in_piece + iter->starts.piece_pos == iter->starts.node->size)
    {
        iter->starts.node_line += iter->starts.node->lcnt;
        iter->starts.node_pos  += iter->starts.node->size;
        iter->starts.piece_line = iter->starts.line_in_piece = 
            iter->starts.piece_pos = iter->starts.pos_in_piece = 0;
        iter->starts.node  = iter->starts.node->next;
        iter->starts.piece = iter->starts.node->pieces;
        if (iter->starts.node == &iter->starts.list->root_sentinel)
        {
            return item;
        }
        changed_buffer = true;
    }

    if (iter->starts.pos_in_piece == iter->starts.piece->size)
    {
        iter->starts.piece_pos    += iter->starts.piece->size;
        iter->starts.piece_line   += iter->starts.piece->lcnt;
        iter->starts.line_in_piece = iter->starts.pos_in_piece = 0;
        iter->starts.piece++;
        changed_buffer = true;
    }

    if (changed_buffer)
    {
        u32 piece_index  = iter->starts.piece - iter->starts.node->pieces;
        buffer_type type = iter->starts.node->b_types[piece_index];
        iter->buffer = get_buffer_2(iter->starts.list, type);
    }

    return item;
}

static inline render_item next_line_alt(render_iter *iter)
{
    render_item item = {};
    line_starts *starts = &iter->starts;
    b32 changed_buffer = false;
    while (starts->piece_line + starts->line_in_piece == starts->node->lcnt)
    {
        starts->node_line += starts->node->lcnt;
        starts->node_pos  += starts->node->size;
        starts->piece_line = starts->line_in_piece = starts->piece_pos = starts->pos_in_piece = 0;
        starts->node  = starts->node->next;
        starts->piece = starts->node->pieces;
        if (starts->node == &starts->list->root_sentinel)
        {
            return item;
        }
        changed_buffer = true;
    }

    while (starts->piece->lcnt == starts->line_in_piece)
    {
        starts->piece_pos    += starts->piece->size;
        starts->piece_line   += starts->piece->lcnt;
        starts->line_in_piece = starts->pos_in_piece = 0;
        starts->piece++;
        changed_buffer = true;
    }

    starts->line_in_piece++;

    if (changed_buffer)
    {
        u32 piece_index  = starts->piece - starts->node->pieces;
        buffer_type type = starts->node->b_types[piece_index];
        iter->buffer = get_buffer_2(starts->list, type);
    }

    item.valid = true;
    u32 piece_offset = (iter->buffer->lines[starts->piece->off.row] + starts->piece->off.col);
    u32 char_offset = (starts->line_in_piece) ?
        iter->buffer->lines[starts->piece->off.row + starts->line_in_piece] :
        piece_offset;
    starts->pos_in_piece = char_offset - piece_offset;
    if (starts->pos_in_piece == starts->piece->size)
    {
        if (starts->piece + 1 < starts->node->pieces + starts->node->count)
        {
            starts->piece_pos    += starts->piece->size;
            starts->piece_line   += starts->piece->lcnt;
            starts->line_in_piece = starts->pos_in_piece = 0;
            starts->piece++;
        }
        else
        {
            if (starts->node->next == &starts->list->root_sentinel)
            {
                item.valid = false;
                return item;
            }
            starts->node_line += starts->node->lcnt;
            starts->node_pos  += starts->node->size;
            starts->piece_line = starts->line_in_piece = starts->piece_pos  = starts->pos_in_piece = 0;
            starts->node  = starts->node->next;
            starts->piece = starts->node->pieces;
        }
        u32 piece_index  = starts->piece - starts->node->pieces;
        buffer_type type = starts->node->b_types[piece_index];
        iter->buffer = get_buffer_2(starts->list, type);
        u32 piece_offset = iter->buffer->lines[starts->piece->off.row] + starts->piece->off.col;
        item.item = iter->buffer->text[piece_offset + starts->pos_in_piece];
    }
    else
    {
        item.item = iter->buffer->text[piece_offset + starts->pos_in_piece];
    }
    return item;
}

static inline render_item next_line(render_iter *iter)
{
    render_item item = {};
    if (iter->starts.node == &iter->starts.list->root_sentinel)
    {
        return item;
   }
    
    {
        item.valid = true;
        u32 piece_offset = iter->buffer->lines[iter->starts.piece->off.row] + iter->starts.piece->off.col;
        item.item = iter->buffer->text[piece_offset + iter->starts.pos_in_piece];
    }
    b32 changed_buffer = false;

    while (iter->starts.node->count == 0 || 
           iter->starts.piece_line + iter->starts.line_in_piece == iter->starts.node->lcnt)
    {
        iter->starts.node_line += iter->starts.node->lcnt;
        iter->starts.node_pos  += iter->starts.node->size;
        iter->starts.piece_line = iter->starts.line_in_piece = iter->starts.piece_pos = iter->starts.pos_in_piece = 0;
        iter->starts.node  = iter->starts.node->next;
        iter->starts.piece = iter->starts.node->pieces;
        if (iter->starts.node == &iter->starts.list->root_sentinel)
        {
            return item;
        }
        changed_buffer = true;
    }

    while (iter->starts.piece->lcnt == iter->starts.line_in_piece)
    {
        iter->starts.piece_pos    += iter->starts.piece->size;
        iter->starts.piece_line   += iter->starts.piece->lcnt;
        iter->starts.line_in_piece = iter->starts.pos_in_piece = 0;
        iter->starts.piece++;
        changed_buffer = true;
    }
    iter->starts.line_in_piece++;

    if (changed_buffer)
    {
        u32 piece_index  = iter->starts.piece - iter->starts.node->pieces;
        buffer_type type = iter->starts.node->b_types[piece_index];
        iter->buffer = get_buffer_2(iter->starts.list, type);
    }

    u32 char_offset = iter->buffer->lines[iter->starts.piece->off.row + iter->starts.line_in_piece];
    u32 piece_offset = (iter->buffer->lines[iter->starts.piece->off.row] + iter->starts.piece->off.col);
    iter->starts.pos_in_piece = char_offset - piece_offset;
    if (iter->starts.pos_in_piece == iter->starts.piece->size)
    {
        if (iter->starts.piece + 1 < iter->starts.node->pieces + iter->starts.node->count)
        {
            iter->starts.piece_pos    += iter->starts.piece->size;
            iter->starts.piece_line   += iter->starts.piece->lcnt;
            iter->starts.line_in_piece = iter->starts.pos_in_piece = 0;
            iter->starts.piece++;
        }
        else
        {
            iter->starts.node_line += iter->starts.node->lcnt;
            iter->starts.node_pos  += iter->starts.node->size;
            iter->starts.piece_line = iter->starts.line_in_piece = iter->starts.piece_pos  = iter->starts.pos_in_piece = 0;
            iter->starts.node  = iter->starts.node->next;
            iter->starts.piece = iter->starts.node->pieces;
        }
        u32 piece_index  = iter->starts.piece - iter->starts.node->pieces;
        buffer_type type = iter->starts.node->b_types[piece_index];
        iter->buffer = get_buffer_2(iter->starts.list, type);

    }
    return item;
}
