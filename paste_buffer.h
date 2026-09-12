typedef enum
{
    Cursor,
    Line,
} paste_type;

#if 0
typedef struct
{
    piece_list *buffer;         // 8 bytes;
    u32 start;                  // 4 bytes;
    u32 end;                    // 4 bytes;
    u32 count;                  // 4 bytes; if count is non zero this is not a shared buffer
    union                       // 8 bytes; 
    {
        piece *pieces;
        undo_memory_header *header;
    };
    paste_type type;
} paste_buffer;

#endif

typedef enum
{
    BufferType_Pieces,
    BufferType_AsStr,
} p_buffer_type;

typedef struct
{
    p_buffer_type type;
    paste_type p_type;
    union
    {
        struct 
        {
            piece_list *buffer;
            u32 capacity;
            u32 count;
            piece *pieces;
        };

        string text;
    };

} p_buffer;

// static inline void reset_paste_buffer(p_buffer *buffer);
static inline b32 is_empty(p_buffer *buffer);
static void free_paste_buffer(p_buffer *buffer);


static inline u32 get_allocation_size_in_bytes(p_buffer *buffer)
{
    u32 result = 0;
    if (buffer->type == BufferType_Pieces)
    {
        result = sizeof(piece) * buffer->capacity;
    }
    else
    {
        result = sizeof(u8) * buffer->text.capacity;
    }
    return result;
}

