
static command_buffer allocate_command_buffer(u32 capacity)
{
    command_buffer buffer = {};
    buffer.capacity = capacity;
    buffer.buffer = (u8 *) malloc(sizeof(u8) * capacity);
    return buffer;
}

static void free_command_buffer(command_buffer *buffer)
{
    if (buffer && buffer->buffer)
    {
        free(buffer->buffer);
    }
}

typedef enum
{
    ParseFlags_Error = 0x0,
    ParseFlags_Quit  = 0x1,
    ParseFlags_Save  = 0x2,
    ParseFlags_Open  = 0x4,
    ParseFlags_Search = 0x8,
    ParseFlags_Replace = 0x10,
} parse_flags;

typedef struct
{
    parse_flags flags;
    str filename;
    str search_string;
    str replace_string;
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

static inline b32 is_alpha_numeric(u8 c)
{
    b32 result = is_alpha(c) || is_number(c);
    return result;
}

static token get_token(tokenizer *tokenizer, b32 skip_space)
{
    // NOTE: This assumes only ascii chars.
    // TODO: Make this utf8 aware;

    if (skip_space)
    {
        skip_whitespace(tokenizer);
    }

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
                   ((is_alpha_numeric(tokenizer->at[0]) || 
                    (tokenizer->at[0] == '_') ||
                    (tokenizer->at[0] == '.') ||
                    (!skip_space && tokenizer->at[0] == ' '))))
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
    token token = get_token(tokenizer, true);
    b32 result = token.type == desired_type;
    return result;
}

typedef enum
{
    Wait,
    Exit,
    Err,
} command_parse_state;


static b32 parse_search_and_replace(parse_tree *tree, tokenizer *tokenizer)
{
    b32 result = true;
    if (require_token(tokenizer, Token_ForwardSlash))
    {
        token s_token = get_token(tokenizer, false);
        if (s_token.type == Token_Identifier)
        {
            tree->search_string.buffer = s_token.text;
            tree->search_string.len = s_token.len;
            tree->flags |= ParseFlags_Search;

            if (require_token(tokenizer, Token_ForwardSlash))
            {
                tree->flags |= ParseFlags_Replace;
                token r_token = get_token(tokenizer, false);
                if (r_token.type == Token_Identifier)
                {
                    tree->replace_string.buffer = r_token.text;
                    tree->replace_string.len = r_token.len;
                }
            }
        }
        else
        {
            result = false;
        }
    }
    else
    {
        result = false;
    }
    return result;
}

static b32 parse_comm(parse_tree *tree, tokenizer *tokenizer)
{
    b32 result = true;
    if (require_token(tokenizer, Token_Colon))
    {
        token c_token = get_token(tokenizer, true);
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
            token file_token = get_token(tokenizer, true);
            tree->flags |= ParseFlags_Open;
            tree->filename.buffer = file_token.text;
            tree->filename.len = file_token.len;
        }
        else
        {
            result = false;
        }
    }
    else
    {
        result = false;
    }
    return result;
}

// static void parse_search(replace

