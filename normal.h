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
    mode_modifier m_mod;
    position_modifier p_mod;
    u32 quantifier;

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
    state state;
    state_result s_result;
} normal_parse_state;


static inline void reset_result(state_result *s_result)
{
    s_result->action       = NoAction;
    s_result->motion       = NoMotion;
    s_result->m_mod        = NoChange;
    s_result->p_mod        = Next;
    s_result->quantifier   = 0;
    s_result->count        = 0;
    s_result->inserted.len = 0;
    s_result->inserted.buffer = NULL;
}

static inline void reset_parse_state(normal_parse_state *p_state)
{
    p_state->state  = Start;
    reset_result(&p_state->s_result);
}

typedef enum 
{
    Ok,
    NotDone,
    Error
} parse_result;
