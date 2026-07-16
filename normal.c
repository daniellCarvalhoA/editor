static void edit(editor_state *state);

static parse_result parse_normal(editor_state *editor, str token) 
{
    parse_result result = Error;

    normal_parse_state *p_state = &editor->p_state;
    state_result *s_result = &p_state->s_result;
    window **active_window      = &editor->screen.active_window;

    if (p_state->state == Middle && p_state->s_result.motion == Search)
    {
        s_result->count = token.len;
        Assert(token.len < ArrayCount(s_result->match));
        memcpy(&s_result->match, token.buffer, token.len);
        if (is_visual(editor->edit_mode))
        {
            (*active_window)->change |= Render_VisualModeCursorChange;
        }
        return Ok;
    }

    if (token.buffer[0] >= '1' && token.buffer[0] <= '9')
    {
        s_result->quantifier = s_result->quantifier * 10 + (token.buffer[0] - '0');
        p_state->state = Middle;
        return NotDone;
    }

    switch (token.buffer[0])
    {
        case '.':
        {
            *s_result = editor->prev_command;
            result = Ok;
             
        } break;
        case ':':
        {
            if (editor->edit_mode == Normal)
            {
                interacting_window = *active_window;
                *active_window = editor->screen.command_window;
                parse_command(editor, token);
            }
            
        } break;

        case '\x1b':
        {
            if (is_visual(editor->edit_mode))
            {
                s_result->m_mod = NormalChange;
                (*active_window)->change |= Render_ModeChange;
                editor->edit_mode = Normal;
            }
        } break;
        case 'v':
        {
            if (editor->edit_mode != Visual)
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
            if (is_visual(editor->edit_mode))
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
            if (is_visual(editor->edit_mode))
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
            if (is_visual(editor->edit_mode))
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
            result = Ok;
        } break;

        case 'h':
        {
            s_result->motion = Left;
            result = Ok;
        } break;

        case 'k':
        {
            s_result->motion = Up;
            result = Ok;
        } break;

        case 'j':
        {
            s_result->motion = Down;
            result = Ok;
        } break;

        case 'G':
        {
            s_result->motion = Absolute;
            if (!s_result->quantifier)
            {
                s_result->quantifier = UINT32_MAX;
            }
            else
            {
                s_result->quantifier--;
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

static void yank(
    window *win,
    paste_buffer *p_buffer,
    mode edit_mode,
    state_result *s_result)
{
    if (p_buffer->buffer)
    {
        free_paste_buffer(p_buffer);
    }

    str match_str = { .buffer = s_result->match, .len = s_result->count };
    win_range w_range = get_cursor_range(
        win,
        s_result->motion,
        s_result->quantifier,
        edit_mode,
        match_str);
    win->dc = w_range.first;

    replace_result rep = yank_(win->buffer, w_range.first, w_range.one_past_end);

    p_buffer->buffer = win->buffer;
    p_buffer->pieces = rep.pieces;
    p_buffer->start  = rep.start;
    p_buffer->end    = rep.end;
    p_buffer->flags  = rep.flags;
    p_buffer->count  = rep.count;
    p_buffer->type   = paste_type_from_motion(s_result->motion, edit_mode);
}

static undo_node *change(
    window *win,
    paste_buffer *p_buffer,
    win_range w_range,
    str inserted)
{
    // Assert(p_buffer);
    if (p_buffer && p_buffer->buffer)
    {
        free_paste_buffer(p_buffer);
    }

    // str match_str = { .buffer = s_result->match, .len = s_result->count };
    // win_range w_range = get_cursor_range(
    //     win,
    //     s_result->motion,
    //     edit_mode,
    //     s_result->quantifier,
    //     match_str);
    //
    //
    // win->dc = w_range.first;

    undo_node *node = allocate_tree_node(&win->buffer->history_arena);
    node->bc = win->bc;

    piece piece;
    piece_range p_range = {};

    // Assert(!s_result.inserted_count);

    if (inserted.len > 0 && inserted.buffer) 
    {
        // NOTE: This below is necessary because the append buffer is per piece_buffer,
        // this does not have to be the case.
        if ((inserted.buffer >= win->buffer->append.text) && 
            (inserted.buffer < win->buffer->append.text + win->buffer->append.text_len))
        {
            // Last inserted was in this buffer
            // TODO: don't copy the text to the append buffer.
        }
        else
        {
            // Must copy text to buffer,
        }

        piece = make_piece(win->buffer, inserted);
        p_range.count = 1;
        p_range.pieces = &piece;

        // NOTE: this is wrong, inserted_count is in bytes / this must 
        // be in grapheme clusters.
        // Its is also wrong becaus it assumes a one line insertion.
        win->dc.x += inserted.len;

    }

    replace_result rep = range_replace__(
        win->buffer,
        w_range.first,
        w_range.one_past_end,
        p_range);

    rep.undo_header->ref_count++;

    if (p_buffer)
    {
        p_buffer->buffer = win->buffer;
        p_buffer->header = rep.undo_header;
        p_buffer->start  = rep.start;
        p_buffer->end    = rep.end;
        p_buffer->flags  = rep.flags;
        p_buffer->count  = 0;
    }
    // p_buffer->type   = paste_type_from_motion(s_result->motion, edit_mode);

    win->buffer->changed = true;
    node->data = rep.undo_header;
    return node;
}

static void edit(editor_state *state)
{
    // normal_parse_state *p_state = &state->p_state;
    state_result *s_result = &state->p_state.s_result;
    window *win = state->screen.active_window;

    switch (s_result->action)
    {
        case NoAction:
        {
            change_mode(state, s_result);
            str match_str = { .buffer = s_result->match, .len = s_result->count };
            move_by_motion(
                win,
                s_result->motion,
                s_result->quantifier,
                match_str,
                state->edit_mode != Insert);

            if (s_result->m_mod == InsertionChange)
            {
                s_result->inserted.buffer =
                    win->buffer->append.text +
                    win->buffer->append.text_len;
                s_result->inserted.len = 0;
                s_result->m_mod = NoChange;
            }

            if (is_visual(state->edit_mode))
            {
                win->change |= Render_VisualModeCursorChange;

            }
        } break;

        case Insertion:
        {
            str s = {};
            move_by_motion(
                win,
                s_result->motion,
                s_result->quantifier,
                s,
                false);

            if (s_result->inserted.len)
            {
                win_range w_range = { win->bc, win->bc };

                undo_node *node = change(win, NULL, w_range, s_result->inserted);

                insert_node(&win->buffer->history, node);

                win->bc = w_range.first;

                // s_result->m_mode = NoChange;
            }
            else
            {
                Assert(s_result->char_pending);

                str s = { 
                    .buffer = s_result->char_pending,
                    .len = s_result->count 
                };
                insert_mode_insert(win, s);
                win->buffer->changed = true;
            }

            if (s_result->m_mod == InsertionChange)
            {
                s_result->inserted.buffer =
                    win->buffer->append.text +
                    win->buffer->append.text_len;
                s_result->inserted.len = 0;
            }
            change_mode(state, s_result);

        } break;

        case Delete:
        {
            str match_str = { .buffer = s_result->match, .len = s_result->count };

            win_range w_range = get_cursor_range(
                win,
                s_result->motion,
                state->edit_mode,
                s_result->quantifier,
                match_str);

            state->p_buffer.type = paste_type_from_motion(
                s_result->motion,
                state->edit_mode);

            undo_node *node = change(
                win,
                &state->p_buffer,
                w_range,
                s_result->inserted);

            if (s_result->m_mod == InsertionChange)
            {
                win->buffer->staged = node;
                state->edit_mode = Insert;
                s_result->inserted.buffer = 
                    win->buffer->append.text + 
                    win->buffer->append.text_len;
                s_result->inserted.len = 0;
                s_result->action = Replace;
                s_result->m_mod  = NoChange;
            }
            else
            {
                insert_node(&win->buffer->history, node);
                state->edit_mode = Normal;
            }

            if (s_result->m_mod == InsertionChange)
            {
                s_result->inserted.buffer =
                    win->buffer->append.text +
                    win->buffer->append.text_len;
                s_result->inserted.len = 0;
            }

            win->bc = w_range.first;

        } break;

        case Replace:
        {
            Assert(state->edit_mode == Normal);

            str match_str = { .buffer = s_result->match, .len = s_result->count };

            state->p_buffer.type = paste_type_from_motion(
                s_result->motion,
                state->edit_mode);

            win_range w_range = get_cursor_range(
                win,
                s_result->motion,
                state->edit_mode,
                s_result->quantifier,
                match_str);

            undo_node *node = change(
                win,
                &state->p_buffer,
                w_range,
                s_result->inserted);

            insert_node(&win->buffer->history, node);

            win->bc = w_range.first;
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
                // Pasting text from another buffer; must serialize the pieces,
                // and copy the text to the current buffer.
                // NOTE: Shoule we change the paste buffer owner,
                // specifically if this is the default paste buffer.
                // If im pasting from another piece_buffer, isn't it likely
                // that I will keep editing that buffer. 
                if (state->p_buffer.buffer != win->buffer)
                {
                    piece = serialize_piece_range_to(
                        state->p_buffer.buffer,
                        win->buffer,
                        p_range);

                    p_range.pieces = &piece;
                    p_range.count  = 1;
                    p_range.start  = 0;
                    p_range.end    = 0;
                    p_range.flags  = Edit_None;
                }

                win_range range;

                if (is_visual(state->edit_mode))
                {
                    range = get_visual_range(win);
                }
                else
                {
                    buffer_cursor bc = win->bc;

                    if (state->p_buffer.type == Line)
                    {
                        bc.x = 0;
                        win->dc.x = 0;
                        if (s_result->p_mod == Next)
                        {
                            bc.y++;
                            win->dc.y++;
                        }
                    }
                    else if (s_result->p_mod == Next)
                    {
                        bc.x++;
                    }

                    range.first = range.one_past_end = bc;
                }

                replace_result rep = range_replace__(
                    win->buffer,
                    range.first,
                    range.one_past_end,
                    p_range);

                node->data = rep.undo_header;

                if (s_result->quantifier > 1)
                {
                    u32 num_repeat = s_result->quantifier - 1;
                    // Try to compress this if possible:
                    // a lot of replaces into a big replace.
                    while (num_repeat > 0)
                    {
                        replace_result rep = range_replace__(
                            win->buffer,
                            win->dc,
                            win->dc,
                            p_range);
                        LIST_INSERT(node->data->next, rep.undo_header);
                        num_repeat--;
                    }
                }
                insert_node(&win->buffer->history, node);
                state->edit_mode = Normal;
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

static void process_normal(editor_state *state, str s)
{
    if (s.len > 1)
    {
        return;
    }
    switch (parse_normal(state, s))
    {
        case Ok:
        {
            edit(state);
            if (state->p_state.s_result.action != NoAction || 
                state->p_state.s_result.m_mod == InsertionChange)
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


