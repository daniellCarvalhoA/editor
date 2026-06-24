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
        u8 data[8]; // This path is not yet implemented and may never be!! 
                    // The idea would be to inline text inside the piece itself if, 
                    // it is smaller than some threshold value.
                    //
                    // Lets say we have a lot of very small sized pieces (1 - 4 bytes maybe),
                    // in  sequence. When rendering, we incur a lot of cache misses if the text correspoinding 
                    // to the pieces is not layed out one after the other in their corresponding buffers.
                    // If we were to inline the text in the pieces we would bypass the buffer indirection.
                    //
                    //  
                    // 
                    // What to do if an edit makes a piece whose size was bigger than threshold, 
                    // smaller than the threshold.
                    // Possible rules.
                    //
                    // 1> if a sequence of insert mode edits results in a piece whose size is 
                    // less than or equal to the threshold, that piece gets inlined.
                    //
                    // To inline the piece we must copy the text from the buffer to the piece.
                    // We shall not remove the copied text from the buffer, since there may exist 
                    // undo pieces which reference it.
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
