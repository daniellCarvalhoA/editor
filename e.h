#include "e_platform.h"
#include "e_share.h"
#include <stdlib.h>
#include <utf8proc.h>

typedef struct piece_list piece_list;
typedef struct window window;

typedef struct buffer_cursor
{
    u32 x;
    u32 y;
} buffer_cursor;

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
#include "normal.h"

const char mode_layout_str[] = " Layout | ";
const char mode_insert_str[] = " Insert | ";
const char mode_normal_str[] = " Normal | ";
const char mode_visual_str[] = " Visual | ";

typedef enum mode
{
    Normal,
    Layout,
    Insert,
    Visual,
} mode;

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
        }
    }
    return result;
}

// window *active_window      = NULL;
window *interacting_window = NULL;

static platform_api Platform;


typedef struct editor_state
{
    memory_arena arena;
    screen screen;
    dlist buffers;
    paste_buffer p_buffer;
    mode edit_mode;
    normal_parse_state p_state;
} editor_state;


#include "insert_mode.h"

// static piece_list *get_active_buffer()
// {
//     piece_list *result = active_window->buffer;
//     return result;
// }

