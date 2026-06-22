


static void parse_command(editor_state *state)
{
}

static void process_command(editor_state *state, u8 *input, u32 input_size)
{
    active_window->change |= Render_BufferChange;
    switch (*input)
    {
        case '\x1b':
        {
            active_window = interacting_window;
            clear_buffer(&state->command_window->c_buffer);
            active_window->bcx = 0;
            active_window->bcy = 0;
        } break;

        case '\r':
        {
            // Todo:
            active_window = interacting_window;
            parse_command(state);
        } break;

        case 127:
        {
            pop(&state->command_window->c_buffer);
            active_window->bcx--;
        } break;

        default:
        {
            append_char(&state->command_window->c_buffer, input, input_size);
            active_window->bcx++;
        } break;
    }
}