static parse_tree parse_command_tree(command_buffer *buffer) 
{
    parse_tree tree = {};
    tokenizer tokenizer_ = { .at = buffer->buffer, .end = buffer->buffer + buffer->len };

    tokenizer prev = tokenizer_;

    if (parse_comm(&tree, &tokenizer_))
    {
        return tree;
    }

    tokenizer_ = prev;

    if (parse_search_and_replace(&tree, &tokenizer_))
    {
        return tree;
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
        write_buffer_to_file(state->screen.active_window->buffer);
    }

    if (p_tree.flags & ParseFlags_Open)
    {
        Assert(!(p_tree.flags & ParseFlags_Quit));
        Assert(!(p_tree.flags & ParseFlags_Save));

        char *filename_nullterminated = (char *) malloc(p_tree.filename.len + 1);
        strncpy(filename_nullterminated, (const char *) p_tree.filename.buffer, p_tree.filename.len);
        filename_nullterminated[p_tree.filename.len] = '\0';
        
        // TODO!: Check if buffer is already loaded into memory.
        piece_list *buffer = create_buffer(&state->arena, filename_nullterminated);

        list_add_tail(&buffer->list, &state->buffers);

        list_del(&state->screen.active_window->next_in_buffer);

        map_buffer_to_window(buffer, state->screen.active_window);

        clear_window(state->screen.active_window);

        state->screen.active_window->change |= Render_BufferExchange;
    }

    if (p_tree.flags & ParseFlags_Quit)
    {
        Assert(!(p_tree.flags & ParseFlags_Open));

        close_active_window(&state->screen);

        // NOTE. This is a hack. 
        result = (state->screen.active_window == state->screen.command_window);

    }

    if (p_tree.flags & ParseFlags_Search)
    {
        piece_list *buffer = interacting_window->buffer;
        if (p_tree.flags & ParseFlags_Replace)
        {
            insert_node(&buffer->history, buffer->staged);
            buffer->staged = NULL;
            buffer->num_matches = 0;
            buffer->current_match = 0;
            buffer->match_len = 0;
            buffer->changed = true;
            buffer->replace_len = 0;
            buffer->replaced = false;
        }
        else
        {
            buffer->last_searched_string.buffer = realloc(buffer->last_searched_string.buffer, sizeof(u8) * buffer->last_searched_string.len);
            memcpy(buffer->last_searched_string.buffer, p_tree.search_string.buffer, buffer->last_searched_string.len);

            if (buffer->num_matches > 0)
            {
                interacting_window->dc = interacting_window->bc = cursor_from_position(&buffer->iter, buffer->matches[buffer->current_match]);
            }
        }
    }

    // clear_buffer(&state->screen.command_window->c_buffer);
    state->screen.command_window->bc.x = 0;

    return result;
}

