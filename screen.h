#include <sys/ioctl.h>

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

typedef struct screen
{
    memory_arena render_arena;

    render_change change;

    u16 rows;
    u16 cols;

    // u16 left;
    // u16 right;
    // u16 top;
    // u16 bot;

    multilevel_grid grid; 
    u32 cursor;
    u8 buffer[8192];

    window *root_window;
    window *command_window;
    dlist first_free_window;

} screen;

#define MAX_NUM_WINDOWS 32

static void place_cursor(screen *screen, u32 y, u32 x);
static void write_string(screen *screen, u8 *s, u32 len);
static void set_color(screen *screen, u32 color);
static void reset_color(screen *screen);
static void move_cursor_right(screen *screen, u32 x);
static void set_cursor_column(screen *screen, u32 x);
static inline grid_view screen_view(screen *screen);


// All writes must be full. No string representing an ansi sequence can 
// span multiple flush boundaries.

