#include "e.h"
#include "prng.c"

// TODO:
//  > Pass a scratch arena to each test so that less time is spent allocating 
//  per test memory.

// #define TESTS 1

u32 num_edits         = 32;
u32 num_tests         = 1000;
u32 num_insert_pieces = 32;

#define MAX_STRING_LEN 40
#define MAX_ORIGINAL_STRING_LEN 40
#define MAX_INSERT_MODE_SEQ 10
#define MAX_UNDO_REDO_SEQ 3
#define MAX_SEQ 10
#define MAX_LINE_LEN 40

typedef win_cursor test_cursor; 

static win_cursor rand_cursor(prng *prng, u32 max_x, u32 max_y)
{
    test_cursor result = 
    {
        .y = rand_range_u32_inclusive(prng, 0, max_y),
        .x = rand_range_u32_inclusive(prng, 0, max_x)
    };
    return result;
}


#include "model.h"
#include "buffer.c"
#include "node.c"
#include "search.c"
#include "iter.c"
#include "piece_list.c"
#include "grid.c"
#include "window.c"
#include "screen.c"
#include "command.c"
#include "paste_buffer.c"
#include "motions.c"
#include "normal.c"
#include "normal_test.c"
#include "insert_mode.c"

static editor_state *rand_editor(prng *prng)
{
    editor_state *e_state = BootstrapPushStruct(editor_state, arena, 4096);
    initialize_screen(&e_state->screen);
    reset_parse_state(&e_state->p_state);
    reset_paste_buffer(&e_state->p_buffer);

    piece_list *buffer = rand_list_2(prng);
    map_buffer_to_window(buffer, e_state->screen.active_window);

    INIT_LIST_HEAD(&e_state->buffers);
    list_add(&buffer->list, &e_state->buffers);

    return e_state;
}

#if 0
static undo_memory_header *rand_replace(piece_list *list, prng *prng)
{
    undo_memory_header *header = 0;
    if (list->size + num_insert_pieces == 0) 
    {
        return header;
    }
    base_iter start, end;
    u32 num_ins_pieces;

    ratio ratio = init_ratio(2, 4);

    for(;;)
    {
        u32 begin, finish;

        if (chance(prng, ratio))
        {
            begin  = rand_range_u32_inclusive(prng, 0, list->size);
            finish = rand_range_u32_inclusive(prng, begin, list->size);
            start  = find_position(&list->iter, begin);
            end    = find_position(&list->iter, finish);
        }
        else
        {
            begin  = rand_range_u32_inclusive(prng, 0, list->lcnt);
            finish = rand_range_u32_inclusive(prng, begin, list->lcnt);
            start  = find_line(&list->iter, begin);
            end    = find_line(&list->iter, finish);
        }

        num_ins_pieces = rand_range_u32_inclusive(prng, 0, num_insert_pieces);

        if ((num_ins_pieces > 0) || (begin != finish) )
        {
            break;
        }

        reset_cursor_(&list->iter);
    }

    fix_iter(&start);
    fix_iter(&end);

    void *pieces_and_types = malloc(num_ins_pieces * sizeof(piece));
    piece *pieces      = (piece *) pieces_and_types;


    for (u32 i = 0; i < num_ins_pieces; ++i)
    {
        INIT_STACK_STRING(s, MAX_STRING_LEN)
        rand_ascii_string(&s, prng, 1, MAX_STRING_LEN);
        if (s.len > 0)
        {
            pieces[i] = make_piece_s(list, from_string(s));
        }
    }

    header = replace_range(list, start, end, pieces, num_ins_pieces);
    free(pieces);
    return header;
}
#endif

typedef struct 
{
    win_cursor min; 
    win_cursor max;
    string text_added;
} insert_seq_result;
//
static insert_seq_result *create(u32 max_length)
{
    insert_seq_result *result = (insert_seq_result *) malloc(sizeof(insert_seq_result)); 
    result->min.x = result->max.x = result->min.y = result->max.y = 0;
    result->text_added.len = 0;
    result->text_added.capacity = max_length;
    result->text_added.buffer = malloc(max_length);
    return result;
}
//
static void free_insert_seq_result(insert_seq_result *seq)
{
    free(seq->text_added.buffer);
    free(seq);
}
//
static void clear_seq(insert_seq_result *seq)
{
    seq->min.x = seq->max.x = seq->min.y = seq->max.y = 0;
    seq->text_added.len = 0;
}

static void insert_mode_sequence(window *win, prng *prng, insert_seq_result *seq)
{
    seq->min.x = win->buffer->size;
    seq->min.y = win->buffer->lcnt;
    seq->max = win->bc;
    u32 seq_size = rand_range_u32_inclusive(prng, 1, MAX_INSERT_MODE_SEQ);

    // state_result s_result = {};
    for (u32 i = 0; i < seq_size; ++i)
    {
        if (rand_b32(prng))
        {
            INIT_STACK_STRING(text, MAX_STRING_LEN);
            rand_ascii_string(&text, prng, 1, MAX_STRING_LEN);
            push_string(&seq->text_added, &text);
            for (u32 j = 0; j < text.len; ++j)
            {
                // char c = (char) text.buffer[j];
                str s = { .buffer = (u8 *) text.buffer + j, .len = 1 };
                insert_mode_insert(win, s); // , &s_result);
            }
        }
        else
        {
            u32 begin_x = rand_range_u32_inclusive(prng, 0, win->bc.x);
            u32 begin_y = win->bc.y;

            win_cursor begin = { .x = begin_x, .y = begin_y };
            u32 delete_amount = win->bc.x - begin.x;
            if (seq->text_added.len > delete_amount)
            {
                seq->text_added.len -= delete_amount;
            }
            else
            {
                seq->text_added.len = 0;
            }

            win_cursor curr_min = minimum(seq->min, begin);
            seq->min = minimum(seq->min, curr_min);
            for (; win->bc.x > begin.x;)
            {
                insert_mode_delete(win);
            }
        }
    }
    seq->min = minimum(seq->min, seq->max);
}

#if 0
static piece_list *rand_list(prng *prng)
{
    piece_list *list = BootstrapPushStruct(piece_list, list_arena, 8 * 4096);

    string original_text = rand_ascii_string_alloc(prng, &list->list_arena, 0, MAX_ORIGINAL_STRING_LEN);
    initialize_piece_list_s(list, original_text);

    for (u32 i = 0; i < num_edits; ++i)
    {
        rand_replace(list, prng);
    }


    return list;
}
#endif
static void free_editor(editor_state *state)
{
    // NOTE: make the editor struct own each buffers memory. 
    free_screen(&state->screen);
    piece_list *buffer;
    piece_list *tmp;
    list_for_each_entry_safe(buffer, tmp, &state->buffers, list)
    {
        free_piece_list(buffer);
    }
    free_arena(&state->arena);
}