static b32 parse_command(editor_state *state, str input)
{
    b32 result = false;
    screen *screen = &state->screen;
    window *active_window = screen->active_window;

    active_window->change |= Render_BufferChange;

    command_buffer *c_buffer = &screen->command_window->c_buffer;
    switch (input.buffer[0])
    {
        case '\x1b':
        {
            screen->active_window = interacting_window;
            clear_buffer(c_buffer);
            active_window->bc = (struct buffer_cursor) {0};

            piece_list *buffer = interacting_window->buffer;
            buffer->num_matches = 0;
            buffer->current_match = 0;
            buffer->match_len = 0;
            buffer->changed = true;

            if (buffer->replaced)
            {
                Assert(buffer->staged);
                undo_node *node = buffer->staged;
                undo_memory_header *head = node->data;
                for (undo_memory_header *header = head;
                    header;
                    header = header->next)
                {
                    cursor start_cursor = abs_idx_to_cursor_2(buffer, header->abs_idx);
                    cursor end_cursor   = abs_idx_to_cursor_2(buffer, header->abs_idx + header->ins_count);

                    piece *pieces = get_pieces_from_header(header);
                    for (u32 i = 0; i < header->del_count; ++i)
                    {
                        piece piece = pieces[i];
                        buffer->size = piece.size;
                        buffer->lcnt = piece.lcnt;
                    }

                    replace(buffer, start_cursor, end_cursor, pieces, header->del_count);
                }

                free_undo_memory_block(&buffer->history, head);
                FREELIST_DEALLOCATE(buffer->staged, buffer->history.free_node);
                buffer->staged = NULL;
                reset_cursor_(&buffer->iter);

                buffer->replaced = false;
                buffer->replace_len = 0;
            }
            state->searching = false;

        } break;

        case '\r':
        {
            screen->active_window = interacting_window;
            result = process_command(state);
            clear_buffer(c_buffer);
            state->searching = false;
        } break;

        case 127:
        {
            pop(c_buffer);

            parse_tree tree = {};
            tokenizer tokenizer = { 
                .at = c_buffer->buffer,
                .end = c_buffer->buffer + c_buffer->len 
            };

            parse_search_and_replace(&tree, &tokenizer);

            piece_list *buffer = interacting_window->buffer;
            if (tree.flags & ParseFlags_Search)
            {
                if (tree.flags & ParseFlags_Replace)
                {
                    Assert(buffer->staged);
                    buffer->replace_len--;
                    u32 removed_size = 0;
                    u32 added_size = 0;

                    utf8proc_int32_t cp;
                    i32 prev = utf8_prev_codepoint(buffer->append.text, buffer->append.text_len, &cp);
                    Assert(prev >= 0);

                    str deleted_str = { 
                        .buffer = buffer->append.text + (u32) prev, 
                        .len = buffer->append.text_len - (u32) prev,
                    };

                    u32 bytes_deleted = deleted_str.len;
                    u32 lines_deleted = count_lines(deleted_str);

                    buffer->append.text_len -= bytes_deleted;
                    buffer->append.num_lines -= lines_deleted;
                    // TODO: Change search by position to search by absolute index, (obtained from buffer->staged);

                    // for (undo_memory_header *header = buffer->staged->data;
                    //     header;
                    //     header = header->next)
                    // {
                    //     base_iter location = find_abs_idx(&buffer->iter, header->abs_idx);
                    //     piece *piece = get_piece_(&location);
                    //     piece->size -= bytes_deleted;
                    //     piece->lcnt -= lines_deleted;
                    //
                    //     location.node->size -= bytes_deleted;
                    //     location.node->lcnt -= lines_deleted;
                    //
                    //     removed_size += buffer->match_len;
                    //     added_size += tree.replace_string.len;
                    // }
                    for (u32 i = 0; i < buffer->num_matches; ++i)
                    {
                        // There already is a dummy piece in the 
                        // deleted places; 
                        // We just need to append.
                        u32 match_position = buffer->matches[i];

                        base_iter location = find_position(&buffer->iter, match_position + added_size - removed_size);
                        fix_iter_(&location);

                        Assert(location.pos_in_piece == 0);

                        piece *piece = get_piece_(&location);

                        piece->size -= bytes_deleted;
                        piece->lcnt -= lines_deleted;

                        location.node->size -= bytes_deleted;
                        location.node->lcnt -= lines_deleted;

                        removed_size += buffer->match_len;
                        added_size += tree.replace_string.len;
                    }

                    buffer->size -= buffer->num_matches * bytes_deleted;
                    buffer->lcnt -= buffer->num_matches * lines_deleted;
                }
                else
                {
                    if (buffer->replaced)
                    {
                        Assert(buffer->staged);
                        undo_node *node = buffer->staged;
                        undo_memory_header *head = node->data;
                        buffer->replaced = false;
                        for (undo_memory_header *header = head;
                            header;
                            header = header->next)
                        {
                            cursor start_cursor = abs_idx_to_cursor_2(buffer, header->abs_idx);
                            cursor end_cursor   = abs_idx_to_cursor_2(buffer, header->abs_idx + header->ins_count);

                            piece *pieces = get_pieces_from_header(header);
                            for (u32 i = 0; i < header->del_count; ++i)
                            {
                                piece piece = pieces[i];
                                buffer->size = piece.size;
                                buffer->lcnt = piece.lcnt;
                            }

                            replace(buffer, start_cursor, end_cursor, pieces, header->del_count);
                        }

                        free_undo_memory_block(&buffer->history, head);
                        FREELIST_DEALLOCATE(buffer->staged, buffer->history.free_node);
                        buffer->staged = NULL;
                        // TODO: This should happend inside replace
                        reset_cursor_(&buffer->iter);
                    }

                    if (buffer->num_matches > 0u)
                    {
                        buffer->matches_capacity = 0;
                        buffer->num_matches = 0;
                        buffer->current_match = 0;
                    }
                    search_str(buffer, 0, tree.search_string);
                }
                buffer->changed = true;
            }
            else if (buffer->num_matches > 0)
            {
                // buffer->matches_capacity = 0;
                buffer->num_matches = 0;
                buffer->current_match = 0;
                buffer->changed = true;
            }
            active_window->bc.x--;
        } break;

        default:
        {
            append_char(c_buffer, input);
            active_window->bc.x++;

            parse_tree tree = {};
            tokenizer tokenizer = { 
                .at = c_buffer->buffer,
                .end = c_buffer->buffer + c_buffer->len };

            str prev_replace_string = tree.replace_string;
            parse_search_and_replace(&tree, &tokenizer);

            if (tree.flags & ParseFlags_Search)
            {
                piece_list *buffer = interacting_window->buffer;
                if (tree.flags & ParseFlags_Replace)
                {
                    buffer->replaced = true;
                    // if (tree.replace_string.len == 0)
                    if (!buffer->staged)
                    {
                        piece piece = make_piece(buffer, tree.replace_string);

                        // undo_node *node = NULL;

                        // Must iterate backwards because otherwise deleting from one position would invalidate the next;
                        undo_memory_header *head_header = NULL;
                        for (u32 i = 0; i < buffer->num_matches; ++i)
                        {
                            u32 match_position = buffer->matches[buffer->num_matches - i - 1];
                            buffer_cursor start = cursor_from_position(&buffer->iter, match_position);
                            buffer_cursor end   = cursor_from_position(&buffer->iter, match_position + buffer->match_len);
                            piece_range p_range = { .pieces = &piece, .count = 1 };
                            replace_result rep = range_replace(buffer, start, end, p_range);

                            rep.undo_header->next = head_header;
                            head_header = rep.undo_header;
                        }

                        if (head_header)
                        {
                            undo_node *node = allocate_tree_node(
                                &buffer->history_arena,
                                &buffer->history);
                            node->bc = interacting_window->bc;
                            node->data = head_header;

                            buffer->staged = node;
                        }
                    }
                    else
                    {
                        buffer->replace_len++;
                        Assert(tree.replace_string.len >= prev_replace_string.len);

                        utf8proc_int32_t cp;
                        i32 prev = utf8_prev_codepoint(tree.replace_string.buffer, tree.replace_string.len, &cp);
                        Assert(prev >= 0);
                        str inserted_str = { 
                            .buffer = tree.replace_string.buffer + (u32) prev, 
                            .len = tree.replace_string.len - (u32) prev
                        };

                        u32 bytes_inserted = inserted_str.len;
                        u32 lines_inserted = count_lines(inserted_str);

                        memcpy(buffer->append.text + buffer->append.text_len, inserted_str.buffer, inserted_str.len);
                        buffer->append.text_len += inserted_str.len;

                        if (lines_inserted > 0 )
                        {
                            buffer->append.lines[buffer->append.num_lines++] = buffer->append.text_len;
                        }

                        u32 removed_size = 0;
                        u32 added_size = 0;
                        reset_cursor_(&buffer->iter);
                        for (u32 i = 0; i < buffer->num_matches; ++i)
                        {
                            // There already is a dummy piece in the deleted places; 
                            // We just need to append.
                            u32 match_position = buffer->matches[i];
                            base_iter location = find_position(&buffer->iter, match_position + added_size - removed_size);
                            fix_iter_(&location);

                            Assert(location.pos_in_piece == 0);

                            piece *piece = get_piece_(&location);

                            piece->size += bytes_inserted;
                            piece->lcnt += lines_inserted;

                            location.node->size += bytes_inserted;
                            location.node->lcnt += lines_inserted;

                            removed_size += buffer->match_len;
                            added_size += tree.replace_string.len;
                        }

                        buffer->size += buffer->num_matches * bytes_inserted;
                        buffer->lcnt += buffer->num_matches * lines_inserted;
                    }
                }
                else
                {
                    if (buffer->matches)
                    {
                        // buffer->matches_capacity = 0;
                        buffer->num_matches = 0;
                        buffer->current_match = 0;
                    }
                    search_str(buffer, 0, tree.search_string);
                }
                buffer->changed = true;
            }

        } break;
    }
    return result;
}
