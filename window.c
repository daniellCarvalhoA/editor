// TODOS:
//
// 1> Test scrolling invariants: 
//  For instance > up down isomorphism: 
//    If one scrolls x times up and then x times down, the state of the window and the grid should be 
//    semantically the same (perhaps even structurally);
// 2> Draw window boundaries; v
// 3> clean window API.
// 4> optimize window rendering.
//      4.1> Maybe use simd when calculating the diff between the old state of the screen and the new state.
//      4.2> Buffer terminal output.
// 

static window *create_window(memory_arena *arena, screen *screen, layout layout, win_flags flags)
{
    window *win = PushStruct(arena, window, default_arena_params());
    win->layout = layout;
    win->flags = flags;
    INIT_LIST_HEAD(&win->sibling);
    INIT_LIST_HEAD(&win->next_in_buffer);
    INIT_LIST_HEAD(&win->first_child);
    return win;
}

static inline void set_window_vertical_dim(window *win, u16 y, u16 height)
{
    Assert(!win->parent || win->parent->layout == Vertical);
    win->offset = y;
    win->full_dim = win->dyn_dim = height;
}

static inline void set_window_horizontal_dim(window *win, u16 x, u16 width)
{
    Assert(!win->parent || win->parent->layout == Horizontal);
    win->offset = x;
    win->full_dim = win->dyn_dim = width;
}

// static inline void 

// static inline void set_window_dimensions(window *win, u16 x, u16 y, u16 width, u16 height)
// {
//     win->screen_x = x;
//     win->screen_y = y;
//     win->width    = width;
//     win->height   = height;
// }

static window *create_from(screen *screen, memory_arena *arena, window *old)
{
    if (!old)
    {
        old = active_window;
    }
    window *new = create_window(arena, screen, LeafBuffer, 0);
    new->top_line = old->top_line;
    return new;
}

static void set_window_params(window *win, screen *screen, layout layout)
{
    if (layout == LeafBuffer || layout == LeafCommand)
    {
        u16 win_height = get_height(screen, win);
        u16 win_width  = get_width(screen, win);
        u16 screen_y   = get_screen_y(screen, win);
        u16 screen_x   = get_screen_x(screen, win);
        // TODO: realloc 
        win->view = default_grid_view(&screen->grid, win_height, win_width);
        u32 line_start = screen_y * screen->cols;
        for (u32 i = 0; i < win_height; ++i)
        {
            win->view.grid_lines[i] = line_start + (i * screen->cols) + screen_x;
        }
    }
}

static window *create_first_window(memory_arena *arena, screen *screen, layout layout) 
{
    window *win = create_window(arena, screen, layout, WinFlags_StatusLineVisible);
    set_window_params(win, screen, layout);
    return win;
}

static u32 window_intersection_vertical(screen *screen, window *root, u32 x, window **result, u32 result_len)
{
    window *win = root;
    window **cursor = result;
    u32 total = 0;
    switch (win->layout)
    {
        case LeafBuffer:
        case LeafCommand:
        {
            u16 screen_x  = get_screen_x(screen, win);
            u16 win_width = get_width(screen, win);
            if (x >= screen_x && x < screen_x + win_width)
            {
                *result = win;
                total++;
            }
        } break;

        case Horizontal:
        {
            window *child;
            list_for_each_entry(child, &win->first_child, sibling)
            {
                u16 screen_x    = get_screen_x(screen, child);
                u16 child_width = get_width(screen, child);
                if (x >= screen_x && x < screen_x + child_width)
                {
                    total += window_intersection_vertical(screen, child, x, result + total, result_len);
                }
            }

        } break;
        case Vertical:
        {
            u16 screen_x  = get_screen_x(screen, win);
            u16 win_width = get_width(screen, win);
            if (x >= screen_x && x < screen_x + win_width)
            {
                window *child;
                list_for_each_entry(child, &win->first_child, sibling)
                {
                    total += window_intersection_vertical(screen, child, x, result + total, result_len);
                }
            }
        } break;
    }

    return total;
}

