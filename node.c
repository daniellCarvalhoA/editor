
static inline b32 equal_offsets(const offset a, const offset b)
{
    b32 result = (a.row == b.row) && (a.col == b.col);
    return result;
}

static inline b32 pieces_are_equal(const piece a, const piece b)
{
    b32 result = (a.size == b.size) && (a.lcnt == b.lcnt) && (equal_offsets(a.off, b.off));
    return result;
}

static b32 nodes_are_equal(const segmented_node *a, const segmented_node *b)
{
    b32 result = (a->count == b->count) && (a->size == b->size) && (a->lcnt == b->lcnt);
    for (u32 i = 0; i < a->count; ++i)
    {
        result = result && (a->b_types[i] == b->b_types[i]);
        result = result && pieces_are_equal(a->pieces[i], b->pieces[i]);
    }
    return result;
}

static b32 semantic_equality(const segmented_node *sentinel_a, const segmented_node *sentinel_b)
{
    b32 result = true;

    segmented_node *node_a = sentinel_a->next;
    segmented_node *node_b = sentinel_b->next;
    u32 index_a = 0;
    u32 index_b = 0;

    for (;;)
    {
        piece *piece_a = 0;
        if (node_a != sentinel_a)
        {
            piece_a = node_a->pieces + index_a++;
            if (index_a == node_a->count)
            {
                index_a = 0;
                node_a = node_a->next;
            }
        }

        piece *piece_b = 0;
        if (node_b != sentinel_b)
        {
            piece_b = node_b->pieces + index_b++;
            if (index_b == node_b->count)
            {
                index_b = 0;
                node_b = node_b->next;
            }
        }

        if (!piece_a)
        {
            result = result && (!piece_b);
            break;
        }
        if (!piece_b)
        {
            result = result && (!piece_a);
            break;
        }

        result = result && pieces_are_equal(*piece_a, *piece_b);
    }

    return result;
}

static b32 strict_equality(const segmented_node *sentinel_a, const segmented_node *sentinel_b)
{
    b32 result = true;
    segmented_node *node_1 = sentinel_a->next;
    segmented_node *node_2 = sentinel_b->next;

    for (;;)
    {
        if (node_1 == sentinel_a)
        {
            Assert(node_2 == sentinel_b);
            result = result && (node_2 == sentinel_b);
            break;
        }

        result = result && (node_2 != sentinel_b);
        result = result && nodes_are_equal(node_1, node_2);

        node_1 = node_1->next;
        node_2 = node_2->next;
    }
    return result;
}

static inline segmented_node *allocate_node(piece_list *list)
{
    segmented_node *node;
    FREELIST_ALLOCATE(node, list->first_free_node, PushStruct(&list->list_arena, segmented_node, NoClear()));
    node->count = node->size = node->lcnt = 0;
    return node;
}

static void fix_size_and_lines(segmented_node *node)
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

static node_size_and_lcnt node_size_lcnt(const segmented_node *node)
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


