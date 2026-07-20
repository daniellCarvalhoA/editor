static void edit(editor_state *state);

static parse_result parse_normal(editor_state *editor, str token) 
{
    parse_result result = Error;

    normal_parse_state *p_state = &editor->p_state;

    action_spec *a_spec = &p_state->command.a_spec;
    motion_spec *m_spec = &p_state->command.m_spec;
    window **active_window = &editor->screen.active_window;


    if (token.buffer[0] >= '1' && token.buffer[0] <= '9')
    {
        a_spec->action_quantifier = a_spec->action_quantifier * 10 + (token.buffer[0] - '0');
        m_spec->motion_quantifier = m_spec->motion_quantifier * 10 + (token.buffer[0] - '0');
        p_state->state = Middle;
        return NotDone;
    }

    if (p_state->state == Middle && m_spec->motion_type == Motion_NoMotion)
    {
        if  (m_spec->flags & Range)
        {
            i32 pair_index = get_pair(token.buffer[0]);

            if (pair_index >= 0)
            {
                m_spec->open_close_index = pair_index;
                m_spec->motion_type = Motion_Search;
                return Ok;
            }
        }
    }

    if (p_state->state == Middle && m_spec->motion_type == Motion_Search)
    {
        m_spec->match_str_len = token.len;
        Assert(token.len < ArrayCount(m_spec->match_str));
        memcpy(&m_spec->match_str, token.buffer, token.len);
        if (is_visual(editor->edit_mode))
        {
            (*active_window)->change |= Render_VisualModeCursorChange;
        }
        return Ok;
    }

    switch (token.buffer[0])
    {
        case '.':
        {
            p_state->command = editor->prev_command;
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
                a_spec->m_mod = NormalChange;
                (*active_window)->change |= Render_ModeChange;
                editor->edit_mode = Normal;
            }
        } break;
        case 'v':
        {
            if (editor->edit_mode != Visual)
            {
                a_spec->m_mod = VisualChange;
                (*active_window)->vc = (*active_window)->bc;
            }
            else
            {
                a_spec->m_mod = NormalChange;

            }
            (*active_window)->change |= Render_ModeChange;
            result = Ok;

        } break;

        case 'V':
        {
            if (editor->edit_mode != LineVisual)
            {
                a_spec->m_mod = LineVisualChange;
                (*active_window)->vc = (*active_window)->bc;
            }
            else
            {
                a_spec->m_mod = NormalChange;

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
        } break ;

        case 'w':
        {
            if (p_state->state == Meta)
            {
                (*active_window)->change |= Render_ModeChange;
                a_spec->m_mod = LayoutChange;
                result = Ok;
            }
            else
            {
                m_spec->motion_type = Motion_Word;
                result = Ok;
            }
        } break;

        case 'b':
        {
            m_spec->motion_type = Motion_Word;
            m_spec->flags |= Backword;
            result = Ok;
        } break;
        case '0':
        {
            switch (p_state->state)
            {
                case Start:
                {
                    m_spec->motion_type = Zero;
                    result = Ok;

                } break;

                case Middle:
                {
                    if (a_spec->action_type == NoAction)
                    {
                        a_spec->action_quantifier *= 10;
                        m_spec->motion_quantifier *= 10;
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
                a_spec->action_type = Yank;
                result = Ok;
            }
            else if (p_state->state == Middle && a_spec->action_type == Yank)
            {
                m_spec->motion_type = Motion_Vertical;
                result = Ok;
            }
            else if (p_state->state == Start)
            {
                a_spec->action_type = Yank;
                p_state->state = Middle;
                result = NotDone;
            }

        } break;

        case 'd':
        {
           if (is_visual(editor->edit_mode))
            {
                a_spec->action_type = Delete;
                result = Ok;
            }
            else if (p_state->state == Middle && a_spec->action_type == Delete)
            {
                m_spec->motion_type = Motion_Vertical;
                result = Ok;
            } 
            else if (p_state->state == Start)
            {
                a_spec->action_type = Delete;
                p_state->state = Middle;
                result = NotDone;
            } 
        } break;

        case 'c':
        {
            a_spec->m_mod = InsertionChange;
            if (is_visual(editor->edit_mode))
            {
                a_spec->action_type = Delete;
                result = Ok;
            }
            else if (p_state->state == Middle && a_spec->action_type == Delete)
            {
                m_spec->motion_type = Motion_Vertical;
                result = Ok;
            } 
            else if (p_state->state == Start)
            {
                a_spec->action_type = Delete;
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
                a_spec->m_mod = InsertionChange;
                result = Ok;
            }
            else if (p_state->state == Middle && a_spec->action_type != NoAction)
            {
                m_spec->flags |= Range;
                m_spec->flags |= Exclusive;
                result = NotDone;
            }
            else if (p_state->state == Start && is_visual(editor->edit_mode))
            {
                m_spec->flags |= Range;
                m_spec->flags |= Exclusive;
                p_state->state = Middle;
                result = NotDone;
            }

        } break;

        case 'I':
        {
            (*active_window)->change |= Render_ModeChange;
            a_spec->m_mod = InsertionChange;
            m_spec->motion_type = Underscore;
            result = Ok;
        } break;

        case '_':
        {
            m_spec->motion_type = Underscore;
            result = Ok;
        } break;

        case 'u':
        {
            if (editor->edit_mode == Normal) 
            {
                for (u32 i = 0; i < Maximum(1, a_spec->action_quantifier); ++i)
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
                for (u32 i = 0; i < Maximum(1, a_spec->action_quantifier); ++i)
                {
                    redo(*active_window);
                }
                (*active_window)->buffer->changed = true;
                result = Ok;
            }
        } break;

        case '$':
        {
            m_spec->motion_type = Dollar;

            result = Ok;
        } break;

        case 'A':
        {
            (*active_window)->change |= Render_ModeChange;
            a_spec->m_mod = InsertionChange; 
            m_spec->motion_type = Dollar;
            // s_result->quantifier = 0;
            result = Ok;
        } break;

        case 'a':
        {
            (*active_window)->change |= Render_ModeChange;
            a_spec->m_mod = InsertionChange; 
            m_spec->motion_type = Motion_Horizontal;
            result = Ok;
        } break;

        case 'o':
        {
            (*active_window)->change |= Render_ModeChange;
            a_spec->m_mod = InsertionChange;
            m_spec->motion_type = Dollar;
            a_spec->action_type = Insertion;
            p_state->count = 1;
            p_state->char_pending[0] = '\n';
            result = Ok;
        } break;

        case 'l':
        {
            m_spec->motion_type = Motion_Horizontal;
            result = Ok;
        } break;

        case 'h':
        {
            m_spec->motion_type = Motion_Horizontal;
            m_spec->flags |= Backword;
            result = Ok;
        } break;

        case 'k':
        {
            if (p_state->state == Meta)
            {
                m_spec->motion_type = Motion_Vertical;
                m_spec->motion_quantifier = get_height(&editor->screen, *active_window) / 2;

                result = Ok;
            }
            else
            {
                m_spec->motion_type = Motion_Vertical;
                result = Ok;
            }
        } break;

        case 'j':
        {
            if (p_state->state == Meta)
            {
                m_spec->motion_type = Motion_Vertical;
                m_spec->flags |= Backword;
                m_spec->motion_quantifier = get_height(&editor->screen, *active_window) / 2;
                result = Ok;
            }
            else
            {
                m_spec->motion_type = Motion_Vertical;
                m_spec->flags |= Backword;
                result = Ok;
            }
        } break;

        case 'G':
        {
            m_spec->motion_type = Absolute;
            if (!m_spec->motion_quantifier)
            {
                m_spec->motion_quantifier = UINT32_MAX;
            }
            else
            {
                m_spec->motion_quantifier--;
            }
            result = Ok;
        } break;

        case 'P':
        {
            a_spec->p_mod = Current;
            a_spec->action_type = Paste;
            result = Ok;
        } break;

        case 'p':
        {
            a_spec->action_type = Paste;
            result = Ok;

        } break;

        case 'f':
        {
            m_spec->motion_type = Motion_Search; 
            if (p_state->state == Start)
            {
                p_state->state = Middle;
                result = NotDone;
            }
            else if (p_state->state == Middle && a_spec->action_type != NoAction)
            {
                result = NotDone;
            }
        } break;

        case 'F':
        {
            m_spec->motion_type = Motion_Search; 
            m_spec->flags = Backword;
            if (p_state->state == Start)
            {
                p_state->state = Middle;
                result = NotDone;
            }
            else if (p_state->state == Middle && a_spec->action_type != NoAction)
            {
                result = NotDone;
            }

        } break;


        case 't':
        {
            m_spec->motion_type = Motion_Search;
            m_spec->flags |= Exclusive;
            if (p_state->state == Start)
            {
                p_state->state = Middle;
                result = NotDone;
            }
            else if (p_state->state == Middle && a_spec->action_type != NoAction)
            {
                result = NotDone;
            }
        } break;

        case 'T':
        {
            m_spec->motion_type = Motion_Search;
            m_spec->flags |= Exclusive;
            m_spec->flags |= Backword;
            if (p_state->state == Start)
            {
                p_state->state = Middle;
                result = NotDone;
            }
            else if (p_state->state == Middle && a_spec->action_type != NoAction)
            {
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

static inline void change_mode(editor_state *state, action_spec *a_spec)
{
    switch (a_spec->m_mod)
    {
        case NoChange:
        {
        } break;

        case InsertionChange:
        {
            state->edit_mode = Insert;
            a_spec->action_type = Insertion;
        } break;

        case LayoutChange:
        {
            state->edit_mode = Layout;
        } break;

        case LineVisualChange:
        {
            state->edit_mode = LineVisual;
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

static void yank(window *win, paste_buffer *p_buffer, mode edit_mode, motion_spec m_spec)
{
    if (p_buffer->buffer)
    {
        free_paste_buffer(p_buffer);
    }

    win_range w_range = get_cursor_range(win, m_spec, edit_mode);
    win->dc = w_range.first;

    replace_result rep = yank_(win->buffer, w_range.first, w_range.one_past_end);

    p_buffer->buffer = win->buffer;
    p_buffer->pieces = rep.pieces;
    p_buffer->start  = rep.start;
    p_buffer->end    = rep.end;
    p_buffer->flags  = rep.flags;
    p_buffer->count  = rep.count;
    p_buffer->type   = paste_type_from_motion(m_spec.motion_type, edit_mode);
}

static undo_node *change(window *win, paste_buffer *p_buffer, win_range w_range, str inserted)
{
    // Assert(p_buffer);
    if (p_buffer && p_buffer->buffer)
    {
        free_paste_buffer(p_buffer);
    }

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
        win->bc.x = win->dc.x;
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

    win->buffer->changed = true;
    node->data = rep.undo_header;
    return node;
}

static void edit(editor_state *state)
{
    command *command = &state->p_state.command;
    action_spec *a_spec = &command->a_spec;
    motion_spec *m_spec = &command->m_spec;
    // state_result *s_result = &state->p_state.s_result;
    window *win = state->screen.active_window;

    switch (a_spec->action_type)
    {
        case NoAction:
        {
            change_mode(state, a_spec);
            move_by_motion(win, *m_spec, state->edit_mode != Insert);

            if (a_spec->m_mod == InsertionChange)
            {
                a_spec->inserted.buffer = win->buffer->append.text + win->buffer->append.text_len;
                a_spec->m_mod = NoChange;
            }

            if (is_visual(state->edit_mode))
            {
                win->change |= Render_VisualModeCursorChange;
            }
        } break;

        case Insertion:
        {
            move_by_motion(win, command->m_spec, false);

            if (a_spec->inserted.len)
            {
                win_range w_range = { win->bc, win->bc };
                undo_node *node = change(win, NULL, w_range, a_spec->inserted);
                insert_node(&win->buffer->history, node);
                win->bc = w_range.first;
            }
            else
            {
                Assert(state->p_state.char_pending);

                str s = { 
                    .buffer = state->p_state.char_pending,
                    .len = state->p_state.count 
                };
                insert_mode_insert(win, s); //, s_result);
                win->buffer->changed = true;
            }

            change_mode(state, a_spec);

        } break;

        case Delete:
        {
            win_range w_range = get_cursor_range(win, *m_spec, state->edit_mode);

            state->p_buffer.type = paste_type_from_motion(m_spec->motion_type, state->edit_mode);

            undo_node *node = change(win, &state->p_buffer, w_range, a_spec->inserted);

            if (a_spec->m_mod == InsertionChange)
            {
                a_spec->inserted.buffer = win->buffer->append.text + win->buffer->append.text_len;
                win->buffer->staged = node;
                state->edit_mode = Insert;
                a_spec->action_type = Replace;
                a_spec->m_mod  = NoChange;
            }
            else
            {
                insert_node(&win->buffer->history, node);
                state->edit_mode = Normal;
            }
            win->bc = w_range.first;

        } break;

        case Replace:
        {
            Assert(state->edit_mode == Normal);

            state->p_buffer.type = paste_type_from_motion(m_spec->motion_type, state->edit_mode);

            win_range w_range = get_cursor_range(win, *m_spec, state->edit_mode);

            undo_node *node = change(win, &state->p_buffer, w_range, a_spec->inserted);

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
                    piece = serialize_piece_range_to(state->p_buffer.buffer, win->buffer, p_range);

                    p_range.pieces = &piece;
                    p_range.count  = 1;
                    p_range.start  = 0;
                    p_range.end    = 0;
                    p_range.flags  = Edit_None;
                }

                win_range range;

                switch (state->edit_mode)
                {
                    case Visual:
                    {
                        range = get_visual_range(win);
                    } break;

                    case LineVisual:
                    {
                        range = get_line_visual_range(win);
                    } break;

                    default:
                    {
                        buffer_cursor bc = win->bc;

                        if (state->p_buffer.type == Line)
                        {
                            bc.x = 0;
                            win->dc.x = 0;
                            if (a_spec->p_mod == Next)
                            {
                                bc.y++;
                                win->dc.y++;
                            }
                        }
                        else if (a_spec->p_mod == Next)
                        {
                            bc.x++;
                        }

                        range.first = range.one_past_end = bc;
                    } break;
                }

                replace_result rep = range_replace__(win->buffer, range.first, range.one_past_end, p_range);

                node->data = rep.undo_header;

                if (a_spec->action_quantifier > 1)
                {
                    u32 num_repeat = a_spec->action_quantifier - 1;
                    // Try to compress this if possible:
                    // a lot of replaces into a single big replace.
                    while (num_repeat > 0)
                    {
                        replace_result rep = range_replace__(win->buffer, win->dc, win->dc, p_range);
                        LIST_INSERT(node->data->next, rep.undo_header);
                        num_repeat--;
                    }
                }
                insert_node(&win->buffer->history, node);
                state->edit_mode = Normal;

                win->bc = range.first;
            }

        } break;

        case Yank:
        {
            yank(win, &state->p_buffer, state->edit_mode, *m_spec);
            if (is_visual(state->edit_mode))
            {
                state->edit_mode = Normal;
                win->change |= Render_VisualModeCursorChange;
            }
        } break;

        default:
        {
        } break;
    }
}

static void process_normal(editor_state *state, str s)
{
    switch (parse_normal(state, s))
    {
        case Ok:
        {
            edit(state);
            if (state->p_state.command.a_spec.action_type != NoAction || 
                state->p_state.command.a_spec.m_mod == InsertionChange)
            {
                state->prev_command = state->p_state.command;
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


