
typedef struct slice_cursor
{
    piece *pieces;
    u32 cursor;
    u32 count;
} slice_cursor;

static inline u32 space_remaining(const slice_cursor *slice_cursor)
{
    u32 result = slice_cursor->count - slice_cursor->cursor;
    return result;
}

static inline void get_slice_cursor_from_header(
    slice_cursor *slice_cursor,
    const undo_memory_header *header)
{
    slice_cursor->count  = header->del_count;
    slice_cursor->pieces = (piece *) (header + 1);
}


static inline void write_start(slice_cursor *slice_cursor, piece piece)
{
    *(slice_cursor->pieces) = piece;
    slice_cursor->cursor++;
}

static inline void write_last(slice_cursor *slice_cursor, piece piece)
{
    *(slice_cursor->pieces + slice_cursor->count - 1) = piece;
}

static inline void copy_slice(slice_cursor *slice_cursor, const piece *pieces, u32 count)
{
    Assert(space_remaining(slice_cursor) >= count);

    piece *dst_pieces = slice_cursor->pieces + slice_cursor->cursor;
    memcpy(dst_pieces, pieces, sizeof(piece) * count);
    slice_cursor->cursor += count;
}

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

typedef enum
{
    Edit_None  = 0x0,
    Edit_Left  = 0x1,
    Edit_Right = 0x2,
    Edit_Both  = 0x4,
} edit_flags;

typedef struct
{
    u32 count;
    const piece *pieces;
    u32 start;
    u32 end;
    edit_flags flags;
} piece_range;

typedef struct
{
    u32 count;
    union {
        undo_memory_header *undo_header;
        piece *pieces;
    };
    u32 start;
    u32 end;
    edit_flags flags;
} replace_result;

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

    // u32 num_pieces;
    u32 size;  
    u32 lcnt;

    base_iter iter;

    buffer original;
    buffer append;

    memory_arena insert_state_arena;
    insert_mode i_state;

    memory_arena history_arena;
    history history;

    undo_node *staged;

    u32 num_windows;
    dlist window_sentinel;
    dlist list;

    // This concerns search. We calculate matches lazily; meaning
    // the matches that are kept (if we are not replacing) are the ones 
    // visible on screen;
    str last_searched_string;
    u32 match_len;
    u32 num_matches;
    u32 first_match_line;
    u32 last_match_line;
    u32 current_match;
    u32 matches_capacity;
    u32 *matches;

    b32 replaced;
    u32 replace_len;


    // We own this string;


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

static b32 freelist_sanity_check(piece_list *list)
{
    b32 result = true;

    if (list)
    {
        segmented_node *free = list->first_free_node;
        if (free)
        {
            temporary_memory tmp = begin_temporary_memory(&list->list_arena);

            u32 num_nodes = 1;

            segmented_node **first_node = PushStruct(&list->list_arena, segmented_node *, NoClear());
            *first_node = &list->root_sentinel;

            for (segmented_node *node = list->root_sentinel.next; 
                node != &list->root_sentinel;
                node = node->next, num_nodes++)
            {
                segmented_node **new_node = PushStruct(&list->list_arena, segmented_node *, NoClear());
                *new_node = node;
            }

            u32 not_found = true;
            for (segmented_node *node = list->first_free_node; node && not_found; node = node->next)
            {
                for (u32 i = 0; i < num_nodes; ++i)
                {
                    segmented_node *n = first_node[i];
                    if (n == node)
                    {
                        result = false;
                        not_found = false;

                        break;
                    }
                }
            }
            end_temporary_memory(tmp);
        }
    }

    return result;
}

static inline void free_piece_list(piece_list *list)
{
    if (list->matches)
    {
        free(list->matches);
    }
    // NOTE: Order matters!! list is bottstraped onto list_arena.
    free_arena(&list->insert_state_arena);
    free_arena(&list->history_arena);
    free_arena(&list->list_arena);
}


