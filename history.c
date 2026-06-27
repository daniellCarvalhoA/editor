inline b32 is_header_empty(const undo_memory_header *header)
{
    b32 result = (header->ins_count == 0) && (header->del_count == 0);
    return result;
}

inline u32 get_data_size(const undo_memory_header *header)
{
    u32 result = header->del_count * (sizeof(piece) + sizeof(buffer_type));
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

static inline piece *get_pieces_from_header(const undo_memory_header *header) 
{
    piece *result = 0;
    if (header->del_count)
    {
        result = (piece *) (header + 1);
    }
    return result;
}

static inline buffer_type *get_types_from_header(const undo_memory_header *header)
{
    buffer_type *result = 0;
    if (header->del_count)
    {
        result = (buffer_type *) (get_pieces_from_header(header) + header->del_count);
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
    // memory_index result = sizeof(undo_memory_header) + del_count * sizeof(piece) + del_count * sizeof(buffer_type);
    memory_index result = sizeof(undo_memory_header) + del_count * (sizeof(piece) + sizeof(buffer_type));
    return result;
}

static undo_memory_header *allocate_undo_memory_block(
    history *history,
    memory_arena *history_arena,
    const u32 del_count)
{
    memory_index size = get_size_for_undo_data(del_count);

    undo_memory_header *data = 0;
    undo_memory_block *prev = 0;
    undo_memory_block *block = find_block_for_size(history, size, &prev);

    if (block)
    {
        data = (undo_memory_header *) (block);
        if ((block->size - size) >=  sizeof(undo_memory_block))
        {
            undo_memory_block *remaining = (undo_memory_block *) (((u8 *) data) + size);
            remaining->size = (block->size - size);
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
        data = (undo_memory_header *) push_size(history_arena, size, NoClear());
    }
    data->next = 0;
    return data;
}

static void free_undo_memory_block(history *history, undo_memory_header *header)
{
    for (undo_memory_header *tmp = header; tmp;)
    {
        undo_memory_header *next = tmp->next;
        memory_index size_of_block = get_size_for_undo_data(tmp->del_count);
        undo_memory_block *block   = (undo_memory_block *) tmp;
        block->size = size_of_block;
        block->next = 0;
        insert_block(history, block);
        tmp = next;
    }
}

static inline void initialize_undo_history(history *history)
{
    history->root = 0;
    history->curr_node = history->root;
    history->first_block = 0;
}

static inline undo_node *allocate_tree_node(memory_arena *arena) 
{
    undo_node *result = PushStruct(arena, undo_node, default_arena_params());
    return result;
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
    u32 cx,
    u32 cy)
{
    undo_node *tree = allocate_tree_node(history_arena);
    tree->data = data;
    tree->cx = cx;
    tree->cy = cy;

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

static inline undo_memory_header **undo_node_pop(history *history)
{
    undo_memory_header **result = 0;

    if (history->curr_node)
    {
        result = &history->curr_node->data;
        history->curr_node = history->curr_node->parent;
    }
    return result;
}

static inline undo_memory_header **undo_node_unpop(history *history)
{
    undo_memory_header **result = 0;
    if (history->root)
    {
        if (history->curr_node)
        {
            if (history->curr_node->first_child)
            {
                history->curr_node = history->curr_node->first_child;
                result = &history->curr_node->data;
            }
        }
        else
        {
            result = &history->root->data;
            history->curr_node = history->root;
        }
    }
    return result;
}


