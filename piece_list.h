
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

struct base_iter;

typedef struct piece_list
{
    memory_arena list_arena;
    memory_arena insert_mode_arena;

    segmented_node root_sentinel;
    segmented_node *first_free_node;

    char *filepath;

    b32 changed;
    b32 changed_since_last_search;
    u32 top_changed;
    u32 bot_changed;
    u32 lines_inserted;
    u32 lines_deleted;
    b32 wrapped;

    u32 size;  
    u32 lcnt;

    base_iter iter;

    buffer original;
    buffer append;

    memory_arena insert_state_arena;
    insert_mode i_state;

    memory_arena undo_arena;
    Undo_Records undo_records;

    memory_arena redo_arena;
    Redo_Records redo_records;

    u32 num_windows;
    dlist window_sentinel;
    dlist list;

    // This concerns search. We calculate matches lazily; meaning
    // the matches that are kept (if we are not replacing) are the ones 
    // visible on screen;
    str last_searched_string;
    //u32 last_num_matches;
    //u32 last_match_len;
    u32 match_len;
    u32 num_matches;

    u32 first_match_line;
    u32 last_match_line;

    u32 current_match;
    u32 matches_capacity;
    u32 *matches;

    b32 replaced;
    u32 replace_len;

} piece_list;

typedef struct 
{
    segmented_node *node;
    u32 abs_idx;
    u32 piece_index;
    u32 pos;
    u32 line;
    piece piece;
} iter;

static buffer_cursor cursor_from_position(piece_list *list, base_iter *last_location, u32 position);
static u32 position_from_cursor(piece_list *list, base_iter *last_location, buffer_cursor bc);
static base_iter find_abs_idx(piece_list *list, base_iter *last_location, u32 abs_idx);
static void replace(piece_list *list, cursor start, cursor end, piece *pieces, u32 num_pieces);
static base_iter find_cursor(piece_list *list, base_iter *last_location, buffer_cursor bc);
static void copy_range(cursor start, cursor end, u32 count, piece_slice slice);

#if TESTS
static inline void list_invariants(piece_list *list);
#endif

static void clear_insert_state(insert_mode *mode)
{
    mode->position = 0;
    mode->ins_count = 0;
    mode->del_count = 0;
    mode->abs_idx = 0;
    mode->state = Init;
    mode->deleted = false;
    INIT_LIST_HEAD(&mode->piece_head);
}

static inline void initialize_insert_state(insert_mode *mode)
{
    clear_insert_state(mode);
}

static inline void free_piece_list(piece_list *list)
{
    if (list->matches)
    {
        free(list->matches);
    }
    // NOTE: Order matters!! list is bottstraped onto list_arena.
    free_arena(&list->insert_state_arena);
    free_arena(&list->list_arena);
}


