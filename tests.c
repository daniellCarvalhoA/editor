#include "e.h"
#include "string.c"
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
#define MAX_COMPOUND_UNDO_SEQ 8
#define MAX_UNDO_REDO_SEQ 4
#define MAX_SEQ 10
#define MAX_LINE_LEN 40
#define DEBUG_UNDO_RECORDS_SIZE Megabytes(1)

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
#include "iter.c"
#include "piece_list.c"
#include "grid.c"
#include "window.c"
#include "undo.c"
#include "screen.c"
#include "command.c"
#include "paste_buffer.c"
#include "motions.c"
#include "normal.c"
#include "normal_test.c"
#include "insert_mode.c"

static buffer_cursor rand_buffer_cursor_clamped(
    piece_list *list,
    prng *prng,
    buffer_cursor max)
{
    u32 y = rand_range_u32_inclusive(prng, 0, max.y);
    u32 x;
    if (y == max.y)
    {
        x = rand_range_u32_inclusive(prng, 0, max.x);
    }
    else
    {
        x = rand_range_u32_inclusive(prng, 0, MAX_LINE_LEN);
        x = clamp_to_length(list, y, x, false);
    }

    return (struct buffer_cursor) { .y = y, .x = x };

}

static editor_state *rand_editor(prng *prng)
{
    editor_state *e_state = BootstrapPushStruct(editor_state, arena, 4096);
    initialize_screen(&e_state->screen);
    reset_parse_state(&e_state->p_state);
    reset_paste_buffer(&e_state->p_buffer);

    piece_list *buffer = rand_list(prng);
    map_buffer_to_window(buffer, e_state->screen.active_window);

    INIT_LIST_HEAD(&e_state->buffers);
    list_add(&buffer->list, &e_state->buffers);

    return e_state;
}

typedef struct 
{
    win_cursor min; 
    win_cursor max;
    string text_added;
} insert_seq_result;

static insert_seq_result *create(u32 max_length)
{
    insert_seq_result *result = (insert_seq_result *) malloc(sizeof(insert_seq_result)); 
    result->min.x = result->max.x = result->min.y = result->max.y = 0;
    result->text_added.len = 0;
    result->text_added.capacity = max_length;
    result->text_added.buffer = malloc(max_length);
    return result;
}

static void free_insert_seq_result(insert_seq_result *seq)
{
    free(seq->text_added.buffer);
    free(seq);
}

static inline void clear_seq(insert_seq_result *seq)
{
    seq->min.x = seq->max.x = seq->min.y = seq->max.y = 0;
    seq->text_added.len = 0;
}

static inline void insert_insert_mode(window *win, string text)
{
    for (u32 j = 0; j < text.len; ++j)
    {
        str s = { .buffer = (u8 *) text.buffer + j, .len = 1 };
        insert_mode_insert(win, s);
    }
}

// static inline void insert_insert_mode_(window *win, string text)
// {
//     for (u32 j = 0; j < text.len; ++j)
//     {
//         str s = { .buffer = (u8 *) text.buffer + j, .len = 1 };
//         insert_mode_insert_(win, s);
//     }
// }

static inline void delete_insert_mode(window *win, win_cursor start)
{
    while (win->bc.x > start.x)
    {
        insert_mode_delete(win);
    }
}
#if 0
static inline void delete_insert_mode_(window *win, win_cursor start)
{
    while (compare(win->bc, start) == GreaterThan)
    {
        insert_mode_delete_(win);
    }
}

static void insert_mode_sequence_2(
    window *win,
    prng *prng,
    insert_seq_result *seq)
{
    seq->min.x = win->buffer->size;
    seq->min.y = win->buffer->lcnt;
    seq->max = win->bc;
    u32 seq_size = rand_range_u32_inclusive(prng, 1, MAX_INSERT_MODE_SEQ);

    for (u32 i = 0; i < seq_size; ++i)
    {
        if (rand_b32(prng))
        {
            INIT_STACK_STRING(text, MAX_STRING_LEN);
            rand_ascii_string(&text, prng, 1, MAX_STRING_LEN);
            push_string(&seq->text_added, from_string(text));
            insert_insert_mode(win, text);
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
            delete_insert_mode_(win, begin);
        }
    }
    seq->min = minimum(seq->min, seq->max);
}
#endif

static void insert_mode_sequence(
    window *win,
    prng *prng,
    insert_seq_result *seq)
{
    seq->min.x = win->buffer->size;
    seq->min.y = win->buffer->lcnt;
    seq->max = win->bc;
    u32 seq_size = rand_range_u32_inclusive(prng, 1, MAX_INSERT_MODE_SEQ);

    for (u32 i = 0; i < seq_size; ++i)
    {
        if (rand_b32(prng))
        {
            INIT_STACK_STRING(text, MAX_STRING_LEN);
            rand_ascii_string(&text, prng, 1, MAX_STRING_LEN);
            push_string(&seq->text_added, from_string(text));
            insert_insert_mode(win, text);
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
            delete_insert_mode(win, begin);
        }
    }
    seq->min = minimum(seq->min, seq->max);
}

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

