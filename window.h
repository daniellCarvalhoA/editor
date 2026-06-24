
typedef struct
{
    u32 x;
    u32 y;
} win_cursor;

typedef struct
{
    win_cursor first;
    win_cursor one_after_last;
} win_range;

typedef win_cursor screen_cursor;

typedef enum  
{
    LessThan,
    EqualTo,
    GreaterThan
} ord;

static inline ord compare(win_cursor a, win_cursor b)
{
    ord result;
    if (a.y > b.y)
    {
        result = GreaterThan;
    }
    else if (a.y == b.y)
    {
        if (a.x > b.x)
        {
            result = GreaterThan;
        }
        else if (a.x == b.x)
        {
            result = EqualTo;
        }
        else
        {
            result = LessThan;
        }
    }
    else
    {
        result = LessThan;
    }
    return result;
}

typedef enum
{
    LeafCommand,
    LeafBuffer,
    Vertical,
    Horizontal,
} layout;

typedef enum
{
    Render_NoChange               = 0x0,
    Render_BufferChange           = 0x1,
    Render_ScrollChange           = 0x2,
    Render_LayoutChange           = 0x4,
    Render_ModeChange             = 0x8,
    Render_StatusChange           = 0x10,
    Render_FocusChange            = 0x20,
    Render_VisualModeCursorChange = 0x40,
    Render_StatusVisibilityChange = 0x80,
    Render_BufferExchange         = 0x100,
} render_change;

typedef enum
{
    WinFlags_Fixed             = 0x1,
    WinFlags_StatusLineVisible = 0x2,
} win_flags;


typedef struct window
{
    layout layout;
    render_change change;
    win_flags flags;

    u16 offset;
    u16 full_dim;
    u16 dyn_dim;
    // Cursor position relative to the window top-left corner
    u16 cx;
    u16 cy;
    // Cursor position in the buffer 
    u32 bcx;
    u32 bcy;
    // Desired cursor position.
    u32 dcx;
    u32 dcy;
    // Visual mode data
    u32 vcx;
    u32 vcy;
    // The buffer line range this window spans
    u32 top_line;
    u32 cx_offset;

    grid_view view;

    struct window *parent;

    dlist first_child;
    dlist sibling;
    dlist next_in_buffer;
    union
    {
        command_buffer c_buffer;
        piece_list *buffer;
    };

    u16 num_children;
    u16 num_fixed;
} window;

static inline void clear_window(window *win)
{
    win->cx = 0;
    win->cy = 0;
    win->bcx = 0;
    win->bcy = 0;
    win->dcx = 0;
    win->dcy = 0;
    win->vcx = 0;
    win->vcy = 0;
    win->top_line = 0;
    win->cx = 0;
}

static inline u16 get_width(screen *screen, window *win)
{
    u16 result;
    window *parent = win->parent;
    if (parent)
    {
        switch (parent->layout)
        {
            case Vertical:
            {
                result = get_width(screen, parent);
            } break;

            default:
            {
                result = win->full_dim;
            } break;
        }
    }
    else
    {
        result = screen->cols;
    }
    return result;
}

static inline u16 get_screen_x(screen *screen, window *win)
{
    u16 result = 0;
    window *parent = win->parent;
    if (parent)
    {
        switch (parent->layout)
        {
            case Vertical:
            {
                result = get_screen_x(screen, parent);
            } break;

            default:
           {
                result = win->offset;
            } break;
        }
    }
    return result;
}

static inline u16 get_dyn_height(screen *screen, window *win)
{
    u16 result;
    window *parent = win->parent;
    if (parent)
    {
        switch (parent->layout)
        {
            case Horizontal:
            {
                result = get_dyn_height(screen, parent);
            } break;

            default:
            {
                result = win->dyn_dim;
            } break;
        }
    }
    else
    {
        result = win->dyn_dim;
    }
    return result;
}


static inline u16 get_height(screen *screen, window *win)
{
    u16 result;
    window *parent = win->parent;
    if (parent)
    {
        switch (parent->layout)
        {
            case Horizontal:
            {
                result = get_height(screen, parent);
            } break;

            default:
            {
                result = win->full_dim;
            } break;
        }
    }
    else
    {
        result = screen->rows;
    }
    return result;
}

static inline u16 get_screen_y(screen *screen, window *win)
{
    u16 result = 0;
    window *parent = win->parent;
    if (parent)
    {
        switch (parent->layout)
        {
            case Horizontal:
            {
                result = get_screen_y(screen, parent);

            } break;

            default:
            {
                result = win->offset;
            } break;
        }
    }
    return result;
}

static inline win_cursor map_screen_cursor_to_win_cursor(window *win, screen_cursor s_cursor)
{
    win_cursor w_cursor = { .x = win->cx_offset + s_cursor.x, .y = win->top_line + s_cursor.y };
    return w_cursor;
}

static inline b32 is_in_range(win_range range, win_cursor cursor)
{
    b32 result = compare(cursor, range.first) > LessThan && compare(cursor, range.one_after_last) < GreaterThan;
    return result;
}

static inline win_range make_range(win_cursor a, win_cursor b)
{
    win_range range;
    switch (compare(a, b))
    {
        case LessThan:
        case EqualTo:
        {
            range.first = a;
            range.one_after_last = b;
        } break; 

        case GreaterThan:
        {
            range.first = b;
            range.one_after_last = a;
        } break;
    }
    return range;
}