// Calculates the set of windows which intesect a a given Horizontal line.
static u32 window_intersection_horizontal(
    screen *screen, window *root, u32 y, window **result, u32 result_len)
{
    window *win = root;
    window **cursor = result;
    u32 total = 0;
    switch (win->layout)
    {
        case LeafBuffer:
        case LeafCommand:
        {
            u16 screen_y   = get_screen_y(screen, win);
            u16 win_height = get_height(screen, win);
            if (y >= screen_y && y < screen_y + win_height)
            {
                *result = win;
                total++;
            }
        } break;

        case Horizontal:
        {
            u16 screen_y   = get_screen_y(screen, win);
            u16 win_height = get_height(screen, win);
            if (y >= screen_y && y < screen_y + win_height)
            {
                window *child;
                list_for_each_entry(child, &win->first_child, sibling)
                {
                    total += window_intersection_horizontal(screen, child, y, result + total, result_len);
                }
            }
        } break;
        case Vertical:
        {
            window *child;
            list_for_each_entry(child, &win->first_child, sibling)
            {
                u16 screen_y     = get_screen_y(screen, win);
                u16 child_height = get_height(screen, win);
                if (y >= screen_y && y < screen_y + child_height)
                {
                    total += window_intersection_horizontal(screen, child, y, result + total, result_len);
                }
            }
        } break;
    }
    Assert(total < result_len);

    return total;
}

// TODO: make count (i32) and make it go left if (count < 0)
static void move_horizontal(screen *screen,  window *root, u32 count, b32 right) 
{
    u32 y = active_window->cy;
    window *horizontal_wins[64];

    u32 total = window_intersection_horizontal(screen, root, y, horizontal_wins, 64);

    // I'm assuming that the windows are in order (left to right on the screen);
    // Moreover: The active_window must be in horizontal_wins.

    // find index of active in horizontal_wins;

    u32 active_index = UINT32_MAX;
    for (u32 i = 0; i < total; ++i)
    {
        if (horizontal_wins[i] == active_window)
        {
            active_index = i;
            break;
        }
    }
    Assert(active_index != UINT32_MAX);


    u32 next_window_index;
    if (right)
    {
        next_window_index = (active_index + count) % total;
    }
    else
    {
        next_window_index = (active_index - count + total) % total;
    }

    if (horizontal_wins[next_window_index]->layout != LeafCommand)
    {
        active_window = horizontal_wins[next_window_index];
    }


    return;
}

static void move_vertical(screen *screen, window *root, u32 count, b32 down)
{
    u32 x = active_window->cx;
    window *vertical_wins[64];

    u32 total = window_intersection_vertical(screen, root, x, vertical_wins, 64);

    u32 active_index = UINT32_MAX;
    for (u32 i = 0; i < total; ++i)
    {
        if (vertical_wins[i] == active_window)
        {
            active_index = i;
            break;
        }
    }
    Assert(active_index != UINT32_MAX);

    u32 next_window_index;
    if (down)
    {
        next_window_index = (active_index + count) % total;
    }
    else
    {
        next_window_index = (active_index - count + total) % total;
    }

    if (vertical_wins[next_window_index]->layout != LeafCommand)
    {

        active_window = vertical_wins[next_window_index];
    }

    return;
}

static u32 intersections_up_horizontal(
    screen *screen, window *win, u16 y, u16 x_min, u16 x_max, u16 *x_points, u32 max_len)
{
    u32 total = 0;
    switch (win->layout)
    {
        case LeafBuffer:
        case LeafCommand:
        {
            u16 screen_x   = get_screen_x(screen, win);
            u16 win_width = get_width(screen, win);
            if (screen_x + win_width < x_max)
            {
                *x_points = screen_x + win_width;
                total++;
            }
        } break;

        case Vertical:
        {
            window *child;
            list_for_each_entry(child, &win->first_child, sibling)
            {
                u16 screen_y     = get_screen_y(screen, child);
                u16 child_height = get_height(screen, child);
                if (screen_y + child_height == y)
                {
                    total += intersections_up_horizontal(
                            screen, child, y, x_min, x_max, x_points + total, max_len);
                }
            }
        } break;

        case Horizontal:
        {
            window *child;
            list_for_each_entry(child, &win->first_child, sibling)
            {
                u16 screen_x = get_screen_x(screen, child);
                u16 child_width = get_width(screen, child);
                if (screen_x + child_width < x_max)
                {
                    total += intersections_up_horizontal(screen, child, y, x_min, x_max, x_points + total, max_len);
                }
            }
        } break;
    }
    Assert(total < max_len);
    return total;
}

