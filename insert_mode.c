
static void commit_insert_mode_undo(piece_list *list)
{
    insert_mode *state = &list->i_state;
    if ((state->del_count > 0) || (state->ins_count))
    {

        undo_memory_header *header = allocate_undo_memory_block(
            &list->undo_history,
            &list->history_arena,
            state->del_count);

        header->abs_idx   = state->abs_idx;
        header->ins_count = state->ins_count;
        header->del_count = state->del_count;

        piece *pieces      = get_pieces_from_header(header);
        buffer_type *types = get_types_from_header(header);

        list_piece *cursor;
        list_for_each_entry(cursor, &state->piece_head, list)
        {
            *(pieces++) = cursor->piece;
            *(types ++) = cursor->type;
        }

        if (list->staged)
        {
            LIST_INSERT(list->staged->data, header);
            insert_node(&list->undo_history, list->staged);
            list->staged = 0;
        }
        else
        {
            undo_node *new_node = allocate_tree_node(&list->history_arena);
            new_node->data = header;
            new_node->cx = state->cx;
            new_node->cy = state->cy;
            insert_node(&list->undo_history, new_node);
        }
    }
    clear_insert_state(&list->i_state);
    clear(&state->insert_mode_arena);
}

static void normal_mode()
{
    commit_insert_mode_undo(active_window->buffer);
    move_by_motion(active_window, Left, 1);
}

// typedef struct insert_mode_undo
// {
//     u32 cx;
//     u32 cy;
//     u32 abs_idx;
//     u32 ins_count;
//     piece *piece;
//     buffer_type *type;
// } insert_mode_undo;


static void insert_mode_insert(window *win, u8 *input, u32 input_size)
{
    // window *win        = active_window;
    piece_list *list   = win->buffer;
    u32 lines_inserted = (*input == '\n');
    base_iter location = find_cursor(&list->iter, win->bcy, win->bcx);

    insert_mode *state   = &list->i_state;
    segmented_node *node = location.node;

    switch (list->i_state.state)
    {
        case Init:
        {
            list->num_pieces++;
            state->state     = Inserted;
            state->position  = position(&location);
            state->abs_idx   = location.abs_idx;
            state->ins_count = 1;
            state->cx = win->bcx;
            state->cy = win->bcy;

            piece pieces[2]      = { make_piece(list, input, input_size) };
            buffer_type types[2] = { BufferType_Append };

            cursor cursor = { node, location.piece_idx };

            if (location.pos_in_piece > 0)
            {
                piece *curr_piece = get_piece_(&location);
                if (curr_piece->size == location.pos_in_piece)
                {
                    state->abs_idx++;
                }
                else 
                {
                    list->num_pieces++;
                    state->ins_count += 2;
                    state->del_count = 1;

                    types[1]       = *get_type_(&location);
                    list_piece *lp = PushStruct(&state->insert_mode_arena, list_piece, NoClear());

                    lp->piece = *curr_piece;
                    lp->type  = types[1];
                    INIT_LIST_HEAD(&lp->list);
                    list_add(&lp->list, &state->piece_head);

                    pieces[1].off  = get_offset(&location);
                    pieces[1].size = curr_piece->size - location.pos_in_piece;
                    pieces[1].lcnt = curr_piece->lcnt - location.line_in_piece;

                    curr_piece->lcnt = location.line_in_piece;
                    curr_piece->size = location.pos_in_piece;

                    node->size -= pieces[1].size;
                    node->lcnt -= pieces[1].lcnt;
                }
                add_to_cursor(list, &cursor, 1);
            }
            replace(list, cursor, cursor, pieces, types, state->ins_count - state->del_count);
            reset_cursor_(&list->iter);
        } break;

        case Inserted:
        {
            piece *piece = get_piece_(&location);
            piece->size += input_size;
            node->size  += input_size;

            memcpy(list->append.text + list->append.text_len, input, input_size);
            list->append.text_len += input_size;

            if (lines_inserted)
            {
                piece->lcnt++;
                node->lcnt++;
                list->append.lines[list->append.num_lines++] = list->append.text_len;
            }
        } break;

        case Deleted:
        {
            piece *curr_piece = get_piece_(&location);
            state->state = Inserted;
            state->ins_count++;
            piece piece = make_piece(list, input, input_size); 
            // make_piece_from_char(list, c);
            buffer_type type  = BufferType_Append;

            cursor cursor = { node, location.piece_idx };
            if (curr_piece && location.pos_in_piece == curr_piece->size)
            {
                add_to_cursor(list, &cursor, 1);
            }
            replace(list, cursor, cursor, &piece, &type, 1);
            list->num_pieces++;
            reset_cursor_(&list->iter);
        } break;
    }

    list->changed        = true;
    list->top_changed    = win->bcy;
    list->lines_inserted = lines_inserted;
    list->bot_changed    = win->bcy + 1;
    list->size += input_size;

    if (lines_inserted)
    {
        list->lcnt++;
        win->dcx = 0;
        win->bcx = 0;
        win->dcy = win->bcy + 1;
        win->bcy = win->dcy;
    }
    else
    {
        win->dcx = win->bcx + 1;
        win->bcx = win->dcx;
    }
}

