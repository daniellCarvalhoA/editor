static void edit(editor_state *state);

static parse_result parse_normal(editor_state *editor, u8 *token, u32 token_len) 
{
    parse_result result = Error;

    normal_parse_state *p_state = &editor->p_state;
    state_result *s_result = &p_state->s_result;
    window **active_window      = &editor->screen.active_window;

    if (p_state->state == Middle && p_state->s_result.motion == Search)
    {
        s_result->count = token_len;
        Assert(token_len < ArrayCount(s_result->match));
        memcpy(&s_result->match, token, token_len);
        if (editor->edit_mode == Visual)
        {
            (*active_window)->change |= Render_VisualModeCursorChange;
        }
        return Ok;
    }

    if (*token >= '1' && *token <= '9')
    {
        s_result->quantifier = s_result->quantifier * 10 + (*token - '0');
        p_state->state = Middle;
        return NotDone;
    }

    switch (*token)
    {
        case '.':
        {
            *s_result = editor->prev_command;
            result = Ok;
             
        } break;
        case ':':
        {
            interacting_window = *active_window;
            *active_window = editor->screen.command_window;
            parse_command(editor, token, token_len);
            
        } break;

        case '\x1b':
        {
            if (editor->edit_mode == Visual)
            {
                s_result->m_mod = NormalChange;
                (*active_window)->change |= Render_ModeChange;
                editor->edit_mode = Normal;
            }
        } break;
        case 'v':
        {
            if (editor->edit_mode == Normal)
            {
                s_result->m_mod = VisualChange;
                (*active_window)->vc = (*active_window)->bc;
            }
            else
            {
                Assert(editor->edit_mode == Visual);
                s_result->m_mod = NormalChange;

            }
            (*active_window)->change |= Render_ModeChange;
            result = Ok;

        } break;

        case ' ':
        {
            if (editor->edit_mode == Normal)
            {
                p_state->state = Meta;
                result = NotDone;
            }
        } break;

        case 'w':
        {
            if (p_state->state == Meta)
            {
                (*active_window)->change |= Render_ModeChange;
                s_result->m_mod = LayoutChange;
                result = Ok;
            }
            else
            {

            }
        } break;
        case '0':
        {
            switch (p_state->state)
            {
                case Start:
                {
                    s_result->motion = Zero;
                    result = Ok;

                } break;

                case Middle:
                {
                    if (s_result->action == NoAction)
                    {
                        s_result->quantifier *= 10;
                        result = NotDone;
                    }
                    else
                    {
                        result = Ok;
                    }
                } break;

                default:
                {
                    result = Error;
                } break;
            }
 
        } break;

        case 'y':
        {
            if (editor->edit_mode == Visual)
            {
                s_result->action = Yank;
                result = Ok;
            }
            else if (p_state->state == Middle && s_result->action == Yank)
            {
                s_result->motion = Down;
                result = Ok;
            }
            else if (p_state->state == Start)
            {
                s_result->action = Yank;
                p_state->state = Middle;
                result = NotDone;
            }

        } break;

        case 'd':
        {
            if (editor->edit_mode == Visual)
            {
                s_result->action = Delete;
                result = Ok;
            }
            else if (p_state->state == Middle && s_result->action == Delete)
            {
                s_result->motion = Down;
                result = Ok;
            } 
            else if (p_state->state == Start)
            {
                s_result->action = Delete;
                p_state->state = Middle;
                result = NotDone;
            } 
        } break;

        case 'c':
        {
            if (editor->edit_mode == Visual)
            {
                s_result->action = Delete;
                s_result->m_mod = InsertionChange;
                result = Ok;
            }
            else if (p_state->state == Middle && s_result->action == Delete)
            {
                s_result->motion = Down;
                s_result->m_mod = InsertionChange;
                result = Ok;
            } 
            else if (p_state->state == Start)
            {
                s_result->action = Delete;
                s_result->m_mod = InsertionChange;
                p_state->state = Middle;
                result = NotDone;
            } 
        } break;
 

        case 'i':
        {
            // I use the meta prefix here so that i can use the char 'i' in a slight different way 
            // then vim. 
            if (p_state->state == Meta)
            {
                (*active_window)->change |= Render_ModeChange;
                s_result->m_mod = InsertionChange;
                result = Ok;
            }

        } break;

        case 'I':
        {
            (*active_window)->change |= Render_ModeChange;
            s_result->m_mod = InsertionChange;
            s_result->motion = Underscore;
            result = Ok;
        } break;

        case '_':
        {
            s_result->motion = Underscore;
            result = Ok;
        } break;

        case 'u':
        {
            if (editor->edit_mode == Normal) 
            {
                for (u32 i = 0; i < Maximum(1, s_result->quantifier); ++i)
                {
                    undo_(*active_window);
                }
                (*active_window)->buffer->changed = true;
                result = Ok;
            }
        } break;

        case 'r':
        {
            if (editor->edit_mode == Normal)
            {
                for (u32 i = 0; i < Maximum(1, s_result->quantifier); ++i)
                {
                    redo(*active_window);
                }
                (*active_window)->buffer->changed = true;
                result = Ok;
            }
        } break;

        case '$':
        {
            s_result->motion = Dollar;

            result = Ok;
        } break;

        case 'A':
        {
            (*active_window)->change |= Render_ModeChange;
            s_result->m_mod = InsertionChange; 
            s_result->motion = Dollar;
            s_result->quantifier = 0;
            result = Ok;
        } break;

        case 'a':
        {
            (*active_window)->change |= Render_ModeChange;
            s_result->m_mod = InsertionChange; 
            s_result->motion = Right;
            s_result->quantifier = Maximum(1, p_state->s_result.quantifier);
            result = Ok;
        } break;

        case 'o':
        {
            (*active_window)->change |= Render_ModeChange;
            s_result->m_mod = InsertionChange;
            s_result->quantifier = 0;
            s_result->motion = Dollar;
            s_result->action = Insertion;
            s_result->count = 1;
            s_result->char_pending[0] = '\n';
            result = Ok;
        } break;

        case 'l':
        {
            s_result->motion = Right;
            if (editor->edit_mode == Visual)
            {
                (*active_window)->change |= Render_VisualModeCursorChange;
            }
            result = Ok;
        } break;

        case 'h':
        {
            s_result->motion = Left;
            if (editor->edit_mode == Visual)
            {
                (*active_window)->change |= Render_VisualModeCursorChange;
            }
            result = Ok;
        } break;

        case 'k':
        {
            s_result->motion = Up;
            if (editor->edit_mode == Visual)
            {
                (*active_window)->change |= Render_VisualModeCursorChange;
            }
            result = Ok;
        } break;

        case 'j':
        {
            s_result->motion = Down;
            if (editor->edit_mode == Visual)
            {
                (*active_window)->change |= Render_VisualModeCursorChange;
            }
            result = Ok;
        } break;

        case 'G':
        {
            s_result->motion = Absolute;
            if (s_result->quantifier)
            {
                s_result->quantifier = UINT32_MAX;
            }
            else
            {
                s_result->quantifier--;
            }
            if (editor->edit_mode == Visual)
            {
                (*active_window)->change |= Render_VisualModeCursorChange;
            }
            result = Ok;
        } break;

        case 'P':
        {
            s_result->p_mod = Current;
            s_result->action = Paste;
            result = Ok;
        } break;

        case 'p':
        {
            s_result->action = Paste;
            result = Ok;

        } break;

        case 'f':
        {
            if (p_state->state == Start)
            {
                s_result->motion = Search; 
                p_state->state = Middle;
                result = NotDone;
            }
            else if (p_state->state == Middle && s_result->action != NoAction)
            {
                s_result->motion = Search;
                result = NotDone;
            }

        } break;

        default: 
        {
            result = Ok;
        } break;
    }
    return result;
}

