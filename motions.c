
static gen_buffer_position get_cursor_from_motion(
    window *win,
    motion_spec m_spec)
{
    gen_buffer_position result = {};
    switch (m_spec.edit_motion)
    {
        case Up:
        {
            result.y = saturating_sub(result.y, m_spec.quantifier);
        } break;

        case Down:
        {
            result.y = win->bc.y + m_spec.quantifier;
        } break;

        case Left:
        {
            result.y = win->bc.y;
            result.x = saturating_sub(win->bc.x, m_spec.quantifier);
        } break;

        case Right:
        {
            result.y = win->bc.y;
            result.x = win->bc.x + m_spec.quantifier;
        } break;

        case Absolute:
        {
            result.y = m_spec.quantifier;
        } break;

        case Dollar:
        {
            u32 quantifier = m_spec.quantifier;
            if (quantifier > 0)
            {
                --quantifier;
            }

            result.y = win->bc.y + quantifier;
            result.x = UINT32_MAX;

        } break;

        case Underscore:
        {
            result.type = NotMatch;
            result.match_str = (u8 *) " ";
            result.match_str_len = 1;
            result.y = win->bc.y;
            result.x = 0;
        } break;

        case Zero:
        {
            result.y = win->bc.y;
            result.x = 0;
        } break;

        case Search:
        {
            result.y = win->bc.y;
            result.x = win->bc.x;
            result.type          = Match;
            result.match_str     = m_spec.match_str;
            result.match_str_len = m_spec.match_str_len;
        } break;

        default: 
        {
        } break;
    }
    return result;
}

static inline u32 clamp_to_length(
    piece_list *list,
    const u32 cy,
    const u32 cx,
    b32 exclusive)
{
    u32 result = cx;
    u32 line_len = get_line_len_(&list->iter, cy);
    if (cx >= line_len)
    {
        result = line_len;
        if ((result > 0) && exclusive)
        {
            result--;
        }
    }
    return result;

}

static void move_by_motion(
    window *win,
    motion motion,
    u32 quantifier,
    str match_str,
    b32 exclusive)
{
    quantifier = Maximum(1, quantifier);
    switch (motion)
    {
         case MotionCount:
         case NoMotion:
         {
             win->dc = win->bc;
         } break;
 
         case Up:
         {
             win->bc.y = saturating_sub(win->bc.y, quantifier);
             win->bc.x = clamp_to_length(
                 win->buffer,
                 win->bc.y,
                 win->dc.x,
                 exclusive);
         } break;
 
         case Down:
         {
             win->bc.y = clamped_add(win->bc.y, quantifier, win->buffer->lcnt);
             win->bc.x = clamp_to_length(
                 win->buffer,
                 win->bc.y,
                 win->dc.x,
                 exclusive);
         } break;
 
         case Left:
         {
             win->dc.x = win->bc.x = saturating_sub(win->bc.x, quantifier);
         } break;
 
         case Right:
         {
             win->dc.x = win->bc.x = clamp_to_length(
                 win->buffer,
                 win->bc.y, 
                 win->bc.x + quantifier,
                 exclusive);
         } break;
 
         case Absolute:
         {
             win->bc.y = Minimum(quantifier, win->buffer->lcnt);
         } break;
 
         case Dollar:
         {
             if (quantifier > 0)
             {
                 --quantifier;
             }
 
             win->bc.y = clamped_add(win->bc.y, quantifier, win->buffer->lcnt);
             win->dc.x = win->bc.x = clamp_to_length(
                 win->buffer,
                 win->bc.y,
                 UINT32_MAX,
                 exclusive);
 
         } break;
 
         case Zero:
         {
             win->dc.x = win->bc.x = 0;
         } break;
 
         case Underscore:
         {
             win->dc.x = win->bc.x = skip_space(&win->buffer->iter, win->bc.y);
 
         } break;
 
         case Search:
         {
             // NOTE: Calculate the length properly:
             buffer_cursor bc = { .y = win->bc.y, .x = win->bc.x + 1};
             win->dc.x = win->bc.x = find_char(&win->buffer->iter, match_str,  bc);
 
         } break;
     }
}


static win_range get_motion_range(
    window *win,
    motion motion,
    u32 quantifier,
    str s)
{
    win_range result = {};
    quantifier = Maximum(1, quantifier);
    switch (motion)
    {
        case Up:
        {
            result.first.y = saturating_sub(win->bc.y, quantifier);
            result.first.x = 0;
            result.one_past_end.x = 0;
            result.one_past_end.y = win->bc.y + 1;

        } break;

        case Down:
        {
            result.first.y = win->bc.y;
            result.first.x = 0;
            result.one_past_end.x = 0;
            result.one_past_end.y = clamped_add(win->bc.y, quantifier, win->buffer->lcnt);
        } break;

        case Left:
        {
            result.first.y = win->bc.y;
            result.first.x = saturating_sub(win->bc.x, quantifier);
            result.one_past_end = win->bc;
        } break;

        case Right:
        {
            result.first = win->bc;
            result.one_past_end.y = win->bc.y;
            // This will clamped
            result.one_past_end.x = clamp_to_length(
                win->buffer,
                win->bc.y,
                win->bc.x + quantifier,
                false);
        } break;

        case Absolute:
        {
            str s = {};
            if (quantifier < win->bc.y)
            {
                result = get_motion_range(win, Up, win->bc.y - quantifier, s);
            }
            else if (quantifier > win->bc.y)
            {
                result = get_motion_range(win, Down, quantifier - win->bc.y, s);
            }
        } break;

        case Dollar:
        {
            result.first = win->bc;
            result.one_past_end.y = win->bc.y;
            result.one_past_end.x = clamp_to_length(
                win->buffer,
                win->bc.y,
                UINT32_MAX,
                false);
            // result.one_past_end.x = UINT32_MAX - win->bc.x;
        } break;

        case Zero:
        {
            result.first.y = win->bc.y;
            result.first.x = 0;
            result.one_past_end = win->bc;
        } break;

        case Search:
        {
            result.first = win->bc;
            result.one_past_end.y = win->bc.y;

            buffer_cursor bc = { .y = win->bc.y, .x = win->bc.x + 1 };
            result.one_past_end.x= find_char(&win->buffer->iter, s, bc);

        } break;

        case Underscore:
        {
            result.one_past_end = win->bc;

            result.first.y = win->bc.y;
            result.first.x = skip_space(&win->buffer->iter, win->bc.y);

        } break;

        default:
        {
        } break;
    }
    return result;
}

static inline win_range get_visual_range(window *win)
{
    win_range result;
    switch (compare(win->bc, win->vc))
    {
        case EqualTo:
        case LessThan:
        {
            result.first = win->bc;
            result.one_past_end = win->vc;
        } break;

        case GreaterThan:
        {
            result.first = win->vc;
            result.one_past_end = win->bc;
        } break;
    }

    result.one_past_end.x++;
    return result;
}

static inline win_range get_cursor_range(
    window *win,
    motion motion,
    mode edit_mode,
    u32 quantifier, 
    str s)
{

    win_range result = {};

    // switch 

    if (is_visual(edit_mode))
    {
        result = get_visual_range(win);
    }
    else
    {
        result = get_motion_range(win, motion, quantifier, s);
    }
    return result;
}

