

static inline segmented_node *allocate_node(piece_list *list)
{
    segmented_node *node;
    FREELIST_ALLOCATE(node, list->first_free_node, PushStruct(&list->list_arena, segmented_node, NoClear()));
    node->count = node->size = node->lcnt = 0;
    return node;
}

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


