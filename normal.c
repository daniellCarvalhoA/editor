static parse_result parse_normal(editor_state *editor, char token) 
{
    parse_result result = Error;

    if (token >= '1' && token <= '9')
    {
        p_state.quantifier = p_state.quantifier * 10 + (token - '0');
        p_state.state = Middle;
        return NotDone;
    }

    switch (token)
    {
        case ':':
        {
            interacting_window = active_window;
            active_window = editor->command_window;
            process_command(editor, (u8 *) &token, 1);
            // append_char(&active_window->c_buffer, (u8 *) ":", sizeof(":") - 1);
            // active_window->change |= Render_BufferChange;
            
        } break;
        case '\x1b':
        {
            if (edit_mode == Visual)
            {
                p_state.change = NormalChange;
                active_window->change |= Render_ModeChange;
                edit_mode = Normal;
            }
        } break;
        case 'v':
        {
            if (edit_mode == Normal)
            {
                p_state.change = VisualChange;
                active_window->vcx = active_window->bcx;
                active_window->vcy = active_window->bcy;
            }
            else
            {
                Assert(edit_mode == Visual);
                p_state.change = NormalChange;

            }
            active_window->change |= Render_ModeChange;
            result = Ok;

        } break;

        case ' ':
        {
            if (edit_mode == Normal)
            {
                p_state.state = Meta;
                result = NotDone;
            }
        } break;

        case 'w':
        {
            if (p_state.state == Meta)
            {
                active_window->change |= Render_ModeChange;
                p_state.change = LayoutChange;
                result = Ok;
            }
            else
            {

            }
        } break;
        case '0':
        {
            switch (p_state.state)
            {
                case Start:
                {
                    p_state.motion = Zero;
                    result = Ok;

                } break;

                case Middle:
                {
                    p_state.quantifier *= 10;
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
            if (edit_mode == Visual)
            {
                p_state.action = Delete;
                result = Ok;
            }
            else if (p_state.state == Middle && p_state.action == Delete)
            {
                p_state.motion = Down;
                p_state.quantifier = Maximum(1, p_state.quantifier);
                result = Ok;
            } 
            else if (p_state.state == Start)
            {
                p_state.action = Delete;
                p_state.state = Middle;
                result = NotDone;
            } 
        } break;

        case 'c':
        {
            if (edit_mode == Visual)
            {
                p_state.action = Delete;
                p_state.change = InsertionChange;
                result = Ok;
            }
            else if (p_state.state == Middle && p_state.action == Delete)
            {
                p_state.motion = Down;
                p_state.change = InsertionChange;
                p_state.quantifier = Maximum(1, p_state.quantifier);
                result = Ok;
            } 
            else if (p_state.state == Start)
            {
                p_state.action = Delete;
                p_state.change = InsertionChange;
                p_state.state = Middle;
                result = NotDone;
            } 
        } break;


        case 'i':
        {
            // I use the meta prefix here so that i can use the char 'i' in a slight differnt way 
            // then vim. 
            if (p_state.state == Meta)
            {
                active_window->change |= Render_ModeChange;
                p_state.change = InsertionChange;
                result = Ok;
            }

        } break;

        case 'u':
        {
            if (edit_mode == Normal) 
            {
                undo_(active_window->buffer);
                active_window->buffer->changed = true;
                result = Ok;
            }
        } break;

        case '$':
        {
            p_state.motion = Dollar;
            // p_state.quantifier = Maximum(1, p_state.quantifier);

            result = Ok;
        } break;

        case 'A':
        {
            active_window->change |= Render_ModeChange;
            p_state.change = InsertionChange; 
            p_state.motion = Dollar;
            p_state.quantifier = 0;
            result = Ok;
        } break;

        case 'a':
        {
            active_window->change |= Render_ModeChange;
            p_state.change = InsertionChange; 
            p_state.motion = Right;
            p_state.quantifier = Maximum(1, p_state.quantifier);
            result = Ok;
        } break;

        case 'o':
        {
            active_window->change |= Render_ModeChange;
            p_state.change = InsertionChange;
            p_state.quantifier = 0;
            p_state.motion = Dollar;
            p_state.action = Insertion;
            p_state.char_pending = '\n';
            result = Ok;
        } break;

        case 'l':
        {
            p_state.motion = Right;
            p_state.quantifier = Maximum(1, p_state.quantifier);
            if (edit_mode == Visual)
            {
                active_window->change |= Render_VisualModeCursorChange;
            }
            result = Ok;
        } break;

        case 'h':
        {
            p_state.motion = Left;
            p_state.quantifier = Maximum(1, p_state.quantifier);
            if (edit_mode == Visual)
            {
                active_window->change |= Render_VisualModeCursorChange;
            }
            result = Ok;
        } break;

        case 'k':
        {
            p_state.motion = Up;
            p_state.quantifier = Maximum(1, p_state.quantifier);
            if (edit_mode == Visual)
            {
                active_window->change |= Render_VisualModeCursorChange;
            }
            result = Ok;
        } break;

        case 'j':
        {
            p_state.motion = Down;
            p_state.quantifier = Maximum(1, p_state.quantifier);
            if (edit_mode == Visual)
            {
                active_window->change |= Render_VisualModeCursorChange;
            }
            result = Ok;
        } break;

        case 'G':
        {
            p_state.motion = Absolute;
            if (!p_state.quantifier)
            {
                p_state.quantifier = UINT32_MAX;
            }
            else
            {
                p_state.quantifier--;
            }
            if (edit_mode == Visual)
            {
                active_window->change |= Render_VisualModeCursorChange;
            }
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
        case NoMotion:
        {

        } break;

        case Up:
        {
            result.y = win->bcy + 1;
            result.x = 0;

        } break;

        case Down:
        {
            result.y = win->bcy;
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
            result.y = win->bcy;
            result.x = 0;
        } break;

        case Dollar:
        {
            result.y = win->bcy;
            result.x = win->bcx;
        } break;

        case Zero:
        {
        } break;

        case Underscore:
        {
        } break;
    }
    return result;
}

static win_cursor get_win_cursor(window *win, motion motion, u32 quantifier)
{
    win_cursor result = {}; 
    switch (motion)
    {
        case NoMotion:
        {

        } break;

        case Up:
        {
            result.y = win->bcy - Minimum(quantifier, win->bcy);
            result.x = 0;
        } break;

        case Down:
        {
            result.y = Minimum(win->buffer->lcnt, win->bcy + Maximum(1, quantifier));
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
            result.y = Minimum(quantifier, win->buffer->lcnt);
            result.x = 0;
        } break;

        case Dollar:
        {
            result.y = Minimum(win->bcy + quantifier, win->buffer->lcnt);
            u32 line_len = get_line_len_(&win->buffer->iter, result.y);
            result.x = line_len;
        } break;

        case Zero:
        {
        } break;

        case Underscore:
        {
        } break;
    }
    return result;
}

static void move_by_motion(window *win, motion motion, u32 quantifier)
{
    switch (motion)
    {
        case NoMotion:
        {
            win->dcy = win->bcy;
            win->dcx = win->bcx;
        } break;

        case Up:
        {
            win->dcy = win->bcy - Minimum(quantifier, win->bcy);
            win->bcy = win->dcy;
            win->bcx = 0;
            u32 prev_dcx = win->dcx;
            move_by_motion(win, Right, win->dcx);
            win->dcx = Maximum(win->dcx, prev_dcx);

        } break;

        case Down:
        {
            win->dcy = Minimum(win->buffer->lcnt, win->bcy + Maximum(1, quantifier));
            win->bcy = win->dcy;
            win->bcx = 0;
            u32 prev_dcx = win->dcx;
            move_by_motion(win, Right, win->dcx);
            win->dcx = Maximum(win->dcx, prev_dcx);
        } break;

        case Left:
        {
            u32 amount = Maximum(1, quantifier);
            if (amount > win->bcx)
            {
                win->dcx = 0;
            }
            else
            {
                win->dcx = win->bcx - amount;
            }
            win->bcx = win->dcx;
        } break;

        case Right:
        {
            win->dcx = win->bcx + quantifier;
            u32 line_len = get_line_len_(&win->buffer->iter, win->dcy);
            win->bcx = Minimum(win->dcx, line_len);
            win->dcx = Minimum(win->dcx, win->bcx);
            if (line_len && (line_len == win->dcx) && edit_mode == Normal)
            {
                win->dcx--;
                win->bcx--;
            }
        } break;

        case Absolute:
        {
            win->dcy = Minimum(quantifier, win->buffer->lcnt);
        } break;

        case Dollar:
        {
            win->dcy = Minimum(win->bcy + quantifier, win->buffer->lcnt);
            win->bcy = win->dcy;
            u32 prev_dcx = win->dcx;
            move_by_motion(win, Right, UINT32_MAX - win->dcx);
            win->dcx = Maximum(win->dcx, prev_dcx);

        } break;

        case Zero:
        {
        } break;

        case Underscore:
        {
        } break;
     }
    fprintf(stderr, "bcx: %u, dcx: %u\n", win->bcx, win->dcx);
}

static inline void change_mode(mode_change change)
{
    switch (change)
    {
        case NoChange:
        {
        } break;

        case InsertionChange:
        {
            edit_mode = Insert;
        } break;

        case LayoutChange:
        {
            edit_mode = Layout;
        } break;

        case VisualChange:
        {
            edit_mode = Visual;
        } break;

        default:
        {
        } break;
    }
}

static void edit(editor_state *state)
{
    piece_list *buffer = get_active_buffer(state);
    switch (p_state.action)
    {
        case NoAction:
        {
            change_mode(p_state.change);
            move_by_motion(active_window, p_state.motion, p_state.quantifier);

        } break;

        case Insertion:
        {
            change_mode(p_state.change);
            move_by_motion(active_window, p_state.motion, p_state.quantifier);
            Assert(p_state.char_pending);

            insert_mode_insert(&p_state.char_pending, 1);
            active_window->buffer->changed = true;

        } break;

        case Delete:
        {
            win_cursor curr; 
            win_cursor next;
            if (edit_mode == Visual)
            {
                fprintf(stderr, "In visual mode\n");
                curr.x = active_window->bcx;
                curr.y = active_window->bcy;
                next.x = active_window->vcx;
                next.y = active_window->vcy;
            }
            else
            {
                curr = get_curr_cursor(active_window, p_state.motion);
                next = get_win_cursor(active_window, p_state.motion, p_state.quantifier); 
            }

            undo_node *node = allocate_tree_node(&active_window->buffer->history_arena);
            node->cx = active_window->cx;
            node->cy = active_window->cy;

            switch (compare(curr, next))
            {
                case EqualTo:
                case LessThan:
                {
                    node->data = range_replace(active_window->buffer, curr.y, curr.x, next.y, next.x, 0, 0, 0);
                    active_window->bcx = curr.x;
                    active_window->bcy = curr.y;
                } break;

                case GreaterThan:
                {
                    node->data = range_replace(active_window->buffer, next.y, next.x, curr.y, curr.x, 0, 0, 0);
                    active_window->bcx = next.x;
                    active_window->bcy = next.y;
                } break;
            }


            if (p_state.change == InsertionChange)
            {
                active_window->buffer->staged = node;
                edit_mode = Insert;
            }
            else
            {
                insert_node(&active_window->buffer->undo_history, node);
                edit_mode = Normal;
            }

            u32 line_len = get_line_len_(&active_window->buffer->iter, active_window->dcy);
            // active_window->bcx = Minimum(active_window->dcx, line_len);
            active_window->buffer->changed = true;
        } break;

        case Paste:
        {
        } break;

        case Yank:
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
            reset_parse_state();
        }  break;

        case Error:
        {
            reset_parse_state();
        } break;

        case NotDone:
        {
        } break;
    }
}


