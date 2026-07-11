


static void insert_mode_insert(window *win, u8 *input, u32 input_size);
static void insert_mode_delete(window *win);
static void commit_insert_mode_undo(piece_list *list);
static b32 process_insert(editor_state *state, u8 *input, u32 input_size);
