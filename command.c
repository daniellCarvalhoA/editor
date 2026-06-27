
typedef enum
{
    ParseFlags_Error = 0x0,
    ParseFlags_Quit  = 0x1,
    ParseFlags_Save  = 0x2,
    ParseFlags_Open  = 0x4,
} parse_flags;

typedef struct
{
    parse_flags flags;
    u32 filename_size;
    u8 *filename;
} parse_tree;

// TODO: Make this utf8.
typedef enum
{
    Token_Colon, // For now we are supportting: quit, write, write and quit, open.
    Token_Identifier,
    Token_EndOfStrem,
    Token_ForwardSlash,
} token_type;

typedef struct 
{
    token_type type;
    size_t len;
    u8 *text;
} token;

typedef struct 
{
    u8 *at;
    u8 *end;
} tokenizer;

static inline b32 is_space(u8 c)
{
    b32 result = (c == ' ');
    return result;
}

static inline b32 token_equals(token token, u8 *match, u32 size)
{
    u8 *at =  match;
    for (u32 i = 0; i < token.len; ++i, ++at)
    {
        if ((at >=  match + size) || ((*at != token.text[i])))
        {
            return false;
        }
    }
    b32 result = at == (match + size);
    return result;
}

static void skip_whitespace(tokenizer *tokenizer)
{
    while (tokenizer->at != tokenizer->end && is_space(tokenizer->at[0]))
    {
        ++tokenizer->at;
    }
}

static inline b32 is_alpha(u8 c)
{
    b32 result = (((c >= 'a') && (c <= 'z')) || ((c >= 'A') && (c <= 'Z')));
    return result;
}

static inline b32 is_number(u8 c)
{
    b32 result = ((c >= '0') && (c <= '9'));
    return result;
}

static inline b32 is_alhpha_numeric(u8 c)
{
    b32 result = is_alpha(c) || is_number(c);
    return result;
}

static token get_token(tokenizer *tokenizer)
{
    // NOTE: This assumes only ascii chars.
    // TODO: Make this utf8 aware;
    skip_whitespace(tokenizer);

    token token = {};
    if (tokenizer->at == tokenizer->end)
    {
        token.type = Token_EndOfStrem;
        return token;
    }

    token.len = 1;
    token.text = tokenizer->at;

    u8 c = tokenizer->at[0];
    ++tokenizer->at;

    switch (c)
    {
        case ':': { token.type = Token_Colon; } break;
        case '/': { token.type = Token_ForwardSlash; } break;
        default:
        {
            token.type = Token_Identifier;
            while ((tokenizer->at != tokenizer->end) && 
                   ((is_alhpha_numeric(tokenizer->at[0]) || 
                    (tokenizer->at[0] == '_') ||
                    (tokenizer->at[0] == '.'))))
            {
                ++tokenizer->at;

            }
            token.len = tokenizer->at - token.text;
        } break;
    }

    return token;
}

static inline b32 require_token(tokenizer *tokenizer, token_type desired_type) 
{
    token token = get_token(tokenizer);
    b32 result = token.type == desired_type;
    return result;
}

typedef enum
{
    Wait,
    Exit,
    Err,
} command_parse_state;

static void parse_comm(parse_tree *tree, tokenizer *tokenizer)
{
    token c_token = get_token(tokenizer);
    if (token_equals(c_token, (u8 *) "w", 1))
    {
        tree->flags |= ParseFlags_Save;
    } 
    else if (token_equals(c_token, (u8 *) "q", 1))
    {
        tree->flags |= ParseFlags_Quit;
    }
    else if (token_equals(c_token, (u8 *) "wq", 2))
    {
        tree->flags |= ParseFlags_Quit;
        tree->flags |= ParseFlags_Save;
    }
    else if (token_equals(c_token, (u8 *) "e", 1))
    {
        token file_token = get_token(tokenizer);
        tree->flags |= ParseFlags_Open;
        tree->filename = file_token.text;
        tree->filename_size = file_token.len;
    }
}

static parse_tree parse_command_tree(command_buffer *buffer) 
{
    parse_tree tree = {};
    tokenizer tokenizer = { .at = buffer->buffer, .end = buffer->buffer + buffer->len };

    if (require_token(&tokenizer, Token_Colon))
    {
        parse_comm(&tree, &tokenizer);
    }

    return tree;
}


static b32 process_command(editor_state *state)
{
    b32 result = false;
    parse_tree p_tree = parse_command_tree(&state->screen.command_window->c_buffer);

    if (p_tree.flags & ParseFlags_Save)
    {
        Assert(!(p_tree.flags & ParseFlags_Open));
        write_buffer_to_file(active_window->buffer);
    }

    if (p_tree.flags & ParseFlags_Open)
    {
        Assert(!(p_tree.flags & ParseFlags_Quit));
        Assert(!(p_tree.flags & ParseFlags_Save));

        char *filename_nullterminated = (char *) malloc(p_tree.filename_size + 1);
        strncpy(filename_nullterminated, (const char *) p_tree.filename, p_tree.filename_size);
        filename_nullterminated[p_tree.filename_size] = '\0';
        
        // TODO: Check if buffer is already loaded into memory.
        piece_list *buffer = create_buffer(&state->arena, filename_nullterminated);

        // list_replace(&active_window->next_in_buffer, &buffer->list);
        list_add_tail(&buffer->list, &state->buffers);

        list_del(&active_window->next_in_buffer);
        // list_add(&active_window->next_in_buffer, &buffer->window_sentinel);

        map_buffer_to_window(buffer, active_window);

        clear_window(active_window);

        active_window->change |= Render_BufferExchange;

    }

    if (p_tree.flags & ParseFlags_Quit)
    {
        Assert(!(p_tree.flags & ParseFlags_Open));

        close_active_window(&state->screen);

        result = list_is_empty(&state->buffers);

    }
    clear_buffer(&state->screen.command_window->c_buffer);
    state->screen.command_window->bcx = 0;

    return result;


}

static b32 parse_command(editor_state *state, u8 *input, u32 input_size)
{
    b32 result = false;
    active_window->change |= Render_BufferChange;
    switch (*input)
    {
        case '\x1b':
        {
            active_window = interacting_window;
            clear_buffer(&state->screen.command_window->c_buffer);
            active_window->bcx = 0;
            active_window->bcy = 0;
        } break;

        case '\r':
        {
            active_window = interacting_window;
            result = process_command(state);
            clear_buffer(&state->screen.command_window->c_buffer);
        } break;

        case 127:
        {
            pop(&state->screen.command_window->c_buffer);
            active_window->bcx--;
        } break;

        default:
        {
            append_char(&state->screen.command_window->c_buffer, input, input_size);
            active_window->bcx++;
        } break;
    }
    return result;
}