TEST(replace_sound)
void replace_sound(prng *prng)
{
    piece_list *list = rand_list(prng);
    list_invariants(list);
    free_piece_list(list);
}

TEST(replace_against_model)
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

TEST(undo_redo_test)
void undo_redo_test(prng *prng)
{
    screen screen = {};
    initialize_screen(&screen);
    window *win = create_window(&screen, LeafBuffer, 0);
    piece_list *list = rand_list(prng);
    map_buffer_to_window(list, win);

    u32 num_reversible_edits = rand_range_u32_inclusive(prng, 1, MAX_UNDO_REDO_SEQ);
    u32 num_actual_reversible_edits = num_reversible_edits;

    for (u32 i = 0; i < num_reversible_edits; ++i)
    {
        u32 num_compound_undo_seq = rand_range_u32_inclusive(
            prng,
            1, MAX_COMPOUND_UNDO_SEQ);

        u32 last_top = get_last_record_top(&list->undo_records);
        begin_undo_sequence(&list->undo_records, 0);
        for (u32 j = 0; j < num_compound_undo_seq; ++j)
        {
            rand_replace(list, prng);
        }
        end_undo_sequence(&list->undo_records, &list->redo_records);

        if (last_top == get_last_record_top(&list->undo_records))
        {
            num_actual_reversible_edits--;
        }
    }

    if (num_actual_reversible_edits <= list->undo_records.num_records)
    {
        u32 size_before = list->size;
        u32 lcnt_before = list->lcnt;

        u8 before[list->size];
        write_to_buffer(list, before, list->size);

        for (u32 j = 0; j < 2; ++j)
        {
            for (u32 i = 0; i < num_actual_reversible_edits; undo(win), ++i);
            if (list->redo_records.num_records == num_actual_reversible_edits) 
            {
                for (u32 i = 0; i < num_actual_reversible_edits; redo(win), ++i);

                u8 after[list->size];

                write_to_buffer(list, after, list->size);

                Assert(size_before == list->size);
                Assert(lcnt_before == list->lcnt);
                Assert(strncmp((const char *) before, (const char *) after, list->size) == 0);
            }
        }
    }

    list_invariants(list);
    free_piece_list(list);
    free_screen(&screen);
}

TEST(undo_test)
void undo_test(prng *prng)
{
    screen screen = {};
    initialize_screen(&screen);
    window *win = create_window(&screen, LeafBuffer, 0);
    piece_list *list = rand_list(prng);

    map_buffer_to_window(list, win);

    u32 size_before = list->size;
    u32 lcnt_before = list->lcnt;

    u8 before[list->size];
    write_to_buffer(list, before, list->size);

    u32 num_reversible_edits = rand_range_u32_inclusive(prng, 1, MAX_UNDO_REDO_SEQ);
    u32 num_actual_reversible_edits = num_reversible_edits;

    for (u32 i = 0; i < num_reversible_edits; ++i)
    {
        u32 num_compound_undo_seq = rand_range_u32_inclusive(prng, 1, MAX_COMPOUND_UNDO_SEQ);
        u32 last_top = get_last_record_top(&list->undo_records);
        begin_undo_sequence(&list->undo_records, 0);
        for (u32 j = 0; j < num_compound_undo_seq; ++j)
        {
            rand_replace(list, prng);
        }
        end_undo_sequence(&list->undo_records, &list->redo_records);
        if (last_top == get_last_record_top(&list->undo_records))
        {
            num_actual_reversible_edits--;
        }
    }

    if (num_actual_reversible_edits <= list->undo_records.num_records)
    {

        for (u32 i = 0; i < num_actual_reversible_edits; ++i)
        {
            undo(win);
        }

        u8 after[list->size];

        write_to_buffer(list, after, list->size);

        Assert(size_before == list->size);
        Assert(lcnt_before == list->lcnt);
        Assert(strncmp((const char *) before, (const char *) after, list->size) == 0);
    }

    list_invariants(list);
    free_piece_list(list);
    free_screen(&screen);
}


