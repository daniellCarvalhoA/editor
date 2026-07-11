static parse_result parse_normal(editor_state *editor, char token) 
{
    parse_result result = Error;

    normal_parse_state *p_state = &editor->p_state;
    window **active_window = &editor->screen.active_window;

    if (token >= '1' && token <= '9')
    {
        p_state->s_result.quantifier = p_state->s_result.quantifier * 10 + (token - '0');
        p_state->state = Middle;
        return NotDone;
    }

    switch (token)
    {
        case ':':
        {
            interacting_window = *active_window;
            *active_window = editor->screen.command_window;
            parse_command(editor, (u8 *) &token, 1);
            
        } break;

        case '\x1b':
        {
            if (editor->edit_mode == Visual)
            {
                p_state->s_result.m_mod = NormalChange;
                (*active_window)->change |= Render_ModeChange;
                editor->edit_mode = Normal;
            }
        } break;
        case 'v':
        {
            if (editor->edit_mode == Normal)
            {
                p_state->s_result.m_mod = VisualChange;
                (*active_window)->vc = (*active_window)->bc;
            }
            else
            {
                Assert(editor->edit_mode == Visual);
                p_state->s_result.m_mod = NormalChange;

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
                p_state->s_result.m_mod = LayoutChange;
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
                    p_state->s_result.motion = Zero;
                    result = Ok;

                } break;

                case Middle:
                {
                    p_state->s_result.quantifier *= 10;
                    result = NotDone;
                } break;

                default:
                {
                    result = Error;
                } break;
            }
 
        } break;

        case 'd':
        {
            if (editor->edit_mode == Visual)
            {
                p_state->s_result.action = Delete;
                result = Ok;
            }
            else if (p_state->state == Middle && p_state->s_result.action == Delete)
            {
                p_state->s_result.motion = Down;
                result = Ok;
            } 
            else if (p_state->state == Start)
            {
                p_state->s_result.action = Delete;
                p_state->state = Middle;
                result = NotDone;
            } 
        } break;

        case 'c':
        {
            if (editor->edit_mode == Visual)
            {
                p_state->s_result.action = Delete;
                p_state->s_result.m_mod = InsertionChange;
                result = Ok;
            }
            else if (p_state->state == Middle && p_state->s_result.action == Delete)
            {
                p_state->s_result.motion = Down;
                p_state->s_result.m_mod = InsertionChange;
                p_state->s_result.quantifier = Maximum(1, p_state->s_result.quantifier);
                result = Ok;
            } 
            else if (p_state->state == Start)
            {
                p_state->s_result.action = Delete;
                p_state->s_result.m_mod = InsertionChange;
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
                p_state->s_result.m_mod = InsertionChange;
                result = Ok;
            }

        } break;

        case 'u':
        {
            if (editor->edit_mode == Normal) 
            {
                for (u32 i = 0; i < Maximum(1, p_state->s_result.quantifier); ++i)
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
                for (u32 i = 0; i < Maximum(1, p_state->s_result.quantifier); ++i)
                {
                    redo(*active_window);
                }
                (*active_window)->buffer->changed = true;
                result = Ok;
            }
        } break;

        case '$':
        {
            p_state->s_result.motion = Dollar;

            result = Ok;
        } break;

        case 'A':
        {
            (*active_window)->change |= Render_ModeChange;
            p_state->s_result.m_mod = InsertionChange; 
            p_state->s_result.motion = Dollar;
            p_state->s_result.quantifier = 0;
            result = Ok;
        } break;

        case 'a':
        {
            (*active_window)->change |= Render_ModeChange;
            p_state->s_result.m_mod = InsertionChange; 
            p_state->s_result.motion = Right;
            p_state->s_result.quantifier = Maximum(1, p_state->s_result.quantifier);
            result = Ok;
        } break;

        case 'o':
        {
            (*active_window)->change |= Render_ModeChange;
            p_state->s_result.m_mod = InsertionChange;
            p_state->s_result.quantifier = 0;
            p_state->s_result.motion = Dollar;
            p_state->s_result.action = Insertion;
            p_state->s_result.char_pending = '\n';
            result = Ok;
        } break;

        case 'l':
        {
            p_state->s_result.motion = Right;
            p_state->s_result.quantifier = Maximum(1, p_state->s_result.quantifier);
            if (editor->edit_mode == Visual)
            {
                (*active_window)->change |= Render_VisualModeCursorChange;
            }
            result = Ok;
        } break;

        case 'h':
        {
            p_state->s_result.motion = Left;
            p_state->s_result.quantifier = Maximum(1, p_state->s_result.quantifier);
            if (editor->edit_mode == Visual)
            {
                (*active_window)->change |= Render_VisualModeCursorChange;
            }
            result = Ok;
        } break;

        case 'k':
        {
            p_state->s_result.motion = Up;
            p_state->s_result.quantifier = Maximum(1, p_state->s_result.quantifier);
            if (editor->edit_mode == Visual)
            {
                (*active_window)->change |= Render_VisualModeCursorChange;
            }
            result = Ok;
        } break;

        case 'j':
        {
            p_state->s_result.motion = Down;
            p_state->s_result.quantifier = Maximum(1, p_state->s_result.quantifier);
            if (editor->edit_mode == Visual)
            {
                (*active_window)->change |= Render_VisualModeCursorChange;
            }
            result = Ok;
        } break;

        case 'G':
        {
            p_state->s_result.motion = Absolute;
            if (!p_state->s_result.quantifier)
            {
                p_state->s_result.quantifier = UINT32_MAX;
            }
            else
            {
                p_state->s_result.quantifier--;
            }
            if (editor->edit_mode == Visual)
            {
                (*active_window)->change |= Render_VisualModeCursorChange;
            }
            result = Ok;
        } break;

        case 'P':
        {
            p_state->s_result.p_mod = Current;
            p_state->s_result.action = Paste;
            result = Ok;
        } break;
        case 'p':
        {
            p_state->s_result.action = Paste;
            result = Ok;

        } break;

        default: 
        {
            result = Ok;
        } break;
    }
    return result;
}



