
typedef struct
{
    u32 len;
    u32 capacity;
    u8 *text;
} command_buffer;

// TODO: Deal with utf8.

static command_buffer allocate_command_buffer(u32 capacity)
{
    command_buffer buffer = {};
    buffer.capacity = capacity;
    buffer.text = (u8 *) malloc(sizeof(u8) * capacity);
    return buffer;
}

static inline void append_char(command_buffer *buffer, u8 *s, u32 size)
{
    Assert(buffer->len + size <= buffer->capacity);
    memcpy(buffer->text + buffer->len, s, sizeof(u8) * size);
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
