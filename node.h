#define MAX_PIECES_PER_NODE 32

typedef enum buffer_type
{
    BufferType_Append,
    BufferType_Original,
} buffer_type;


typedef struct offset
{
    u32 row;
    u32 col;
} offset;

#define INLINE_THRESHOLD 8

typedef struct piece
{
    u32 size;
    u32 lcnt;
    offset off;
   // union
   // {
     //   u8 data[INLINE_THRESHOLD]; 
                    // This path is not yet implemented and may never be?!! 
                    // The idea would be to inline text inside the piece itself if, 
                    // it is smaller than some threshold value.
                    //
                    // Lets say we have a lot of very small sized pieces (1 - 4 bytes maybe),
                    // in  sequence. When rendering, we incur a lot of cache misses if the text correspoinding 
                    // to the pieces is not layed out one after the other in their corresponding buffers.
                    // If we were to inline the text in the pieces we would bypass the buffer indirection.
                    //
                    // What to do if an edit makes a piece whose size was bigger than threshold, 
                    // smaller than the threshold.
                    //
                    // Possible rules:
                    //
                    // 1> if a sequence of insert mode edits results in a piece whose size is 
                    // less than or equal to the threshold, that piece gets inlined.
                    //
                    // To inline the piece we must copy the text from the buffer to the piece.
                    // We shall not remove the copied text from the buffer, since there may exist 
                    // undo pieces which reference it.
                    //
                    // 2> An edit of an inlined piece results in inlined pieces 
                    // (A normal mode edit of a piece never increases its size.
                    // 
                    // 3> if an edit of a non-inlined piece results in insertion of 
                    // a piece(s) whose size meet(s) the inlined criteria.
                    // => The inserted pieces will be inlined, copies must be made from the buffer 
                    // referenced by the affected piece.
                    // In this case we cannot delete the piece range from the buffer, 
                    // because other pieces may reference it. 
                    //
                    // Another approach.
                    //
                    // 1> and 2> stay the same but 
                    //
                    // 3> edits on non-inlined pieces remain non-inlined even if their sizes 
                    // meet the inlined criteria.
                    //   => This would mean that to ascertain the type of the piece (inlined vs non-inlined)
                    //   checking the size would not be the criteria.
                    //   We would have to add another enumerant to buffer_type.
    //};
    buffer_type type;
} piece;

typedef struct
{
    piece *base;
    u32 count;
} piece_slice;


typedef struct segmented_node
{
    u32 count;
    u32 size;
    u32 lcnt;
    piece pieces[MAX_PIECES_PER_NODE];
    // buffer_type b_types[MAX_PIECES_PER_NODE];
    struct segmented_node *next;
    struct segmented_node *prev;
} segmented_node;


static inline b32 equal_offsets(const offset a, const offset b)
{
    b32 result = (a.row == b.row) && (a.col == b.col);
    return result;
}

static inline b32 pieces_are_equal(const piece a, const piece b)
{
    b32 result = (a.size == b.size) && 
                 (a.lcnt == b.lcnt) && 
                 (equal_offsets(a.off, b.off)) &&
                 (a.type == b.type);
    return result;
}

static inline b32 nodes_are_equal(const segmented_node *a, const segmented_node *b)
{
    b32 result = (a->count == b->count) && (a->size == b->size) && (a->lcnt == b->lcnt);
    for (u32 i = 0; i < a->count; ++i)
    {
        // result = result && (a->b_types[i] == b->b_types[i]);
        result = result && pieces_are_equal(a->pieces[i], b->pieces[i]);
    }
    return result;
}

static inline b32 semantic_equality(const segmented_node *sentinel_a, const segmented_node *sentinel_b)
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

static inline b32 strict_equality(const segmented_node *sentinel_a, const segmented_node *sentinel_b)
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

