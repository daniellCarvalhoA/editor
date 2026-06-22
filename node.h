#define MAX_PIECES_PER_NODE 32

typedef struct offset
{
    u32 row;
    u32 col;
} offset;

typedef struct piece
{
    u32 size;
    u32 lcnt;
    union
    {
        offset off;
        u8 data[8]; // This path is not yet implemented!! 
                    // What to do if an edit makes a piece whose size was bigger than 64 bytes, 
                    // smaller than 64 bytes.
                    // Possible rules.
                    //
                    // 1> if a sequence of insert mode edits results in a piece whose size is 
                    // less than or equal to 64 bytes, that piece gets inlined.
                    //
                    // 2> An edit of an inlined piece results in an inlined pieces 
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
    };
} piece;

typedef enum buffer_type
{
    BufferType_Append,
    BufferType_Original,
} buffer_type;

typedef struct segmented_node
{
    u32 count;
    u32 size;
    u32 lcnt;
    piece pieces[MAX_PIECES_PER_NODE];
    buffer_type b_types[MAX_PIECES_PER_NODE];
    struct segmented_node *next;
    struct segmented_node *prev;
} segmented_node;
