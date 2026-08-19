static void into_insert_mode(editor_state *state) 
{
    state->p_state.command.a_spec.inserted = (struct str) {
        .buffer = get_last_append(state->screen.active_window),
        .len  = 0
    };
}

static void commit_insert_mode_undo(piece_list *list)
{
    insert_mode *state = &list->i_state;
    if ((state->del_count > 0) || (state->ins_count > 0))
    {

        undo_record *record = allocate_undo_record(
            &list->undo_records,
            state->del_count);

        if (record)
        {
            record->abs_idx = state->abs_idx;
            record->ins_count = state->ins_count;
            record->del_count = state->del_count;

            piece_slice pieces = get_pieces_from_record(record);

            list_piece *cursor;
            list_for_each_entry(cursor, &state->piece_head, list)
            {
                *(pieces.base++) = cursor->piece;
            }
        }
    }
    clear_insert_state(&list->i_state);
    clear(&state->insert_mode_arena);
}

static void into_normal_mode(editor_state *state)
{
    window *win = state->screen.active_window;
    commit_insert_mode_undo(win->buffer);

    motion_spec m_spec = {
        .motion_type = Motion_Horizontal,
        .motion_quantifier = 1,
        .flags = MotionFlags_Backwards | MotionFlags_Exclusive,
        .open_close_index = -1,
    };

    move_by_motion(win, m_spec);


    state->edit_mode = Normal;
    state->prev_command.a_spec.inserted.len = 
        get_last_append(win) -
        state->prev_command.a_spec.inserted.buffer;

    win->change |= Render_ModeChange;
    end_undo_sequence(&win->buffer->undo_records, &win->buffer->redo_records);
}

static void insert_mode_insert(window *win, str s)
{
    piece_list *list   = win->buffer;
    list->changed_since_last_search = true;
    u32 lines_inserted = count_lines(s);
    base_iter location = find_cursor(list, &list->iter, win->bc);

    insert_mode *state   = &list->i_state;
    segmented_node *node = location.node;

    switch (list->i_state.state)
    {
        case Init:
        {
            state->state     = Inserted;
            state->position  = position(&location);
            state->abs_idx   = location.abs_idx;
            state->ins_count = 1;
            state->cx = win->bc.x;
            state->cy = win->bc.y;

            piece pieces[2] = { make_piece(list, s) };
            cursor cursor = { node, location.piece_idx };

            if (location.pos_in_piece > 0)
            {
                piece *curr_piece = get_piece(&location);
                if (curr_piece->size == location.pos_in_piece)
                {
                    state->abs_idx++;
                }
                else 
                {
                    state->ins_count += 2;
                    state->del_count = 1;

                    list_piece *lp = PushStruct(
                        &state->insert_mode_arena,
                        list_piece,
                        NoClear());
                    lp->piece = *curr_piece;
                    INIT_LIST_HEAD(&lp->list);
                    list_add(&lp->list, &state->piece_head);

                    pieces[1].off  = get_offset(list, &location);
                    pieces[1].size = curr_piece->size - location.pos_in_piece;
                    pieces[1].lcnt = curr_piece->lcnt - location.line_in_piece;
                    pieces[1].type = curr_piece->type;

                    curr_piece->lcnt = location.line_in_piece;
                    curr_piece->size = location.pos_in_piece;

                    node->size -= pieces[1].size;
                    node->lcnt -= pieces[1].lcnt;
                    list->size -= pieces[1].size;
                    list->lcnt -= pieces[1].lcnt;
                }
                add_to_cursor(list, &cursor, 1);
            }
            replace(list, cursor, cursor, pieces, state->ins_count - state->del_count);
        } break;

        case Inserted:
        {
            piece *piece = get_piece(&location);
            piece->size += s.len;
            node->size  += s.len;
            list->size  += s.len;
            piece->lcnt += lines_inserted;
            node->lcnt  += lines_inserted;
            list->lcnt  += lines_inserted;


            memcpy(list->append.text + list->append.text_len, s.buffer, s.len);
            list->append.text_len += s.len;

            // NOTE: this is wrong. Or is it?
            if (lines_inserted)
            {
                list->append.lines[list->append.num_lines++] = list->append.text_len;
            }
        } break;

        case Deleted:
        {
            piece *curr_piece = get_piece(&location);
            state->state = Inserted;
            state->ins_count++;
            piece piece = make_piece(list, s); 

            cursor cursor = { node, location.piece_idx };
            if (curr_piece && location.pos_in_piece == curr_piece->size)
            {
                add_to_cursor(list, &cursor, 1);
            }
            replace(list, cursor, cursor, &piece, 1);
        } break;
    }

    list->changed = true;

    if (lines_inserted)
    {
        win->dc.x = 0;
        win->bc.x = 0;
        win->dc.y = win->bc.y + 1;
        win->bc.y = win->dc.y;
    }
    else
    {
        win->dc.x = win->bc.x + 1;
        win->bc.x = win->dc.x;
    }
    reset_cursor(list, &list->iter);
}

