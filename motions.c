static u32 clamp_to_length(piece_list *list, const u32 cy, const u32 cx, b32 exclusive)
{
    u32 result = cx;
    u32 line_len = get_line_len(list, &list->iter, cy);
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

static inline buffer_cursor find_next_paragraph(window *win, u32 count) 
{
    // TODO! Make this work for all types of newline characters;
    buffer_cursor result = {};
    base_iter iter = find_line(win->buffer, &win->buffer->iter, win->bc.y + 1);
    u32 prev_line_pos = get_position(win->buffer, &iter);
    for (;;)
    {
        if (!base_next_line(win->buffer, &iter))
        {
            break;
        }

        u32 curr_line_pos = get_position(win->buffer, &iter);
        if (prev_line_pos + 1 == curr_line_pos)
        {
            count--;
            if (count == 0)
            {
                result.y = line_number(&iter) - 1;
                result.x = 0;
                break;
            }
        }

        prev_line_pos = curr_line_pos;
    }
    return result;
}


static inline buffer_cursor find_prev_paragraph(window *win, u32 count)
{
    // TODO! Make this work for all types of newline characters;
    buffer_cursor result = { .x = UINT32_MAX, .y = UINT32_MAX };
    base_iter iter = find_line(win->buffer, &win->buffer->iter, win->bc.y);
    u32 prev_line_pos = get_position(win->buffer, &iter);
    for (;;)
    {
        if (!base_prev_line(win->buffer, &iter))
        {
            break;
        }

        u32 curr_line_pos = get_position(win->buffer, &iter);
        if (prev_line_pos == curr_line_pos + 1)
        {
            count--;
            if (count == 0)
            {
                result.y = line_number(&iter);
                result.x = 0;
                break;
            }
        }
        prev_line_pos = curr_line_pos;
    }
    return result;
}


static void move_by_motion(window *win, motion_spec m_spec) 
{
    u32 quantifier = Maximum(1, m_spec.motion_quantifier);
    switch (m_spec.motion_type)
    {
         case MotionCount:
         case Motion_NoMotion:
         {
             win->dc = win->bc;
         } break;
 
         case Motion_Vertical:
         {
             if (m_spec.flags & MotionFlags_Backwards)
             {
                 win->bc.y = clamped_add(win->bc.y, quantifier, win->buffer->lcnt);
             }
             else
             {
                 win->bc.y = saturating_sub(win->bc.y, quantifier);
             }
             win->bc.x = clamp_to_length(win->buffer, win->bc.y, win->dc.x, true); 
         } break;
 
 
         case Motion_Horizontal:
         {
             if (m_spec.flags & MotionFlags_Backwards)
             {
                 win->dc.x = win->bc.x = saturating_sub(win->bc.x, quantifier);
             }
             else
             {
                 win->dc.x = win->bc.x = clamp_to_length(
                    win->buffer,
                    win->bc.y,
                    win->bc.x + quantifier,
                    (m_spec.flags & MotionFlags_Inclusive) == 0);
             }
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
             win->dc.x = win->bc.x = clamp_to_length(win->buffer, win->bc.y, UINT32_MAX, (m_spec.flags & MotionFlags_Inclusive) == 0);
         } break;
 
         case Zero:
         {
             win->dc.x = win->bc.x = 0;
         } break;
 
         case Underscore:
         {
             win->dc.x = win->bc.x = skip_space(win->buffer, &win->buffer->iter, win->bc.y);
 
         } break;
 
         case Motion_Search:
         {
             if (m_spec.flags & MotionFlags_Range)
             {
                Assert(m_spec.open_close_index >= 0 && 
                       m_spec.open_close_index < (i32) ArrayCount(open_close_pairs));
                char_pair pair = open_close_pairs[m_spec.open_close_index];

                iter_range range = range_search(
                    win->buffer,
                    &win->buffer->iter,
                    win->bc, 
                    pair.open,
                    pair.close);

                if (m_spec.flags & MotionFlags_Exclusive)
                {
                    base_next_cell_(win->buffer, &range.start);
                }
                else
                {     
                    base_next_cell_(win->buffer, &range.end);
                }

                if (m_spec.flags & MotionFlags_Visual)
                {
                    base_prev_cell(win->buffer, &range.end);
                }
                win->bc = win->dc = get_cursor(win->buffer, &range.end);
                win->vc = get_cursor(win->buffer, &range.start);
             }
             else if (m_spec.flags & MotionFlags_Backwards)
             {
                 str match_str = Str(m_spec.match_str, m_spec.match_str_len);
                 win->dc.x = win->bc.x = find_char_back(
                     win->buffer,
                     &win->buffer->iter,
                     match_str,
                     quantifier,
                     win->bc);
                 if (m_spec.flags & MotionFlags_Exclusive)
                 {
                     win->dc.x = win->bc.x = win->bc.x + 1;
                 }
             }
             else
             {   
                 str match_str = Str(m_spec.match_str, m_spec.match_str_len);
                 win->dc.x = win->bc.x = find_char(
                     win->buffer,
                     &win->buffer->iter,
                     match_str,
                     quantifier,
                     win->bc);
                 if (m_spec.flags & MotionFlags_Exclusive)
                 {
                     win->dc.x = win->bc.x = saturating_sub(win->bc.x, 1);
                 }
             }
         } break;

         case Motion_Word:
         {
             if (m_spec.flags & MotionFlags_Range)
             {
                 win->vc = find_word_back(win->buffer, &win->buffer->iter, quantifier, win->bc);
                 win->dc = win->bc = find_word(win->buffer, &win->buffer->iter, quantifier, win->bc);
             }
             else if (m_spec.flags & MotionFlags_Backwards)
             {
                 win->dc = win->bc = find_word_back(win->buffer, &win->buffer->iter, quantifier, win->bc);
             }
             else
             {
                 win->dc = win->bc = find_word(win->buffer, &win->buffer->iter, quantifier, win->bc);
             }
         } break;

         case Motion_Paragraph:
         {
             if (m_spec.flags & MotionFlags_Backwards)
             {
                 base_iter iter = find_line(win->buffer, &win->buffer->iter, win->bc.y);
                 u32 prev_line_pos = get_position(win->buffer, &iter);
                 for (;;)
                 {
                     if (!base_prev_line(win->buffer, &iter))
                     {
                         break;
                     }

                     u32 curr_line_pos = get_position(win->buffer, &iter);
                     if (prev_line_pos == curr_line_pos + 1)
                     {
                         quantifier--;
                         if (quantifier == 0)
                         {
                             win->bc.y = win->dc.y = line_number(&iter);
                             win->bc.x = win->dc.x = 0;
                             break;
                         }
                     }
                     prev_line_pos = curr_line_pos;
                 }
             }
             else
             {

                 base_iter iter = find_line(win->buffer, &win->buffer->iter, win->bc.y + 1);
                 u32 prev_line_pos = get_position(win->buffer, &iter);
                 for (;;)
                 {
                     if (!base_next_line(win->buffer, &iter))
                     {
                         break;
                     }

                     u32 curr_line_pos = get_position(win->buffer, &iter);
                     if (prev_line_pos + 1 == curr_line_pos)
                     {
                         quantifier--;
                         if (quantifier == 0)
                         {
                             win->bc.y = win->dc.y = line_number(&iter) - 1;
                             win->bc.x = win->dc.x = 0;
                             break;
                         }
                     }

                     prev_line_pos = curr_line_pos;
                 }
             }
         } break;
     }
}

static win_range get_motion_range(window *win, motion_spec m_spec)
{
    win_range result = {};
    switch (m_spec.motion_type)
    {
        case Motion_Vertical:
        {
            u32 quantifier = m_spec.motion_quantifier + 1;
            result.first.x = 0;
            result.one_past_end.x = 0;
            if (m_spec.flags & MotionFlags_Backwards)
            {
                result.first.y = win->bc.y;
                result.one_past_end.y = clamped_add(
                    win->bc.y, quantifier, win->buffer->lcnt + 1);
            }
            else
            {
                result.first.y = saturating_sub(win->bc.y, quantifier - 1);
                result.one_past_end.y = win->bc.y + 1;
            }

            if (m_spec.flags & MotionFlags_Exclusive)
            {
                result.one_past_end.y--;
                result.one_past_end.x = clamp_to_length(win->buffer, win->bc.y, UINT32_MAX, false);
            }

        } break;

        case Motion_Horizontal:
        {
            u32 quantifier = Maximum(1, m_spec.motion_quantifier);
            result.first.y = result.one_past_end.y = win->bc.y;
            if (m_spec.flags & MotionFlags_Backwards)
            {
                result.first.x = saturating_sub(win->bc.x, quantifier);
                result.one_past_end.x = win->bc.x;
            }
            else
            {
                result.first.x = win->bc.x;
                result.one_past_end.x = clamp_to_length(win->buffer, win->bc.y, win->bc.x + quantifier, false);
            }
        } break;

        // Make this vertical;
        case Absolute:
        {
        } break;

        case Dollar:
        {
            u32 quantifier = m_spec.motion_quantifier;
            result.first = win->bc;
            result.one_past_end.y = win->bc.y + saturating_sub(quantifier, 1);
            result.one_past_end.x = clamp_to_length(win->buffer, win->bc.y, UINT32_MAX, false);
        } break;

        case Zero:
        {
            result.first.y = win->bc.y;
            result.first.x = 0;
            result.one_past_end = win->bc;
        } break;

        case Motion_Search:
        {
            u32 quantifier = Maximum(1, m_spec.motion_quantifier);
            if (m_spec.flags & MotionFlags_Range)
            {
                Assert(m_spec.open_close_index >= 0 && m_spec.open_close_index < (i32) ArrayCount(open_close_pairs));

                char_pair pair = open_close_pairs[m_spec.open_close_index];

                result = find_boundary(win->buffer, &win->buffer->iter, pair.open, pair.close, win->bc);

                if (m_spec.flags & MotionFlags_Exclusive)
                {
                    result.first.x++;
                }
                else
                {
                    result.one_past_end.x++;
                }
            }
            else
            {
                result.first = win->bc;
                result.one_past_end.y = win->bc.y;
                buffer_cursor bc = { .y = win->bc.y, .x = win->bc.x};
                str s = { .buffer = m_spec.match_str, .len = m_spec.match_str_len };
                result.one_past_end.x = find_char(win->buffer, &win->buffer->iter, s, quantifier, bc) + 1;
                if (m_spec.flags & MotionFlags_Exclusive)
                {
                    result.one_past_end.x = saturating_sub(result.one_past_end.x, 1);
                }
            }

        } break;

        case Underscore:
        {
            u32 quantifier = m_spec.motion_quantifier;
            result.one_past_end = win->bc;
            result.first.y = saturating_sub(win->bc.y, quantifier);
            result.first.x = skip_space(win->buffer, &win->buffer->iter, win->bc.y);

        } break;

        case Motion_Word:
        {
            u32 quantifier = m_spec.motion_quantifier;
            if (m_spec.flags & MotionFlags_Range)
            {
                result.first = find_word_back(win->buffer, &win->buffer->iter, quantifier, win->bc);
                result.one_past_end = find_word(win->buffer, &win->buffer->iter, quantifier, win->bc);
            }
            else if (m_spec.flags & MotionFlags_Backwards)
            {
                result.first = find_word_back(win->buffer, &win->buffer->iter, quantifier, win->bc);
                result.one_past_end = win->bc;
            }
            else
            {
                result.first = win->bc;
                result.one_past_end = find_word(win->buffer, &win->buffer->iter, quantifier, win->bc);
            }
        } break;

        case Motion_Paragraph:
        {
            u32 quantifier = m_spec.motion_quantifier;
            if (m_spec.flags & MotionFlags_Range)
            {
                result.first = find_prev_paragraph(win, quantifier);
                result.one_past_end = find_next_paragraph(win, quantifier);
            }
            else if (m_spec.flags & MotionFlags_Backwards)
            {
                result.one_past_end = (struct buffer_cursor) {. x = 0, .y = win->bc.y + 1 };
                result.first = find_prev_paragraph(win, quantifier);
            }
            else
            {
                result.first = (struct buffer_cursor) { .x = 0, .y = win->bc.y };
                result.one_past_end = find_next_paragraph(win, quantifier);
            }
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

static inline win_range get_line_visual_range(window *win)
{
    win_range result = {};
    if (win->bc.y <= win->vc.y)
    {
        result.first.y = win->bc.y;
        result.one_past_end.y = win->vc.y + 1;
    }
    else
    {
        result.first.y = win->vc.y;
        result.one_past_end.y = win->bc.y + 1;
    }
    return result;
}
            //
static inline win_range get_cursor_range(window *win, motion_spec m_spec, mode edit_mode)
{

    win_range result = {};

    switch (edit_mode)
    {
        case Visual:
        {
            result = get_visual_range(win);
        } break;

        case LineVisual:
        {
            result = get_line_visual_range(win);
        } break;

        default:
        {
            result = get_motion_range(win, m_spec);
        } break;
    }
    return result;
}
