#define TAB_STOP 4
static void fill_command_grid(screen *screen, window *win, grid_view grid) 
{
    command_buffer *c_buffer = &win->c_buffer;

    u16 w_height = get_height(screen, win);

    for (u32 j = 0; j < w_height; ++j)
    {
        grid_line g_line = get_grid_line(grid, j);
        u16 i = 0;
        u16 col = 0;
        while (i < c_buffer->len)
        {
            u32 cell_len = utf8_charlen_unchecked(c_buffer->buffer + i , c_buffer->len - i);

            grid_type type = U32;
            if (cell_len <= 3)
            {
                type = cell_len - 1;
            }

            g_line.grid->types[g_line.line_start + col] = type;
            u8 *data = get_cell_data(g_line, col, type);
            attr *at = get_cell_attr_(g_line, col);

            memcpy(data, (void *) (c_buffer->buffer + i), sizeof(u8) * (type + 1));
            *at = Default;

            col++;
            i += cell_len;
        }
    }
}

static void fill_grid(screen *screen, window *win, grid_view grid, mode edit_mode)
{
    window *active_window = screen->active_window;
    base_iter iter;
    b32 not_over = base_init_(win->buffer, LineNumber, &iter);
    // Assert(not_over);
    base_advance_by_line(&iter, win->top_line);
    normalize(&iter);

    u32 line   = line_number(&iter);
    u32 width  = get_width(screen, win);
    u32 height = get_height(screen, win);
    height     = (win->flags & WinFlags_StatusLineVisible) ? (height - 1) : height;

    win_cursor visual_cursor = win->vc; // { .x = win->vcx, .y = win->vcy }; 
    win_cursor curr_cursor   = win->bc; //{ .x = win->bcx, .y = win->bcy };
    win_range range = make_range(visual_cursor, curr_cursor);
    while (not_over && line < win->top_line + height)
    {
        u32 i = 0;
        // skip left scroll region
        while ((i++ < win->cx_offset) && line == line_number(&iter) && not_over)
        {
            not_over = base_next_cell_(&iter);
        }

        if (line_number(&iter) != line)
        {
            line++;
            continue;
        }

        if (!not_over)
        {
            break;
        }

        u32 col = 0;
        u32 v_col = 0;
        while (not_over && col < width)
        {
            cell_item item   = base_next_cell(&iter);
            if (!item.valid)
            {
                not_over = false;
                break;
            }
            grid_line g_line = get_grid_line(grid, (line - win->top_line));

            if ((u8) item.cell == '\n')
            {
                break;
            }

            grid_type type = U32;
            if (item.len <= 3)
            {
                type = item.len - 1;
            }

            if (item.cell == '\t')
            {
                Assert(type == U8);
                u32 next_col = 1 + col;
                u32 tab_length = 1 + TAB_STOP - (next_col % TAB_STOP);
                tab_length = Minimum(tab_length, width - col);
                memset(
                    g_line.grid->types + g_line.line_start + col,
                    type,
                    tab_length);
                u8 *data = get_cell_data(g_line, col, type);
                attr *at = get_cell_attr_(g_line, col);

                memset(data, ' ', tab_length);
                if (edit_mode == Visual) 
                {
                    screen_cursor s_cursor = { 
                        .x = col + tab_length - 1,
                        .y = line  - win->top_line
                    };
                    win_cursor w_cursor = map_screen_cursor_to_win_cursor(
                        active_window,
                        s_cursor); 

                    if (is_in_range(range, w_cursor) && compare(w_cursor, active_window->bc) != EqualTo)
                    {
                        memset(at, Reversed, tab_length);
                    }
                }

                u8 *gv_col = get_cell_vcol_(g_line, v_col);
                *gv_col = tab_length;
                
                col += tab_length;

                not_over = item.valid;
            }
            else
            {

                g_line.grid->types[g_line.line_start + col] = type;
                u8 *data = get_cell_data(g_line, col, type);
                attr *at = get_cell_attr_(g_line, col);

                memcpy(data, (void *) (&item.cell), sizeof(u8) * (type + 1));
                *at = Default;

                if (edit_mode == Visual) 
                {
                    screen_cursor s_cursor = { .x = col, .y = line  - win->top_line};
                    win_cursor w_cursor = map_screen_cursor_to_win_cursor(active_window, s_cursor); 


                    if (is_in_range(range, w_cursor) && compare(w_cursor, active_window->bc) != EqualTo)
                    {
                        *at = Reversed;
                    }
                }
                u8 *gv_col = get_cell_vcol_(g_line, v_col);
                *gv_col = 1;
                
                col++;

                not_over = item.valid;
            }
            v_col++;
        }

        if (col == width)
        {
            not_over = base_next_line(&iter);
        }
        line++;
    }

    if (win->flags & WinFlags_StatusLineVisible)
    {
        grid_line g_line = get_grid_line(grid, height);

        u8 *types = get_cell_type_(g_line, 0);
        attr *attribute  = get_cell_attr_(g_line, 0);
        u8 *data         = get_cell_data(g_line, 0, U8);

        for (u32 i = 0; i < width; ++i)
        {
            types[i] = U8;
        }
        // memset(types, U8, sizeof(grid_type) * width);
        memset(attribute, Reversed, width);

        u32 len = 0;
        if (win == active_window)
        {
            const char *str_mode = mode_str(edit_mode);
            len = Minimum(width, strlen(str_mode));
            memcpy(data, (u8 *) str_mode, len);
        }
        char *file_name = win->buffer->filepath;
        u32 file_len = Minimum(width - len, strlen(file_name));

        memcpy(data + len , (u8 *) file_name, file_len);
        memset(data + len + file_len, ' ', width - (len + file_len));
    }
}

