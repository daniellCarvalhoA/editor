
static void move_by_motion(
    window *win,
    motion motion,
    u32 quantifier,
    u8 *match_str,
    u32 match_str_len)
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
            win->dc.y = saturating_sub(win->bc.y, quantifier);

        } break;

        case Down:
        {
            win->dc.y = clamped_add(win->bc.y, quantifier, win->buffer->lcnt);
        } break;

        case Left:
        {
            win->dc.x= saturating_sub(win->bc.x, quantifier);
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
            move_by_motion(win, Right, UINT32_MAX - win->bc.x, NULL, 0);

        } break;

        case Zero:
        {
            win->dc.x = 0;
        } break;

        case Underscore:
        {
            base_iter iter = find_non_white_space(&win->buffer->iter, win->bc.y);
            win->dc.x = position(&iter) - get_position(&win->buffer->iter);
        } break;

        case Search:
        {
            // NOTE: Calculate the length properly:
            buffer_cursor bc = { .y = win->bc.y, .x = win->bc.x + 1 };
            base_iter iter = find_char(
                &win->buffer->iter,
                match_str,
                match_str_len,
                bc);
            win->dc.x = position(&iter) - get_position(&win->buffer->iter);

        } break;
     }
}

// static win_cursor get_desired_cursor(
//     window *win,
//     motion motion,
//     u32 quantifier,
//     u8 *match_str,
//     u32 match_str_len)
// {
//     win_cursor dc = {} ;
//     switch (motion)
//     {
//         case Up:
//         {
//             dc.x = 0;
//             dc.y = saturating_sub(win->bc.y, quantifier + 1);
//         } break;
//
//         case Down:
//         {
//
//         } break;
//     }
// }

static win_range get_motion_range(
    window *win,
    motion motion,
    u32 quantifier,
    u8 *match_str,
    u32 match_str_len)
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
            result.one_past_end.y = 
                clamped_add(win->bc.y, quantifier, win->buffer->lcnt);
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
            Assert(quantifier < UINT32_MAX - win->bc.x);
            // This will clamped
            result.one_past_end.x = win->bc.x + quantifier;
        } break;

        case Absolute:
        {
            if (quantifier < win->bc.y)
            {
                result = get_motion_range(win, Up, win->bc.y - quantifier, NULL, 0);
            }
            else if (quantifier > win->bc.y)
            {
                result = get_motion_range(win, Down, quantifier - win->bc.y, NULL, 0);
            }
        } break;

        case Dollar:
        {
            result.first = win->bc;
            result.one_past_end.y = win->bc.y;
            result.one_past_end.x = UINT32_MAX - win->bc.x;
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
            base_iter iter = find_char(
                &win->buffer->iter,
                match_str,
                match_str_len,
                bc);
            result.one_past_end.x = position(&iter) - get_position(&win->buffer->iter);

        } break;

        case Underscore:
        {
            result.one_past_end = win->bc;

            result.first.y = win->bc.y;
            base_iter iter = find_non_white_space(&win->buffer->iter, win->bc.y);
            result.first.x = position(&iter) - get_position(&win->buffer->iter);

        } break;

        default:
        {
        } break;
    }
    return result;
}


static win_range get_cursor_range(
    window *win,
    motion motion,
    mode edit_mode,
    u32 quantifier, 
    u8 *match_str,
    u32 match_str_len)
{

    win_range result = {};

    if (edit_mode == Visual)
    {
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
    }
    else
    {
        result = get_motion_range(win, motion, quantifier, match_str, match_str_len);
    }
    return result;
}

