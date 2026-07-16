
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

typedef enum
{
    Inclusive, 
    Exclusive,
} bound_type;

typedef enum
{
    NotRelative,
    Match,
    NotMatch,
} position_type;

typedef struct 
{
    u32 x;
    u32 y;
    u32 match_str_len;
    u8 *match_str;
    position_type type;
} gen_buffer_position;

typedef struct
{
    gen_buffer_position start;
    gen_buffer_position end;
    bound_type start_flags;
    bound_type end_flags;
} gen_buffer_range;

typedef struct
{
    u32 quantifier;
    u32 match_str_len;
    u8 *match_str;
    motion edit_motion;
    mode   edit_mode;
} motion_spec;


static win_range get_motion_range(
    window *win,
    motion motion,
    u32 quantifier, 
    str match_str);

static inline win_range get_cursor_range(
    window *win,
    motion motion,
    mode edit_mode,
    u32 quantifier, 
    str match_str);

static void move_by_motion(
    window *win,
    motion motion,
    u32 quantifier,
    str match_str, 
    b32 exclusive);