#if 0
//EST(replace_sound)
void replace_sound(prng *prng)
{
    piece_list *list = rand_list(prng);
    list_invariants(list);
    free_piece_list(list);
}
#endif

TEST(replace_sound_2)
void replace_sound_2(prng *prng)
{
    piece_list *list = rand_list_2(prng);
    list_invariants(list);
    free_piece_list(list);
}

//EST(paste)
void paste(prng *prng)
{
    // editor_state *editor = rand_editor(prng);
    //
    // piece_list *list = editor->screen.active_window->buffer;
    // u32 before_size = list->size; 
    // u32 before_lcnt = list->lcnt;
    // u8 *before_buffer = malloc(sizeof(u8) * list->size);
    // write_to_buffer(list, before_buffer, list->size);
    //
    // editor->p_state.s_result.action     = NoAction;
    // editor->p_state.s_result.motion     = rand_motion(prng);
    // editor->p_state.s_result.m_mod      = NoChange;
    // editor->p_state.s_result.p_mod      = Current;
    // editor->p_state.s_result.quantifier = rand_range_u32_inclusive(prng, 0, editor->screen.active_window->buffer->lcnt);
    //
    // edit(editor);
    // commit_cursor(editor->screen.active_window, editor->edit_mode);
    // reset_parse_state(&editor->p_state);
    //
    // editor->p_state.s_result.action     = Delete;
    // editor->p_state.s_result.motion     = rand_motion(prng);
    // editor->p_state.s_result.m_mod      = NoChange;
    // editor->p_state.s_result.p_mod      = rand_position_modifier(prng);
    // editor->p_state.s_result.quantifier = rand_range_u32_inclusive(prng, 0, editor->screen.active_window->buffer->lcnt);
    //
    // motion delete_motion = editor->p_state.s_result.motion;
    //
    // u32 prev_cx = editor->screen.active_window->bc.x;
    // edit(editor);
    // commit_cursor(editor->screen.active_window, editor->edit_mode);
    // reset_parse_state(&editor->p_state);
    //
    // u32 mid_size = list->size; 
    // u32 mid_lcnt = list->lcnt;
    // u8 *mid_buffer = malloc(sizeof(u8) * list->size);
    // write_to_buffer(list, mid_buffer, list->size);
    //
    // u32 curr_cx = editor->screen.active_window->bc.x;
    //
    // editor->p_state.s_result.action     = Paste;
    // editor->p_state.s_result.motion     = NoMotion;
    // editor->p_state.s_result.m_mod      = NoChange;
    // editor->p_state.s_result.p_mod      = (curr_cx < prev_cx) ? Next : Current;
    // editor->p_state.s_result.quantifier = 1;
    //
    // edit(editor);
    // commit_cursor(editor->screen.active_window, editor->edit_mode);
    // reset_parse_state(&editor->p_state);
    //
    // u32 after_size = list->size;
    // u32 after_lcnt = list->lcnt;
    // u8 *after_buffer = malloc(sizeof(u8) * list->size);
    //
    // write_to_buffer(list, after_buffer, list->size);
    //
    // Assert(before_size == after_size);
    // Assert(before_lcnt == after_lcnt);
    // Assert(strncmp((const char *) before_buffer, (const char *) after_buffer, list->size) == 0);
    //
    // free(before_buffer);
    // free(after_buffer);
    // free(mid_buffer);
    // free_editor(editor);
}
#if 0
//EST(replace_against_model)
void replace_against_model(prng *p)
{
    prng clone = clone_prng(p);

    model *m = rand_model(p);
    piece_list *list = rand_list(&clone);

    Assert(list->size == m->s.len);

    u8 *buf = malloc(sizeof(u8) * list->size);

    write_to_buffer(list, buf, list->size);

    Assert(strncmp((const char *) buf, (const char *) m->s.buffer, list->size) == 0);

    free(buf);
    free_piece_list(list);
    free_arena(&m->arena);
}
#endif

TEST(replace_against_model_2)
void replace_against_model_2(prng *p)
{
    prng clone = clone_prng(p);

    model *m = rand_model_2(p);
    piece_list *list = rand_list_2(&clone);

    Assert(list->size == m->s.len);

    u8 *buf = malloc(sizeof(u8) * list->size);

    write_to_buffer(list, buf, list->size);

    Assert(strncmp((const char *) buf, (const char *) m->s.buffer, list->size) == 0);

    free(buf);
    free_piece_list(list);
    free_arena(&m->arena);
}


// TST(replace_against_model)
// void replace_against_model(prng *p)
// {
//     prng clone = clone_prng(p);
//
//     model *m = rand_model(p);
//     piece_list *list = rand_list(&clone);
//
//     Assert(list->size == m->s.len);
//
//     u8 *buf = malloc(sizeof(u8) * list->size);
//
//     write_to_buffer(list, buf, list->size);
//
//     Assert(strncmp((const char *) buf, (const char *) m->s.buffer, list->size) == 0);
//
//     free(buf);
//     free_piece_list(list);
//     free_arena(&m->arena);
// }

#if 0
//EST(undo)
void undo(prng *prng)
{
    screen screen = {};
    initialize_screen(&screen);
    window *win = create_window(&screen, LeafBuffer, 0);
    piece_list *list = rand_list(prng);

    map_buffer_to_window(list, win);

    // u32 num_pieces_before = list->num_pieces;
    u32 size_before = list->size;
    u32 lcnt_before = list->lcnt;

    u8 before[list->size];
    write_to_buffer(list, before, list->size);

    u32 num_reversible_edits = rand_range_u32_inclusive(prng, 1, MAX_UNDO_REDO_SEQ);

    for (u32 i = 0; i < num_reversible_edits; ++i)
    {
        undo_node *node = allocate_tree_node(&list->history_arena);
        undo_memory_header *header = rand_replace(list, prng);
        if (header)
        {
            node->data = header;
            insert_node(&list->history, node);
        }
        else
        {
            num_reversible_edits = i;
            break;
        }
    }

    for (u32 i = 0; i < num_reversible_edits; undo_(win), ++i);

    u8 after[list->size];

    write_to_buffer(list, after, list->size);

    Assert(size_before == list->size);
    // Assert(num_pieces_before == list->num_pieces);
    Assert(lcnt_before == list->lcnt);
    Assert(strncmp((const char *) before, (const char *) after, list->size) == 0);

    list_invariants(list);
    free_piece_list(list);
    free_screen(&screen);
}
#endif

