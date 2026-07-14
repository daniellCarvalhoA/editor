typedef string command_buffer;

static command_buffer allocate_command_buffer(u32 capacity);
static void free_command_buffer(command_buffer *buffer);

static inline void append_char(command_buffer *buffer, u8 *s, u32 size)
{
    Assert(buffer->len + size <= buffer->capacity);
    memcpy(buffer->buffer + buffer->len, s, sizeof(u8) * size);
    buffer->len += size;
}

static inline void pop(command_buffer *buffer)
{
    if (buffer->len > 0)
    {
        buffer->len--;
    }
}

static inline void clear_buffer(command_buffer *buffer)
{
    buffer->len = 0;
}

