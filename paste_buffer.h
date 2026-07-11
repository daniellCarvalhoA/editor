typedef enum
{
    Cursor,
    Line,
} paste_type;


typedef struct
{
    piece_list *buffer;         // 8 bytes;
    u32 start;                  // 4 bytes;
    u32 end;                    // 4 bytes;
    u32 count;                  // 4 bytes; if count is non zero this is not a shared buffer
    edit_flags flags;           // 4 bytes;
    union                       // 8 bytes; 
    {
        piece *pieces;
        undo_memory_header *header;
    };
    paste_type type;
} paste_buffer;

static inline void free_paste_buffer(paste_buffer *buffer);
static inline piece *get_pieces(paste_buffer *buffer);
static inline u32 get_count(paste_buffer *buffer);
static inline void reset_paste_buffer(paste_buffer *buffer);