TEST(undo_2)
void undo_2(prng *prng)
{
    screen screen = {};
    initialize_screen(&screen);
    window *win = create_window(&screen, LeafBuffer, 0);
    piece_list *list = rand_list_2(prng);

    map_buffer_to_window(list, win);

    // u32 num_pieces_before = list->num_pieces;
    u32 size_before = list->size;
    u32 lcnt_before = list->lcnt;

    u8 before[list->size];
    write_to_buffer(list, before, list->size);

    u32 num_reversible_edits = rand_range_u32_inclusive(prng, 1, MAX_UNDO_REDO_SEQ);

    for (u32 i = 0; i < num_reversible_edits; ++i)
    {
        undo_node *node = allocate_tree_node(&list->history_arena);
        replace_result rep = rand_replace_(list, prng);
        if (rep.undo_header)
        {
            node->data = rep.undo_header;
            insert_node(&list->history, node);
        }
        else
        {
            num_reversible_edits = i;
            break;
        }
    }

    for (u32 i = 0; i < num_reversible_edits; undo_(win), ++i);

    u8 after[list->size];

    write_to_buffer(list, after, list->size);

    Assert(size_before == list->size);
    // Assert(num_pieces_before == list->num_pieces);
    Assert(lcnt_before == list->lcnt);
    Assert(strncmp((const char *) before, (const char *) after, list->size) == 0);

    list_invariants(list);
    free_piece_list(list);
    free_screen(&screen);
}

#if 0
//EST(undo_redo)
void undo_redo(prng *prng)
{
    screen screen = {};
    initialize_screen(&screen);
    window *win = create_window(&screen, LeafBuffer, 0);
    piece_list *list = rand_list(prng);

    map_buffer_to_window(list, win);

    u32 num_reversible_edits = rand_range_u32_inclusive(prng, 1, MAX_UNDO_REDO_SEQ);

    for (u32 i = 0; i < num_reversible_edits; ++i)
    {
        undo_node *node = allocate_tree_node(&list->history_arena);
        replace_result rep = rand_replace_(list, prng);
        if (rep.undo_header)
        {
            node->data = rep.undo_header;
            insert_node(&list->history, node);
        }
        else
        {
            num_reversible_edits = i;
            break;
        }
    }

    // u32 num_pieces_before = list->num_pieces;
    u32 size_before = list->size;
    u32 lcnt_before = list->lcnt;

    u8 before[list->size];
    write_to_buffer(list, before, list->size);

    for (u32 i = 0; i < num_reversible_edits; undo_(win), ++i);
    for (u32 i = 0; i < num_reversible_edits; redo(win), ++i);

    u8 after[list->size];

    write_to_buffer(list, after, list->size);

    // Assert(num_pieces_before == list->num_pieces);
    Assert(size_before == list->size);
    Assert(lcnt_before == list->lcnt);
    Assert(strncmp((const char *) before, (const char *) after, list->size) == 0);

    list_invariants(list);
    free_piece_list(list);
    free_screen(&screen);
}
#endif

TEST(undo_redo_2)
void undo_redo_2(prng *prng)
{
    screen screen = {};
    initialize_screen(&screen);
    window *win = create_window(&screen, LeafBuffer, 0);
    piece_list *list = rand_list_2(prng);

    map_buffer_to_window(list, win);

    u32 num_reversible_edits = rand_range_u32_inclusive(prng, 1, MAX_UNDO_REDO_SEQ);

    for (u32 i = 0; i < num_reversible_edits; ++i)
    {
        undo_node *node = allocate_tree_node(&list->history_arena);
        replace_result rep = rand_replace_(list, prng);
        if (rep.undo_header)
        {
            node->data = rep.undo_header;
            insert_node(&list->history, node);
        }
        else
        {
            num_reversible_edits = i;
            break;
        }
    }

    // u32 num_pieces_before = list->num_pieces;
    u32 size_before = list->size;
    u32 lcnt_before = list->lcnt;

    u8 before[list->size];
    write_to_buffer(list, before, list->size);

    for (u32 i = 0; i < num_reversible_edits; undo_(win), ++i);
    for (u32 i = 0; i < num_reversible_edits; redo(win), ++i);

    u8 after[list->size];

    write_to_buffer(list, after, list->size);

    // Assert(num_pieces_before == list->num_pieces);
    Assert(size_before == list->size);
    Assert(lcnt_before == list->lcnt);
    Assert(strncmp((const char *) before, (const char *) after, list->size) == 0);

    list_invariants(list);
    free_piece_list(list);
    free_screen(&screen);
}
//
#if 0
//ST(insert_mode_seq)
void insert_mode_seq(prng *p)
{
    // This simulates, a insert mode sequence of zero or more insertions,
    // followed by 0 or more deletions, followed by an insert mode sequence.
    // This repeats at most 10 times.
    // 10 Should be a good enough number of times to ascertain code correctness.

    u32 max_length_added = MAX_INSERT_MODE_SEQ * MAX_STRING_LEN;
    insert_seq_result *seq = create(max_length_added);

    prng clone = clone_prng(p);

    screen screen = {};
    initialize_screen(&screen);


    piece_list *list_a = rand_list(p);
    window *win_a = create_window(&screen, LeafBuffer, 0);
    map_buffer_to_window(list_a, win_a);

    str s = {};
    u32 cy = rand_range_u32_inclusive(p, 0, list_a->lcnt);
    u32 cx = rand_range_u32_inclusive(p, 0, MAX_LINE_LEN);
    {
        Assert(win_a->bc.y == 0);

        move_by_motion(win_a, Down, cy, s, false, 0);
        move_by_motion(win_a, Right, cx, s, false, 0);

        insert_mode_sequence(win_a, p, seq);
        commit_insert_mode_undo(list_a);
    }

    piece_list *list_b = rand_list(&clone);
    window *win_b = create_window(&screen, LeafBuffer, 0);
    map_buffer_to_window(list_b, win_b);
    {
        move_by_motion(win_b, Down, cy, s, false, 0);
        move_by_motion(win_b, Right, cx, s, false, 0);

        if (seq->min.x != seq->max.x || seq->min.y != seq->max.y || seq->text_added.len > 0)
        {

            undo_node *node = allocate_tree_node(&list_b->history_arena);
            node->bc = win_b->bc;

            if (seq->text_added.len > 0)
            {
                piece piece = make_piece_s(list_b, from_string(seq->text_added));
                node->data = range_replace(list_b, seq->min, seq->max, &piece, 1);
            }
            else
            {
                node->data = range_replace(list_b, seq->min, seq->max, 0, 0);
            }

            insert_node(&list_b->history, node);
        }
    }

    b32 equal_lists = lists_are_equal(list_a, list_b);
    Assert(equal_lists);
    free_insert_seq_result(seq);
    free_screen(&screen);
    free_piece_list(list_a);
    free_piece_list(list_b);
}
#endif