static void insert_mode_delete(window *win)
{
    // window *win = active_window;
    piece_list *list = win->buffer;
    insert_mode *state   = &list->i_state;

    base_iter last_iter = find_cursor(&list->iter, win->bcy, win->bcx);
    fix_iter(&last_iter);

    base_iter iter = find_cursor(&list->iter, win->bcy, win->bcx - 1);
    fix_iter(&iter);

    u32 del_size = position(&last_iter) - position(&iter);

    segmented_node *node = iter.node;
    u32 num_lines_deleted = get_char(&iter) == '\n';

    switch (state->state)
    {
        case Init:
        {
            state->state    = Deleted;
            state->deleted  = true;
            state->position = position(&iter);
            state->cx = win->bcx;
            state->cy = win->bcy;

            piece *curr_piece = get_piece_(&iter);
            buffer_type *type = get_type_(&iter);
            list_piece *lp = PushStruct(&list->i_state.insert_mode_arena, list_piece, NoClear());

            lp->piece = *curr_piece;
            lp->type  = *type;

            INIT_LIST_HEAD(&lp->list);
            list_add(&lp->list, &list->i_state.piece_head);

            state->del_count++;
            state->del_count = 1;

            if (iter.pos_in_piece == 0)
            {
                state->abs_idx = iter.abs_idx;
                if (last_iter.pos_in_piece == 0 || last_iter.pos_in_piece == curr_piece->size)
                {
                    cursor start_cursor = { node, iter.piece_idx };
                    cursor end_cursor   = add_to_cursor_by_value(list, start_cursor, 1);
                    replace(list, start_cursor, end_cursor, 0, 0, 0);
                    list->num_pieces--;
                } 
                else
                {
                    state->ins_count = 1;

                    curr_piece->off = get_offset(&last_iter);
                    curr_piece->size -= last_iter.pos_in_piece;
                    curr_piece->lcnt -= last_iter.line_in_piece;

                    node->size -= last_iter.pos_in_piece;
                    node->lcnt -= last_iter.line_in_piece;
                }
            }
            else if (iter.pos_in_piece == curr_piece->size)
            {
                state->abs_idx = iter.abs_idx + 1;

                if (last_iter.pos_in_piece == 0 || last_iter.pos_in_piece == curr_piece->size)
                {
                    cursor start_cursor = { node, iter.piece_idx };
                    add_to_cursor(list, &start_cursor, 1);
                    cursor end_cursor = add_to_cursor_by_value(list, start_cursor, 1);
                    replace(list, start_cursor, end_cursor, 0, 0, 0);
                    list->num_pieces--;
                } 
                else
                {
                    state->ins_count = 1;

                    curr_piece->off = get_offset(&last_iter);
                    curr_piece->size -= last_iter.pos_in_piece;
                    curr_piece->lcnt -= last_iter.line_in_piece;

                    node->size -= last_iter.pos_in_piece;
                    node->lcnt -= last_iter.line_in_piece;
                }
            } 
            else
            {
                state->abs_idx = iter.abs_idx;
                if (last_iter.pos_in_piece == 0 || last_iter.pos_in_piece == curr_piece->size)
                {
                    state->ins_count = 1;

                    curr_piece->size = iter.pos_in_piece;
                    curr_piece->lcnt = iter.line_in_piece;

                    node->size -= position(&last_iter) - position(&iter);
                    node->lcnt -= line_number(&last_iter) - line_number(&iter);
                }
                else
                {
                    state->ins_count = 2;
                    list->num_pieces++;

                    piece right;
                    right.off = get_offset(&last_iter);
                    right.size = curr_piece->size - last_iter.pos_in_piece;
                    right.lcnt = curr_piece->lcnt - last_iter.line_in_piece;

                    node->size -= curr_piece->size - iter.pos_in_piece;
                    node->lcnt -= curr_piece->lcnt - iter.line_in_piece;

                    curr_piece->size = iter.pos_in_piece;
                    curr_piece->lcnt = iter.line_in_piece;

                    cursor start_cursor = { node, iter.piece_idx };
                    add_to_cursor(list, &start_cursor, 1);
                    replace(list, start_cursor, start_cursor, &right, type, 1);
                }
            }
        } break;

        case Inserted:
        {
            if (position(&iter) == state->position)
            {
                state->state = Deleted;
                cursor start_cursor = { node, iter.piece_idx };

                if (!state->deleted)
                {
                    if (iter.pos_in_piece == 0)
                    {
                        sub_from_cursor(&start_cursor, state->del_count);
                    }
                    cursor end_cursor = add_to_cursor_by_value(list, start_cursor, state->ins_count);

                    list_piece *lp = 0;
                    if (state->del_count)
                    {
                        lp = list_first_entry(&state->piece_head, list_piece, list);
                    }
                    replace(list, start_cursor, end_cursor, &lp->piece, &lp->type, state->del_count);

                    list->num_pieces = (list->num_pieces + state->del_count) - state->ins_count;
                    clear_insert_state(state);
                }
                else
                {
                    cursor end_cursor = add_to_cursor_by_value(list, start_cursor, 1);
                    replace(list, start_cursor, end_cursor, 0, 0, 0);
                    state->ins_count--;
                    list->num_pieces--;
                }
            }
            else
            {
                piece *curr_piece = get_piece_(&iter);
                Assert(curr_piece->size > 1);

                curr_piece->size -= del_size;
                curr_piece->lcnt -= num_lines_deleted;

                node->size -= del_size;
                node->lcnt-= num_lines_deleted;
            }
            list->append.text_len -= del_size;
            list->append.num_lines -= num_lines_deleted;
        } break;

        case Deleted:
        {
            state->position = position(&iter);

            piece *curr_piece = get_piece_(&iter);
            buffer_type *type = get_type_(&iter);

            cursor start_cursor = { node, iter.piece_idx };
            cursor end_cursor   = { last_iter.node, last_iter.piece_idx };

            if (iter.pos_in_piece == 0)
            {
                if (iter.abs_idx < state->abs_idx)
                {
                    list_piece *lp = PushStruct(&state->insert_mode_arena, list_piece, NoClear());
                    lp->piece = *curr_piece;
                    lp->type  = *type;
                    INIT_LIST_HEAD(&lp->list);
                    list_add(&lp->list, &state->piece_head);
                    state->del_count++;

                    if (last_iter.pos_in_piece == 0) 
                    {
                        replace(list, start_cursor, end_cursor, 0, 0, 0);
                        list->num_pieces--;
                    } 
                    else if (last_iter.pos_in_piece == curr_piece->size)
                    {
                        add_to_cursor(list, &end_cursor, 1);
                        replace(list, start_cursor, end_cursor, 0, 0, 0);
                        list->num_pieces--;
                    }
                    else
                    {
                        curr_piece->size -= del_size;
                        curr_piece->lcnt -= num_lines_deleted;

                        node->size -= del_size;
                        node->lcnt -= num_lines_deleted;
                    }
                    state->abs_idx = iter.abs_idx;
                }
                else
                {
                    if (last_iter.pos_in_piece == curr_piece->size)
                    {
                        add_to_cursor(list, &end_cursor, 1);
                    }
                    replace(list, start_cursor, end_cursor, 0, 0, 0);
                    state->ins_count--;
                    list->num_pieces--;
                }
            }
            else if (iter.pos_in_piece == curr_piece->size)
            {
                add_to_cursor(list, &start_cursor, 1);
                if (iter.abs_idx < state->abs_idx)
                {
                    list_piece *lp = PushStruct(&state->insert_mode_arena, list_piece, NoClear());
                    lp->piece = *curr_piece;
                    lp->type  = *type;
                    INIT_LIST_HEAD(&lp->list);
                    list_add(&lp->list, &state->piece_head);
                    state->del_count++;

                    if (last_iter.pos_in_piece == 0) 
                    {
                        replace(list, start_cursor, end_cursor, 0, 0, 0);
                        list->num_pieces--;
                    } 
                    else if (last_iter.pos_in_piece == curr_piece->size)
                    {
                        add_to_cursor(list, &end_cursor, 1);
                        replace(list, start_cursor, end_cursor, 0, 0, 0);
                        list->num_pieces--;
                    }
                    else
                    {
                        curr_piece->size -= del_size;
                        curr_piece->lcnt -= num_lines_deleted;

                        node->size -= del_size;
                        node->lcnt -= num_lines_deleted;
                    }
                    state->abs_idx = iter.abs_idx;
                }
                else
                {
                    if (last_iter.pos_in_piece == curr_piece->size)
                    {
                        add_to_cursor(list, &end_cursor, 1);
                    }
                    replace(list, start_cursor, end_cursor, 0, 0, 0);
                    state->ins_count--;
                    list->num_pieces--;
                }
            } 
            else
            {
                if (iter.abs_idx < state->abs_idx)
                {
                    state->abs_idx = iter.abs_idx;
                    list_piece *lp = PushStruct(&state->insert_mode_arena, list_piece, NoClear());
                    lp->piece = *curr_piece;
                    lp->type  = *type;
                    INIT_LIST_HEAD(&lp->list);
                    list_add(&lp->list, &state->piece_head);
                    state->del_count++;
                    state->ins_count++;
                }

                curr_piece->size -= del_size;
                curr_piece->lcnt -= num_lines_deleted;
                node->size -= del_size;
                node->lcnt -= num_lines_deleted;
            }
        } break;
    }
    reset_cursor_(&list->iter);

    list->changed = true;
    list->top_changed   = win->bcy - num_lines_deleted;
    list->lines_deleted = num_lines_deleted;
    list->bot_changed   = win->bcy + 1;

    list->size -= del_size;
    list->lcnt -= num_lines_deleted;

    win->dcx = win->bcx - 1;
    win->bcx = win->dcx;
    list->line_len--;
}

static b32 process_insert(u8 *input, u32 input_size)
{
    b32 result = false;
    switch (*input)
    {
        case 'q':
        {
            result = true;
        } break;

        case 127:
        {
            if (active_window->bcx != 0)
            {
                insert_mode_delete(active_window);
            }
        } break;

        case '\x1b':
        {
            normal_mode();
            edit_mode = Normal;
            active_window->change |= Render_ModeChange;
        } break;

        case '\r':
        {
            insert_mode_insert(active_window, (u8 *) "\n", input_size);
        } break;

        default:
        {
            insert_mode_insert(active_window, input, input_size);
        } break;

    }
    return result;
}