static void insert_mode_delete(window *win)
{
    piece_list *list = win->buffer;
    list->changed_since_last_search = true;
    insert_mode *state   = &list->i_state;

    base_iter last_iter = find_cursor(list, &list->iter, win->bc);
    fix_iter(list, &last_iter);

    win_cursor prev_cursor = { .y = win->bc.y, .x = win->bc.x - 1 };
    base_iter iter = find_cursor(list, &list->iter, prev_cursor);
    fix_iter_(list, &iter);

    u32 del_size = position(&last_iter) - position(&iter);
    u32 del_lcnt = line_number(&last_iter) - line_number(&iter);

    segmented_node *node = iter.node;

    switch (state->state)
    {
        case Init:
        {
            state->state    = Deleted;
            state->deleted  = true;
            state->position = position(&iter);
            state->cx = win->bc.x;
            state->cy = win->bc.y;

            piece *curr_piece = get_piece(&iter);
            Assert(curr_piece);

            list_piece *lp = PushStruct(&state->insert_mode_arena, list_piece, NoClear());

            lp->piece = *curr_piece;

            INIT_LIST_HEAD(&lp->list);
            list_add(&lp->list, &list->i_state.piece_head);

            state->del_count = 1;

            if (iter.pos_in_piece == 0)
            {
                state->abs_idx = iter.abs_idx;
                if (last_iter.pos_in_piece == 0) 
                {
                    cursor start_cursor = { node, iter.piece_idx };
                    cursor end_cursor   = add_to_cursor_by_value(list, start_cursor, 1);
                    replace(list, start_cursor, end_cursor, 0, 0);
                } 
                else
                {
                    state->ins_count = 1;

                    curr_piece->off = get_offset(list, &last_iter);
                    curr_piece->size -= last_iter.pos_in_piece;
                    curr_piece->lcnt -= last_iter.line_in_piece;

                    node->size -= del_size;
                    node->lcnt -= del_lcnt;
                }
                list->size -= del_size;
                list->lcnt -= del_lcnt;
            }
            else
            {
                state->abs_idx = iter.abs_idx;
                if (last_iter.pos_in_piece == 0)
                {
                    state->ins_count = 1;

                    curr_piece->size = iter.pos_in_piece;
                    curr_piece->lcnt = iter.line_in_piece;

                    node->size -= del_size;
                    node->lcnt -= del_lcnt;
                    list->size -= del_size;
                    list->lcnt -= del_lcnt;
                }
                else
                {
                    state->ins_count = 2;

                    piece right;
                    right.off = get_offset(list, &last_iter);
                    right.size = curr_piece->size - last_iter.pos_in_piece;
                    right.lcnt = curr_piece->lcnt - last_iter.line_in_piece;
                    right.type = curr_piece->type;

                    node->size -= curr_piece->size - iter.pos_in_piece;
                    node->lcnt -= curr_piece->lcnt - iter.line_in_piece;

                    list->size -= curr_piece->size - iter.pos_in_piece;
                    list->lcnt -= curr_piece->lcnt - iter.line_in_piece;

                    curr_piece->size = iter.pos_in_piece;
                    curr_piece->lcnt = iter.line_in_piece;

                    cursor start_cursor = { node, iter.piece_idx };
                    add_to_cursor(list, &start_cursor, 1);
                    replace(list, start_cursor, start_cursor, &right, 1);
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
                    cursor end_cursor = add_to_cursor_by_value(
                        list,
                        start_cursor,
                        state->ins_count);

                    list_piece *lp = 0;
                    if (state->del_count)
                    {
                        lp = list_first_entry(&state->piece_head, list_piece, list);
                    }

                    // NOTE:This is a hack!!
                    u32 prev_size = list->size;
                    u32 prev_lcnt = list->lcnt;
                    replace(
                        list, start_cursor, end_cursor, &lp->piece, state->del_count);
                    list->size = prev_size - del_size;
                    list->lcnt = prev_lcnt - del_lcnt;
                    clear_insert_state(state);
                }
                else
                {
                    cursor end_cursor = add_to_cursor_by_value(list, start_cursor, 1);
                    replace(list, start_cursor, end_cursor, 0, 0);
                    list->size -= del_size;
                    list->lcnt -= del_lcnt;
                    state->ins_count--;
                }
            }
            else
            {
                piece *curr_piece = get_piece(&iter);
                Assert(curr_piece->size > 1);

                curr_piece->size -= del_size;

                node->size -= del_size;
                node->lcnt -= del_lcnt;

                list->size -= del_size;
                list->lcnt -= del_lcnt;
            }
            list->append.text_len -= del_size;
            list->append.num_lines -= del_lcnt;
        } break;

        case Deleted:
        {
            state->position = position(&iter);

            piece *curr_piece = get_piece(&iter);

            cursor start_cursor = { node, iter.piece_idx };
            cursor end_cursor   = { last_iter.node, last_iter.piece_idx };

            if (iter.pos_in_piece == 0)
            {
                if (iter.abs_idx < state->abs_idx)
                {
                    list_piece *lp = PushStruct(
                        &state->insert_mode_arena,
                        list_piece,
                        NoClear());
                    lp->piece = *curr_piece;
                    INIT_LIST_HEAD(&lp->list);
                    list_add(&lp->list, &state->piece_head);
                    state->del_count++;

                    if (last_iter.pos_in_piece == 0) 
                    {
                        replace(list, start_cursor, end_cursor, 0, 0);
                    } 
                    else
                    {
                        curr_piece->size -= del_size;
                        curr_piece->lcnt -= del_lcnt;
                        node->size -= del_size;
                        node->lcnt -= del_lcnt;
                    }
                    list->size -= del_size;
                    list->lcnt -= del_lcnt;
                    state->abs_idx = iter.abs_idx;
                }
                else
                {
                    replace(list, start_cursor, end_cursor, 0, 0);
                    state->ins_count--;
                    list->size -= del_size;
                    list->lcnt -= del_lcnt;
                }
            }
            else if (iter.pos_in_piece == curr_piece->size)
            {
                add_to_cursor(list, &start_cursor, 1);
                if (iter.abs_idx < state->abs_idx)
                {
                    list_piece *lp = PushStruct(
                        &state->insert_mode_arena,
                        list_piece,
                        NoClear());
                    lp->piece = *curr_piece;
                    INIT_LIST_HEAD(&lp->list);
                    list_add(&lp->list, &state->piece_head);
                    state->del_count++;

                    if (last_iter.pos_in_piece == 0) 
                    {
                        replace(list, start_cursor, end_cursor, 0, 0);
                    } 
                    else
                    {
                        curr_piece->size -= del_size;
                        node->size -= del_size;
                    }
                    state->abs_idx = iter.abs_idx;
                }
                else
                {
                    replace(list, start_cursor, end_cursor, 0, 0);
                    state->ins_count--;
                }
            } 
            else
            {
                if (iter.abs_idx < state->abs_idx)
                {
                    state->abs_idx = iter.abs_idx;
                    list_piece *lp = PushStruct(
                        &state->insert_mode_arena,
                        list_piece,
                        NoClear());
                    lp->piece = *curr_piece;
                    INIT_LIST_HEAD(&lp->list);
                    list_add(&lp->list, &state->piece_head);
                    state->del_count++;
                    state->ins_count++;
                }

                curr_piece->size -= del_size;
                curr_piece->lcnt -= del_lcnt;
                node->size -= del_size;
                node->lcnt -= del_lcnt;
                list->size -= del_size;
                list->lcnt -= del_lcnt;

            }
        } break;
    }
    reset_cursor(list, &list->iter);

    list->changed = true;
    win->dc.x = win->bc.x - 1;
    win->bc.x = win->dc.x;
}

static b32 process_insert(editor_state *state, str s)
{
    b32 result = false;
    window *win = state->screen.active_window;
    switch (s.buffer[0])
    {
        case 'q':
        {
            result = true;
        } break;

        case 127:
        {
            if (win->bc.x != 0)
            {
                insert_mode_delete(win);
            }
        } break;

        case '\x1b':
        {
            into_normal_mode(state);
        } break;

        case '\r':
        {
            str new = STR_LIT("\n"); 
            insert_mode_insert(win, new); 
        } break;

        default:
        {
            insert_mode_insert(win, s); 
        } break;

    }
    return result;
}

