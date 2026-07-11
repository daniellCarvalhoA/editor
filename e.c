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
#include "paste_buffer.c"
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
            render_window(win, &state->screen, state->edit_mode);
        }

        buffer->changed        = false;
        buffer->top_changed    = 0;
        buffer->bot_changed    = 0;
        buffer->lines_inserted = 0;
        buffer->lines_deleted  = 0;
    }

    if (state->screen.change & Render_RedrawBorders)
    {
        set_color(&state->screen, 32);
        draw_borders(&state->screen, state->screen.root_window);
        reset_color(&state->screen);
    }

    state->screen.change = Render_NoChange;

    render_command_window(&state->screen);

    reset_window_cursor(&state->screen, state->edit_mode);
    window *active_window = state->screen.active_window;
    place_cursor(&state->screen, active_window->cy, active_window->cx);

    flush_buffer(&state->screen);
}

static void initialize_editor(editor_state *state, char *filepath)
{
    initialize_screen(&state->screen);
    reset_parse_state(&state->p_state);
    reset_paste_buffer(&state->p_buffer);

    piece_list *buffer = create_buffer(&state->arena, filepath);
    map_buffer_to_window(buffer, state->screen.active_window);

    INIT_LIST_HEAD(&state->buffers);
    list_add(&buffer->list, &state->buffers);
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
        switch (editor->edit_mode)
        {
            case Insert:
            {
                if (process_insert(editor, input, input_size))
                {
                    return true;
                }
            } break;

            case Normal:
            {
                if (editor->screen.active_window == editor->screen.command_window)
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


