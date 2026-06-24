static inline grid_view screen_view(screen *screen)
{
    grid_view result = default_grid_view(&screen->grid, screen->rows, screen->cols);
    return result;
}
static void flush_buffer(screen *screen)
{
    ssize_t ret = write(1, screen->buffer, screen->cursor);
    if (ret < 0)
    {
        fprintf(stderr, "Error writint to terminal\n");
        exit(0);
    }
    if (ret < screen->cursor)
    {
        fprintf(stderr, "Wrote less than expected\n");
        exit(0);
    }
    screen->cursor = 0;
}

static void write_string(screen *screen, u8 *s, u32 len) 
{
    if (screen->cursor + len > ArrayCount(screen->buffer))
    {
        flush_buffer(screen);
    }
    memcpy(screen->buffer + screen->cursor, s, len);
    screen->cursor += len;

}

static void reset_color(screen *screen)
{
    char reset_string[] = "\x1b[0m";
    write_string(screen, (u8 *) reset_string, sizeof(reset_string) - 1);
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
        // fprintf(stderr, "set color\n");
        // exit(3);
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
        // fprintf(stderr, "place_cursor\n");
        // exit(4);
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
        // fprintf(stderr, "move_cursor\n");
        // exit(5);
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
        // fprintf(stderr, "set_cursor_column\n");
        // exit(6);
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

static inline void reset_screen_diff_bounds(screen *screen)
{
    screen->left = UINT16_MAX;
    screen->top = UINT16_MAX;
    screen->right = 0; 
    screen->bot = 0;
}

static inline void initialize_screen(screen *screen)
{
    initialize_arena_with_size(&screen->render_arena, 8 * 4096);
    reset_window_size(screen);

    initialize_multilevel_grid(&screen->grid, screen->rows, screen->cols);
    reset_screen_diff_bounds(screen);
    INIT_LIST_HEAD(&screen->first_free_window);

    screen->root_window    = create_first_window(screen, LeafBuffer);
    active_window = screen->root_window;
    screen->command_window = create_window(screen, LeafCommand, WinFlags_Fixed);

    // attach_window(screen, 

    attach_window(screen, screen->command_window, active_window, Vertical, 1);
    screen->command_window->c_buffer = allocate_command_buffer(screen->cols);







}