static u32 intersections_down_horizontal(
    screen *screen, window *win, u16 y, u16 x_min, u16 x_max, u16 *x_points, u32 max_len)
{
    u32 total = 0;
    switch (win->layout)
    {
        case LeafCommand:
        case LeafBuffer:
        {
            u16 screen_x  = get_screen_x(screen, win);
            u16 win_width = get_width(screen, win);
            if (screen_x + win_width < x_max)
            {
                *x_points = screen_x + win_width;
                total++;
            }
        } break;

        case Vertical:
        {
            window *child;
            list_for_each_entry(child, &win->first_child, sibling)
            {
                u16 screen_y = get_screen_y(screen, child);
                if (screen_y - 1 == y)
                {
                    total += intersections_down_horizontal(screen, child, y, x_min, x_max, x_points + total, max_len);
                }
            }
        } break;

        case Horizontal:
        {
            window *child;
            list_for_each_entry(child, &win->first_child, sibling)
            {
                u16 screen_x    = get_screen_x(screen, child);
                u16 child_width = get_width(screen, child);
                if (screen_x + child_width < x_max)
                {
                    total += intersections_down_horizontal(screen, child, y, x_min, x_max, x_points + total, max_len);
                }
            }
        } break;
    }

    Assert(total < max_len);
    return total;
}

static u32 intersections_left_vertical(
    screen *screen, window *win, u16 x, u16 y_min, u16 y_max, u16 *y_points, u32 max_len)
{
    u32 total = 0;
    switch (win->layout)
    {
        case LeafCommand:
        case LeafBuffer:
        {
            u16 screen_y   = get_screen_y(screen, win);
            u16 win_height = get_height(screen, win);
            if (screen_y + win_height < y_max)
            {
                *y_points = screen_y + win_height;
                total++;
            }
        } break;

        case Vertical:
        {
            window *child;
            list_for_each_entry(child, &win->first_child, sibling)
            {
                u16 screen_y = get_screen_y(screen, child);
                u16 child_height = get_height(screen, child);
                if (screen_y + child_height < y_max)
                {
                    total += intersections_left_vertical(screen, child, x, y_min, y_max, y_points + total, max_len);
                }
            }
        } break;

        case Horizontal:
        {
            window *child;
            list_for_each_entry(child, &win->first_child, sibling)
            {
                u16 screen_x = get_screen_x(screen, child);
                u16 child_width = get_width(screen, child);
                if (screen_x + child_width == x)
                {
                    total += intersections_left_vertical(screen, child, x, y_min, y_max, y_points + total, max_len);
                }
            }
        } break;
    }
    Assert(total < max_len);
    return total;
}

static u32 intersections_right_vertical(
    screen *screen, window *win, u16 x, u16 y_min, u16 y_max, u16 *y_points, u32 max_len)
{
    u32 total = 0;
    switch (win->layout)
    {
        case LeafCommand:
        case LeafBuffer:
        {
            u16 screen_y = get_screen_y(screen, win);
            u16 win_height = get_height(screen, win);
            if (screen_y + win_height < y_max)
            {
                *y_points = screen_y + win_height;
                total++;
            }
        } break;

        case Vertical:
        {
            window *child;
            list_for_each_entry(child, &win->first_child, sibling)
            {
                u16 screen_y = get_screen_y(screen, child);
                u16 child_height = get_height(screen, child);
                if (screen_y + child_height < y_max)
                {
                    total += intersections_right_vertical(screen, child, x, y_min, y_max, y_points + total, max_len);
                }
            }
        } break;

        case Horizontal:
        {
            window *child;
            list_for_each_entry(child, &win->first_child, sibling)
            {
                u16 screen_x = get_screen_x(screen, child);
                if (screen_x == x)
                {
                    total += intersections_right_vertical(screen, child, x, y_min, y_max, y_points + total, max_len);
                }
            }
        } break;
    }

    Assert(total < max_len);
    return total;
}