static void line_diff(screen *screen, window *win, grid_line old, grid_line new)
{
    u16 prev_j = 0;
    u16 width = get_width(screen, win);
    for (u16 j = 0; j < width; j++)
    {
        while ((j < width) && cells_are_equal_(old, new, j, j))
        {
            j++;
        }

        if (j == width)
        {
            break;
        }

        if (j - prev_j)
        {
            move_cursor_right(screen, j - prev_j);
        }

        u32 type_idx = j;

        grid_type prev_type = get_cell_type(new, j);
        attr prev_attr = get_cell_attr(new, j);
        j++;

        while ((j < width) && !cells_are_equal_(old, new, j, j))
        {
            grid_type curr_type = get_cell_type(new, j);
            attr curr_attr = get_cell_attr(new, j);
            // u8 vcol = get_cell_vcol(new, j);
            if (curr_type != prev_type || curr_attr != prev_attr)
            {
                u32 same_type_seq_diff_length = j - type_idx;
                copy_cells(old, new, type_idx, same_type_seq_diff_length);

                u8 *cell_data = get_cell_data(new, type_idx, prev_type);
                if (curr_attr == Reversed)
                {
                    write_string(screen, (u8 *) "\x1b[7m", sizeof("\x1b[7m") - 1);
                }
                write_string(screen, cell_data, same_type_seq_diff_length * (prev_type + 1));

                type_idx = j;
                prev_type = curr_type;
                prev_attr = curr_attr;

                if (curr_attr == Reversed)
                {
                    write_string(screen, (u8 *) "\x1b[27m", sizeof("\x1b[27m") - 1);
                }
            }
            j++;
        }

        prev_j = j;

        if (type_idx < j)
        {
            u32 same_type_seq_diff_length = j - type_idx;
            copy_cells(old, new, type_idx, same_type_seq_diff_length);

            u8 *cell_data = get_cell_data(new, type_idx, prev_type);
            if (prev_attr == Reversed)
            {
                write_string(screen, (u8 *) "\x1b[7m", sizeof("\x1b[7m") - 1);
            }
            write_string(screen, cell_data, same_type_seq_diff_length * (prev_type + 1));

            if (prev_attr == Reversed)
            {
                write_string(screen, (u8 *) "\x1b[27m", sizeof("\x1b[27m") - 1);
            }
        }
    } 
}

static void grid_diff(screen *screen, window *win, grid_view old, grid_view new)
{
    u16 screen_y = get_screen_y(screen, win);
    u16 screen_x = get_screen_x(screen, win);

    place_cursor(screen, screen_y, screen_x);

    u16 win_height = get_height(screen, win);
    for (u16 i = 0; i < win_height; ++i)
    {
        grid_line old_line = get_grid_line(old, i);
        grid_line new_line = get_grid_line(new, i);

        old_line.line_start += screen->cols * new.y_offset + new.x_offset;

        set_cursor_column(screen, screen_x);
        line_diff(screen, win, old_line, new_line); 
        if (i + 1 < win_height)
        {
            write_string(screen, (u8 *) "\r\n", sizeof("\r\n") - 1);
        }
    }
}
