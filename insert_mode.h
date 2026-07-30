static void insert_mode_insert(window *win, str s);
static void insert_mode_delete(window *win);
static void commit_insert_mode_undo(piece_list *list);
static b32 process_insert(editor_state *state, str s);