static inline void change_mode(editor_state *state, state_result *s_result )
{
    switch (s_result->m_mod)
    {
        case NoChange:
        {
        } break;

        case InsertionChange:
        {
            state->edit_mode = Insert;
            s_result->inserted = 
                state->screen.active_window->buffer->append.text + 
                state->screen.active_window->buffer->append.text_len;
            s_result->action = Insertion;
        } break;

        case LayoutChange:
        {
            state->edit_mode = Layout;
        } break;

        case VisualChange:
        {
            state->edit_mode = Visual;
        } break;

        default:
        {
        } break;
    }
}

static void yank(window *win, paste_buffer *p_buffer, mode edit_mode, state_result s_result)
{
    if (p_buffer->buffer)
    {
        free_paste_buffer(p_buffer);
    }

    win_range w_range = get_cursor_range(
        win,
        s_result.motion,
        s_result.quantifier,
        edit_mode,
        s_result.match,
        s_result.count);
    win->dc = w_range.first;

    replace_result rep = yank_(win->buffer, w_range.first, w_range.one_past_end);

    p_buffer->buffer = win->buffer;
    p_buffer->pieces = rep.pieces;
    p_buffer->start  = rep.start;
    p_buffer->end    = rep.end;
    p_buffer->flags  = rep.flags;
    p_buffer->count  = rep.count;
    p_buffer->type   = paste_type_from_motion(s_result.motion, edit_mode);
}

