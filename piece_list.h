typedef struct slice_cursor
{
    piece *pieces;
    buffer_type *types;
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
    slice_cursor->types  = (buffer_type *) (slice_cursor->pieces + header->del_count);
}

static inline void write_start(slice_cursor *slice_cursor, piece piece, buffer_type type)
{
    *(slice_cursor->pieces) = piece;
    *(slice_cursor->types) = type;
    slice_cursor->cursor++;
}

static inline void write_last(slice_cursor *slice_cursor, piece piece, buffer_type type)
{
    *(slice_cursor->pieces + slice_cursor->count - 1) = piece;
    *(slice_cursor->types + slice_cursor->count - 1) = type;
}

static inline void copy_slice(
    slice_cursor *slice_cursor,
    const piece *pieces,
    const buffer_type *types,
    u32 count)
{
    Assert(space_remaining(slice_cursor) >= count);

    piece *dst_pieces = slice_cursor->pieces + slice_cursor->cursor;
    buffer_type *dst_types = slice_cursor->types + slice_cursor->cursor;
    memcpy(dst_pieces, pieces, sizeof(piece) * count);
    memcpy(dst_types, types, sizeof(buffer_type) * count);
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
    buffer_type type;
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
    segmented_node root_sentinel;
    segmented_node *first_free_node;

    char *filepath;

    b32 changed;
    u32 top_changed;
    u32 bot_changed;
    u32 lines_inserted;
    u32 lines_deleted;
    b32 wrapped;

    u32 num_pieces;
    u32 size;  
    u32 lcnt;

    u32 line_len;
    u32 cx;
    u32 cy;

    base_iter iter;

    buffer original;
    buffer append;

    memory_arena insert_state_arena;
    insert_mode i_state;

    memory_arena history_arena;
    history undo_history;

    undo_node *staged;

    dlist window_sentinel;
    dlist list;

} piece_list;

typedef struct 
{
    segmented_node *node;
    u32 abs_idx;
    u32 piece_index;
    u32 pos;
    u32 line;
    piece piece;
    buffer_type type;
} iter;


#define node_init(sentinel, iter) (iter).node = (sentinel)->next
#define node_cond(sentinel, iter) (iter).node != (sentinel)
#define node_init_from(from, iter) (iter).node = (from)

#define node_advance(iter)                  \
    (iter).pos     += (iter).node->size,    \
    (iter).line    += (iter).node->lcnt,    \
    (iter).abs_idx += (iter).node->count,   \
    (iter).node     = (iter).node->next

#define piece_init(iter)                   \
    (iter).piece_index = 0,                \
    (iter).piece = (iter).node->pieces[0], \
    (iter).type  = (iter).node->b_types[0] \

#define piece_init_from(iter, i)           \
    (iter).piece_index = i,                \
    (iter).piece = (iter).node->pieces[i], \
    (iter).type  = (iter).node->b_types[i] \

#define piece_cond(iter) (iter).piece_index < (iter).node->count

#define piece_advance(iter)                                      \
    (iter).pos      += (iter).piece.size,                        \
    (iter).line     += (iter).piece.lcnt,                        \
    (iter).piece = (iter).node->pieces[(iter).piece_index + 1],  \
    (iter).type  = (iter).node->b_types[(iter).piece_index + 1], \
    (iter).piece_index++


#define advance_piece(iter)            \
    (piece_cond((iter))) ? (piece_advance((iter))) : (node_advance((iter)))

#define node_each(sentinel, iter) \
    for (node_init((sentinel), (iter)); node_cond((sentinel), (iter)); node_advance((iter)))

#define node_each_from(sentinel, from, iter) \
    for (node_init_from((from), (iter)); node_cond((sentinel), (iter)); node_advance((iter))

#define node_from(sentinel, iter)  \
    for (; node_cond((sentinel), (iter)); node_advance((iter)))

#define piece_each(iter) \
    for (piece_init((iter)); piece_cond((iter)); piece_advance(iter))

#define piece_each_from(iter, i)  \
    for (piece_init_from((iter), (i)); piece_cound((iter)); piece_advance(iter))

#define pieces(sentinel, iter, body)     \
    do                                   \
    {                                    \
        node_each((sentinel), (iter))    \
        {                                \
            piece_each((iter))           \
            {                            \
                body                     \
            }                            \
        }                                \
    } while (0)


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
    // NOTE: Order matters!! list is bottstraped onto list_arena.
    free_arena(&list->insert_state_arena);
    free_arena(&list->history_arena);
    free_arena(&list->list_arena);
}


