#include "e_share.h"
#include "e_platform.h"
#include <stdlib.h>

typedef struct piece_list piece_list;
typedef struct window window;

typedef struct buffer_cursor
{
    u32 x;
    u32 y;
} buffer_cursor;

typedef struct buffer_cursor buffer_cursor_diff;

static inline buffer_cursor_diff buffer_diff(buffer_cursor a, buffer_cursor b)
{
    Assert(a.y > b.y || ((a.y == b.y) && a.x >= b.x));

    buffer_cursor_diff diff = {};
    if (a.y > b.y)
    {
        diff.y = a.y - b.y;
        diff.x = b.x;
    }
    else
    {
        diff.x = a.x - b.x;
    }
    return diff;
}

static inline buffer_cursor buffer_add(buffer_cursor a, buffer_cursor_diff b)
{
    buffer_cursor add = {};

    if (b.y == 0)
    {
        add.y = a.y;
        add.x = a.x + b.x;
    }
    else
    {
        add.y = a.y + b.y;
        add.x = b.x;
    }

    return add;
}


#include "math.h"
#include "command.h"
#include "memory.h"
#include "lists.h"
#include "history.h"
#include "buffer.h"
#include "node.h"
#include "undo.h"
#include "iter.h"
#include "piece_list.h"
#include "paste_buffer.h"
#include "grid.h"
#include "screen.h"
#include "window.h"
#include "motions.h"
#include "normal.h"

const char mode_layout_str[] = " LAYOUT | ";
const char mode_insert_str[] = " INSERT | ";
const char mode_normal_str[] = " NORMAL | ";
const char mode_visual_str[] = " VISUAL | ";
const char mode_line_visual_str[] = " VLINE | "; 
const char mode_block_viusal_str[] = " VBLOCK | "; 

// mode edit_mode = Normal;

static const char *mode_str(mode edit_mode)
{
    const char *result = 0;
    switch (edit_mode)
    {
        case Layout:
        {
            result = mode_layout_str;
        } break;

        case Insert:
        {
            result =  mode_insert_str;
        } break;

        case Normal:
        {
            result =  mode_normal_str;
        } break;

        case Visual:
        {
            result = mode_visual_str;
        } break;

        case LineVisual:
        {
            result = mode_line_visual_str;
        } break;

        case BlockVisual:
        {
            result = mode_block_viusal_str;
        } break;
    }
    return result;
}

window *interacting_window = NULL;

static platform_api Platform;

typedef struct
{
    u8 open;
    u8 close;
} char_pair;

static char_pair open_close_pairs[] = 
{
    { '(', ')' },
    { '"', '"' },
    { '{', '}' },
    { '[', ']' },
    { '<', '>' },
};

static inline i32 get_pair(u8 token)
{
    i32 result = - 1;
    for (u32 i = 0; i < ArrayCount(open_close_pairs); ++i)
    {
        char_pair test_pair = open_close_pairs[i];
        if (test_pair.open == token || test_pair.close == token)
        {
            result = i;
            break;
        }
    }
    return result;
}

typedef struct editor_state
{
    memory_arena arena;
    screen screen;
    dlist buffers;
    p_buffer p_buffer;
    mode edit_mode;
    normal_parse_state p_state;
    command prev_command;
} editor_state;

static piece_list *find_buffer(editor_state *state, str filename)
{
    piece_list *buffer;
    list_for_each_entry(buffer, &state->buffers, list)
    {
        str filepath = c_str_to_str(buffer->filepath);
        if ((filepath.len == filename.len) &&
            (memcmp(filepath.buffer, filename.buffer, filepath.len) == 0))
        {
            return buffer;
        }
    }
    return NULL;
}

#include "insert_mode.h"


