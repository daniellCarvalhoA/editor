
static segmented_node *allocate_node(piece_list *list)
{
    segmented_node *node;
    FREELIST_ALLOCATE(
        node,
        list->first_free_node,
        PushStruct(&list->list_arena, segmented_node, NoClear()));
    node->count = node->size = node->lcnt = 0;
    return node;
}


static inline void next_cursor(cursor *cursor)
{
    Assert(cursor->piece_index < cursor->node->count);
    ++cursor->piece_index;
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

static cursor add_to_cursor_by_value(
    const piece_list *list,
    const cursor old_cursor, u32 count)
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

#if TESTS
static inline void fix_size_and_lines(segmented_node *node)
{
    Assert(node);
    node->size = node->lcnt = 0;
    for (u32 i = 0; i < node->count; ++i)
    {
        piece piece = node->pieces[i];
        node->size += piece.size;
        node->lcnt += piece.lcnt;
    }
}

typedef struct node_size_and_lcnt
{
    u32 size;
    u32 lcnt;
} node_size_and_lcnt;

static inline node_size_and_lcnt node_size_lcnt(const segmented_node *node)
{
    Assert(node);
    node_size_and_lcnt result = {};
    for (u32 piece_index = 0; piece_index < node->count; ++piece_index)
    {
        piece piece = node->pieces[piece_index];
        result.size += piece.size;
        result.lcnt += piece.lcnt;
    }
    return result;
}
#endif