static win_cursor get_curr_cursor(window *win, motion motion)
{
    win_cursor result = {};
    switch (motion)
    {
        case MotionCount:
        case NoMotion:
        {

        } break;

        case Up:
        {
            result.y = win->bc.y + 1;
            result.x = 0;
        } break;

        case Down:
        {
            result.y = win->bc.y;
            result.x = 0;
        } break;

        case Left:
        {
        } break;

        case Right:
        {
        } break;

        case Absolute:
        {
            result.y = win->bc.y;
            result.x = 0;
        } break;

        case Dollar:
        {
            result = win->bc;
        } break;

        case Zero:
        {
        } break;

        // case Underscore:
        // {
        // } break;
    }
    return result;
}

static win_cursor get_win_cursor(window *win, motion motion, u32 quantifier)
{
    win_cursor result = {}; 
    switch (motion)
    {
        case MotionCount:
        case NoMotion:
        {

        } break;

        case Up:
        {
            result.y = win->bc.y - Minimum(quantifier, win->bc.y);
            result.x = 0;
        } break;

        case Down:
        {

            u32 dcy = win->bc.y + Maximum(1, quantifier);
            result.y = Minimum(win->buffer->lcnt, dcy);
            result.x = (dcy > win->buffer->lcnt) ? (UINT32_MAX) : 0;
        } break;

        case Left:
        {
        } break;

        case Right:
        {
        } break;

        case Absolute:
        {
            result.y = Minimum(quantifier, win->buffer->lcnt);
            result.x = 0;
        } break;

        case Dollar:
        {
            result.y = Minimum(win->bc.y + quantifier, win->buffer->lcnt);
            u32 line_len = get_line_len_(&win->buffer->iter, result.y);
            result.x = line_len;
        } break;

        case Zero:
        {
        } break;

        // case Underscore:
        // {
        // } break;
    }
    return result;
}