TEST(search_string)
void search_string(prng *prng)
{
    screen screen = {};
    initialize_screen(&screen);
    window *win = create_window(&screen, LeafBuffer, 0);
    piece_list *list = rand_list(prng);
    map_buffer_to_window(list, win);

    INIT_STACK_STRING(s, MAX_STRING_LEN);
    do 
    {
        rand_ascii_string(&s, prng, 1, MAX_STRING_LEN);
    } while (s.len == 0);

    piece piece = make_piece(list, from_string(s));

    piece_slice p_slice = { .base = &piece, .count = 1 };

    buffer_cursor bc = rand_buffer_cursor(list, prng);

    range_replace(list, bc, bc, p_slice);

    search_str(list, from_string(s));

    b32 found = false;

    for (u32 i = 0; i < list->num_matches; ++i)
    {
        u32 test_position = list->matches[i];
        buffer_cursor test_cursor = cursor_from_position(
            list,
            &list->iter,
            test_position);
        found |= (test_cursor.x == bc.x && test_cursor.y == bc.y);
    }

    Assert(found);
    free_piece_list(list);
    free_screen(&screen);
}

TEST(search_string_2)
void search_string_2(prng *prng)
{
    screen screen = {};
    initialize_screen(&screen);
    window *win = create_window(&screen, LeafBuffer, 0);
    piece_list *list = rand_list(prng);
    map_buffer_to_window(list, win);

    buffer_range br = rand_buffer_range(list, prng);

    base_iter iter = find_cursor(list, &list->iter, br.first);
    u32 start_position = position(&iter);
    iter = find_cursor(list, &list->iter, br.one_past_end);
    u32 end_position = position(&iter);

    if (start_position == end_position)
    {
        free_piece_list(list);
        free_screen(&screen);
        search_string_2(prng);
        return;
    }
    Assert(end_position >= start_position);
     

    p_buffer p_buffer = {};
    yank(list, &p_buffer, br.first, br.one_past_end);

    string s_string = {};
    if (p_buffer.type == BufferType_AsStr)
    {
        s_string = p_buffer.text;
    }
    else
    {
        s_string.capacity = end_position - start_position;
        s_string.buffer = (u8 *) malloc(sizeof(u8) * s_string.capacity);
        piece_slice p_slice = { .base = p_buffer.pieces, .count = p_buffer.count };
        write_piece_text(p_buffer.buffer, p_slice, &s_string);
    }

    Assert(s_string.len == s_string.capacity);

    search_str(list, from_string(s_string));

    b32 found = false;

    for (u32 i = 0; i < list->num_matches; ++i)
    {
        u32 test_position = list->matches[i];
        found |= (test_position == start_position);
    }

    Assert(found);
    free_piece_list(list);
    free_screen(&screen);
    free_paste_buffer(&p_buffer);

    if (p_buffer.type != BufferType_AsStr)
    {
        free(s_string.buffer);
    }
}

TEST(insert_mode_seq)
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

    u32 cy = rand_range_u32_inclusive(p, 0, list_a->lcnt);
    u32 cx = rand_range_u32_inclusive(p, 0, MAX_LINE_LEN);
    {
        Assert(win_a->bc.y == 0);

        motion_spec m_spec = {
            .motion_type = Motion_Vertical,
            .motion_quantifier = cy,
            .flags = MotionFlags_Backwards | MotionFlags_Exclusive,
            .open_close_index = -1
        };
        move_by_motion(win_a, m_spec);

        m_spec.motion_type = Motion_Horizontal;
        m_spec.motion_quantifier = cx;
        m_spec.flags = MotionFlags_Exclusive;

        move_by_motion(win_a, m_spec);

        begin_undo(win_a);
        insert_mode_sequence(win_a, p, seq);
        commit_insert_mode_undo(list_a);
        end_undo(win_a);
    }

    piece_list *list_b = rand_list(&clone);
    window *win_b = create_window(&screen, LeafBuffer, 0);
    map_buffer_to_window(list_b, win_b);
    {
        motion_spec m_spec = {
            .motion_type = Motion_Vertical,
            .motion_quantifier = cy,
            .flags = MotionFlags_Backwards | MotionFlags_Exclusive,
            .open_close_index = -1
        };
        move_by_motion(win_b, m_spec);

        m_spec.motion_type = Motion_Horizontal;
        m_spec.motion_quantifier = cx;
        m_spec.flags = MotionFlags_Exclusive;

        move_by_motion(win_b, m_spec);
        begin_undo(win_b) ;

        if ((seq->min.x != seq->max.x) || 
            (seq->min.y != seq->max.y) || 
            (seq->text_added.len > 0))
        {
            piece piece;
            piece_slice p_slice =  {};
            if (seq->text_added.len > 0)
            {
                piece = make_piece(list_b, from_string(seq->text_added));
                p_slice.base = &piece;
                p_slice.count = 1;
            }
            range_replace(list_b, seq->min, seq->max, p_slice);
        }
        end_undo(win_b);
    }

    b32 equal_lists = lists_are_equal(list_a, list_b);
    Assert(equal_lists);
    free_insert_seq_result(seq);
    free_screen(&screen);
    free_piece_list(list_a);
    free_piece_list(list_b);
}

