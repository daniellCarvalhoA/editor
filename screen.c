static inline grid_view screen_view(screen *screen)
{
    grid_view result = default_grid_view(&screen->grid, 0, 0, screen->rows, screen->cols);
    return result;
}

static void flush_buffer(screen *screen)
{
    ssize_t ret = write(1, screen->buffer, screen->cursor);
    if (ret < 0)
    {
        fprintf(stderr, "Error writing to terminal\n");
        abort();
    }
    if (ret < screen->cursor)
    {
        fprintf(stderr, "Wrote less than expected\n");
        abort();
    }
    screen->cursor = 0;
}

static void write_string(screen *screen, str s) 
{
    if (screen->cursor + s.len > ArrayCount(screen->buffer))
    {
        flush_buffer(screen);
    }
    memcpy(screen->buffer + screen->cursor, s.buffer, s.len);
    screen->cursor += s.len;

}

static void reset_color(screen *screen)
{
    // char reset_string[] = "\x1b[0m";
    write_string(screen, STR_LIT("\x1b[0m")); //(u8 *) reset_string, sizeof(reset_string) - 1);
}

static void set_color(screen *screen, u32 color)
{
    Assert(color < 10000);
    size_t max_size = (size_t) (ArrayCount(screen->buffer) - screen->cursor);

    char *buffer = (char *) (screen->buffer + screen->cursor);
    int len = snprintf(buffer, max_size, "\x1b[1;%um", color);
    Assert(len > 0);
    if ((size_t) len >= max_size)
    {
        flush_buffer(screen);
        set_color(screen, color);
        return;
    }

    screen->cursor += (u32) len;
}

static void place_cursor(screen *screen, u32 y, u32 x)
{
    Assert(x < 10000);
    Assert(y < 10000);
    size_t max_size = (size_t) (ArrayCount(screen->buffer) - screen->cursor);

    char *buffer = (char *) (screen->buffer + screen->cursor);
    int len = snprintf(buffer, max_size, "\x1b[%u;%uH", 1 + y, 1 + x);
    Assert(len > 0);
    if ((size_t) len >= max_size)
    {
        flush_buffer(screen);
        place_cursor(screen, y, x);
        return;
    }

    screen->cursor += (u32) len;
}

static void move_cursor_right(screen *screen, u32 x)
{
    Assert(x < 10000);
    size_t max_size = (size_t) (ArrayCount(screen->buffer) - screen->cursor);

    char *buffer = (char *) (screen->buffer + screen->cursor);
    int len = snprintf(buffer, max_size, "\x1b[%uC", x);
    Assert(len > 0);
    if ((size_t) len >= max_size)
    {
        flush_buffer(screen);
        move_cursor_right(screen, x);
        return;
    }
    Assert((u32) len < ArrayCount(screen->buffer));

    screen->cursor += (u32) len;
}

static void set_cursor_column(screen *screen, u32 x)
{
    Assert(x < 10000);
    size_t max_size = (size_t) (ArrayCount(screen->buffer) - screen->cursor);

    char *buffer = (char *) (screen->buffer + screen->cursor);
    int len = snprintf(buffer, max_size, "\x1b[%uG", 1 + x);
    Assert(len > 0);
    if ((size_t) len >= max_size)
    {
        flush_buffer(screen);
        set_cursor_column(screen, x);
        return;
    }
    Assert((u32) len < ArrayCount(screen->buffer));

    screen->cursor += (u32) len;
}

static void append_to_buffer(screen *screen, u8 *s, u32 len)
{
    if (screen->cursor + len > ArrayCount(screen->buffer))
    {
        flush_buffer(screen);
    }
    memcpy(screen->buffer + screen->cursor, s, len);
    screen->cursor += len;
}

#if 0
static void rotate(u32 left, u16 *mid, u32 right)
{
    if ((left == 0) || (right == 0))
    {
        return;
    }
    
    if (Minimum(left, right) <= ((sizeof(u64) * 32) / sizeof(u16)))
    {
        u16 buf[sizeof(u16) * 32];

        u16 *dim = mid - left + right;

        if (left <= right)
        {
            memcpy(buf, mid - left, sizeof(u16) * left);
            memmove(mid - left, mid, sizeof(u16) * right);
            memcpy(dim, buf, sizeof(u16) * left);
        }
        else
        {
            memcpy(buf, mid, sizeof(u16) * right);
            memmove(dim, mid - left, sizeof(u16) * left);
            memcpy(mid - left, buf, sizeof(u16) * right);
        }
    }
    else
    {
        Assert(false);
    }
}
#endif

static inline void update_window_size(screen *screen)
{
    platform_terminal_handle handle = Platform.GetTerminalHandle();
    platform_window_dim dim = Platform.GetTerminalDim(handle);
    screen->rows = dim.height;
    screen->cols = dim.width;

    resize_multilevel_grid(&screen->grid, screen->rows, screen->cols);

    update_layout(screen, screen->root_window);
    screen->change |= Render_RedrawBorders;
}



static inline void reset_window_size(screen *screen)
{
    struct window_size {
        u16 ws_row;
        u16 ws_col;
        u16 xpixel;
        u16 ypixel;
    } win;

    i32 n = ioctl(1, TIOCGWINSZ, &win);
    if (n == - 1)
    {
        perror("ioctl");
        abort();
    }

    screen->rows = win.ws_row;
    screen->cols = win.ws_col;
}

static inline void initialize_screen(screen *screen)
{
    initialize_arena_with_size(&screen->render_arena, 8 * 4096);

#if TESTS
    screen->rows = 40;
    screen->cols = 40;
#else
    platform_terminal_handle handle = Platform.GetTerminalHandle();
    platform_window_dim dim = Platform.GetTerminalDim(handle);
    screen->rows = dim.height;
    screen->cols = dim.width;
#endif

    initialize_multilevel_grid(&screen->grid, screen->rows, screen->cols);
    INIT_LIST_HEAD(&screen->first_free_window);

    screen->root_window    = create_first_window(screen, LeafBuffer);
    screen->active_window = screen->root_window;
    screen->command_window = create_window(screen, LeafCommand, WinFlags_Fixed);

    attach_window(screen, screen->command_window, screen->root_window, Vertical, 1);
    screen->command_window->c_buffer = allocate_command_buffer(screen->cols);
    screen->change |= Render_RedrawBorders;

}

static void free_screen(screen *screen)
{
    if (screen)
    {
        free_multilevel_grid(&screen->grid);
    }
    free_command_buffer(&screen->command_window->c_buffer);
    free_arena(&screen->render_arena);
}







