#include "e.h"
#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include "buffer.c"
#include "node.c"
#include "search.c"
#include "iter.c"
#include "piece_list.c"
#include "grid.c"
#include "window.c"
#include "screen.c"
#include "command.c"
#include "normal.c"
#include "insert_mode.c"

static void render(editor_state *state)
{
    piece_list *buffer;
    list_for_each_entry(buffer, &state->buffers, list)
    {
        window *win;
        list_for_each_entry(win, &buffer->window_sentinel, next_in_buffer)
        {
            render_window(win, &state->screen, win == active_window);
        }

        buffer->changed        = false;
        buffer->top_changed    = 0;
        buffer->bot_changed    = 0;
        buffer->lines_inserted = 0;
        buffer->lines_deleted  = 0;
    }

    render_command_window(&state->screen);

    reset_window_cursor(&state->screen, active_window);
    place_cursor(&state->screen, active_window->cy, active_window->cx);

    flush_buffer(&state->screen);
}

static void initialize_editor(editor_state *state, char *filepath)
{
    initialize_screen(&state->screen);

    piece_list *buffer = create_buffer(&state->arena, filepath);
    map_buffer_to_window(buffer, active_window);

    INIT_LIST_HEAD(&state->buffers);
    list_add(&buffer->list, &state->buffers);

    set_color(&state->screen, 32);
    draw_borders(&state->screen, state->screen.root_window);
    reset_color(&state->screen);
}

extern UPDATE_WINDOW_DIM(update_window_dim)
{
    Platform = memory->Platform;
    editor_state *editor = memory->editor;
    if (editor)
    {
        update_window_size(&editor->screen);
        editor->screen.change |= Render_LayoutChange;
        render(editor);
        editor->screen.change = Render_NoChange;
    }
}

extern UPDATE_AND_RENDER(update_and_render)
{
    Platform = memory->Platform;
    editor_state *editor =  memory->editor;

    if (!editor)
    {
        memory->editor = editor = BootstrapPushStruct(editor_state, arena, 8 * 1024);
        initialize_editor(editor, cmdl[1]);
        if (cmdc > 1)
        {
            fprintf(stderr, "file\n");
        }
    }
    else
    {
        switch (edit_mode)
        {
            case Insert:
            {
                if (process_insert(input, input_size))
                {
                    return true;
                }
            } break;

            case Normal:
            {
                if (active_window == editor->screen.command_window)
                {
                    if (parse_command(editor, input, input_size))
                    {
                        return true;
                    }
                }
                else
                {
                    process_normal(editor, input, input_size);
                }
            } break;

            case Layout:
            {
                process_layout(editor, input, input_size);
            } break;

            case Visual:
            {
                process_normal(editor, input, input_size);
            } break;
        }
    }

    render(editor);
    return false;
}
