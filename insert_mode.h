
typedef enum insert_state
{
    Init, 
    Inserted,
    Deleted,
} insert_state;

typedef struct list_piece
{
    piece piece;
    dlist list;
} list_piece;

typedef struct insert_mode
{
    memory_arena insert_mode_arena;
    u32 abs_idx;
    u32 position;
    u32 cx;
    u32 cy;

    u32 ins_count;
    u32 del_count;

    dlist piece_head;

    b32 deleted;

    insert_state state;
} insert_mode;

static inline void initialize_insert_state(insert_mode *mode);
static void insert_mode_insert(window *win, str s);
static void insert_mode_delete(window *win);
static void commit_insert_mode_undo(piece_list *list);
static void into_insert_mode(editor_state *state);
static b32 process_insert(editor_state *state, str s);
