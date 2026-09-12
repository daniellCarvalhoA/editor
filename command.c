static inline command_buffer allocate_command_buffer(u32 size)
{
    command_buffer result = allocate_string(size);
    return result;
}

#if TESTS
static void free_command_buffer(command_buffer *buffer)
{
    if (buffer && buffer->buffer)
    {
        free(buffer->buffer);
    }
}
#endif

typedef enum
{
    ParseFlags_Error             = 0x0,
    ParseFlags_Quit              = 0x1,
    ParseFlags_Save              = 0x2,
    ParseFlags_Open              = 0x4,
    ParseFlags_Search            = 0x8,
    ParseFlags_Replace           = 0x10,
    ParseFlags_NoSearchHighlight = 0x20,
    ParseFlags_Force             = 0x40,
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
                    (tokenizer->at[0] == '!') || 
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
        else if (token_equals(c_token, (u8 *) "q!", 2))
        {
            tree->flags |= ParseFlags_Quit;
            tree->flags |= ParseFlags_Force;
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
        else if (token_equals(c_token, (u8 *) "nh", 2))
        {
            tree->flags |= ParseFlags_NoSearchHighlight;
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
    command_buffer *c_buffer = &state->screen.command_window->c_buffer;

    if (p_tree.flags & ParseFlags_Save)
    {
        Assert(!(p_tree.flags & ParseFlags_Open));
        write_buffer_to_file(state->screen.active_window->buffer);
        state->screen.active_window->buffer->up_to_date = true;
    }

    if (p_tree.flags & ParseFlags_Open)
    {
        Assert(!(p_tree.flags & ParseFlags_Quit));
        Assert(!(p_tree.flags & ParseFlags_Save));

        piece_list *buffer = find_buffer_by_name(state, p_tree.filename); 
        if (!buffer)
        {
            char *filename = (char *) malloc(p_tree.filename.len + 1);
            strncpy(filename, (const char *) p_tree.filename.buffer, p_tree.filename.len);
            filename[p_tree.filename.len] = '\0';
            
            buffer = create_buffer(&state->arena, filename);
            list_add_tail(&buffer->list, &state->buffers);
        }

        list_del(&state->screen.active_window->next_in_buffer);

        map_buffer_to_window(buffer, state->screen.active_window);

        clear_window(state->screen.active_window);

        state->screen.active_window->change |= Render_BufferExchange;
        clear_buffer(c_buffer);
    }

    if (p_tree.flags & ParseFlags_Quit)
    {
        clear_buffer(c_buffer);
        Assert(!(p_tree.flags & ParseFlags_Open));
        piece_list *buffer = state->screen.active_window->buffer;

        if (buffer->up_to_date || (p_tree.flags & ParseFlags_Force))
        {
            close_active_window(&state->screen);
            result = (!state->screen.num_leaf_windows);
        }
        else
        {
            append_char(c_buffer, STR_LIT("Changes not yet committed!"));
        }
    }

    if (p_tree.flags & ParseFlags_Search)
    {
        piece_list *buffer = interacting_window->buffer;
        if (p_tree.flags & ParseFlags_Replace)
        {
            end_undo(interacting_window);
            buffer->num_matches = 0;
            buffer->current_match = 0;
            buffer->match_len = 0;
            buffer->changed = true;
            buffer->replace_len = 0;
        }
        else
        {
            copy_to(p_tree.search_string, &buffer->last_searched_string);
            if (buffer->num_matches > 0)
            {
                interacting_window->dc = interacting_window->bc = cursor_from_position(
                    buffer,
                    &buffer->iter,
                    buffer->matches[buffer->current_match]);
            }
        }
        clear_buffer(c_buffer);
    }

    if (p_tree.flags & ParseFlags_NoSearchHighlight)
    {
        state->screen.change &= ~Render_ShowSearchHighlight;
        piece_list *buffer = interacting_window->buffer;
        buffer->changed = true;
        clear_buffer(c_buffer);

    }

    state->screen.command_window->bc.x = 0;
    return result;
}


static void backtrack(window *win)
{
    Assert(win);
    piece_list *buffer = win->buffer;
    Assert(is_in_middle_of_undo(&buffer->undo_records));
    end_undo(win);

    record_iter iter = get_records(&buffer->undo_records);

    u32 old_position = position_from_cursor(
        buffer, &buffer->iter, interacting_window->bc);

    u32 new_position = old_position;
    if (iter.count != 0)
    {
        new_position = iter.position;
    }

    undo_record *record;
    while ((record = get_record(&iter)))
    {
        undo_once(buffer, record, NULL);
    }

    buffer_cursor new_cursor = cursor_from_position(
        buffer, &buffer->iter, new_position);
    interacting_window->bc = new_cursor;
    buffer->replace_len = 0;
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

            buffer->num_matches   = 0;
            buffer->current_match = 0;
            buffer->match_len     = 0;
            buffer->changed       = true;

            if (is_in_middle_of_undo(&buffer->undo_records))
            {
                backtrack(interacting_window);
            }

            if (buffer->last_searched_string.len > 0)
            {
                search_str(buffer, buffer->last_searched_string);
                buffer->match_len = buffer->last_searched_string.len;
            }
        } break;

        case '\r':
        {
            screen->active_window = interacting_window;
            result = process_command(state);
            // clear_buffer(c_buffer);
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
                screen->change |= Render_ShowSearchHighlight;
                if (tree.flags & ParseFlags_Replace)
                {
                    Assert(is_in_middle_of_undo(&buffer->undo_records));
                    if (tree.replace_string.len == 0)
                    {
                        backtrack(interacting_window);
                    }
                    else
                    {
                        buffer->replace_len = tree.replace_string.len;
                        // buffer->replace_len--;
                        u32 removed_size = 0;
                        u32 added_size   = 0;

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

                        for (u32 i = 0; i < buffer->num_matches; ++i)
                        {
                            // There already is a dummy piece in the deleted places; 
                            // We just need to append.
                            u32 match_position = buffer->matches[i];

                            base_iter location = find_position(
                                buffer,
                                &buffer->iter,
                                match_position + added_size - removed_size);
                            fix_iter_(buffer, &location);

                            Assert(location.pos_in_piece == 0);

                            piece *piece = get_piece(&location);

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
                }
                else
                {
                    if (is_in_middle_of_undo(&buffer->undo_records))
                    {
                        end_undo(interacting_window);

                        record_iter iter = get_records(&buffer->undo_records);

                        u32 old_position = position_from_cursor(
                            buffer,
                            &buffer->iter,
                            interacting_window->bc);

                        u32 new_position = old_position;
                        if (iter.count != 0)
                        {
                            new_position = iter.position;
                        }

                        undo_record *record;
                        while ((record = get_record(&iter)))
                        {
                            undo_once(buffer, record, NULL);
                        }

                        buffer_cursor new_cursor = cursor_from_position(
                            buffer,
                            &buffer->iter,
                            new_position);
                        interacting_window->bc = new_cursor;
                    }

                    if (buffer->num_matches > 0)
                    {
                        buffer->matches_capacity = 0;
                        buffer->num_matches = 0;
                        buffer->current_match = 0;
                    }
                    search_str(buffer, tree.search_string);

                    if (buffer->num_matches > 0)
                    {
                        interacting_window->dc = interacting_window->bc = 
                            cursor_from_position(
                                buffer, 
                                &buffer->iter,
                                buffer->matches[buffer->current_match]);
                    }

                }
                buffer->changed = true;
            }
            else if (buffer->num_matches > 0)
            {
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
                screen->change |= Render_ShowSearchHighlight;
                piece_list *buffer = interacting_window->buffer;
                if ((tree.flags & ParseFlags_Replace) && (tree.replace_string.len > 0))
                {
                    buffer->replace_len = tree.replace_string.len;
                    if (!is_in_middle_of_undo(&buffer->undo_records))
                    {
                        begin_undo(interacting_window);
                        piece piece = make_piece(buffer, tree.replace_string);

                        for (u32 i = 0; i < buffer->num_matches; ++i)
                        {
                            u32 match_position = buffer->matches[buffer->num_matches - i - 1];
                            // This is stupid!! make range_replace api better, so it can also be passed a position range.
                            buffer_cursor start = cursor_from_position(
                                buffer, &buffer->iter, match_position);
                            buffer_cursor end   = cursor_from_position(
                                buffer, &buffer->iter, match_position + buffer->match_len);
                            piece_slice p_slice = { .base = &piece, .count = 1 };
                            range_replace(buffer, start, end, p_slice);
                        }
                    }
                    else
                    {
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

                        if (lines_inserted > 0)
                        {
                            buffer->append.lines[buffer->append.num_lines++] = buffer->append.text_len;
                        }

                        u32 removed_size = 0;
                        u32 added_size = 0;
                        reset_cursor(buffer, &buffer->iter);
                        for (u32 i = 0; i < buffer->num_matches; ++i)
                        {
                            // There already is a dummy piece in the deleted places; 
                            // We just need to append.
                            u32 match_position = buffer->matches[i];
                            base_iter location = find_position(
                                buffer,
                                &buffer->iter,
                                match_position + added_size - removed_size);
                            fix_iter_(buffer, &location);

                            Assert(location.pos_in_piece == 0);

                            piece *piece = get_piece(&location);

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
                        buffer->num_matches = 0;
                        buffer->current_match = 0;
                    }
                    search_str(buffer, tree.search_string);

                    if (buffer->num_matches > 0)
                    {
                        interacting_window->bc = interacting_window->dc =
                            cursor_from_position(
                                buffer,
                                &buffer->iter,
                                buffer->matches[buffer->current_match]);
                    }
                }
                buffer->changed = true;
            }

        } break;
    }
    return result;
}
