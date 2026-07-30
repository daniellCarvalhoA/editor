
typedef struct undo_memory_header
{
    union
    {
        struct undo_memory_header *next;
        u32 count;
    };
    u16 abs_idx;
    u16 ins_count;
    u16 del_count;
    u16 ref_count; 
    // May put the next pointer as a footer in the data
} undo_memory_header;

typedef struct undo_memory_block
{
    struct undo_memory_block *next; 
    memory_index size;             
} undo_memory_block;

typedef struct undo_node 
{
    struct undo_node *next; 
    struct undo_node *prev;
    struct undo_node *first_child;
    struct undo_node *last_child;
    struct undo_node *parent;

    buffer_cursor bc;

    undo_memory_header *data;
} undo_node;

typedef struct history
{
    undo_node *root;
    undo_node *curr_node;
    undo_node *free_node;
    undo_memory_block *first_block;
} history;

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

    b32 same_idx       = (a->abs_idx == b->abs_idx);
    b32 same_ref_count = (a->ref_count == b->ref_count);
    b32 same_ins_count = (a->ins_count == b->ins_count);
    b32 same_del_count = (a->del_count == b->del_count);
    b32 same_next      = headers_are_equal(a->next, b->next);

    b32 result  = same_ref_count && same_idx && same_ins_count && same_del_count && same_next;
    return result;
}

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

    b32 equal_cx      = a->bc.x == b->bc.x;
    b32 equal_cy      = a->bc.y == b->bc.y;
    b32 equal_headers = headers_are_equal(a->data, b->data);
    b32 equal_next    = trees_are_equal(a->next, b->next);
    b32 equal_child   = trees_are_equal(a->first_child, b->first_child);

    b32 result = equal_cx && equal_cy && equal_headers && equal_next && equal_child;
    return result;
}

static inline b32 histories_are_equal(history a, history b)
{
    b32 roots_are_equal = trees_are_equal(a.root, b.root);
    b32 curr_is_equal   = trees_are_equal(a.curr_node, b.curr_node);
    b32 equal_blocks    = blocks_are_equal(a.first_block, b.first_block);
    b32 result =  roots_are_equal && curr_is_equal && equal_blocks;
    return result;
}




