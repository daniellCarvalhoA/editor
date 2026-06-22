
typedef struct cursor
{
    segmented_node *node;
    u32 piece_index;
} cursor;

typedef struct search
{
    u32 abs_idx;
    u32 in_piece;
    u32 in_line;
    u32 piece_pos;
    u32 piece_line;
    offset piece_offset;
    cursor search_cursor;
} search;
