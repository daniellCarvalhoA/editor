
typedef enum
{
    Motion_NoMotion,
    Motion_Vertical,
    Motion_Horizontal,
    Motion_Word,
    Motion_Search,
    Motion_Paragraph,
    //Motion_FollowInserted,
    Absolute,
    Dollar,
    Zero,
    Underscore,
    MotionCount
} motion;

typedef enum
{
    Exclusive = 0x1,
    Inclusive = 0x2,
    Backwards = 0x4,
    Range     = 0x8,
    Follow    = 0x100,
} motion_flags;

typedef struct
{
    motion motion_type;
    u32 motion_quantifier;
    u32 match_str_len;
    u8 match_str[4];
    //
    motion_flags flags;
    i32 open_close_index;
} motion_spec;

static inline void reset_motion_spec(motion_spec *spec)
{
    memset(spec, 0, sizeof(motion_spec));
}

static inline win_range get_motion_range(window *win, motion_spec m_spec);
static inline win_range get_cursor_range(window *win, motion_spec m_spec, mode edit_mode);
static void move_by_motion(window *win, motion_spec m_spec); 