TEST(insert_mode_seq_2)
void insert_mode_seq_2(prng *p)
{
    // This simulates, a insert mode sequence of zero or more insertions,
    // followed by 0 or more deletions, followed by an insert mode sequence.
    // This repeats at most 10 times.
    // 10 Should be a good enough number of times to ascertain code correctness.

    u32 max_length_added = MAX_INSERT_MODE_SEQ * MAX_STRING_LEN;
    insert_seq_result *seq = create(max_length_added);

    prng clone = clone_prng(p);

    screen screen = {};
    initialize_screen(&screen);


    piece_list *list_a = rand_list_2(p);
    window *win_a = create_window(&screen, LeafBuffer, 0);
    map_buffer_to_window(list_a, win_a);

    u32 cy = rand_range_u32_inclusive(p, 0, list_a->lcnt);
    u32 cx = rand_range_u32_inclusive(p, 0, MAX_LINE_LEN);
    {
        Assert(win_a->bc.y == 0);

        motion_spec m_spec = {
            .motion_type = Motion_Vertical,
            .motion_quantifier = cy,
            .flags = Backword,
            .open_close_index = -1
        };
        move_by_motion(win_a, m_spec, false);

        m_spec.motion_type = Motion_Horizontal;
        m_spec.motion_quantifier = cx;
        m_spec.flags = 0;

        move_by_motion(win_a, m_spec, false);

        insert_mode_sequence(win_a, p, seq);
        commit_insert_mode_undo(list_a);
    }

    piece_list *list_b = rand_list_2(&clone);
    window *win_b = create_window(&screen, LeafBuffer, 0);
    map_buffer_to_window(list_b, win_b);
    {
        motion_spec m_spec = {
            .motion_type = Motion_Vertical,
            .motion_quantifier = cy,
            .flags = Backword,
            .open_close_index = -1
        };
        move_by_motion(win_b, m_spec, false);

        m_spec.motion_type = Motion_Horizontal;
        m_spec.motion_quantifier = cx;
        m_spec.flags = 0;

        move_by_motion(win_b, m_spec, false);

        if (seq->min.x != seq->max.x || seq->min.y != seq->max.y || seq->text_added.len > 0)
        {
            undo_node *node = allocate_tree_node(&list_b->history_arena);
            node->bc = win_b->bc;

            if (seq->text_added.len > 0)
            {
                piece piece = make_piece_s(list_b, from_string(seq->text_added));
                piece_range p_range = { .pieces = &piece, .count = 1 };
                replace_result rep = range_replace__(list_b, seq->min, seq->max, p_range);
                node->data = rep.undo_header;
            }
            else
            {
                piece_range p_range = {};
                replace_result rep = range_replace__(list_b, seq->min, seq->max, p_range);
                node->data = rep.undo_header;
            }
            insert_node(&list_b->history, node);
        }
    }

    b32 equal_lists = lists_are_equal(list_a, list_b);
    Assert(equal_lists);
    free_insert_seq_result(seq);
    free_screen(&screen);
    free_piece_list(list_a);
    free_piece_list(list_b);
}

