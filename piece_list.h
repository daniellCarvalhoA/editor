
struct base_iter;

typedef struct piece_list
{
    b32 up_to_date;
    memory_arena list_arena;
    memory_arena insert_mode_arena;

    segmented_node root_sentinel;
    segmented_node *first_free_node;

    char *filepath;

    b32 changed;
    b32 changed_since_last_search;

#if 0
    u32 top_changed;
    u32 bot_changed;
    u32 lines_inserted;
    u32 lines_deleted;
    b32 wrapped;
#endif

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

    // This concerns search. 
    str last_searched_string;
    u32 match_len;
    u32 num_matches;

    u32 first_match_line;
    u32 last_match_line;

    u32 current_match;
    u32 matches_capacity;
    u32 *matches;

    u32 replace_len;
} piece_list;

/* ----------------------------------- Piece List Search -------------------------------------- */


static base_iter find_abs_idx(piece_list *list, base_iter *last_location, u32 abs_idx);
static base_iter find_position(piece_list *list, base_iter *last_location, u32 position);
static base_iter find_line(piece_list *list, base_iter *last_location, u32 line);
static base_iter find_cursor(piece_list *list, base_iter *last_location, buffer_cursor cursor);
static u32 find_char(piece_list *list, base_iter *last_location, str match, u32 count, buffer_cursor cursor);
static u32 find_char_back(
    piece_list *list,
    base_iter *last_location,
    str match,
    u32 count, 
    buffer_cursor cursor);
static buffer_cursor find_word(piece_list *list, base_iter *last_location, u32 count, buffer_cursor cursor);
static buffer_cursor find_word_back(
    piece_list *list,
    base_iter *last_location,
    u32 count,
    buffer_cursor cursor);

static u32 skip_space(piece_list *list, base_iter *last_location, u32 cy);
static iter_range range_search(
    piece_list *list,
    base_iter *last_location,
    buffer_cursor cursor,
    u8 open,
    u8 close);

static void search_str(piece_list *list, str search_string);

static u32 get_line_len(piece_list *list, base_iter *last_location, u32 line);

/* -------------------------------------- Piece List Operations -------------------------------- */

static void replace(piece_list *list, cursor start, cursor end, piece *pieces, u32 num_pieces);
static void copy_serialized(
    piece_list *list,
    cursor start,
    u32 start_offset,
    cursor end, 
    u32 end_offset,
    string text);

static void copy_range(cursor start, cursor end, u32 count, piece_slice slice);
#if TESTS
static void write_piece_to_text(piece_list *list, piece_slice p_slice, string *buf);
#endif
static piece serialize_piece_range_to(piece_list *a, piece_list *b, piece_slice p_slice);
static void yank(piece_list *list, p_buffer *buffer, buffer_cursor c0, buffer_cursor c1);
static void range_replace(piece_list *list, buffer_cursor c0, buffer_cursor c1, piece_slice p_slice);

/* -----------------------------------Piece List Initialization ----------------------------------- */

static piece_list *create_buffer(memory_arena *arena, char *filepath);

/* ----------------------------------- Piece List Flush Operations ---------------------------------- */

#if TESTS
static void write_to_buffer(piece_list *list, u8 *buf, u32 len);
#endif
static void write_buffer_to_file(piece_list *list);


#if TESTS
static inline void list_invariants(piece_list *list);
#endif


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


