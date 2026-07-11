
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
}

static void add_to_cursor_(cursor *cursor, u32 count)
{
    for (; count > 0; cursor->node = cursor->node->next)
    {
        if (cursor->piece_index + count > cursor->node->count)
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

    if (cursor->piece_index == MAX_PIECES_PER_NODE)
    {
        cursor->node = cursor->node->next;
        cursor->piece_index = 0;
    }
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

