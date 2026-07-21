
typedef enum
{
    Motion_NoMotion,
    Motion_Vertical,
    Motion_Horizontal,
    Motion_Word,
    Motion_Search,
    Absolute,
    Dollar,
    Zero,
    Underscore,
    MotionCount
} motion;

typedef enum
{
    Exclusive = 0x1,
    Backword  = 0x2,
    Range     = 0x4,
} motion_flags;

//typedef enum
//{
//    NoModifier,
//    Forward,
//    Backward,
//    MotionModifierCount
//} motion_modifier;

//typedef enum
//{
    //Inclusive, 
    //Exclusive,
//} bound_type;
//
//typedef enum
//{
    //NotRelative,
    //Match,
    //NotMatch,
//} position_type;
//
//typedef struct 
//{
    //u32 x;
    //u32 y;
    //u32 match_str_len;
    //u8 *match_str;
    //position_type type;
//} gen_buffer_position;
//
//typedef struct
//{
    //gen_buffer_position start;
    //gen_buffer_position end;
    //bound_type start_flags;
    //bound_type end_flags;
//} gen_buffer_range;

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
static void move_by_motion(window *win, motion_spec m_spec, b32 exclusive);
//static win_range get_motion_range(
 //   window *win,
  //  motion motion,
   // u32 quantifier, 
   // str match_str,
    //motion_flags flags, 
    //i32 open_close_index);

//static inline win_range get_cursor_range(
    //window *win,
    //motion motion,
    //mode edit_mode,
    //u32 quantifier, 
    //str match_str,
    //motion_flags flags,
    //i32 open_close_index);
//
//static void move_by_motion(
    //window *win,
    //motion motion,
    //u32 quantifier,
    //str match_str, 
    //b32 exclusive,
    //motion_flags flags,
    //i32 open_close_index);
//
