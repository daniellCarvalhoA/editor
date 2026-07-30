inline b32 is_header_empty(const undo_memory_header *header)
{
    b32 result = (header->ins_count == 0) && (header->del_count == 0);
    return result;
}

#if 0
inline u32 get_data_size(const undo_memory_header *header)
{
    u32 result = header->del_count * sizeof(piece);
    return result;
}
static inline void *get_data_start(const undo_memory_header *header)
{
    void *result = 0;
    if (header->del_count)
    {
        result = (void *) (header + 1);
    }
    return result;
}
#endif
static inline piece *get_pieces_from_header(const undo_memory_header *header) 
{
    piece *result = 0;
    if (header->del_count)
    {
        result = (piece *) (header + 1);
    }
    return result;
}

static b32 merge_if_possible(undo_memory_block *a, const undo_memory_block *b)
{
    b32 result = false;

    u8 *start_of_a = (u8 *) a;
    u8 *start_of_b = (u8 *) b;
    u8 *end_of_a   = start_of_a + a->size;

    if (end_of_a == start_of_b)
    {
        a->next = b->next;
        a->size += b->size;
        result = true;
    }
    return result;
}

static void insert_block(history *history, undo_memory_block *block)
{
    if (history->first_block)
    {
        if (merge_if_possible(block, history->first_block))
        {
            history->first_block = block;
        }
        else
        {
            block->next = history->first_block;
            history->first_block = block;
        }
    }
    else
    {
        history->first_block = block;
    }
}

static undo_memory_block *find_block_for_size(
    const history *history,
    const memory_index size,
    undo_memory_block **prev)
{
    undo_memory_block *result = 0;
    for (undo_memory_block *block = history->first_block; block; block = block->next)
    {
        if (block->size >= size)
        {
            result = block;
            break;
        }
        *prev = block;
    }
    return result;
}

static inline memory_index get_size_for_undo_data(const u32 del_count)
{
    memory_index result = sizeof(undo_memory_header) + del_count * sizeof(piece);
    return result;
}

static void *allocate_memory_block(
    history *history,
    memory_arena *history_arena,
    memory_index size)
{
    void *result = 0;

    undo_memory_block *prev = 0;
    undo_memory_block *block = find_block_for_size(history, size, &prev);
    if (block)
    {
        Assert(block->size >= size);
        result = (void *) block;
        if ((block->size - size) >= sizeof(undo_memory_block))
        {
            undo_memory_block *remaining = (undo_memory_block *) (((u8 *) result) + size);
            remaining->size = block->size - size;
            block->size = size;
            LIST_REPLACE(prev, block, remaining, history->first_block);
        }
        else
        {
            LIST_REMOVE(prev, block, history->first_block);
        }
    }
    else
    {
        result = push_size(history_arena, size, NoClear());
    }
    return result;
}

static undo_memory_header *allocate_undo_memory_block(
    history *history,
    memory_arena *history_arena,
    const u32 del_count)
{
    memory_index size = get_size_for_undo_data(del_count);

    undo_memory_header *data = allocate_memory_block(history, history_arena, size);
    if (!data)
    {
        abort();
    }
    data->next = 0;
    return data;
}

static inline void free_memory_block(history *history, void *ptr, memory_index block_size)
{
    undo_memory_block *block = (undo_memory_block *) ptr;
    block->size = block_size;
    block->next = 0;
    insert_block(history, block);
}

static void free_undo_memory_block(history *history, undo_memory_header *header)
{
    for (undo_memory_header *tmp = header; tmp;)
    {
        Assert(tmp->ref_count > 0);
        tmp->ref_count--;
        undo_memory_header *next = tmp->next;
        if (tmp->ref_count == 0)
        {
            memory_index size_of_block = get_size_for_undo_data(tmp->del_count);
            free_memory_block(history, (void *) tmp, size_of_block);
        }
        tmp = next;
    }
}

static inline void initialize_undo_history(history *history)
{
    history->root = 0;
    history->curr_node = history->root;
    history->first_block = 0;
}

static inline undo_node *allocate_tree_node(
    memory_arena *arena,
    history *history) 
{
    undo_node *new_node;
    FREELIST_ALLOCATE(
        new_node,
        history->free_node, 
        PushStruct(arena, undo_node, default_arena_params()));
    return new_node;
}

static inline void insert_node(history *history, undo_node *node)
{
    if (history->curr_node)
    {
        Assert(history->root);
        node->next = history->curr_node->first_child;
        history->curr_node->first_child = node;
        node->parent = history->curr_node;
        history->curr_node = node;
    }
    else 
    {
        if (history->root)
        {
            node->next = history->root;
        }
        history->root = node;
        history->curr_node = node;
    }
}

static void insert_at_current(
    history *history,
    memory_arena *history_arena,
    undo_memory_header *data,
    buffer_cursor bc)
{
    undo_node *tree = allocate_tree_node(history_arena, history);
    tree->data = data;
    tree->bc = bc;

    if (history->curr_node)
    {
        Assert(history->root);
        tree->next = history->curr_node->first_child;
        history->curr_node->first_child = tree;
        tree->parent = history->curr_node;
        history->curr_node = tree;
    }
    else 
    {
        if (history->root)
        {
            tree->next = history->root;
        }
        history->root = tree;
        history->curr_node = tree;
    }
}

static inline void append_to_current(history *history, undo_memory_header *data)
{
    LIST_INSERT(history->curr_node->data, data);
}

static inline undo_node *undo_node_pop(history *history)
{
    undo_node *result = 0;

    if (history->curr_node)
    {
        result = history->curr_node;
        history->curr_node = history->curr_node->parent;
    }
    return result;
}

static inline undo_node *undo_node_unpop(history *history)
{
    undo_node *result = 0;
    if (history->root)
    {
        if (history->curr_node)
        {
            if (history->curr_node->first_child)
            {
                history->curr_node = history->curr_node->first_child;
                result = history->curr_node;
            }
        }
        else
        {
            result = history->root;
            history->curr_node = history->root;
        }
    }
    return result;
}