static void draw_borders(screen *screen, window *win)
{
    switch (win->layout)
    {
         case Vertical:
         {
             window *prev = 0;
             window *child;

             u16 w_width   = get_width(screen, win); 
             u16 w_screen_x = get_screen_x(screen, win);
             list_for_each_entry(child, &win->first_child, sibling)
             {
                 if (prev)
                 {
                     // Make this dynamic or track number of total children, and 
                     // make that the maximum size; 
                     u16 p_screen_y = get_screen_y(screen, prev);
                     u16 c_screen_y = get_screen_y(screen, child);
                     u16 p_height   = get_height(screen, prev);

                     u16 up_x_points[16];
                     u16 down_x_points[16];

                     u16 num_up_points = intersections_up_horizontal(
                         screen,
                         prev,
                         p_screen_y + p_height,
                         w_screen_x,
                         w_screen_x + w_width,
                         up_x_points,
                         ArrayCount(up_x_points));

                     u16 num_down_points = intersections_down_horizontal(
                         screen,
                         child,
                         c_screen_y,
                         w_screen_x,
                         w_screen_x + w_width,
                         down_x_points,
                         ArrayCount(down_x_points));
                      
                     u16 row = c_screen_y - 1;
                     u16 up_index = 0;
                     u16 down_index = 0;

                     place_cursor(screen, row, w_screen_x);
                     for (u16 i = w_screen_x; i < w_screen_x + w_width; i++)
                     {
                         b16 is_in_up = false; 
                         for (u16 j = up_index; j < num_up_points; ++j)
                         {
                             is_in_up |= up_x_points[j] == i;
                         }
 
                         b16 is_in_down = false;
                         for (u16 j = down_index; j < num_down_points; j++)
                         {
                             is_in_down |= down_x_points[j] == i;
                         }
 
                         if (is_in_up && is_in_down)
                         {
                             up_index++;
                             down_index++;
                             write_string(screen,  (u8 *) "┼", sizeof("┼") - 1);
                         }
                         else if (is_in_up)
                         {
                             write_string(screen,  (u8 *) "┴", sizeof("┴") - 1);
                             up_index++;
                         } 
                         else if (is_in_down)
                         {
                             write_string(screen,  (u8 *) "┬", sizeof("┬") - 1);
                             down_index++;
                         }
                         else
                         {
                             write_string(screen,  (u8 *) "─", sizeof("─") - 1);
                         }
                     }
                 }
                 draw_borders(screen, child);
                 prev = child;
             }
         } break;

         case Horizontal:
         {
            window *prev = 0;
            window *child;

            u16 w_screen_y = get_screen_y(screen, win);
            u16 w_height   = get_height(screen, win);
            list_for_each_entry(child, &win->first_child, sibling)
            {
                 if (prev)
                 {
                     // Make this dynamic or track number of total children, and 
                     // make that the maximum size; 
                     u16 left_y_points[16];
                     u16 right_y_points[16];

                     u16 p_screen_x = get_screen_x(screen, prev);
                     u16 c_screen_x = get_screen_x(screen, child);
                     u16 p_width    = get_width(screen, prev);

                     u16 num_left_points  = intersections_left_vertical(
                            screen,
                            prev,
                            p_screen_x + p_width,
                            w_screen_y,
                            w_screen_y + w_height,
                            left_y_points,
                            ArrayCount(left_y_points));

                     u16 num_right_points = intersections_right_vertical(
                            screen,
                            child,
                            c_screen_x,
                            w_screen_y,
                            w_screen_y + w_height,
                            right_y_points,
                            ArrayCount(right_y_points));
                     
                     u16 col = c_screen_x - 1;
                     u16 left_index = 0;
                     u16 right_index = 0;
                     for (u16 j = w_screen_y; j < w_screen_y + w_height; ++j)
                     {
                         b16 is_in_left = false; 
                         for (u16 i = left_index; i < num_left_points; ++i)
                         {
                             is_in_left |= left_y_points[i] == j;
                         }
 
                         b16 is_in_right = false;
                         for (u16 i = right_index; i < num_right_points; i++)
                         {
                             is_in_right |= right_y_points[i] == j;
                         }
 
                         place_cursor(screen, j, col);
                         if (is_in_left && is_in_right)
                         {
                             left_index++;
                             right_index++;
                             write_string(screen,  (u8 *) "┼", sizeof("┼") - 1);
                         }
                         else if (is_in_left)
                         {
                             write_string(screen,  (u8 *) "┤", sizeof("┤") - 1);
                             left_index++;
                         } 
                         else if (is_in_right)
                         {
                             write_string(screen,  (u8 *) "├", sizeof("├") - 1);
                             right_index++;
                         }
                         else
                         {
                             write_string(screen,  (u8 *) "│", sizeof("│") - 1);
                         }
                     }
                }
                draw_borders(screen, child);
                prev = child;
            }
         } break;

         default:
         {
         } break;
    }
}

