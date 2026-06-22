
static inline search default_search()
{
    search search = {};
    return search;
}

static inline u32 get_pos(const search *search)
{
    u32 result = search->piece_pos + search->in_piece;
    return result;
}

static inline u32 get_line(const search *search)
{
    u32 result = search->piece_line + search->in_line;
    return result;
}

static inline piece *get_piece(const cursor cursor)
{
    piece *result = cursor.node->pieces + cursor.piece_index;
    return result;
}

static inline buffer_type get_type(const cursor cursor)
{
    buffer_type result = cursor.node->b_types[cursor.piece_index];
    return result;
}

static inline void next_cursor(cursor *cursor)
{
    if (cursor->piece_index == MAX_PIECES_PER_NODE - 1)
    {
        cursor->node = cursor->node->next;
        cursor->piece_index = 0;
    }
    else
    {
        ++cursor->piece_index;
    }
}

static inline void next_cursor_(cursor *cursor)
{
    if (cursor->piece_index + 1 < cursor->node->count)
    {
        ++cursor->piece_index;
    }
    else
    {
        cursor->node = cursor->node->next;
        cursor->piece_index = 0;
    }
}

static void sub_from_cursor(cursor *cursor, u32 count)
{
    while (count > cursor->piece_index)
    {
        count -= cursor->piece_index;
        cursor->node = cursor->node->prev;
        cursor->piece_index = cursor->node->count;
    }

    cursor->piece_index -= count;
    // while (count > 0)
    // {
    //     if (count > cursor->piece_index)
    //     {
    //         count -= cursor->piece_index;
    //     }
    //     else
    //     {
    //         cursor->piece_index -= count;
    //         break;
    //     }
    //     cursor->node = 
    // }
    // for (; (cursor->node != &list->root_sentinel) && (count > 0);
    //         cursor->node = cursor->node->prev, cursor->piece_idx = )
    // {
    //     if (count > cursor->piece_index)
    //     {
    //         count -= cursor->piece_index;
    //     }
    // }
}

static void add_to_cursor(const piece_list *list, cursor *cursor, u32 count)
{
    for (; (cursor->node != &list->root_sentinel) && (count > 0); 
            cursor->node = cursor->node->next)
    {
        if ((cursor->piece_index + count >= cursor->node->count) && 
            (cursor->node->next != &list->root_sentinel))
        {
            count -= cursor->node->count - cursor->piece_index;
            cursor->piece_index = 0;
        }
        else
        {
            cursor->piece_index += count;
            break;
        }
    }

    if (cursor->piece_index ==  MAX_PIECES_PER_NODE)
    {
        cursor->node = cursor->node->next;
        cursor->piece_index = 0;
    }
}

static cursor add_to_cursor_by_value(const piece_list *list, const cursor old_cursor, u32 count)
{
    cursor new_cursor = old_cursor;

    for (; 
        (new_cursor.node != &list->root_sentinel) && (count > 0);
        new_cursor.node = new_cursor.node->next)
    {
        if ((new_cursor.piece_index + count >= new_cursor.node->count) &&
            (new_cursor.node->next != &list->root_sentinel)) 
        {
            count -= new_cursor.node->count - new_cursor.piece_index;
            new_cursor.piece_index = 0;
        }
        else
        {
            new_cursor.piece_index += count;
            break;
        }
    }

    return new_cursor;
}

static cursor abs_idx_to_cursor_2(const piece_list *list, u32 i)
{
    cursor cursor = { .node = list->root_sentinel.next, .piece_index = 0 };

    for (;
        cursor.node != &list->root_sentinel;
        i -= cursor.node->count, cursor.node = cursor.node->next)
    {
        if (i < cursor.node->count || (i == cursor.node->count && cursor.node->count < MAX_PIECES_PER_NODE))
        {
            break;
        }
    }
    cursor.piece_index = i;
    return cursor;
}

static cursor abs_idx_to_cursor(const piece_list *list, u32 i)
{
    cursor cursor = { .node = list->root_sentinel.next, .piece_index = 0 };

    for (; (cursor.node != &list->root_sentinel);
         i -= cursor.node->count, cursor.node = cursor.node->next)
    {
        if (i < cursor.node->count)
        {
            break;
        }
    }
    cursor.piece_index = i;
    return cursor;
}

static inline b32 validate_search(const piece_list *list, const search *search)
{
    segmented_node *node = search->search_cursor.node;
    piece search_piece = node->pieces[search->search_cursor.piece_index];
    u32 rest = search->in_piece == search_piece.size;

    cursor cursor = abs_idx_to_cursor(list, search->abs_idx + rest);

    b32 result = cursor.node == search->search_cursor.node && 
                cursor.piece_index == search->search_cursor.piece_index;
    return result;
}

static search find_by_line_(piece_list *list, u32 line)
{
    search search = {};
    iter iter = {};

    node_each(&list->root_sentinel, iter)
    {
        if (line >= iter.line && line <= iter.line + iter.node->lcnt)
        {
            piece_each(iter)
            {
                if (line >= iter.line && line <= iter.line + iter.piece.lcnt)
                {
                    buffer *buffer = get_buffer(list, iter.node, iter.piece_index);
                    u32 line_delta = line_offset(buffer, iter.piece, line - iter.line);

                    search.abs_idx = iter.abs_idx + iter.piece_index;

                    if (line_delta == iter.piece.size)
                    {
                        search.piece_pos  = iter.pos + line_delta;
                        search.piece_line = line;

                        search.search_cursor.piece_index = iter.piece_index;
                        search.search_cursor.node = iter.node;
                        next_cursor_(&search.search_cursor);

                        piece *next_piece = get_piece(search.search_cursor);
                        search.piece_offset = next_piece->off;
                        search.abs_idx++;
                    }
                    else
                    {
                        search.in_piece   = line_delta;
                        search.in_line    = line - iter.line;
                        search.piece_pos  = iter.pos;
                        search.piece_line = iter.line;
                        search.piece_offset.row = iter.piece.off.row + line - iter.line;
                        search.search_cursor.piece_index = iter.piece_index;
                        search.search_cursor.node = iter.node;
                    }
                    return search;
                }
            }
        }
    }

    search.abs_idx    = iter.abs_idx;
    search.piece_pos  = iter.pos;
    search.piece_line = iter.line;
    search.search_cursor.node = iter.node;
    return search;
}

