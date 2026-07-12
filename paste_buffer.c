
static inline b32 is_empty(paste_buffer *buffer)
{
    b32 result = buffer->buffer == 0;
    return result;
}
static inline void free_paste_buffer(paste_buffer *buffer)
{
    history *history = &buffer->buffer->undo_history;
    if (buffer->count)
    {
        Assert(buffer->pieces);
        free_memory_block(history, (void *) buffer->pieces, buffer->count * sizeof(piece));
    }
    else
    {
        free_undo_memory_block(history, buffer->header);
    }
    memset(buffer, 0, sizeof(paste_buffer));
}

static inline piece *get_pieces(paste_buffer *buffer)
{
    piece *result = 0;
    if (buffer->count > 0)
    {
        result = buffer->pieces;
    }
    else
    {
        result = get_pieces_from_header(buffer->header);
    }
    return result;
}

static inline u32 get_count(paste_buffer *buffer)
{
    u32 count = buffer->count;
    if (!count)
    {
        count = buffer->header->del_count;
    }
    return count;
}


static inline void reset_paste_buffer(paste_buffer *buffer)
{
    memset(buffer, 0, sizeof(paste_buffer));
}

static inline paste_type paste_type_from_motion(motion motion, mode edit_mode)
{
    paste_type result = Cursor;
    switch (motion)
    {
        case NoMotion:
        {
            // Assert(edit_mode == Visual);
        } break;;
        case Up:
        case Down: 
        case Absolute:
        {
            result = Line;
        } break;

        default:
        {
        } break;
    }
    return result;
}
