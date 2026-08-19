typedef string command_buffer;

static command_buffer allocate_command_buffer(u32 capacity);
static void free_command_buffer(command_buffer *buffer);

static inline void append_char(command_buffer *buffer, str s)
{
    Assert(buffer->len + s.len <= buffer->capacity);
    memcpy(buffer->buffer + buffer->len, s.buffer, sizeof(u8) * s.len);
    buffer->len += s.len;
}

static inline void pop(command_buffer *buffer)
{
    if (buffer->len > 0)
    {
        utf8proc_int32_t cp;
        buffer->len = utf8_prev_codepoint(buffer->buffer, buffer->len, &cp);
        Assert(cp);
    }
}

static inline void clear_buffer(command_buffer *buffer)
{
    buffer->len = 0;
}