static search find_by_line(piece_list *list, const u32 line)
{
    search search  = {};
    iter iter = {};

    node_each(&list->root_sentinel, iter)
    {
        if (line >= iter.line && line <= iter.line + iter.node->lcnt)
        {
            piece_each(iter)
            {
                if (line >= iter.line && line <= iter.line + iter.piece.lcnt)
                {
                    buffer *buffer = get_buffer(list, iter.node, iter.piece_index);
                    u32 line_delta = line_offset(buffer, iter.piece, line - iter.line);

                    search.abs_idx = iter.abs_idx + iter.piece_index;

                    if (line_delta == iter.piece.size)
                    {
                        search.piece_pos  = iter.pos + line_delta;
                        search.piece_line = line;

                        search.search_cursor.piece_index = iter.piece_index;
                        search.search_cursor.node = iter.node;
                        next_cursor(&search.search_cursor);

                        piece *next_piece = get_piece(search.search_cursor);
                        search.piece_offset = next_piece->off;
                        search.abs_idx++;
                    }
                    else
                    {
                        search.in_piece   = line_delta;
                        search.in_line    = line - iter.line;
                        search.piece_pos  = iter.pos;
                        search.piece_line = iter.line;
                        search.piece_offset.row = iter.piece.off.row + line - iter.line;
                        search.search_cursor.piece_index = iter.piece_index;
                        search.search_cursor.node = iter.node;
                    }
                    return search;
                }
            }
        }
    }
    search.abs_idx    = iter.abs_idx;
    search.piece_pos  = iter.pos;
    search.piece_line = iter.line;
    search.search_cursor.node = iter.node;
    return search;
}

static search find_by_pos_(piece_list *list, const u32 pos)
{
    search search = {};
    iter iter = {};
    node_each(&list->root_sentinel, iter)
    {
        if (pos >= iter.pos && pos < iter.pos + iter.node->size)
        {
            piece_each(iter)
            {
                if (pos >= iter.pos && pos < iter.pos + iter.piece.size)
                {
                    search.abs_idx    = iter.abs_idx + iter.piece_index;
                    search.in_piece   = pos - iter.pos;
                    search.piece_pos  = iter.pos;
                    search.piece_line = iter.line;

                    buffer *buffer = get_buffer(list, iter.node, iter.piece_index);
                    offset offset  = search_piece(buffer, iter.piece, search.in_piece);
                    search.piece_offset = offset;
                    search.in_line      = offset.row - iter.piece.off.row;
                    search.search_cursor.piece_index = iter.piece_index;
                    search.search_cursor.node = iter.node;
                    return search;
                }
            }
        }
    }
    search.abs_idx    = iter.abs_idx;
    search.piece_pos  = iter.pos;
    search.piece_line = iter.line;
    search.search_cursor.node = iter.node;
    return search;
}

static search find_by_pos(piece_list *list, const u32 pos)
{
    search search = {};
    iter iter = {};

    node_each(&list->root_sentinel, iter)
    {
        if (pos >= iter.pos && pos < iter.pos + iter.node->size)
        {
            piece_each(iter)
            {
                if (pos >= iter.pos && pos < iter.pos + iter.piece.size)
                {
                    search.abs_idx      = iter.abs_idx + iter.piece_index;
                    search.in_piece     = pos - iter.pos;
                    search.piece_pos    = iter.pos;
                    search.piece_line   = iter.line;

                    buffer *buffer = get_buffer(list, iter.node, iter.piece_index);
                    offset offset  = search_piece(buffer, iter.piece, search.in_piece);
                    search.piece_offset = offset;
                    search.in_line      = offset.row - iter.piece.off.row;
                    search.search_cursor.piece_index = iter.piece_index;
                    search.search_cursor.node = iter.node;
                    return search;
                }
            }
        }
        else if ((pos == iter.pos + iter.node->size) && (iter.node->count < MAX_PIECES_PER_NODE))
        {
            search.abs_idx      = iter.abs_idx  + iter.node->count;
            search.piece_pos    = pos;
            search.piece_line   = iter.line + iter.node->lcnt;
            search.search_cursor.piece_index = iter.node->count;
            search.search_cursor.node = iter.node;
            return search;
        }
    }
    search.abs_idx    = iter.abs_idx;
    search.piece_pos  = iter.pos;
    search.piece_line = iter.line;
    search.search_cursor.node = iter.node;
    return search;
}

static b32 search_strict_equality(const search *a, const search *b)
{
    b32 result = (a->abs_idx            == b->abs_idx)    &&
                 (a->in_piece           == b->in_piece)   &&
                 (a->in_line            == b->in_line)    &&
                 (a->piece_pos          == b->piece_pos)  &&
                 (a->piece_line         == b->piece_line) &&
                 (a->piece_offset.row   == b->piece_offset.row)   &&
                 (a->piece_offset.col   == b->piece_offset.row)   &&
                 (a->search_cursor.node == b->search_cursor.node) &&
                 (a->search_cursor.piece_index == b->search_cursor.piece_index);
    return result;
}