static undo_node *change(
    window *win,
    paste_buffer *p_buffer,
    mode edit_mode,
    state_result s_result)
{
    Assert(p_buffer);
    if (p_buffer->buffer)
    {
        free_paste_buffer(p_buffer);
    }

    win_range w_range = get_cursor_range(
        win,
        s_result.motion,
        s_result.quantifier,
        edit_mode,
        s_result.match,
        s_result.count);

    win->dc = w_range.first;

    undo_node *node = allocate_tree_node(&win->buffer->history_arena);
    node->bc = win->bc;

    piece piece;
    piece_range p_range = {};

    // Assert(!s_result.inserted_count);

    if (s_result.inserted_count > 0 && s_result.inserted) 
    {
        // NOTE: This below is necessary because the append buffer is per piece_buffer,
        // this does not have to be the case.
        if ((s_result.inserted >= win->buffer->append.text) && 
            (s_result.inserted < win->buffer->append.text + win->buffer->append.text_len))
        {
            // Last inserted was in this buffer
            // TODO: don't copy the text to the append buffer.
        }
        else
        {
            // Must copy text to buffer,
        }

        piece = make_piece(win->buffer, s_result.inserted, s_result.inserted_count);
        p_range.count = 1;
        p_range.pieces = &piece;

        // NOTE: this is wrong, inserted_count is in bytes / this must 
        // be in grapheme clusters.
        // Its is also wrong becaus it assumes a one line insertion.
        win->dc.x += s_result.inserted_count;

    }
    replace_result rep = range_replace__(
        win->buffer,
        w_range.first,
        w_range.one_past_end,
        p_range);

    rep.undo_header->ref_count++;

    p_buffer->buffer = win->buffer;
    p_buffer->header = rep.undo_header;
    p_buffer->start  = rep.start;
    p_buffer->end    = rep.end;
    p_buffer->flags  = rep.flags;
    p_buffer->count  = 0;
    p_buffer->type   = paste_type_from_motion(s_result.motion, edit_mode);

    win->buffer->changed = true;
    node->data = rep.undo_header;
    return node;
}

