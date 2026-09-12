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
    // Delete,
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
    action action_type;
    u32 action_quantifier;
    position_modifier p_mod;
    str inserted;
    mode_modifier m_mod;

    u32 count;
    u8 char_pending[4];
} action_spec;

static inline void reset_action_spec(action_spec *spec)
{
    memset(spec, 0, sizeof(action_spec));
}

// A command is an action *action_spec* (delete, change, yank, paste, undo) 
// within a given buffer range described by a motion *change_spec*, 
// followed by a motion *motion_spec*
typedef struct 
{
    action_spec a_spec;
    // Describes the range within the buffer a change will be made;
    motion_spec m_spec;
} command;

typedef struct 
{
    state state;
    command command;
    // these are a temporary variable, it is not kept for the 'dot' command

} normal_parse_state;

static inline void reset_parse_state(normal_parse_state *p_state)
{
    p_state->state  = Start;
    reset_action_spec(&p_state->command.a_spec);
    reset_motion_spec(&p_state->command.m_spec);
}

typedef enum 
{
    Ok,
    NotDone,
    Error
} parse_result;
