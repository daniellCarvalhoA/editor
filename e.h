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

#include "math.h"
#include "command.h"
#include "memory.h"
#include "lists.h"
#include "history.h"
#include "buffer.h"
#include "node.h"
#include "search.h"
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
    paste_buffer p_buffer;
    mode edit_mode;
    normal_parse_state p_state;
    command prev_command;

    b32 searching;
} editor_state;


#include "insert_mode.h"