static void edit(editor_state *state)
{
    state_result s_result = state->p_state.s_result;
    normal_parse_state *p_state = &state->p_state;
    window *win = state->screen.active_window;

    u8 *char_pending = p_state->s_result.char_pending;
    // TODO: Calculate the length properly;
    u32 char_pending_len = (char_pending) ? 1 : 0;

    switch (s_result.action)
    {
        case NoAction:
        {
            change_mode(state, &s_result);
            move_by_motion(
                win,
                s_result.motion,
                s_result.quantifier,
                char_pending,
                char_pending_len);
        } break;

        case Insertion:
        {
            change_mode(state, &s_result);
            move_by_motion(win, s_result.motion, s_result.quantifier, NULL, 0);
            commit_cursor(win, state->edit_mode);

            Assert(s_result.char_pending);

            insert_mode_insert(win, s_result.char_pending, s_result.count);
            win->buffer->changed = true;

        } break;

        case Delete:
        {
            undo_node *node = change(win, &state->p_buffer, state->edit_mode, s_result);

            if (s_result.m_mod == InsertionChange)
            {
                win->buffer->staged = node;
                state->edit_mode = Insert;
                p_state->s_result.inserted = 
                    state->screen.active_window->buffer->append.text + 
                    state->screen.active_window->buffer->append.text_len;
                p_state->s_result.action = Replace;
                p_state->s_result.m_mod  = NoChange;
            }
            else
            {
                insert_node(&win->buffer->history, node);
                state->edit_mode = Normal;
            }


        } break;

        case Replace:
        {
            Assert(state->edit_mode == Normal);
            undo_node *node = change(win, &state->p_buffer, state->edit_mode, s_result);
            insert_node(&win->buffer->history, node);
        } break;

        case Paste:
        {
            if (!is_empty(&state->p_buffer))
            {
                undo_node *node = allocate_tree_node(&win->buffer->history_arena);
                node->bc = win->bc;

                piece_range p_range = {
                    .start  = state->p_buffer.start,
                    .end    = state->p_buffer.end,
                    .pieces = get_pieces(&state->p_buffer),
                    .count  = get_count(&state->p_buffer),
                    .flags  = state->p_buffer.flags,
                };

                piece piece;
                // Pasting text from another buffer; must serialize the pieces, and copy the text
                // to the current buffer.
                // NOTE: Shoule we change the paste buffer owner, specifically if this is the 
                // default paste buffer. If im pasting from another piece_buffer, isn't it likely
                // that I will keep editing that buffer. 
                if (state->p_buffer.buffer != win->buffer)
                {
                    piece = serialize_piece_range_to(state->p_buffer.buffer, win->buffer, p_range);
                    p_range.pieces = &piece;
                    p_range.count  = 1;
                    p_range.start  = 0;
                    p_range.end    = 0;
                    p_range.flags  = Edit_None;
                }

                buffer_cursor bc = win->bc;

                if (state->p_buffer.type == Line)
                {
                    bc.x = 0;
                    win->dc.x = 0;
                    if (s_result.p_mod == Next)
                    {
                        bc.y++;
                        win->dc.y++;
                    }
                }
                else if (s_result.p_mod == Next)
                {
                    bc.x++;
                }

                replace_result rep = range_replace__(win->buffer, bc, bc, p_range);

                node->data = rep.undo_header;

                if (p_state->s_result.quantifier > 1)
                {
                    u32 num_repeat = p_state->s_result.quantifier - 1;
                    // Try to compress this if possible:  a lot of replaces into a big replace.
                    while (num_repeat > 0)
                    {
                        replace_result rep = range_replace__(win->buffer, win->dc, win->dc, p_range);
                        LIST_INSERT(node->data->next, rep.undo_header);
                        num_repeat--;
                    }
                }
                insert_node(&win->buffer->history, node);
            }

        } break;

        case Yank:
        {
            yank(win, &state->p_buffer, state->edit_mode, s_result);
            state->edit_mode = Normal;
            win->change |= Render_VisualModeCursorChange;
        } break;

        default:
        {
        } break;
    }
}

static void process_normal(editor_state *state, u8 *input, u32 input_size)
{
    if (input_size > 1)
    {
        return;
    }
    switch (parse_normal(state, input, input_size))
    {
        case Ok:
        {
            edit(state);
            if (state->p_state.s_result.action != NoAction)
            {
                state->prev_command = state->p_state.s_result;
            }
            reset_parse_state(&state->p_state);
        }  break;

        case Error:
        {
            reset_parse_state(&state->p_state);
        } break;

        case NotDone:
        {
        } break;
    }
}