static void clean_borders(screen *screen, window *root)
{
    window *win = root;

    u16 w_screen_y = get_screen_y(screen, win);
    u16 w_screen_x = get_screen_x(screen, win);
    u16 w_height   = get_height(screen, win);
    u16 w_width    = get_width(screen, win);

    switch (win->layout)
    {
         case Vertical:
         {

             window *child;
             list_for_each_entry(child, &win->first_child, sibling)
             {
                 u16 c_screen_y = get_screen_y(screen, child);
                 u16 c_height   = get_height(screen, child);

                 if (c_screen_y + c_height < w_screen_y + w_height)

                 {
                     u32 row = c_screen_y + c_height;
                     u8 *grid_start = screen->grid.text_8 + row * screen->grid.cols + w_screen_x;
                     u32 num_spaces = w_width;
                     if (w_screen_x + w_width < screen->cols)
                     {
                         num_spaces++;
                     }
                     memset(grid_start, ' ', num_spaces);

                     u8 space[w_width];
                     memset(space, ' ', w_width);
                     place_cursor(screen, row, w_screen_x);
                     write_string(screen, space, w_width);

                     u8 *attr_start = screen->grid.attr + row * screen->grid.cols + w_screen_x;
                     memset(attr_start, Default, w_width);
                 }
                 clean_borders(screen, child);
             }
         } break;

         case Horizontal:
         {
            window *child;
            list_for_each_entry(child, &win->first_child, sibling)
            {
                u16 c_screen_x = get_screen_x(screen, child);
                u16 c_width    = get_width(screen, child);

                if (c_screen_x + c_width < w_screen_x + w_width)
                {
                    u32 col = c_screen_x + c_width;
                    u8 *grid_start = screen->grid.text_8 + w_screen_y * screen->grid.cols + col;
                    u8 *attr_start = screen->grid.attr   + w_screen_y * screen->grid.cols + col;

                    for (u32 j = 0; j < w_height; ++j)
                    {
                        place_cursor(screen, j + w_screen_y, col);
                        write_string(screen, (u8 *) " ", 1);
                        grid_start[screen->grid.cols * j] = ' ';
                        attr_start[screen->grid.cols * j] = Default;
                    }
                }
                clean_borders(screen, child);
            }
         } break;

         default:
         {
         } break;
    }
}

static void update_layout(screen *screen, window *root)
{
    u32 num_separators = root->num_children - (root->num_fixed + 1);
    switch (root->layout)
    {
        case Vertical:
        {
            u16 total_height      = get_dyn_height(screen, root) - num_separators;
            u16 height_per_window = total_height / (root->num_children - root->num_fixed);
            u16 rem               = total_height % (root->num_children - root->num_fixed);;
            u16 y                 = get_screen_y(screen, root);

            window *win;
            list_for_each_entry(win, &root->first_child, sibling)
            {
                u16 this_win_height;
                if (win->flags & WinFlags_Fixed)
                {
                    this_win_height = win->full_dim;
                }
                else
                {
                    this_win_height = height_per_window;
                    if (rem > 0)
                    {
                        this_win_height++;
                        rem--;
                    }
                }
                set_window_vertical_dim(win, y, this_win_height);

                y += this_win_height + 1;

                if (win->buffer)
                {
                    win->buffer->changed = true;
                    win->buffer->top_changed = win->top_line;
                }

                set_window_params(win, screen, win->layout);
                update_layout(screen, win);
            }
        } break;

        case Horizontal:
        {
            u16 total_width      = get_width(screen, root) - num_separators;
            u16 width_per_window = total_width / root->num_children;
            u16 rem              = total_width % root->num_children;
            u16 x                = get_screen_x(screen, root);

            window *win;
            list_for_each_entry(win, &root->first_child, sibling)
            {
                u16 this_win_width;
                if (win->flags & WinFlags_Fixed)
                {
                    this_win_width = win->full_dim;
                }
                else
                {
                    this_win_width = width_per_window;
                    if (rem > 0)
                    {
                        this_win_width++;
                        rem--;
                    }
                }
                set_window_horizontal_dim(win, x, this_win_width);
                x += this_win_width + 1;

                if (win->buffer)
                {
                    win->buffer->changed = true;
                    win->buffer->top_changed = win->top_line;
                }

                set_window_params(win, screen, win->layout);
                update_layout(screen, win);
            }
        } break;

        default:
        {
        } break;
    }
}