static void move_by_motion(window *win, motion motion, u32 quantifier, mode edit_mode)
{
    switch (motion)
    {
        case MotionCount:
        case NoMotion:
        {
            win->dc = win->bc;
        } break;

        case Up:
        {
            win->dc.y = win->bc.y - Minimum(quantifier, win->bc.y);

        } break;

        case Down:
        {
            win->dc.y = Minimum(win->buffer->lcnt, win->bc.y + Maximum(1, quantifier));
        } break;

        case Left:
        {
            u32 amount = Maximum(1, quantifier);
            if (amount > win->bc.x)
            {
                win->dc.x = 0;
            }
            else
            {
                win->dc.x = win->bc.x - amount;
            }
        } break;

        case Right:
        {
            win->dc.x = win->bc.x + quantifier;
        } break;

        case Absolute:
        {
            win->dc.y = Minimum(quantifier, win->buffer->lcnt);
        } break;

        case Dollar:
        {
            if (quantifier > 0)
            {
                --quantifier;
            }
            win->dc.y = Minimum(win->bc.y + quantifier, win->buffer->lcnt);
            move_by_motion(win, Right, UINT32_MAX - win->bc.x, edit_mode);

        } break;

        case Zero:
        {
            win->dc.x = 0;
        } break;

        // case Underscore:
        // {
        // } break;
     }
}

static inline void change_mode(editor_state *state, mode_modifier mod)
{
    switch (mod)
    {
        case NoChange:
        {
        } break;

        case InsertionChange:
        {
            state->edit_mode = Insert;
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

static undo_node *delete(window *win, paste_buffer *p_buffer, mode edit_mode, state_result s_result)
{
    Assert(p_buffer);
    if (p_buffer->buffer)
    {
        free_paste_buffer(p_buffer);
    }

    win_cursor curr; 
    win_cursor next;
    if (edit_mode == Visual)
    {
        curr = win->bc;
        next = win->vc;
    }
    else
    {
        curr = get_curr_cursor(win, s_result.motion);
        next = get_win_cursor(win, s_result.motion, s_result.quantifier);
    }

    undo_node *node = allocate_tree_node(&win->buffer->history_arena);
    node->bc = win->bc;

    piece_range p_range = {};
    replace_result rep;
    switch (compare(curr, next))
    {
        case EqualTo:
        case LessThan:
        {
            rep = range_replace__(win->buffer, curr, next, p_range);
            win->dc.y = curr.y;
        } break;

        case GreaterThan:
        {
            rep = range_replace__(win->buffer, next, curr, p_range);
            win->dc.y = next.y;
        } break;
    }

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

    switch (s_result.action)
    {
        case NoAction:
        {
            change_mode(state, s_result.m_mod);
            move_by_motion(win, s_result.motion, s_result.quantifier, state->edit_mode);

        } break;

        case Insertion:
        {
            change_mode(state, s_result.m_mod);
            move_by_motion(win, s_result.motion, s_result.quantifier, state->edit_mode);
            commit_cursor(win, state->edit_mode);

            Assert(s_result.char_pending);

            insert_mode_insert(win, &s_result.char_pending, 1);
            win->buffer->changed = true;

        } break;

        case Delete:
        {
            state->p_state.s_result.quantifier++;
            undo_node *undo_node = delete(win, &state->p_buffer, state->edit_mode, s_result);

            if (s_result.m_mod == InsertionChange)
            {
                win->buffer->staged = undo_node;
                state->edit_mode = Insert;
            }
            else
            {
                insert_node(&win->buffer->undo_history, undo_node);
                state->edit_mode = Normal;
            }
        } break;

        case Paste:
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
                if (s_result.p_mod == Next)
                {
                    bc.y++;
                }
            }
            else if (s_result.p_mod == Next)
            {
                bc.x++;
            }

            replace_result rep = range_replace__(win->buffer, bc, bc, p_range);

            node->data = rep.undo_header;
            insert_node(&win->buffer->undo_history, node);

        } break;

        case Yank:
        {
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
    switch (parse_normal(state, *input))
    {
        case Ok:
        {
            edit(state);
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


