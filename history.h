
typedef struct undo_memory_header
{
    u32 abs_idx;
    u16 ins_count;
    u16 del_count;
    struct undo_memory_header *next;
} undo_memory_header;

static b32 headers_are_equal(undo_memory_header *a, undo_memory_header *b)
{
    if (!a)
    {
        return (b == 0);
    }
    if (!b)
    {
        return (a == 0);
    }

    b32 same_idx = (a->abs_idx == b->abs_idx);
    b32 same_ins_count = (a->ins_count == b->ins_count);
    b32 same_del_count = (a->del_count == b->del_count);
    b32 same_next = headers_are_equal(a->next, b->next);

    b32 result  = same_idx && (same_ins_count) && same_del_count && same_next;
    return result;
}

typedef struct undo_memory_block
{
    struct undo_memory_block *next; 
    memory_index size;             

} undo_memory_block;

static b32 blocks_are_equal(undo_memory_block *a, undo_memory_block *b)
{
    if (!a)
    {
        return (b == 0);
    }
    if (!b)
    {
        return (a == 0);
    }

    b32 result = (a->size = b->size) && blocks_are_equal(a->next, b->next);
    return result;
}

// TODO: maybe use a large virtual alloc for the undo history,
// and turn pointers into indexes of size u32. 
// This should be reasonable since, there are as many nodes as 
// non contiguous edits, and it is very unlikely that 
// an editing session exceeds 2 ^ 32 -1, non contiguous edits, 
// even 2 ^ 16 - 1 is a bit much;
//
// We could get the pointer to the a node, by adding its index 
// to the start of the memory block, and then casting.

// typedef struct
// {
//
// } undo_data 


typedef struct undo_node 
{
    struct undo_node *next; 
    struct undo_node *prev;
    struct undo_node *first_child;
    struct undo_node *last_child;
    struct undo_node *parent;

    // u32 pos;
    u32 cx;
    u32 cy;

    undo_memory_header *data;
} undo_node;

static b32 trees_are_equal(undo_node *a, undo_node *b)
{
    if (!a)
    {
        return (b == 0);
    }
    if (!b)
    {
        return (a == 0);
    }

    b32 equal_cx = a->cx == b->cx;
    b32 equal_cy = a->cy == b->cy;
    // b32 equal_pos = a->pos == b->pos;
    b32 equal_headers = headers_are_equal(a->data, b->data);
    b32 equal_next    = trees_are_equal(a->next, b->next);
    b32 equal_child   = trees_are_equal(a->first_child, b->first_child);

    b32 result = equal_cx && equal_cy && equal_headers && equal_next && equal_child;
    return result;
}

typedef struct history
{
    undo_node *root;
    undo_node *curr_node;
    undo_memory_block *first_block;
} history;

static inline b32 histories_are_equal(history a, history b)
{
    b32 roots_are_equal = trees_are_equal(a.root, b.root);
    b32 curr_is_equal = trees_are_equal(a.curr_node, b.curr_node);
    b32 equal_blocks = blocks_are_equal(a.first_block, b.first_block);
    b32 result =  roots_are_equal && curr_is_equal && equal_blocks;
    return result;
}