// static void set_window_height(window *win, screen *screen, u16 new_height)
// {
//     window *parent = win->parent;
//     if (parent && new_height < parent->height - (parent->num_children + 1))
//     {
//         switch (parent->layout)
//         {
//             case Vertical:
//             { 
//                 win->height = new_height;
//                 u32 num_separators    = parent->num_children - 1;
//                 u32 total_height      = parent->height - win->height - num_separators;
//                 u32 height_per_window = total_height / num_separators;
//                 u32 rem               = total_height % num_separators;
//                 u32 y                 = parent->screen_y;
//
//                 window *child;
//                 list_for_each_entry(child, &parent->first_child, sibling)
//                 {
//                     if (child != win) 
//                     {
//                         child->height = height_per_window;
//                         if (rem > 0)
//                         {
//                             child->height++;
//                             rem--;
//                         }
//                     }
//
//                     child->screen_y = y;
//                     y += child->height + 1;
//
//                     if (child->buffer)
//                     {
//                         child->buffer->changed = true;
//                         child->buffer->top_changed = child->top_line;
//                     }
//
//                     set_window_params(child, screen, child->layout, true);
//                     update_layout(screen, child);
//                 }
//  
//             } break;
//
//             case Horizontal:
//             {
//                 
//             } break;
//
//             default:
//             {
//                 Assert(0);
//             }
//         }
//
//     }
// }

static void increase_window_height(screen *screen, window *win)
{
    window *parent = win->parent;

    if (parent)
    {
        switch (parent->layout)
        {
            case Vertical:
            {
                u16 w_height = get_height(screen, win);
                u16 p_height = get_height(screen, parent);
                if (w_height < p_height - parent->num_children)
                {
                    win->full_dim++;

                    u16 num_separators    = parent->num_children - 1;
                    u16 total_height      = p_height - win->full_dim - num_separators;
                    u16 height_per_window = total_height / num_separators;
                    u16 rem               = total_height % num_separators;
                    u16 y                 = get_screen_y(screen, parent);

                    window *child;
                    list_for_each_entry(child, &parent->first_child, sibling)
                    {
                        if (child != win) 
                        {
                            child->full_dim = height_per_window;
                            if (rem > 0)
                            {
                                child->full_dim++;
                                rem--;
                            }
                        }

                        child->offset = y;
                        y += child->full_dim + 1;

                        if (child->buffer)
                        {
                            child->buffer->changed = true;
                            child->buffer->top_changed = child->top_line;
                        }

                        set_window_params(child, screen, child->layout);
                        update_layout(screen, child);
                    }
                }
            } break;

            case Horizontal:
            {
                increase_window_height(screen, parent);
            } break;

            default:
            {
                Assert(0);
            } break;
        }
    }
}

static void increase_window_width(screen *screen, window *win)
{
    window *parent = win->parent;

    if (parent)
    {
        switch (parent->layout) 
        {
            case Vertical:
            {
                increase_window_width(screen, parent);
            } break;

            case Horizontal:
            {
                u16 w_width = get_width(screen, win);
                u16 p_width = get_width(screen, parent);

                if (w_width < p_width - parent->num_children)
                {
                    win->full_dim++;
                    u32 num_separators   = parent->num_children - 1;
                    u32 total_width      = p_width - win->full_dim - num_separators;
                    u32 width_per_window = total_width / num_separators;
                    u32 rem              = total_width % num_separators;
                    u32 x                = get_screen_x(screen, parent);

                    window *child;
                    list_for_each_entry(child, &parent->first_child, sibling)
                    {
                        if (child != win)
                        {
                            child->full_dim = width_per_window;
                            if (rem > 0)
                            {
                                child->full_dim++;
                                rem--;
                            }
                        }

                        child->offset = x;
                        x += child->full_dim + 1;
                        if (child->buffer)
                        {
                            child->buffer->changed = true;
                            child->buffer->top_changed = child->top_line;
                        }

                        set_window_params(child, screen, child->layout);
                        update_layout(screen, child);
                    }
                }
            } break;

            default:
            {
            } break;
        }
    }
}

