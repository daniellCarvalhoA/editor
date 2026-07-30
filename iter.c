typedef b32 (*search_pred)(u8 *buf, u32 len, str s);




static inline b32 base_init_(piece_list *list, iter_type type, base_iter *iter)
{
    iter->list = list;
    iter->node = list->root_sentinel.next;
    iter->node_pos      = 0;
    iter->node_line     = 0;
    iter->piece_pos     = 0;
    iter->piece_line    = 0;
    iter->pos_in_piece  = 0;
    iter->line_in_piece = 0;
    iter->piece_idx     = 0;
    iter->abs_idx       = 0;
    iter->type          = type;
    b32 result = iter->list->size > 0; 
    return result;
}

static inline piece *get_piece_(base_iter *iter)
{
    piece *result = (iter->piece_idx < iter->node->count) ? iter->node->pieces + iter->piece_idx : 0;
    return result;
}

static inline u32 get_position_from_line_unsafe(base_iter *iter, piece piece)
{
    const buffer *buffer = get_buffer(iter->list, piece.type);
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


static inline u32 get_line_from_position(base_iter *iter)
{
    u32 result = 0;
    piece *piece = get_piece_(iter);
    if (piece)
    {
        const buffer *buffer = get_buffer(iter->list, piece->type);
        result = search_piece(buffer, *piece, iter->pos_in_piece).row - piece->off.row;
    }
    return result;
}

static inline offset get_offset_from_position(base_iter *iter)
{
    piece *piece         = get_piece_(iter);
    Assert(piece);
    const buffer *buffer = get_buffer(iter->list, piece->type);
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

static inline b32 base_advance_by(base_iter *iter, u32 count)
{
    if (get_position(iter) == iter->list->size)
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

    iter->pos_in_piece = 0;
    iter->line_in_piece = 0;

    return true;
}

static inline b32 base_reverse_by(base_iter *iter, u32 count)
{
    if (iter->abs_idx == 0)
    {
        return false;
    }

    while (count > iter->piece_idx)
    {
        count -= iter->piece_idx;
        iter->node = iter->node->prev;
        iter->abs_idx   -= iter->piece_idx;
        iter->piece_idx  = iter->node->count;

        iter->node_pos  -= iter->node->size;
        iter->node_line -= iter->node->lcnt;

        iter->piece_pos  = iter->node->size;
        iter->piece_line = iter->node->lcnt;
    }

    while (count > 0)
    {
        count--;
        iter->piece_idx--;
        iter->abs_idx--;
        iter->piece_pos  -= iter->node->pieces[iter->piece_idx].size;
        iter->piece_line -= iter->node->pieces[iter->piece_idx].lcnt;
    }

    iter->pos_in_piece = 0;
    iter->line_in_piece = 0;
    return true;
}


static inline u32 get_row_from_line(base_iter *iter)
{
    piece *piece = get_piece_(iter);
    u32 result = piece->off.row + iter->line_in_piece;
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
            const buffer *buffer = get_buffer(iter->list, piece->type);
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


static inline void fix_iter(base_iter *iter) 
{
    if (iter->node == &iter->list->root_sentinel)
    {
    }
    else if (iter->pos_in_piece + iter->piece_pos >= iter->node->size && iter->piece_idx + 1 == MAX_PIECES_PER_NODE)
    {
        iter->abs_idx   += iter->node->count - iter->piece_idx;
        iter->node_line += iter->node->lcnt;
        iter->node_pos  += iter->node->size;
        iter->piece_idx = iter->piece_line = iter->piece_pos = iter->pos_in_piece = iter->line_in_piece = 0 ;
        iter->node = iter->node->next;
    } 
    else if (iter->pos_in_piece >= iter->node->pieces[iter->piece_idx].size) //  && iter->piece_idx + 1 > iter->node->count)
    {
        iter->piece_pos  += iter->node->pieces[iter->piece_idx].size;
        iter->piece_line += iter->node->pieces[iter->piece_idx].lcnt;
        iter->pos_in_piece = iter->line_in_piece = 0;
        iter->piece_idx++;
        iter->abs_idx++;
    }
}

static inline void fix_iter_(base_iter *iter) 
{
    if (iter->node == &iter->list->root_sentinel)
    {
    }
    else if (iter->pos_in_piece + iter->piece_pos >= iter->node->size && (iter->node->pieces[iter->piece_idx].size > 0))
    {
        iter->abs_idx   += iter->node->count - iter->piece_idx;
        iter->node_line += iter->node->lcnt;
        iter->node_pos  += iter->node->size;
        iter->piece_idx = iter->piece_line = iter->piece_pos = iter->pos_in_piece = iter->line_in_piece = 0 ;
        iter->node = iter->node->next;
    } 
    else if (iter->pos_in_piece >= iter->node->pieces[iter->piece_idx].size && (iter->node->pieces[iter->piece_idx].size > 0)) 
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
    // Why am i copying the data?.
    cell_item result = {};

    if (get_position(iter) == iter->list->size)
    {
        result.valid = false;
    } 
    else
    {
        while (iter->pos_in_piece + iter->piece_pos >= iter->node->size)
        {
            iter->abs_idx   += iter->node->count - iter->piece_idx;
            iter->node_line += iter->node->lcnt;
            iter->node_pos  += iter->node->size;
            iter->piece_idx  = iter->piece_line = iter->piece_pos = iter->pos_in_piece = iter->line_in_piece = 0;
            iter->node       = iter->node->next;
        } 
        while (iter->pos_in_piece >= iter->node->pieces[iter->piece_idx].size)
        {
            iter->piece_pos  += iter->node->pieces[iter->piece_idx].size;
            iter->piece_line += iter->node->pieces[iter->piece_idx].lcnt;
            iter->pos_in_piece = iter->line_in_piece = 0;
            iter->piece_idx++;
            iter->abs_idx++;
        }

        piece piece = iter->node->pieces[iter->piece_idx];

        const buffer *buffer = get_buffer(iter->list, piece.type);
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
            result.cell = cell_from_buffer_index(piece_offset, piece.type).value;
        }
        iter->line_in_piece += (*cell_start == '\n');
        iter->pos_in_piece += cell_len;
    }
    return result;
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

#if 0
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
#endif


static inline b32 is_white_space(u8 *buf, u32 len, str needle)
{
    b32 result = (len > 0) && ((*buf == ' ') || (*buf == '\t'));
    return result;
}

static inline b32 not_equal_to(u8 *buf, u32 len, str needle)
{
    b32 result = true; 

    if (len == needle.len)
    {
        result = (memcmp(buf, needle.buffer, len) != 0);
    }
    return result;
}


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
            iter->piece_idx = iter->piece_line = iter->piece_pos = iter->pos_in_piece = iter->line_in_piece = 0;
            iter->node      = iter->node->next;
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

        const buffer *buffer = get_buffer(iter->list, piece.type);
        u32 piece_offset = buffer->lines[piece.off.row] + piece.off.col;
        const u8 *cell_start   = buffer->text + piece_offset + iter->pos_in_piece;
        u32 cell_len = utf8_charlen_unchecked(cell_start, piece.size - iter->piece_pos);
        
        iter->pos_in_piece += cell_len;
        iter->line_in_piece += (*cell_start == '\n');
        return true;
    }
}

static inline b32 base_prev_cell(base_iter *iter)
{
    if (get_position(iter) == 0)
    {
        return false;
    }
    else
    {
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
            iter->line_in_piece = iter->node->pieces[iter->piece_idx].lcnt;
        }

        if (iter->pos_in_piece == 0)
        {
            iter->piece_idx--;
            iter->abs_idx--;
            iter->piece_line   -= iter->node->pieces[iter->piece_idx].lcnt;
            iter->piece_pos    -= iter->node->pieces[iter->piece_idx].size;
            iter->pos_in_piece  = iter->node->pieces[iter->piece_idx].size;
            iter->line_in_piece = iter->node->pieces[iter->piece_idx].lcnt;
        }

        piece piece = iter->node->pieces[iter->piece_idx];

        const buffer *buffer = get_buffer(iter->list, piece.type);
        u32 piece_offset = buffer->lines[piece.off.row] + piece.off.col;
        u8 *buf = buffer->text + piece_offset;

        str cell = get_grapheme_backward(buf, iter->pos_in_piece);
        Assert(iter->pos_in_piece >= cell.len);
        iter->line_in_piece -= (cell.buffer[0] == '\n');
        iter->pos_in_piece -= cell.len;
    }
    return true;
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
#if 0
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
#endif

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
        if (iter->piece_idx + 1 == iter->node->count)
        {
            segmented_node *node = iter->node->next;
            Assert(node != &iter->list->root_sentinel);
            piece = *node->pieces;
        }
        else
        {
            piece = *(iter->node->pieces  + iter->piece_idx + 1);
        }
        const buffer *buffer = get_buffer(iter->list, piece.type);
        u32 piece_offset = buffer->lines[piece.off.row] + piece.off.col;
        u8 result        = buffer->text[piece_offset];

        return result;
    }

    piece piece = iter->node->pieces[iter->piece_idx];
    const buffer *buffer = get_buffer(iter->list, piece.type);

    u32 piece_offset = buffer->lines[piece.off.row] + piece.off.col;

    return buffer->text[piece_offset + iter->pos_in_piece];
}

static inline str get_char_utf8(base_iter *iter)
{
    str result = {};

    if (!(iter->type & Position))
    {
        Assert(iter->type & LineNumber);
        iter->type |= Position;
        iter->pos_in_piece = get_position_from_line(iter);
    }

    fix_iter_(iter);

    piece piece = iter->node->pieces[iter->piece_idx];

    const buffer *buffer = get_buffer(iter->list, piece.type);

    u32 piece_offset = buffer->lines[piece.off.row] + piece.off.col;

    result.buffer = buffer->text + piece_offset + iter->pos_in_piece;
    result.len = utf8_len_table[result.buffer[0]];
    return result;
}

static inline b32 base_next_pred(
    base_iter *iter,
    search_pred pred,
    str needle)
{
    str s = get_char_utf8(iter);

    b32 result = pred(s.buffer, s.len, needle) && base_next_cell_(iter);

    return result;
}




























