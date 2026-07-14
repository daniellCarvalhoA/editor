
typedef enum
{
    NoMotion,
    Up,
    Down, 
    Left,
    Right,
    Absolute,
    Dollar,
    Zero,
    Underscore,
    Search,
    MotionCount
} motion;

typedef enum
{
    NoModifier,
    Forward,
    Backward,
    MotionModifierCount
} motion_modifier;

static win_range get_motion_range(
    window *win,
    motion motion,
    u32 quantifier, 
    u8 *match_str,
    u32 match_str_len);

static win_range get_cursor_range(
    window *win,
    motion motion,
    mode edit_mode,
    u32 quantifier, 
    u8 *match_str,
    u32 match_str_len);

static void move_by_motion(
    window *win,
    motion motion,
    u32 quantifier,
    u8 *match_str,
    u32 match_str_len);

