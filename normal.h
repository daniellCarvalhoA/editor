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
    Underscore
} motion;

typedef enum
{
    NoAction,
    Insertion,
    Paste,
    Delete,
    Yank,
} action;

typedef enum 
{
    NoChange,
    InsertionChange,
    VisualChange,
    LayoutChange,
    NormalChange,
} mode_change;

typedef enum 
{
    Start,
    Meta,
    Middle,
} state;

typedef struct 
{
    state state;
    action action;
    motion motion;
    mode_change change;
    u32 quantifier;
    u8  char_pending;
} normal_parse_state;

normal_parse_state p_state = 
{
    .state  = Start,
    .action = NoAction,
    .motion = NoMotion,
    .change = NoChange,
    .quantifier = 0,
};

static inline void reset_parse_state()
{
    p_state.state  = Start;
    p_state.action = NoAction;
    p_state.motion = NoMotion;
    p_state.change = NoChange;
    p_state.quantifier = 0;
    p_state.char_pending = '\0';
}

typedef enum 
{
    Ok,
    NotDone,
    Error
} parse_result;