static void attach_window(
    memory_arena *arena,
    screen *screen,
    window **root,
    window *new,
    window *old,
    layout layout,
    u16 fixed_dim)
{
    if (!old)
    {
        old = active_window;
    }

    window *parent = old->parent;
    if (!parent)
    {
        parent = create_window(arena, screen, layout, 0);
        list_add_tail(&old->sibling, &parent->first_child);
        list_add_tail(&new->sibling, &parent->first_child);

        parent->num_children = 2;
        new->parent = parent;
        old->parent = parent;
        new->idx = 1; 

        *root = parent;

        switch (layout)
        {
            case Vertical:
            {
                parent->full_dim = parent->dyn_dim = get_height(screen, parent);
            } break;

            case Horizontal:
            {
                parent->full_dim = parent->dyn_dim = get_width(screen, parent);
            } break;

            default:
            {
            } break;
        }
    }
    else
    {
        Assert(parent->layout != LeafCommand && parent->layout != LeafBuffer);
        clean_borders(screen, *root);

        if (parent->layout == layout)
        {
            list_add(&new->sibling, &active_window->sibling);
            parent->num_children++;
            new->parent = parent;
            new->idx    = parent->num_children - 1;
        }
        else
        {
            window *new_parent = create_window(arena, screen, layout, 0);
            switch (layout)
            {
                case Vertical:
                {
                    u16 o_screen_x = get_screen_x(screen, old);
                    u16 o_width    = get_width(screen, old);
                    set_window_horizontal_dim(new_parent, o_screen_x, o_width);
                } break;

                case Horizontal:
                {
                    u16 o_screen_y = get_screen_y(screen, old);
                    u16 o_height   = get_height(screen, old);
                    set_window_vertical_dim(new_parent, o_screen_y, o_height);
                } break;

                default:
                {
                } break;
            }

            new_parent->num_children = 2;

            new->parent = new_parent;
            old->parent = new_parent;

            new_parent->parent = parent;
            new_parent->idx    = old->idx;

            old->idx = 0;
            new->idx = 1;

            list_replace(&old->sibling, &new_parent->sibling);
            INIT_LIST_HEAD(&new_parent->first_child);
            list_add(&old->sibling, &new_parent->first_child);
            list_add(&new->sibling, &old->sibling);

            parent = new_parent;
        }
    }

    if (new->flags & WinFlags_Fixed)
    {
        Assert(fixed_dim < parent->dyn_dim - parent->num_children);
        new->full_dim = new->dyn_dim = fixed_dim;
        parent->dyn_dim -= (new->full_dim + 1);
        parent->num_fixed++;
    }

    update_layout(screen, parent);
    set_color(screen, 32);
    draw_borders(screen, *root);
    reset_color(screen);
}

static inline void map_buffer_to_window(piece_list *buffer, window *window)
{
    list_add_tail(&window->next_in_buffer, &buffer->window_sentinel);
    window->buffer = buffer;
}

static inline void reset_window_cursor(screen *screen, window *win)
{
    win->bcy = win->dcy;

    u16 w_screen_y = get_screen_y(screen, win);
    u16 w_screen_x = get_screen_x(screen, win);

    u16 window_cy = w_screen_y + (u16)(win->bcy - win->top_line);
    u16 window_cx = w_screen_x + (u16)(win->bcx - win->cx_offset);

    win->cx = window_cx;
    win->cy = window_cy;
}

static void render_command_window(window *win, screen *screen)
{
    u16 w_height   = get_height(screen, win);
    u16 w_width    = get_width(screen, win);

    u16 w_screen_y = get_screen_y(screen, win);
    u16 w_screen_x = get_screen_x(screen, win);

    if (win->change & Render_BufferChange)
    {
        multilevel_grid m_grid;
        initialize_multilevel_grid(&m_grid, w_height, w_width);

        grid_view new_view = default_grid_view(&m_grid, w_height, w_width);

        fill_command_grid(screen, win, new_view);

        grid_diff(screen, win, win->view, new_view);

        free_grid_view(new_view);
    }
}

