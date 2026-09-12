#include <stdlib.h>
#include "string.c"

#define ANSI_BUFFER_SIZE 8192

// NOTE: This is always constant form terminal renderer, or should it depend on 
// the terminal window size?
static const render_view terminal_view = {
    .top_margin = 4,
    .bot_margin = 4,
    .left_margin = 4,
    .right_margin = 4
};

typedef struct
{
    // maybe put a filde descriptor here for stdout, in case of socket.

    u8 ansi_buffer[ANSI_BUFFER_SIZE];
    u32 ansi_buffer_cursor;

} terminal_renderer;

static void flush_text(terminal_renderer *t_renderer)
{
    // Substitute 1 for output file descriptor eventually,
    ssize_t ret = write(1, t_renderer->ansi_buffer, t_renderer->ansi_buffer_cursor);
    Assert(ret == t_renderer->ansi_buffer_cursor);
    t_renderer->ansi_buffer_cursor = 0;
}

static void place_cursor(terminal_renderer *t_renderer, u32 y, u32 x)
{
    size_t max_size = (size_t) (ArrayCount(t_renderer->ansi_buffer) - t_renderer->ansi_buffer_cursor);

    char *buffer = (char *) (t_renderer->ansi_buffer + t_renderer->ansi_buffer_cursor);
    int len = snprintf(buffer, max_size, "\x1b[%u;%uH", 1 + y, 1 + x);
    Assert(len > 0);

    if ((size_t) len >= max_size)
    {
        flush_text(t_renderer);
        place_cursor(t_renderer, y, x);
        return;
    }

    t_renderer->ansi_buffer_cursor += (u32) len;
}

static void move_cursor_down(terminal_renderer *t_renderer, u32 y)
{
    size_t max_size = (size_t) 
        (ArrayCount(t_renderer->ansi_buffer) - t_renderer->ansi_buffer_cursor);

    char *buffer = (char *) (t_renderer->ansi_buffer + t_renderer->ansi_buffer_cursor);
    int len = snprintf(buffer, max_size, "\x1b[%uE", y);
    Assert(len > 0);

    if ((size_t) len >= max_size)
    {
        flush_text(t_renderer);
        move_cursor_down(t_renderer, y);
        return;
    }

    t_renderer->ansi_buffer_cursor += (u32) len;
}


static void move_cursor_right(terminal_renderer *t_renderer, u32 x)
{
    size_t max_size = (size_t) (ArrayCount(t_renderer->ansi_buffer) - t_renderer->ansi_buffer_cursor);

    char *buffer = (char *) (t_renderer->ansi_buffer + t_renderer->ansi_buffer_cursor);
    int len = snprintf(buffer, max_size, "\x1b[%uC", x);
    Assert(len > 0);

    if ((size_t) len >= max_size)
    {
        flush_text(t_renderer);
        move_cursor_right(t_renderer, x);
        return;
    }

    t_renderer->ansi_buffer_cursor += (u32) len;
}




static void render_text(terminal_renderer *t_renderer, str text)
{
    if (t_renderer->ansi_buffer_cursor + text.len > ArrayCount(t_renderer->ansi_buffer))
    {
        flush_text(t_renderer);
    }
    memcpy(t_renderer->ansi_buffer + t_renderer->ansi_buffer_cursor, text.buffer, text.len);
    t_renderer->ansi_buffer_cursor += text.len;
}

static void render_terminal(
    terminal_renderer *t_renderer,
    render_commands *r_commands)
{
    color prev = Default;
    //u32 prev_row = 0;
    //u32 prev_col = 0;
    //u32 prev_len = 0;
    place_cursor(t_renderer, 0, 0);
    for (u32 i = 0; i < r_commands->count; ++i)
    {
        render_command command = r_commands->commands[i];
        switch (command.type)
        {
            case RenderCommand_Text:
            {
                u32 row = command.text.row;
                u32 col = command.text.col;
                str text = command.text.text;
                //if (prev_row == row)
                //{
                    //move_cursor_right(t_renderer, col - (prev_col + prev_len));
                //}
                //else 
                {
                    place_cursor(t_renderer, row, col);
                }

                if (prev != command.color)
                {
                    if (command.color == Reversed)
                    {
                        render_text(t_renderer, STR_LIT("\x1b[7m"));
                    }
                    else
                    {
                        render_text(t_renderer, STR_LIT("\x1b[m"));
                    }
                }
                render_text(t_renderer, text);

                prev = command.color;
                //prev_row = row;
                //prev_col = col;
                //prev_len = text.len;

            } break;

            case RenderCommand_Rect:
            {
                if (prev != command.color)
                {
                    if (command.color == Reversed)
                    {
                        render_text(t_renderer, STR_LIT("\x1b[7m"));
                    }
                    else
                    {
                        render_text(t_renderer, STR_LIT("\x1b[m"));
                    }
                }
                prev = command.color;
                place_cursor(t_renderer, command.cursor.row, command.cursor.col);
            } break;

            case RenderCommand_VLine: 
            {
                if (prev != command.color)
                {
                    prev = command.color;
                    render_text(t_renderer, STR_LIT("\x1b[m"));
                }
                for (u32 y = command.line.low; y < command.line.low + command.line.len - 1; ++y)
                {
                    place_cursor(t_renderer, y, command.line.dim);
                    render_text(t_renderer, STR_LIT("│"));
                }
            } break;

            default:
            {
            } break;
        }

    }
    flush_text(t_renderer);
    r_commands->count = 0;
}

