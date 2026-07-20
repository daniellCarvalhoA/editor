typedef enum
{
    Current,
    Next,
    PositionModifierCount,
} position_modifier;

typedef enum
{
    NoAction,
    Insertion,
    Replace,
    Paste,
    Delete,
    Yank,
    ActionCount,
} action;

typedef enum 
{
    NoChange,
    InsertionChange,
    VisualChange,
    LineVisualChange,
    LayoutChange,
    NormalChange,
    ModeModifierCount,
} mode_modifier;

typedef enum 
{
    Start,
    Meta,
    Middle,
} state;

typedef struct
{
    action action;
    motion motion;
    motion_flags s_flags;
    mode_modifier m_mod;
    position_modifier p_mod;
    u32 quantifier;

    i32 open_close_index;

    u32 count;
    union
    {
        u8 char_pending[4];
        u8 match[4];
    };

    str inserted;
} state_result;

typedef struct 
{
    action action_type;
    u32 action_quantifier;
    position_modifier p_mod;
    str inserted;
    mode_modifier m_mod;
} action_spec;

static inline void reset_action_spec(action_spec *spec)
{
    memset(spec, 0, sizeof(action_spec));
}

typedef struct 
{
    action_spec a_spec;
    motion_spec m_spec;
} command;

typedef struct 
{
    state state;
    command command;
    // these are a temporary variable, it is not kept for the 'dot' command
    u32 count;
    u8 char_pending[4];

} normal_parse_state;

static inline void reset_parse_state(normal_parse_state *p_state)
{
    p_state->state  = Start;
    reset_action_spec(&p_state->command.a_spec);
    reset_motion_spec(&p_state->command.m_spec);

    p_state->count = 0;
}

typedef enum 
{
    Ok,
    NotDone,
    Error
} parse_result;