static void render_window(window *win, screen *screen, b32 is_active)
{
    u16 w_height   = get_height(screen, win);
    u16 w_width    = get_width(screen, win);

    u16 w_screen_y = get_screen_y(screen, win);
    u16 w_screen_x = get_screen_x(screen, win);

    if (is_active && win->layout == LeafBuffer)
    {
        u32 height = (win->flags & WinFlags_StatusLineVisible) ? (w_height - 1) : w_height;

        if (win->dcy >= win->top_line + height) 
        {
            win->top_line += 1 + win->dcy - (win->top_line + height);
            win->change |= Render_ScrollChange;
        } 
        else if (win->dcy < win->top_line)
        {
            win->top_line = win->dcy;
            win->change |= Render_ScrollChange;
        }

        if (win->bcx >= win->cx_offset + w_width)
        {
            win->cx_offset += 1 + win->bcx - (win->cx_offset + w_width);
            win->change |= Render_ScrollChange;
        }
        else if (win->bcx < win->cx_offset)
        {
            win->cx_offset = win->bcx;
            win->change |= Render_ScrollChange;
        }
    }

    if (win->buffer->changed || win->change != Render_NoChange)
    {
        multilevel_grid m_grid;
        initialize_multilevel_grid(&m_grid, w_height, w_width);

        grid_view new_view = default_grid_view(&m_grid, w_height, w_width);

        fill_grid(screen, win, new_view);

        grid_diff(screen, win, win->view, new_view);

        free_grid_view(new_view);
    }

    win->change = Render_NoChange;
}

static void process_layout(editor_state *editor, u8 *input, u32 input_size)
{
    if (input_size > 1)
    {
        return;
    }

    switch (*input)
    {
        case ' ':
        {
            edit_mode = Normal;
            active_window->change |= Render_ModeChange;
        } break;
        case 'w':
        {
            if (active_window->layout != LeafCommand)
            {
                window *new_window = create_from(&editor->screen, &editor->arena, NULL);
                attach_window(
                    &editor->arena, &editor->screen, &editor->root_window, new_window, NULL, Horizontal, 0);
                map_buffer_to_window(active_window->buffer, new_window);
            }
        } break;

        case 's':
        {
            if (active_window->layout != LeafCommand)
            {
                window *new_window = create_from(&editor->screen, &editor->arena,  NULL);
                attach_window(
                        &editor->arena, &editor->screen, &editor->root_window, new_window, NULL, Vertical, 0);
                map_buffer_to_window(active_window->buffer, new_window);
            }
        } break;

        case 'l':
        {
            active_window->change |= Render_FocusChange;
            move_horizontal(&editor->screen, editor->root_window, 1, true);
            active_window->change |= Render_FocusChange;
            if (active_window->layout == LeafCommand)
            {
            
                edit_mode = Insert;
            }
        } break;

        case 'h':
        {
            active_window->change |= Render_FocusChange;
            move_horizontal(&editor->screen, editor->root_window, 1, false); 
            active_window->change |= Render_FocusChange;
        } break;

        case 'j':
        {
            active_window->change |= Render_FocusChange;
            move_vertical(&editor->screen, editor->root_window, 1, true);
            active_window->change |= Render_FocusChange;
            // if (active_window->layout == LeafCommand)
            // {
            //     edit_mode = Insert;
            // }
        } break;

        case 'k':
        {
            active_window->change |= Render_FocusChange;
            move_vertical(&editor->screen, editor->root_window, 1, false);
            active_window->change |= Render_FocusChange;
        } break;

        case 'L':
        {
            clean_borders(&editor->screen, editor->root_window);
            increase_window_width(&editor->screen, active_window);
            set_color(&editor->screen, 32);
            draw_borders(&editor->screen, editor->root_window);
            reset_color(&editor->screen);
        } break;

        case 'R':
        {
            clean_borders(&editor->screen, editor->root_window);
            increase_window_height(&editor->screen, active_window);
            set_color(&editor->screen, 32);
            draw_borders(&editor->screen, editor->root_window);
            reset_color(&editor->screen);
        } break;

        case 't':
        {
            active_window->flags = active_window->flags & (~WinFlags_StatusLineVisible);
            // active_window->status_line_visible = !active_window->status_line_visible;
            active_window->change |= Render_StatusVisibilityChange;

        } break;

        default:
        {
        } break;

    }
}