// TEST(insert_mode_insert_simple)
// void insert_mode_insert_simple(prng *p)
// {
//     prng clone = clone_prng(p);
//     piece_list *list_a = rand_list(p);
//     {
//         INIT_STACK_STRING(text, MAX_STRING_LEN);
//         rand_ascii_string(&text, p, 1, MAX_STRING_LEN);
//         piece piece = make_piece_s(list_a, text);
//         u32 pos = rand_range_u32_inclusive(p, 0, list_a->size);
//         buffer_type type = BufferType_Append;
//         base_iter start = find_position(&list_a->iter, pos);
//         replace_range_(list_a, start, start, &piece, &type, 1);
//     }
//
//     piece_list *list_b = rand_list(&clone);
//     {
//         INIT_STACK_STRING(text, MAX_STRING_LEN);
//         rand_ascii_string(&text, &clone, 1, MAX_STRING_LEN);
//         u32 pos = rand_range_u32_inclusive(&clone, 0, list_b->size);
//         move_to(list_b, pos);
//         for (u32 i = 0; i < text.len; ++i) 
//         {
//             char c = (char ) text.buffer[i];
//             insert_mode_insert(list_b, c);
//         }
//         commit_insert_mode_undo(list_b);
//     }
//     b32 equal_lists = lists_are_equal(list_a, list_b);
//     Assert(equal_lists);
//     free_piece_list(list_a);
//     free_piece_list(list_b);
// }
//
// TEST(insert_mode_replace)
// void insert_mode_replace(prng *p)
// {
//     prng clone = clone_prng(p);
//     piece_list *list_a = rand_list(p);
//     {
//         INIT_STACK_STRING(text, MAX_STRING_LEN);
//         rand_ascii_string(&text, p, 1, MAX_STRING_LEN);
//
//         piece piece      = make_piece_s(list_a, text);
//         buffer_type type = BufferType_Append;
//
//         u32 begin  = rand_range_u32_inclusive(p, 0, list_a->size);
//         u32 finish = rand_range_u32_inclusive(p, begin, list_a->size);
//
//         base_iter start = find_position(&list_a->iter, begin);
//         base_iter end   = find_position(&list_a->iter, finish);
//         replace_range_(list_a, start, end, &piece, &type, 1);
//     }
//     piece_list *list_b = rand_list(&clone);
//     {
//         INIT_STACK_STRING(text, MAX_STRING_LEN);
//         rand_ascii_string(&text, &clone, 1, MAX_STRING_LEN);
//
//         u32 begin   = rand_range_u32_inclusive(&clone, 0, list_b->size);
//         u32 finish  = rand_range_u32_inclusive(&clone, begin, list_b->size);
//
//         move_to(list_b, finish);
//         for (u32 pos = finish ;pos > begin; --pos)
//         {
//             insert_mode_delete(list_b);
//         }
//
//         for (u32 i = 0; i < text.len; ++i)
//         {
//             char c = (char) text.buffer[i];
//             insert_mode_insert(list_b, c);
//         }
//         commit_insert_mode_undo(list_b);
//     }
//     b32 equal_lists = lists_are_equal(list_a, list_b);
//     Assert(equal_lists);
//     free_piece_list(list_a);
//     free_piece_list(list_b);
// }
//
// TEST(insert_mode_delete_simple)
// void insert_mode_delete_simple(prng *p)
// {
//     prng clone = clone_prng(p);
//     piece_list *list_a;
//     {
//         do 
//         {
//             list_a = rand_list(p);
//         } while (list_a->size == 0);
//
//         u32 begin, finish;
//         do
//         {
//             begin  = rand_range_u32_inclusive(p, 0, list_a->size);
//             finish = rand_range_u32_inclusive(p, begin, list_a->size);
//         } while (finish == begin);
//
//         base_iter start = find_position(&list_a->iter, begin);
//         base_iter end   = find_position(&list_a->iter, finish);
//         replace_range_(list_a, start, end, 0, 0, 0);
//     }
//     piece_list *list_b;
//     {
//         do 
//         {
//             list_b = rand_list(&clone);
//         } while (list_b->size == 0);
//         u32 begin, finish;
//         do 
//         {
//             begin   = rand_range_u32_inclusive(&clone, 0, list_b->size);
//             finish  = rand_range_u32_inclusive(&clone, begin, list_b->size);
//         } while (finish == begin);
//         move_to(list_b, finish);
//         for (u32 pos = finish; pos > begin; --pos)
//         {
//             insert_mode_delete(list_b);
//         }
//         commit_insert_mode_undo(list_b);
//     }
//     b32 equal_lists = lists_are_equal(list_a, list_b);
//     Assert(equal_lists);
//     free_piece_list(list_a);
//     free_piece_list(list_b);
// }
//
// TEST(insert_mode_insert_delete_iso)
// void insert_mode_insert_delete_iso(prng *p)
// {
//     prng clone = clone_prng(p);
//
//     piece_list *list_a = rand_list(p);
//     piece_list *list_b = rand_list(&clone);
//     {
//         INIT_STACK_STRING(text, MAX_STRING_LEN);
//         rand_ascii_string(&text, &clone, 1, MAX_STRING_LEN);
//         u32 pos = rand_range_u32_inclusive(&clone, 0, list_b->size);
//
//         move_to(list_b, pos);
//         for (u32 i = 0; i < text.len; ++i) 
//         {
//             char c = (char ) text.buffer[i];
//             insert_mode_insert(list_b, c);
//         }
//
//         for (u32 i = 0; i < text.len; ++i)
//         {
//             insert_mode_delete(list_b);
//         }
//         commit_insert_mode_undo(list_b);
//     }
//
//     b32 equal_lists = lists_are_equal(list_a, list_b);
//     Assert(equal_lists);
//     free_piece_list(list_a);
//     free_piece_list(list_b);
// }
//
// TEST(write_range)
// void write_range(prng *p)
// {
//     prng clone = clone_prng(p);
//
//     model *m = rand_model(p);
//     piece_list *list = rand_list(&clone);
//
//     u32 start = rand_range_u32_inclusive(p, 0, list->size);
//     u32 end   = rand_range_u32_inclusive(p, start, list->size);
//     u32 len = end - start;
//
//     u8 *buf = malloc(sizeof(u8) * len);
//
//     write_range_to_buffer(list, start, buf, len);
//     Assert(strncmp((const char *) buf, (const char *) (m->s.buffer + start), len) == 0);
//
//     free(buf);
//     free_piece_list(list);
//     free_arena(&m->arena);
// }
//
// TEST(write_range_rev)
// void write_range_rev(prng *prng)
// {
//     piece_list *list = rand_list(prng);
//
//     u32 start = rand_range_u32_inclusive(prng, 0, list->size);
//     u32 end   = rand_range_u32_inclusive(prng, start, list->size);
//     u32 len = end - start;
//
//     void *buf = malloc(2 * sizeof(u8) * len);
//     u8 *buf_fwd = (u8 *) buf;
//
//     write_range_to_buffer(list, start, buf_fwd, len);
//
//     u8 *buf_rev = buf + len; 
//
//     write_range_to_buffer_rev(list, start, buf_rev, len);
//     Assert(strncmp((const char *) buf_fwd, (const char *) buf_rev, len) == 0);
//
//     free(buf);
//     free_piece_list(list);
// }
//
// TEST(write_rev)
// void write_rev(prng *p)
// {
//     piece_list *list = rand_list(p);
//     void *buf = malloc(2 * sizeof(u8) * list->size);
//     u8 *buf_fwd = (u8 *) buf;
//     write_to_buffer(list, buf_fwd, list->size);
//     u8 *buf_rev = buf + list->size;
//     write_to_buffer(list, buf_rev, list->size);
//
//     Assert(strncmp((const char *) buf_fwd, (const char *) buf_rev, list->size) == 0);
//
//     free(buf);
//     free_piece_list(list);
// }
//
// TEST(write_range_line)
// void write_range_line(prng *p)
// {
//     prng clone = clone_prng(p);
//
//     model *m = rand_model(p);
//     piece_list *list = rand_list(&clone);
//
//     u32 start = rand_range_u32_inclusive(p, 0, list->lcnt);
//     u32 end   = rand_range_u32_inclusive(p, start, list->lcnt);
//
//     u8 *buf = malloc(sizeof(u8) * list->size);
//
//     u32 len = write_line_range_to_buffer(list, start, buf, end - start);
//     u32 begin = line_position(m, start);
//     Assert(strncmp((const char *) buf, (const char *) (m->s.buffer + begin), len) == 0);
//
//     free(buf);
//     free_piece_list(list);
//     free_arena(&m->arena);
// }
// //
// TEST(write_range_line_rev)
// void write_range_line_rev(prng *prng)
// {
//     piece_list *list = rand_list(prng);
//
//     u32 start = rand_range_u32_inclusive(prng, 0, list->lcnt);
//     u32 end   = rand_range_u32_inclusive(prng, start, list->lcnt);
//
//     u8 *buf = malloc(sizeof(u8) *list->size);
//     u32 len = write_line_range_to_buffer(list, start, buf, end - start);
//
//     u8 *buf_rev = malloc(sizeof(u8) * len);
//     write_line_range_to_buffer_rev(list, start, buf_rev, end - start, len);
//
//     Assert(strncmp((const char *) buf, (const char *) buf_rev, len) == 0);
//
//     free(buf);
//     free(buf_rev);
//     free_piece_list(list);
// }
//
//
// TEST(chars)
// void chars(prng *p)
// {
//     piece_list *list = rand_list(p);
//
//     u8 *buffer = malloc(sizeof(u8) * 2 * list->size);
//     u8 *buf_1 = buffer;
//     u8 *buf_2 = buffer + list->size;
//
//     write_to_buffer(list, buf_1, list->size);
//
//     u32 i = 0;
//     base_iter iter;
//     for (b32 not_over = base_init_(list, Position, &iter); 
//         not_over;
//         not_over = base_next_pos(&iter))
//     {
//         buf_2[i++] = get_char(&iter);
//     }
//
//     Assert(strncmp((const char *) buf_1, (const char *) buf_2, list->size) == 0);
//
//     free(buffer);
//     free_piece_list(list);
// }
//
// TEST(chars_prev)
// void chars_prev(prng *p)
// {
//     piece_list *list = rand_list(p);
//
//     base_iter iter;
//
//     u8 *buffer = malloc(sizeof(u8) * 2 * list->size);
//     u8 *buf_1 = buffer;
//     u8 *buf_2 = buffer + list->size;
//
//     write_to_buffer(list, buf_1, list->size);
//
//     u32 i = 0;
//     for (b32 not_over = base_init_rev_(list, Position, &iter);
//         not_over;
//         not_over = base_prev_pos(&iter))
//     {
//         buf_2[list->size - i++ - 1] = get_char(&iter);
//     }
//     Assert(strncmp((const char *) buf_1, (const char *) buf_2, list->size) == 0);
//
//     free(buffer);
// }
//
// TEST(lines)
// void lines(prng *p)
// {
//     prng clone = clone_prng(p);
//
//     piece_list *list = rand_list(p);
//     model *m = rand_model(&clone);
//
//     base_iter iter;
//     if (base_init_(list, LineNumber, &iter))
//     {
//         void *buf = malloc(2 * sizeof(u32) * (list->lcnt + (list->size > 0)));
//         u32 *buf_l = (u32 *) buf; 
//         u32 *buf_m = buf_l + list->lcnt + (list->size > 0); 
//
//         u32 i = 0;
//         do 
//         {
//             buf_l[i++] = get_position(&iter);
//         } while (base_next_line(&iter));
//
//         line_positions(m, 0, buf_m, list->lcnt + 1);
//
//         Assert(memcmp(buf_l, buf_m, (list->lcnt + 1) * sizeof(u32)) == 0);
//
//         free(buf);
//     }
//
//     free_piece_list(list);
//     free_arena(&m->arena);
// }
//
// TEST(lines_prev)
// void lines_prev(prng *p)
// {
//     prng clone = clone_prng(p);
//
//     piece_list *list = rand_list(p);
//     model *m = rand_model(&clone);
//
//     u32 len = list->lcnt + (list->size > 0);
//     void *buf = malloc(2 * sizeof(u32) * len);
//     u32 *buf_l = (u32 *) buf; 
//     u32 *buf_m = buf_l + len; 
//
//     u32 i = 0;
//     base_iter iter;
//     for (b32 not_over = base_init_rev_(list, LineNumber, &iter);
//         not_over;
//         not_over = base_prev_line(&iter))
//     {
//         buf_l[len - i++ - 1] = get_position(&iter);
//     }
//
//     line_positions(m, 0, buf_m, len);
//     Assert(memcmp(buf_l, buf_m, len * sizeof(u32)) == 0);
//
//     free(buf);
//     free_piece_list(list);
//     free_arena(&m->arena);
// }
//
//
// TEST(advance_line)
// void advance_line(prng *p)
// {
//     prng clone = clone_prng(p);
//     piece_list *list = rand_list(p);
//     model *m = rand_model(&clone);
//
//     u32 list_lcnt = (list->size) ? (list->lcnt + 1) : 0;
//
//     base_iter iter;
//
//     if (base_init_(list, LineNumber, &iter))
//     {
//         for (u32 i = 0;;)
//         {
//             u32 count = rand_range_u32_inclusive(p, 0, list_lcnt - i);
//
//             b32 not_over = base_advance_by_line(&iter, count);
//             if (not_over)
//             {
//                 u32 line_pos = get_position(&iter);
//                 Assert(line_pos == line_position(m, i + count));
//                 i += count;
//             }
//             else
//             {
//                 break;
//             }
//         }
//     }
//
//     free_piece_list(list);
//     free_arena(&m->arena);
// }
//
// TEST(reverse_line)
// void reverse_line(prng *p)
// {
//     prng clone = clone_prng(p);
//     piece_list *list = rand_list(p);
//     model *m = rand_model(&clone);
//
//     u32 list_lcnt = (list->size) ? (list->lcnt + 1) : 0;
//
//     base_iter iter;
//
//     if (base_init_(list, LineNumber, &iter))
//     {
//         for (u32 i = 0;;)
//         {
//             u32 count = rand_range_u32_inclusive(p, 0, list_lcnt - i);
//
//             b32 not_over = base_advance_rev_by_line(&iter, count);
//             if (not_over)
//             {
//                 u32 line_pos = get_position(&iter);
//                 Assert(line_pos == line_position(m, list_lcnt + i + count - 1));
//                 i += count;
//             }
//             else
//             {
//                 break;
//             }
//         }
//     }
//
//     free_piece_list(list);
//     free_arena(&m->arena);
// }
//
// TEST(advance_pos)
// void advance_pos(prng *p)
// {
//     piece_list *list = rand_list(p);
//     base_iter iter;
//     if (base_init_(list, Position, &iter))
//     {
//         u8 *buf = malloc(sizeof(u8) * list->size);
//         write_to_buffer(list, buf, list->size);
//
//         for (u32 i = 0;;)
//         {
//             u32 count = rand_range_u32_inclusive(p, 0, list->size - i);
//             b32 not_over = base_advance_pos_by(&iter, count);
//             if (not_over && get_position(&iter) < list->size)
//             {
//                 u8 value = get_char(&iter);
//                 Assert(buf[i + count] == value);
//                 i +=  count;
//             }
//             else
//             {
//                 break;
//             } 
//         }
//         free(buf);
//     }
//
//     free_piece_list(list);
// }
// //
// TEST(reverse)
// void reverse(prng *p)
// {
//     piece_list *list = rand_list(p);
//
//     u8 *buf = malloc(sizeof(u8) * list->size);
//     write_to_buffer(list, buf, list->size);
//
//     base_iter iter;
//     u32 i = 0;
//     u32 count = 0;
//     for (b32 not_over = base_init_rev_(list, Position, &iter);
//         not_over;
//         not_over = base_advance_pos_rev_by(&iter, count))
//     {
//         u8 value = get_char(&iter);
//         Assert(buf[list->size - (i + count) - 1] == value);
//         i += count;
//         count = rand_range_u32_inclusive(p, 0, list->size - i - 1);
//     }
//
//     free(buf);
//     free_piece_list(list);
// }
//
// TEST(motions)
// void motions(prng *p)
// {
//     prng clone = clone_prng(p);
//     piece_list *list = rand_list(p);
//     model *m = rand_model(&clone);
//
//     for (u32 i = 0; i < MAX_SEQ; ++i)
//     {
//         motion motion = rand_range_u32_inclusive(p, Up, Right);
//
//         u32 max =  (motion == Up || motion || Down) ? (list->lcnt) : list->size;
//         u32 quant = rand_range_u32_inclusive(p, 0, max);
//
//         move_by_motion(list, motion, quant, Normal);
//         model_move_by_motion(m, motion, quant);
//     }
//
//     Assert(m->cx == list->cx);
//     Assert(m->cy == list->cy);
//     Assert(m->pos == list->pos);
//     Assert(m->line_len == list->line_len);
// }
//
//
// TEST(motions_and_insert)
// void motions_and_insert(prng *p)
// {
//     prng clone = clone_prng(p);
//
//     piece_list *list = rand_list(p);
//     model *model = rand_model(&clone);
//
//     u32 max_length_added = MAX_INSERT_MODE_SEQ * MAX_STRING_LEN;
//     insert_seq_result *seq = create(max_length_added);
//
//     for (u32 i = 0; i < MAX_SEQ; ++i)
//     {
//         motion motion = rand_range_u32_inclusive(p, Up, Right);
//
//         u32 max =  (motion == Up || motion || Down) ? (list->lcnt) : list->size;
//         u32 quant = rand_range_u32_inclusive(p, 0, max);
//
//         move_by_motion(list, motion, quant, Normal);
//         model_move_by_motion(model, motion, quant);
//
//         insert_mode_sequence(list, p, seq);
//         commit_insert_mode_undo(list);
//
//         Assert(seq->max == model->pos);
//         model_replace(model, seq->min, seq->max, seq->text_added);
//         {
//             model->pos      -= (seq->max - seq->min);
//             model->cx       -= (seq->max - seq->min);
//             model->line_len -= (seq->max - seq->min);
//
//             u32 num_lines_inserted     = count_token(seq->text_added, '\n');
//             u32 last_line_inserted_len = count_rev_until(seq->text_added, '\n');
//             model->cy += num_lines_inserted;
//             if (num_lines_inserted)
//             {
//                 model->line_len -= model->cx;
//                 model->cx = last_line_inserted_len;
//             }
//             else
//             {
//                 model->cx += last_line_inserted_len;
//             }
//             model->line_len += last_line_inserted_len;
//             model->pos += seq->text_added.len;
//         }
//
//         Assert(model->cx == list->cx);
//         Assert(model->cy == list->cy);
//         Assert(model->pos == list->pos);
//         Assert(model->line_len == list->line_len);
//
//         // This mimics going into normal mode
//         move_by_motion(list, Left, 1, Normal);
//         model_move_by_motion(model, Left, 1);
//
//         clear_seq(seq);
//     }
//
//     free_insert_seq_result(seq);
//     free_piece_list(list);
//     free_arena(&model->arena);
// }
//
//
// TEST(rows)
// void rows(prng *p)
// {
//     prng clone = clone_prng(p);
//     piece_list *list = rand_list(p);
//     model *model = rand_model(&clone);
//
//     u32 cursor = 0;
//     {
//         base_iter iter;
//         base_init_(list, Position | LineNumber, &iter);
//
//         u32 max_row_size = rand_range_u32_inclusive(p, 1, 20);
//
//         u8 *row = malloc(max_row_size);
//         u8 *row_pos = model->s.buffer;
//
//         for (;;)
//         {
//             u32 row_size_a = max_row_size;
//             b32 not_over_a = (base_next_row(&iter, row, &row_size_a) != Over);
//
//             u32 row_size_b = max_row_size;
//             u8 *prev_row_pos = row_pos;
//             b32 not_over_b = next_row(model, &row_pos, &row_size_b);
//
//             Assert(not_over_a == not_over_b);
//             Assert(row_size_a == row_size_b);
//
//             if (not_over_a)
//             {
//                 Assert(strncmp((char *) row, (char *) prev_row_pos, row_size_a) == 0)
//             }
//             else
//             {
//                 break;
//             }
//         }
//         free(row);
//     }
//
//     free_piece_list(list);
//     free_arena(&model->arena);
//
// }
//
//
//
//
//
//
// // TST(next_lines)
// void next_lines(prng *p)
// {
//     prng clone = clone_prng(p);
//     piece_list *list = rand_list(p); 
//     model *m = rand_model(&clone);
//
//     u32 *buf = malloc(sizeof(u32) * (list->lcnt + 1));
//
//     line_positions(m, 0, buf, list->lcnt + 1);
//
//     render_iter iter = render_iter_init_alt(list);
//     for (u32 i = 0;;)
//     {
//         render_item item = next_line_alt(&iter);
//         if (!item.valid)
//         {
//             break;
//         }
//         Assert(item.item == m->s.buffer[buf[i++]]);
//     }
//
//     free(buf);
//     free_piece_list(list);
//     free_arena(&m->arena);
//
//
// }
//
// // TST(line_starts_pos)
// void line_starts_pos(prng *p)
// {
//     prng clone = clone_prng(p);
//
//     piece_list *list = rand_list(p);
//     model *m = rand_model(&clone);
//
//     void *buf = malloc(2 * sizeof(u32) * (list->lcnt + 1));
//     u32 *buf_l = (u32 *) buf; 
//     u32 *buf_m = buf_l + list->lcnt + 1; 
//
//     u32 line_pos = 0;
//     u32 i = 0;
//     LINE_STARTS(list, line_pos, 
//     {
//         buf_l[i++] = line_pos;
//     });
//
//     line_positions(m, 0, buf_m, list->lcnt + 1);
//
//     Assert(memcmp(buf_l, buf_m, (list->lcnt + 1) * sizeof(u32)) == 0);
//
//     free(buf);
//     free_piece_list(list);
//     free_arena(&m->arena);
// }
//
// // TST(line_starts_pos_rev)
// void line_starts_pos_rev(prng *p)
// {
//     prng clone = clone_prng(p);
//
//     piece_list *list = rand_list(p);
//     model *m = rand_model(&clone);
//     u32 len = list->lcnt + 1;
//
//     void *buf = malloc(2 * sizeof(u32) * len);
//     u32 *buf_l = (u32 *) buf;
//     u32 *buf_m = buf_l + len;
//
//     u32 i = 0;
//     u32 line_pos = 0;
//     LINE_STARTS_REV(list, line_pos, {
//         buf_l[len - i++ - 1] = line_pos;
//     });
//
//     line_positions(m, 0, buf_m, len);
//
//     Assert(memcmp(buf_l, buf_m, len * sizeof(u32)) == 0);
//
//     free(buf);
//     free_piece_list(list);
//     free_arena(&m->arena);
// }
//
// // TST(line_starts_pos_rev_range)
// void line_starts_pos_rev_range(prng *p)
// {
//     piece_list *list = rand_list(p);
//     u32 start = rand_range_u32_inclusive(p, 0, list->lcnt);
//     u32 end   = rand_range_u32_inclusive(p, start, list->lcnt);
//     u32 len = end - start + 1;
//
//     void *buf = malloc(2 * sizeof(u32) * len);
//     u32 *buf_fwd = (u32 *) buf;
//     u32 *buf_rev = buf_fwd + len;
//
//     u32 i = 0;
//     u32 line_pos = 0;
//     LINE_STARTS_RANGE(list, line_pos, start, end, 
//     {
//         buf_fwd[i++] = line_pos;
//     });
//
//     i = 0;
//     LINE_STARTS_REV_RANGE(list, line_pos, start, end,
//     {
//         buf_rev[len - i++ - 1] = line_pos;
//     });
//
//     Assert(memcmp(buf_fwd, buf_rev, len * sizeof(u32)) == 0);
//
//     free(buf);
//     free_piece_list(list);
// }
//
// // TST(line_starts_pos_range)
// void line_starts_pos_range(prng *p)
// {
//     prng clone = clone_prng(p);
//
//     piece_list *list = rand_list(p);
//     model *m = rand_model(&clone);
//
//     u32 start = rand_range_u32_inclusive(p, 0, list->lcnt);
//     u32 end   = rand_range_u32_inclusive(p, start, list->lcnt);
//     u32 len = end - start + 1;
//
//     void *buf = malloc(2 * sizeof(u32) * len);
//     u32 *buf_l = (u32 *) buf;
//     u32 *buf_m = buf_l + len; 
//
//     u32 i = 0;
//     u32 line_pos = 0;
//     LINE_STARTS_RANGE(list, line_pos, start, end, 
//     {
//         buf_l[i++] = line_pos;
//     });
//
//     line_positions(m, start, buf_m, len);
//
//     Assert(memcmp(buf_l, buf_m, len * sizeof(u32)) == 0);
//
//     free(buf);
//     free_piece_list(list);
//     free_arena(&m->arena);
// }
//
// // TST(lines_lengths)
// void lines_lengths(prng *p)
// {
//     prng clone = clone_prng(p);
//
//     piece_list *list = rand_list(p);
//     model *m = rand_model(&clone);
//
//     void *lengths = malloc(2 * sizeof(u32) * (list->lcnt + 1));
//     u32 *lens_l = (u32 *) lengths;
//     u32 *lens_m = lens_l + list->lcnt + 1;
//
//     u32 i = 0;
//     u32 len = 0;
//     LINE_LENGTHS(list, len, 
//     {
//         Assert(len <= list->size);
//         lens_l[i++] = len;
//     });
//
//     line_lens(m, lens_m, 0, m->s.len);
//
//     Assert(memcmp(lens_l, lens_m, i  * sizeof(u32)) == 0);
//     free(lengths);
//     free_piece_list(list);
//     free_arena(&m->arena);
// }
//
// // TST(lines_lengths_range)
// void lines_lengths_range(prng *p)
// {
//     prng clone = clone_prng(p);
//
//     piece_list *list = rand_list(p);
//     model *m = rand_model(&clone);
//
//     if (list->size > 0)
//     {
//         u32 start = rand_range_u32_inclusive(p, 0, list->lcnt);
//         u32 end   = rand_range_u32_inclusive(p, start + 1, list->lcnt + 1);
//         u32 len   = end - start;
//         void *lengths = malloc(2 * sizeof(u32) * (len + 1));
//         u32 *lens_l   = (u32 *) lengths;
//         u32 *lens_m   = lens_l + len;
//
//         u32 i = 0;
//         u32 len_ = 0;
//         LINE_LENGTHS_RANGE(list, len_, start, end, 
//         {
//             Assert(item.len <= list->size);
//             lens_l[i++] = len_;
//         });
//
//         line_lens(m, lens_m, start, start + len);
//
//         Assert(memcmp(lens_l, lens_m, len  * sizeof(u32)) == 0);
//         free(lengths);
//     }
//
//     free_piece_list(list);
//     free_arena(&m->arena);
// }
//
// // TST(lines_lengths_rev)
// void lines_lengths_rev(prng *p) 
// {
//     piece_list *list = rand_list(p);
//     if (list->size > 0)
//     {
//         void *lengths = malloc(2 * sizeof(u32) * (list->lcnt + 1));
//         u32 *lens_fwd = (u32 *) lengths;
//         u32 *lens_bck = lens_fwd + list->lcnt + 1;
//
//         u32 length = list->lcnt;
//
//
//         u32 i = 0;
//         u32 len = 0;
//         LINE_LENGTHS(list, len, {
//             lens_fwd[i++] = len;
//         });
//
//         i = 0;
//         LINE_LENGTHS_REV(list, len,
//         {
//             Assert(item.len <= list->size);
//             lens_bck[length - i++] = len;
//         });
//
//         Assert(memcmp(lens_fwd, lens_bck, i  * sizeof(u32)) == 0);
//         free(lengths);
//     }
//
//     free_piece_list(list);
// }
//
//
// // TST(lines_lengths_range_rev)
// void lines_lengths_range_rev(prng *p)
// {
//     piece_list *list = rand_list(p);
//
//     u32 start = rand_range_u32_inclusive(p, 0, list->lcnt);
//     u32 end   = rand_range_u32_inclusive(p, start + 1, list->lcnt + 1);
//     u32 len = end - start;
//
//     if (list->size > 0)
//     {
//         void *lengths = malloc(2 * sizeof(u32) * (len + 1));
//         u32 *lens_l = (u32 *) lengths;
//         u32 *lens_m = lens_l + len;
//
//         u32 i = 0;
//         u32 len_ = 0;
//         LINE_LENGTHS_RANGE(list, len_, start, end, 
//         {
//             lens_l[i++] = len_;
//         });
//
//         i = 0;
//         LINE_LENGTHS_REV_RANGE(list, len_, start, end,
//         {
//             Assert(item.len <= list->size);
//             lens_m[len - i++ - 1] = len_;
//         });
//
//         Assert(memcmp(lens_l, lens_m, len  * sizeof(u32)) == 0);
//         free(lengths);
//     }
//     free_piece_list(list);
// }
//
// // TST(num_lines_in_row_range)
// void num_lines_in_row_range(prng *p)
// {
//     prng clone = clone_prng(p);
//
//     piece_list *list = rand_list(p);
//     model *m = rand_model(&clone);
//     if (list->size > 0)
//     {
//
//         u32 start = rand_range_u32_inclusive(p, 0, list->lcnt);
//         u32 row_size = rand_range_u32_inclusive(p, 1, MAX_STRING_LEN);
//         u32 num_rows = rand_range_u32_inclusive(p, start, list->lcnt + 1);
//
//         lines_result a = num_lines_from(list, start, row_size, num_rows);
//         lines_result b = model_num_lines_from(m, start, row_size, num_rows);
//
//         Assert(equal_lines_result(a, b));
//     }
//
//     free_piece_list(list);
//     free_arena(&m->arena);
// }
//
// // TST(next_chars)
// void next_chars(prng *p)
// {
//     piece_list *list = rand_list(p);
//
//     u8 *buffer = malloc(sizeof(u8) * 2 * list->size);
//     u8 *buf_1 = buffer;
//     u8 *buf_2 = buffer + list->size;
//
//     write_to_buffer(list, buf_1, list->size);
//
//     render_iter iter = render_iter_init_alt(list);
//
//     for (u32 i = 0;;)
//     {
//         render_item item = next_char_alt(&iter);
//         if (!item.valid)
//         {
//             break;
//         }
//
//         buf_2[i++] = item.item;
//
//     }
//
//     Assert(strncmp((const char *) buf_1, (const char *) buf_2, list->size) == 0);
//
//     free(buffer);
//     free_piece_list(list);
// }
//
//
//
