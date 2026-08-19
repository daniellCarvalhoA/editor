
// NOTE: This is very imcomplete and unoptimized
static buffer original_from_text(memory_arena *arena, u8 *text, u32 text_len)
{
    buffer buf = {};
    if (text)
    {
        buf.text_len = text_len;
        buf.text = text;
        buf.lines = PushStruct(arena, u32, NoClear());
        buf.lines[0] = 0;
        buf.num_lines = 1;

        for (u32 index = 0; index < buf.text_len; ++index)
        {
            if (buf.text[index] == '\n')
            {
                PushStruct(arena, u32, NoClear());
                buf.lines[buf.num_lines++] = index + 1;
            }
        }
    }
    return buf;
}

static inline piece original_piece(buffer *buffer)
{
    piece piece = {
        .lcnt = buffer->num_lines - 1,
        .size = buffer->text_len,
        .type = BufferType_Original 
    };
    return piece;
}

static buffer allocate_append_buffer(memory_arena *arena)
{
    buffer buffer;
    buffer.text_len = 0;
    buffer.text_capacity = Megabytes(1);
    buffer.text = PushArray(arena, buffer.text_capacity, u8, NoClear());

    buffer.num_lines = 1;
    buffer.lines_capacity = Kilobytes(128);
    buffer.lines = PushArray(
        arena,
        (buffer.lines_capacity) / sizeof(u32),
        u32,
        NoClear());
    buffer.lines[0] = 0;
    return buffer;
}

static inline buffer allocate_original_buffer(memory_arena *arena, u8 *original_text, u32 original_text_len)
{
    buffer buffer = original_from_text(arena, original_text, original_text_len);
    buffer.text_capacity = original_text_len;
    buffer.lines_capacity = buffer.num_lines;
    return buffer;
}

static inline piece make_piece(piece_list *list, str s)//  u8 *text, u32 text_len)
{
    u32 append_len    = list->append.text_len;
    u32 start_row_idx = list->append.num_lines - 1;
    u32 start_row     = list->append.lines[start_row_idx];
    u32 start_col     = list->append.text_len - start_row; 

    Assert(list->append.text_capacity > list->append.text_len + s.len);
    memcpy(list->append.text + list->append.text_len, s.buffer, s.len);
    list->append.text_len += s.len;

    for (u32 i = 0; i < s.len; i++)
    {
        if (s.buffer[i] == '\n')
        {
            Assert(list->append.lines_capacity > list->append.num_lines + 1);
            list->append.lines[list->append.num_lines++] = append_len + i + 1;
        }
    }

    u32 end_row_idx = list->append.num_lines - 1;
    piece new_piece;
    new_piece.size    = s.len;
    new_piece.lcnt    = end_row_idx - start_row_idx;
    new_piece.off.row = start_row_idx;
    new_piece.off.col = start_col;
    new_piece.type    = BufferType_Append;

    return new_piece;
}

static inline base_iter find_abs_idx(
    piece_list *list,
    base_iter *last_location,
    u32 abs_idx)
{
    base_iter iter = *last_location;
    u32 last_abs_idx = iter.abs_idx;

    if (abs_idx < last_abs_idx)
    {
        base_reverse_by(&iter, last_abs_idx - abs_idx);
    }
    else if (abs_idx > last_abs_idx)
    {
        base_advance_by(list, &iter, abs_idx - last_abs_idx);
    }
    else
    {
        iter.pos_in_piece = iter.line_in_piece = 0;
    }

    normalize(list, &iter);
    *last_location = iter;

    return iter;
}

static base_iter find_position(piece_list *list, base_iter *last_location, u32 position)  
{
    base_iter iter = *last_location;
    u32 last_position = get_position(list, &iter);
    iter.type = Position;

    if (position < last_position)
    {
        base_advance_pos_rev_by(&iter, last_position - position);
    }
    else if (position > last_position)
    {
        base_advance_pos_by(list, &iter, position - last_position);
    }

    normalize(list, &iter);
    *last_location = iter;

    return iter;
}

static u32 position_from_cursor(
    piece_list *list,
    base_iter *last_location,
    buffer_cursor bc) 
{
    base_iter iter = find_cursor(list, last_location, bc);
    u32 result = get_position(list, &iter);
    return result;
}

static buffer_cursor get_cursor(piece_list *list, base_iter *iter)
{
    buffer_cursor result = { .y = line_number(iter) };
    while (base_prev_cell(list, iter) && result.y == line_number(iter))
    {
        result.x++;
    }
    return result;
}

static buffer_cursor cursor_from_position(
    piece_list *list, 
    base_iter *last_location,
    u32 position)
{
    buffer_cursor result = {};
    base_iter iter = find_position(list, last_location, position);
    result.y = line_number(&iter);

    while (base_prev_cell(list, &iter) && result.y == line_number(&iter))
    {
        result.x++;
    }
    return result;
}

static inline base_iter find_line(
    piece_list *list, base_iter *last_location, u32 line)
{
    base_iter iter = *last_location;
    u32 last_line = get_line_number(list, &iter);
    iter.type = LineNumber;

    if (line <= last_line)
    {
        base_advance_rev_by_line(list, &iter, last_line - line);
    }
    else if (line > last_line)
    {
        base_advance_by_line(list, &iter, line - last_line);
    } 

    normalize(list, &iter);
    *last_location = iter;

    return iter;
}

static inline base_iter find_cursor(
    piece_list *list, base_iter *last_location, buffer_cursor cursor)
{
    base_iter iter;
    if (cursor.y > list->lcnt)
    {
        iter = find_position(list, last_location, list->size);
    }
    else
    {
        iter = find_line(list, last_location, cursor.y);
        u32 count = cursor.x;

        str s = { .buffer = (u8 *) "\n", .len = 1 };
        while ((count > 0) && base_next_pred(list, &iter, not_equal_to, s))
        {
            count--;
        }
    }
    return iter;
}

static u32 skip_space(piece_list *list, base_iter *last_location, u32 cy)
{
    u32 result = 0;
    base_iter iter = find_line(list, last_location, cy);
    str s = {};
    while (base_next_pred(list, &iter, is_white_space, s)) 
    {
        result++;
    }
    return result;
}

static u32 get_line_len(piece_list *list, base_iter *last_location, u32 line)
{
    base_iter iter = find_line(list, last_location, line);
    u32 len = 0;

    while (base_next_cell_(list, &iter) && line_number(&iter) == line)
    {
        len++;
    }

    return len;
}

static u32 find_char_back(
    piece_list *list,
    base_iter *last_location,
    str match,
    u32 count, 
    buffer_cursor cursor)
{
    u32 result = 0;
    base_iter iter = find_cursor(list, last_location, cursor);

    for (; count > 0;)
    {
        if (!base_prev_cell(list, &iter))
        {
            result = 0;
            break;
        }

        str s = get_char_utf8(list, &iter);
        if ((s.len == 1) && memcmp(s.buffer, "\n", 1) == 0)
        {
            result = 0;
        }

        result++;
        if ((match.len == s.len) && (memcmp(match.buffer, s.buffer, match.len) == 0))
        {
            count--;
        }
    }

    return cursor.x - result;
}

static iter_range range_search(
    piece_list *list,
    base_iter *last_location,
    buffer_cursor cursor,
    u8 open,
    u8 close)
{
    base_iter backward_iter = find_cursor(list, last_location, cursor);
    base_iter forward_iter  = backward_iter;

    // Search backwards,
    i32 back_score = 0;

    while ((back_score < 1) && base_prev_cell(list, &backward_iter))
    {
        str s = get_char_utf8(list, &backward_iter);
        Assert(s.len > 0);

        if (s.buffer[0] == open)
        {
            back_score++;
        }
        else if (s.buffer[0] == close)
        {
            back_score--;
        }
    }

    base_iter second_best_start = forward_iter;
    base_iter second_best_end   = forward_iter;

    b32 found_second_best = false;

    i32 second_best_score = 0;
    i32 forward_score = 0;

    do 
    {
        str s = get_char_utf8(list, &forward_iter);

        if (close == open)
        {
            if (s.buffer[0] == open)
            {
                if (back_score > 0)
                {
                    forward_score++;
                    break;
                }
                else if (found_second_best)
                {
                    second_best_end = forward_iter;
                    break;
                }
                else
                {
                    found_second_best = true;
                    second_best_start = forward_iter;
                }
            }
        }
        else if (s.buffer[0] == close)
        {
            forward_score++;
            if (found_second_best)
            {
                second_best_score++;
                if (second_best_score == 0)
                {
                    second_best_end = forward_iter;
                    if (back_score < 1)
                    {
                        break;
                    }
                }
            }
        }
        else if (s.buffer[0] == open)
        {
            forward_score--;
            if (!found_second_best)
            {
                found_second_best = true;
                second_best_start = forward_iter;
            }
            second_best_score--;
        }
    } while ((forward_score < 1) && (base_next_cell_(list, &forward_iter)));

    iter_range result = { *last_location, *last_location }; 
    if ((back_score == 1) && (forward_score == 1))
    {
        result.start = backward_iter;
        result.end   = forward_iter;
    }
    else if (found_second_best && (second_best_score == 0))
    {
        result.start = second_best_start;
        result.end = second_best_end;
    }
    return result;


}

static buffer_range find_boundary(
    piece_list *list,
    base_iter *last_location,
    u8 open,
    u8 close,
    buffer_cursor cursor)
{
    base_iter copy_iter = find_cursor(list, last_location, cursor);
    base_iter iter = copy_iter;


    buffer_cursor back = cursor;

    // Find back cursor, 

    i32 back_score = 0;

    while (back_score < 1)
    {
        if (!base_prev_cell(list, &iter))
        {
            break;
        }

        str s = get_char_utf8(list, &iter);

        if (s.buffer[0] == open)
        {
            back_score++;
            back.x--;
        } 
        else if (s.buffer[0] == close)
        {
            back_score--;
            back.x--;
        }
        else if ((s.len == 1) && memcmp(s.buffer, "\n", 1) == 0)
        {
            back.y--;
            base_iter copy_iter = *last_location;
            back.x = get_line_len(list, &copy_iter, back.y);
        }
        else
        {
            back.x--;
        }
    }

    iter = copy_iter;

    buffer_cursor second_best_first = cursor;
    buffer_cursor second_best_last = cursor;

    b32 found_second_best = false; 

    i32 second_best_score = 0;
    // b32 found_second_best

    buffer_cursor forward = cursor;
    i32 forward_score = 0;

    while (forward_score < 1)
    {
        str s = get_char_utf8(list, &iter);
        if (!base_next_cell_(list, &iter))
        {
            break;
        }


        if (s.buffer[0] == close)
        {
            forward_score++;
            if (found_second_best)
            {
                second_best_score++;
                if (second_best_score == 0)
                {
                    second_best_last = forward;
                    if (back_score < 1)
                    {
                        break;
                    }
                }
            }
            forward.x++;
        }
        else if (s.buffer[0] == open)
        {
            forward_score--;
            if (!found_second_best)
            {
                found_second_best = true;
                second_best_first = forward;
            }
            second_best_score--;
            forward.x++;
        }
        else if ((s.len == 1) && memcmp(s.buffer, "\n", 1) == 0)
        {
            forward.y++;
            forward.x = 0;
        }
        else
        {
            forward.x++;
        }
    }

    buffer_range result = { cursor, cursor };

    if ((back_score == 1) && (forward_score == 1))
    {
        result.first = back;
        result.one_past_end = forward;
        result.one_past_end.x--;
    }
    else if (found_second_best && second_best_score == 0)
    {
        result.first = second_best_first;
        result.one_past_end = second_best_last;
    }


    return result;
}

static u32 find_char(
    piece_list *list,
    base_iter *last_location,
    str match,
    u32 count,
    buffer_cursor cursor)
{
    u32 result = 0;
    base_iter iter = find_cursor(list, last_location, cursor);

    for (; count > 0;)
    {
        if (!base_next_cell_(list, &iter))
        {
            result = 0;
            break;
        }

        str s = get_char_utf8(list, &iter);

        if ((s.len == 1) && (memcmp(s.buffer, "\n", 1) == 0))
        {
            result = 0;
            break;
        }

        result++;
        if ((match.len == s.len) && 
            (memcmp(match.buffer, s.buffer, match.len) == 0))
        {
            count--;
        }
    }
    return result + cursor.x;
}

static buffer_cursor find_word_back(
    piece_list *list,
    base_iter *last_location,
    u32 count,
    buffer_cursor cursor)
{
    buffer_cursor result = cursor;
    base_iter iter = find_cursor(list, last_location, cursor);

    b32 normal_exit = false;
    for (; count > 0; )
    {
        // skip non_space;
        while (base_prev_cell(list, &iter))
        {
            str s = get_char_utf8(list, &iter);
            Assert(s.len);
            // NOTE: IMCOMPLETE/WRONG
            if (!(s.buffer[0] == ' ' || s.buffer[0] == '\t' || s.buffer[0] == '\n'))
            {
                normal_exit |= true;
                break;
            }
        }
        // skip space
        while (base_prev_cell(list, &iter))
        {
            normal_exit = false;
            str s = get_char_utf8(list, &iter);
            Assert(s.len);
            if (s.buffer[0] == ' ' || s.buffer[0] == '\t' || s.buffer[0] == '\n')
            {
                normal_exit |= true;
                break;
            }
        }

        count--;
    }

    if (normal_exit)
    {
        b32 valid = base_next_cell_(list, &iter);
        Assert(valid);
    }
    result = get_cursor(list, &iter);
    return result;
}

static buffer_cursor find_word(
    piece_list *list, 
    base_iter *last_location,
    u32 count,
    buffer_cursor cursor)
{
    buffer_cursor result = cursor;
    base_iter iter = find_cursor(list, last_location, cursor);

    for(; count > 0;)
    {
        // skip non_space
        do 
        {
            str s = get_char_utf8(list, &iter);
            Assert(s.len);
            result.x++;
            // NOTE: IMCOMPLETE/WRONG
            if (s.buffer[0] == ' ' || s.buffer[0] == '\t' || s.buffer[0] == '\n')
            {
                if (s.buffer[0] == '\n')
                {
                    result.x = 0;
                    result.y++;
                }
                // else
                // {
                //     result.x++;
                // }
                break;
            }
        } while (base_next_cell_(list, &iter));
        // skip space;
        while (base_next_cell_(list, &iter))
        {
            str s = get_char_utf8(list, &iter);
            Assert(s.len);
            // NOTE: IMCOMPLETE/WRONG
            if (!(s.buffer[0] == ' ' || s.buffer[0] == '\t' || s.buffer[0] == '\n'))
            {
                break;
            }
            if (s.buffer[0] == '\n')
            {
                result.x = 0;
                result.y++;
            }
            else
            {
                result.x++;
            }
        }

        count--;
    }
    return result;
}


static inline void maybe_merge_with_next(piece_list *list, segmented_node *node)
{
    segmented_node *next_node = node->next;
    if (next_node != &list->root_sentinel)
    {
        piece *src_pieces = next_node->pieces;
        piece *dst_pieces = node->pieces + node->count;

        if (node->count + next_node->count <= MAX_PIECES_PER_NODE)
        {
            memcpy(dst_pieces, src_pieces, sizeof(piece) * next_node->count);

            for (u32 index = 0; index < next_node->count; ++index)
            {
                piece piece = next_node->pieces[index];
                node->size += piece.size;
                node->lcnt += piece.lcnt;
            }

            node->count += next_node->count;

            DLIST_REMOVE(next_node);
            LIST_INSERT(list->first_free_node, next_node);
        }
        else
        {
            u32 right_to_left_transfer_count = next_node->count - (node->count + next_node->count) / 2;

            memcpy(dst_pieces, src_pieces, sizeof(piece) * right_to_left_transfer_count);

            for (u32 index = 0; index < right_to_left_transfer_count; ++index)
            {
                piece piece = next_node->pieces[index];
                node->size += piece.size;
                node->lcnt += piece.lcnt;
                next_node->size -= piece.size;
                next_node->lcnt -= piece.lcnt;
            }

            node->count += right_to_left_transfer_count;
            next_node->count -= right_to_left_transfer_count;

            src_pieces = next_node->pieces + right_to_left_transfer_count;
            dst_pieces = next_node->pieces;
            memmove(dst_pieces, src_pieces, sizeof(piece) * next_node->count);
        }
    }
}

static inline void append(piece_list  *list, piece *pieces, u32 num_pieces)
{
    u32 const num_alloc = 1 + (num_pieces / (MAX_PIECES_PER_NODE - 1));
    u32 const pieces_per_node = num_pieces / num_alloc;
    u32 rem = num_pieces % num_alloc;
    u32 at = 0;

    for (u32 i = 0; i < num_alloc; ++i)
    {
        segmented_node *new_node = allocate_node(list);

        u32 pieces_per_this_node = pieces_per_node + (rem > 0);

        memcpy(new_node->pieces,  pieces + at, sizeof(piece) * pieces_per_this_node);

        new_node->count = pieces_per_this_node;

        for (u32 j = 0; j < new_node->count; ++j)
        {
            piece piece = new_node->pieces[j];
            new_node->size += piece.size;
            new_node->lcnt += piece.lcnt;
        }
        rem = (rem > 0) ? (rem - 1) : 0;
        at += pieces_per_this_node;

        DLIST_INSERT_TAIL(&list->root_sentinel, new_node);
    }
}

static inline void copy_serialized(
    piece_list *list,
    cursor start,
    u32 start_offset,
    cursor end, 
    u32 end_offset,
    string text)
{
    if (text.len > 0)
    {
        u32 cursor = 0;
        if (start.node == end.node)
        {
            for (u32 i = start.piece_index; i < end.piece_index; ++i)
            {
                piece *piece = start.node->pieces + i;
                const buffer *buffer = get_buffer(list, piece->type);
                u8 *src_text = buffer->text + 
                               buffer->lines[piece->off.row] +
                               piece->off.col;

                u32 src_len = piece->size;
                if (i == start.piece_index && start_offset)
                {
                    src_text += start_offset;
                    src_len  -= start_offset;
                }

                if (i == end.piece_index - 1 && end_offset)
                {
                    src_len -= piece->size - end_offset;

                }

                Assert(cursor + src_len <= text.len);
                memcpy(text.buffer + cursor, src_text, src_len);

                cursor += src_len;
            }
        }
        else
        {
            for (u32 i = start.piece_index; i < start.node->count; ++i)
            {
                piece *piece = start.node->pieces + i;
                const buffer *buffer = get_buffer(list, piece->type);
                u8 *src_text = buffer->text + 
                               buffer->lines[piece->off.row] +
                               piece->off.col;

                u32 src_len = piece->size;
                if (i == start.piece_index && start_offset)
                {
                    src_text += start_offset;
                    src_len  -= start_offset;
                }

                Assert(cursor + src_len <= text.len);
                memcpy(text.buffer + cursor, src_text, src_len);

                cursor += src_len;
            }

            for (segmented_node *node = start.node->next;
                node != end.node;
                node = node->next)
            {
                for (u32 i = 0; i < node->count; ++ i)
                {
                    piece *piece = node->pieces + i;
                    const buffer *buffer = get_buffer(list, piece->type);
                    u8 *src_text = buffer->text + 
                                   buffer->lines[piece->off.row] +
                                   piece->off.col;

                    Assert(cursor + piece->size <= text.len);
                    memcpy(text.buffer + cursor, src_text, piece->size);

                    cursor += piece->size;
                }
            }

            for (u32 i = 0; i < end.piece_index; ++i)
            {
                piece *piece = end.node->pieces + i;
                const buffer *buffer = get_buffer(list, piece->type);
                u8 *src_text = buffer->text + 
                               buffer->lines[piece->off.row] +
                               piece->off.col;

                u32 src_len = piece->size;
                if (i == end.piece_index - 1 && end_offset)
                {
                    src_len -= piece->size - end_offset;
                }

                Assert(cursor + src_len <= text.len);
                memcpy(text.buffer + cursor, src_text, src_len);

                cursor += src_len;
            }
        }
    }
}

static void copy_range(cursor start, cursor end, u32 count, piece_slice slice)
{
    if (slice.count > 0)
    {
        piece *piece_start = start.node->pieces + start.piece_index;
        u32 cursor = 0;

        if (start.node == end.node)
        {
            Assert(count == (end.piece_index - start.piece_index));
            memcpy(slice.base, piece_start, count * sizeof(piece));
        }
        else
        {
            u32 len = start.node->count - start.piece_index;

            memcpy(slice.base, piece_start, len * sizeof(piece));
            cursor += len;

            for (segmented_node *node = start.node->next;
                node != end.node;
                node = node->next)
            {
                memcpy(slice.base + cursor, node->pieces, node->count * sizeof(piece));
                cursor += node->count;

            }
            memcpy(slice.base + cursor, end.node->pieces, end.piece_index * sizeof(piece));
        }
    }
}

static cursor make_space(piece_list *list, cursor start, cursor end, u32 size)
{
    cursor result;

    if (start.node == &list->root_sentinel)
    {
        Assert(start.piece_index == 0);
        if (size == 0)
        {
            return start;
        }
        Assert(size > 0);

        segmented_node *new_node = allocate_node(list);
        DLIST_INSERT_TAIL(&list->root_sentinel, new_node);
        start.node = new_node;
        result = start;
    }

    if (end.node == &list->root_sentinel)
    {
        end.node = list->root_sentinel.prev;
        end.piece_index = end.node->count;
    }

    if (start.node == end.node)
    {
        segmented_node *node = start.node;
        u32 distance = end.piece_index - start.piece_index;
        u32 total    = size + node->count - distance;

        if (total <= MAX_PIECES_PER_NODE)
        {
            for (u32 i = start.piece_index; i < end.piece_index; ++i)
            {
                piece piece = node->pieces[i];
                node->size -= piece.size;
                node->lcnt -= piece.lcnt;
            }

            piece *src_pieces = node->pieces + end.piece_index;
            piece *dst_pieces = node->pieces + start.piece_index + size;

            u32 num_moved = node->count - end.piece_index;

            if (start.piece_index + size < total)
            {
                memmove(dst_pieces, src_pieces, sizeof(piece) * num_moved);
            }

            src_pieces = node->pieces + start.piece_index;

            node->count = total;

            if (total == 0)
            {
                DLIST_REMOVE(node);
                LIST_INSERT(list->first_free_node, node);
            } 
            else if (total < MAX_PIECES_PER_NODE / 2)
            {
                maybe_merge_with_next(list, node);
            }
            result = start;
        }
        else
        {
            segmented_node *node = start.node;
            u32 num_nodes = 1 + ((total - 1) / MAX_PIECES_PER_NODE);
            u32 num_alloc = num_nodes - 1;
            u32 pieces_per_node = total / num_nodes;
            u32 rem = total % num_nodes;

            u32 left_len = start.piece_index;

            piece *left_pieces = node->pieces;

            u32 delete_end = start.piece_index + distance;
            u32 right_len  = node->count - delete_end;

            piece *right_pieces = node->pieces + delete_end;

            u32 right_count  = 0; // pieces copied so far from right part of split to newly allocated nodes
            u32 pieces_count = 0; // pieces copied so far from *pieces* to newly allocated nodes

            Assert(num_alloc > 0);
            for (u32 i = 0; i < num_alloc; ++i)
            {
                segmented_node *new_node = allocate_node(list);
                // Copy pieces from the right part of the split to the newly allocated node
                u32 pieces_per_this_node = (rem > 0) ? pieces_per_node + 1 : pieces_per_node;
                u32 right_remaining      = (right_count > right_len) ? 0 : right_len - right_count;
                u32 right_to_new_count   = Minimum(pieces_per_this_node, right_remaining);
                u32 right_start          = pieces_per_this_node - right_to_new_count;
                u32 right_end            = right_len - right_count;

                piece *dst_pieces = new_node->pieces + right_start;
                piece *src_pieces = right_pieces + right_end - right_to_new_count;

                memcpy(dst_pieces, src_pieces, sizeof(piece) * right_to_new_count);

                for (u32 j = 0; j < right_to_new_count; ++j) 
                {
                    piece piece = src_pieces[j];
                    new_node->size += piece.size;
                    new_node->lcnt += piece.lcnt;
                    node->size     -= piece.size;
                    node->lcnt     -= piece.lcnt;
                }


                right_count += right_to_new_count;

                u32 pieces_end          = size - pieces_count;
                u32 pieces_to_new_count = Minimum(pieces_end, right_start);
                u32 pieces_start        = right_start - pieces_to_new_count;

                pieces_count += pieces_to_new_count;

                u32 from_left = left_len - pieces_start;
                dst_pieces = new_node->pieces;
                src_pieces = left_pieces + from_left;

                memcpy(dst_pieces, src_pieces, sizeof(piece) * pieces_start);

                for (u32 j = 0; j < pieces_start; ++j) 
                {
                    piece piece = src_pieces[j];
                    new_node->size += piece.size;
                    new_node->lcnt += piece.lcnt;
                    node->size     -= piece.size;
                    node->lcnt     -= piece.lcnt;
                }

                new_node->count = pieces_per_this_node;
                rem = (rem > 0) ? (rem - 1) : 0;
                DLIST_INSERT_AFTER(node, new_node);
            }

            for (u32 j = 0; j < distance; ++j)
            {
                piece piece = node->pieces[start.piece_index + j];
                node->size -= piece.size;
                node->lcnt -= piece.lcnt;

            }
            u32 pieces_remaining = size - pieces_count;

            piece *src_pieces = node->pieces + delete_end;
            piece *dst_pieces = node->pieces + start.piece_index + pieces_remaining;

            u32 count = (node->count - right_count) - delete_end;

            memmove(dst_pieces, src_pieces, sizeof(piece) * count);
            
            node->count = pieces_per_node;

            if (start.piece_index >= node->count)
            {
                start.piece_index = start.piece_index - node->count;
                start.node = start.node->next;
            }

            result = start;
        }
    }
    else
    {
        u32 right_len = end.node->count - end.piece_index;
        u32 total = size + start.piece_index + right_len;
        segmented_node *left = start.node;
        segmented_node *right = end.node;

        if (left->next != right)
        {
            segmented_node *first_freed = left->next;
            segmented_node *last_freed = right->prev;

            first_freed->prev = 0;
            last_freed->next = 0;

            left->next = right;
            right->prev = left;

            last_freed->next = list->first_free_node;
            list->first_free_node = first_freed;
        }

        if (total <= MAX_PIECES_PER_NODE)
        {
            Assert(right == left->next);

            piece *dst_pieces = left->pieces + start.piece_index + size;

            for (u32 j = start.piece_index; j < left->count; ++j)
            {
                piece piece = left->pieces[j];
                left->size -= piece.size;
                left->lcnt -= piece.lcnt;
            }

            memcpy(dst_pieces, right->pieces + end.piece_index, sizeof(piece) * right_len);

            for (u32 j = 0; j < right_len; ++j)
            {
                piece piece = dst_pieces[j];
                left->size += piece.size;
                left->lcnt += piece.lcnt;
            }

            left->count = total;

            // free deleted nodes
            segmented_node *after = left->next;

            left->next = right->next;
            right->next->prev = left;

            right->next = list->first_free_node;
            list->first_free_node = after;

            if (total == 0)
            {
                DLIST_REMOVE(left);
                LIST_INSERT(list->first_free_node, left);
            } 
            else if (total < MAX_PIECES_PER_NODE / 2)
            {
                maybe_merge_with_next(list, left);
            }

            result = start;
        }
        else
        {
            // Number of nodes that will be updated (some of which might need to be allocated)
            u32 const num_used_nodes = 1 + (total - 1) / MAX_PIECES_PER_NODE;
            // Number off allocated nodes
            u32 num_alloc = (num_used_nodes > 2) ? (num_used_nodes - 2) : 0;

            u32 const pieces_per_node = total / num_used_nodes;
            u32 rem = total % num_used_nodes;
            u32 pieces_cursor = size;

            // TODO: Try to chooses node counts such that we minimize the amount of copyining.
            u32 const left_pieces_per_node = pieces_per_node + (rem > 0);
            rem = (rem > 0) ? (rem - 1) : 0;
            u32 const right_pieces_per_node = pieces_per_node + (rem > 0);
            rem = (rem > 0) ? (rem - 1) : 0;

            // Number of pieces that must be moved from *left* to *new_nodes* and/or *right*.
            u32 left_to_right = (start.piece_index > left_pieces_per_node) ?
                                (start.piece_index - left_pieces_per_node) : 0;
            // Number of pieces that must be moved from *pieces* and/or *right* to *left*.
            u32 left_to_right_rev = (left_pieces_per_node > start.piece_index) ?
                                    (left_pieces_per_node - start.piece_index) : 0;
            // Number of pieces that must be moved from *right* to *new_nodes* and/or *left*.
            u32 right_to_left = (right_len > right_pieces_per_node) ?
                                (right_len - right_pieces_per_node) : 0;
            // Number of pieces that must be moved from *pieces* and/or *left* to *right*.
            u32 right_to_left_rev = (right_pieces_per_node > right_len) ?
                                    (right_pieces_per_node - right_len) : 0;

            for (u32 j = start.piece_index; j < left->count; ++j)
            {
                piece piece = left->pieces[j];
                left->size -= piece.size;
                left->lcnt -= piece.lcnt;
            }

            for (u32 j = 0; j < end.piece_index; ++j)
            {
                piece piece = right->pieces[j];
                right->size -= piece.size;
                right->lcnt -= piece.lcnt;
            }

            // Within the loop we only move pieces from:
            //  - *right* to *new_nodes*;
            //  - *pieces* to *right*; -> this should only happen in the first iteration
            //  - *left* to *new_nodes*;
            //  - *pieces* to *left* -> this should only happen in the last iteration
            //  - pieces to new_node;
            //
            // Note that these are mutually exlusive; if we move *right* to *new_nodes*,
            // we won't move *pieces* to *right*.
            // Only after the loop, if *left_to_right* is non-zero or *right_to_left* is
            // non-zero, do we move pieces from *left* to *right* or vice-versa.
            //
            // Note that if we enter the loop there won't be a need to copy from
            // *right* to *left* and vice-versa.

            segmented_node *new_node;
            for (u32 i = 0; i < num_alloc; ++i)
            {
                FREELIST_ALLOCATE(
                    new_node,
                    list->first_free_node,
                    PushStruct(&list->list_arena, segmented_node, NoClear()));
                new_node->size = 0;
                new_node->lcnt = 0;

                u32 pieces_per_this_node = pieces_per_node + (rem > 0);
                u32 node_cursor = pieces_per_this_node - right_to_left;

                for (u32 j = 0; j < right_to_left; ++ j)
                {
                    piece piece = right->pieces[end.piece_index + j];
                    right->size -= piece.size;
                    right->lcnt -= piece.lcnt;
                    new_node->size += piece.size;
                    new_node->lcnt += piece.lcnt;
                }

                memcpy(new_node->pieces + node_cursor,
                       right->pieces + end.piece_index,
                       sizeof(piece) * right_to_left);

                // Adjust right:
                // Note this must work when:
                //  - (1) we move from *right* to *node*;
                //        -- In this case we must move pieces within *right* to the
                //           start of the slice (left_to_right == 0).
                //  - (2) we move from *pieces* to *right*;
                //        -- In this case we must moves pieces within *right* such that
                //           we leave room for pieces from *pieces*. (right_to_left == 0).
                //  - (3) neither.
                //        -- In this cast we do nothing.

                memmove(right->pieces + right_to_left_rev,
                        right->pieces + end.piece_index + right_to_left,
                        sizeof(piece) * (right_len - right_to_left));

                pieces_cursor -= right_to_left_rev;

                // Copy from *pieces* to *new_node*
                u32 const rest_pieces_node = Minimum(pieces_cursor, node_cursor);

                node_cursor -= rest_pieces_node;
                pieces_cursor -= rest_pieces_node;

                u32 const left_to_node = Minimum(left_to_right, node_cursor);

                for (u32 j = 0; j < left_to_node; ++ j)
                {
                    piece piece = left->pieces[start.piece_index - left_to_node + j];
                    left->size -= piece.size;
                    left->lcnt -= piece.lcnt;
                    new_node->size += piece.size;
                    new_node->lcnt += piece.lcnt;
                }

                memcpy(new_node->pieces,
                       left->pieces + start.piece_index - left_to_node,
                       sizeof(piece) * left_to_node);

                Assert(pieces_cursor >= left_to_right_rev);

                // Copy from *pieces* to *left*
                u32 const pieces_to_left = (pieces_cursor == left_to_right_rev) * left_to_right_rev;

                new_node->count = pieces_per_this_node;

                right_to_left      = 0;
                right_to_left_rev  = 0;
                left_to_right     -= left_to_node;
                left_to_right_rev -= pieces_to_left;

                rem = (rem > 0) ? (rem - 1) :0;
                end.piece_index = 0;

                DLIST_INSERT_AFTER(left, new_node);
            }

            left_to_right_rev = Minimum(left_to_right_rev, size);
            right_to_left_rev = Minimum(right_to_left_rev, size);

            memcpy(left->pieces + start.piece_index + left_to_right_rev,
                   right->pieces + end.piece_index,
                   sizeof(piece) * right_to_left);

            for (u32 j = 0; j < right_to_left; ++j)
            {
                piece piece = left->pieces[start.piece_index + left_to_right_rev + j];
                left->size += piece.size;
                left->lcnt += piece.lcnt;
                right->size -= piece.size;
                right->lcnt -= piece.lcnt;
            }

            memmove(right->pieces + left_to_right + right_to_left_rev,
                    right->pieces + end.piece_index + right_to_left,
                    sizeof(piece) * (right_len - right_to_left));

            u32 copy_offset = start.piece_index - left_to_right;

            memcpy(right->pieces, left->pieces + copy_offset, sizeof(piece) * left_to_right);

            for (u32 j = 0; j < left_to_right; ++j)
            {
                piece piece = right->pieces[j];
                right->size += piece.size;
                right->lcnt += piece.lcnt;
                left->size  -= piece.size;
                left->lcnt  -= piece.lcnt;
            }

            left->count  = left_pieces_per_node;
            right->count = right_pieces_per_node;

            if (start.piece_index >= left->count)
            {
                start.piece_index = start.piece_index - left->count;
                start.node = start.node->next;
            }

            result = start;
        }
    }

    return result;
}

static void insert_many(
    piece_list *list,
    cursor at,
    piece_slice *slices,
    u32 num_slices)
{
    for (u32 i = 0; i < num_slices; ++i)
    {
        piece_slice slice = slices[i];
        u32 slice_count  = slice.count;
        u32 slice_cursor = 0;

        while (slice_cursor < slice_count)
        {
            const piece *const src_pieces = slice.base + slice_cursor;
            piece *dst_pieces = at.node->pieces + at.piece_index;

            u32 remaining = slice_count - slice_cursor;
            u32 copy_amount = Minimum(remaining, (at.node->count - at.piece_index));

            memcpy(dst_pieces, src_pieces, sizeof(piece) * copy_amount);

            for (u32 j = 0; j < copy_amount; ++j)
            {
                piece piece = src_pieces[j];
                at.node->size += piece.size;
                at.node->lcnt += piece.lcnt;
                list->size += piece.size;
                list->lcnt += piece.lcnt;
                
            }

            if (at.node->count == at.piece_index + copy_amount)
            {
                at.node = at.node->next;
                at.piece_index = 0;
            }
            else
            {
                at.piece_index += copy_amount;
            }
            slice_cursor += copy_amount;
        }
    }
}

static void replace(piece_list *list, cursor start, cursor end, piece *pieces, u32 num_pieces)
{
    cursor cursor = make_space(list, start, end, num_pieces);
    piece_slice p_slice[1] = { { .base = pieces, .count = num_pieces } };
    insert_many(list, cursor, p_slice, ArrayCount(p_slice));
}

static void write_piece_text(piece_list *list, piece_slice p_slice, string *buf)
{
    Assert(buf->len == 0);
    for (u32 i = 0; i < p_slice.count; ++i)
    {
        const piece *piece = p_slice.base + i;
        const buffer *buffer = get_buffer(list, piece->type);

        u8 *src_text = buffer->text + buffer->lines[piece->off.row] + piece->off.col;
        str src = { .buffer = src_text, .len = piece->size };
        push_string(buf, src);
    }
}

static piece serialize_piece_range_to(piece_list *a, piece_list *b, piece_slice p_slice)
{
    piece result = {};
    result.type = BufferType_Append;
    result.off.row = b->append.num_lines - 1;
    result.off.col = b->append.text_len - b->append.lines[result.off.row];

    for (u32 i = 0; i < p_slice.count; ++i)
    {
        const piece *a_piece = p_slice.base + i;
        const buffer *buffer = get_buffer(a, a_piece->type);

        result.size += a_piece->size;
        result.lcnt += a_piece->lcnt;

        u8 *src_text = buffer->text + buffer->lines[a_piece->off.row] + a_piece->off.col;
        u8 *dst_text = b->append.text + b->append.text_len;
        memcpy(dst_text, src_text, a_piece->size);
        b->append.text_len += a_piece->size;

        u32 *src_lines = buffer->lines + a_piece->off.row;
        u32 *dst_lines = a->append.lines + a->append.num_lines - 1;
        memcpy(dst_lines, src_lines, a_piece->lcnt);

        for (u32 i = 0; i < a_piece->lcnt; ++i)
        {
            dst_lines[i] = dst_lines[i] - buffer->num_lines + b->append.num_lines;
        }
        b->append.num_lines += a_piece->lcnt;
    }
    return result;
}

static void yank(piece_list *list, p_buffer *buffer, buffer_cursor c0, buffer_cursor c1)
{
    base_iter start = find_cursor(list, &list->iter, c0);
    fix_iter(list, &start);
    base_iter end   = find_cursor(list, &list->iter, c1);
    fix_iter(list, &end);

    u32 not_boundary_end = end.pos_in_piece > 0;
    u32 num_pieces = (end.abs_idx + not_boundary_end) - start.abs_idx;

    u32 deleted_size = position(&end) - position(&start);

    cursor start_cursor = { start.node, start.piece_idx };
    cursor end_cursor   = { end.node, end.piece_idx };

    if (not_boundary_end)
    {
        next_cursor(&end_cursor);
    }

    if (deleted_size + sizeof(piece) > num_pieces * sizeof(piece))
    {

        u32 prev_size = get_allocation_size_in_bytes(buffer);
        u32 new_size  = num_pieces * sizeof(piece);

        buffer->type = BufferType_Pieces;
        buffer->count = num_pieces;
        buffer->buffer = list;

        if (prev_size < new_size)
        {
            buffer->capacity = buffer->count;
            buffer->pieces = reallocarray(buffer->pieces, buffer->capacity, sizeof(piece));
        }

        piece_slice p_slice =  { .base = buffer->pieces, .count = buffer->count } ;
        copy_range(start_cursor, end_cursor, buffer->count, p_slice);

        if (end.pos_in_piece > 0)
        {
            piece *piece = buffer->pieces + buffer->count - 1;
            piece->size = end.pos_in_piece;
            piece->lcnt = end.line_in_piece;;
        }

        if (start.pos_in_piece > 0)
        {
            piece *piece = buffer->pieces;
            piece->size -= start.pos_in_piece;
            piece->lcnt -= start.line_in_piece;
            piece->off = get_offset(list, &start);
        }

    }
    else
    {
        u32 prev_size = get_allocation_size_in_bytes(buffer);
        u32 new_size  = deleted_size;

        buffer->type = BufferType_AsStr;
        buffer->text.len = deleted_size;

        if (prev_size < new_size)
        {
            buffer->text.capacity = buffer->text.len;
            buffer->text.buffer = reallocarray(buffer->text.buffer, buffer->text.capacity, sizeof(u8));
        }
        
        copy_serialized(list, start_cursor, start.pos_in_piece, end_cursor, end.pos_in_piece, buffer->text);
    }

}

static void replace_range(piece_list *list, base_iter start, base_iter end, piece_slice inserted_pieces, piece_slice undo_buffer)

{
    cursor start_cursor = { start.node, start.piece_idx };
    cursor end_cursor   = { end.node, end.piece_idx };
    cursor cursor = end_cursor;
    u32 copy_amount = end.abs_idx - start.abs_idx;

    if (end.pos_in_piece > 0)
    {
        add_to_cursor_(&cursor, 1);
        copy_amount++;

    }
    copy_range(start_cursor, cursor, copy_amount, undo_buffer);

    piece_slice p_slice[2] = {};
    p_slice[0] = inserted_pieces;
    u32 slice_count = 1;

    piece *start_piece = get_piece(&start);
    Assert((!start_piece) ? (start.pos_in_piece == 0) : true);
    piece right;

    if (start.abs_idx == end.abs_idx && start.pos_in_piece > 0) 
    {
        offset offset = get_offset(list, &end);

        right.size = start_piece->size - end.pos_in_piece;
        right.lcnt = start_piece->lcnt - end.line_in_piece;
        right.off  = offset;
        right.type = start_piece->type;

        start_cursor.node->size -= start_piece->size - start.pos_in_piece;
        start_cursor.node->lcnt -= start_piece->lcnt - start.line_in_piece;

        list->size -= right.size;
        list->lcnt -= right.lcnt;

        start_piece->size = start.pos_in_piece;
        start_piece->lcnt = start.line_in_piece;

        add_to_cursor(list, &start_cursor, 1);

        start_cursor = make_space(
            list,
            start_cursor,
            start_cursor,
            inserted_pieces.count + 1);

        p_slice[slice_count].base  = &right;
        p_slice[slice_count].count = 1;
        slice_count++;
    }
    else
    {
        if (start.pos_in_piece > 0)
        {
            start_cursor.node->size -= start_piece->size - start.pos_in_piece;
            start_cursor.node->lcnt -= start_piece->lcnt - start.line_in_piece;

            start_piece->size = start.pos_in_piece;
            start_piece->lcnt = start.line_in_piece;

            add_to_cursor_(&start_cursor, 1);
            start.abs_idx++;
        }

        if (end.pos_in_piece > 0)
        {
            piece *end_piece = get_piece(&end);
            Assert(end_piece);

            end_cursor.node->size -= end.pos_in_piece;
            end_cursor.node->lcnt -= end.line_in_piece;

            end_piece->off = get_offset(list, &end);
            end_piece->size -= end.pos_in_piece;
            end_piece->lcnt -= end.line_in_piece;
        }

        start_cursor = make_space(list, start_cursor, end_cursor, inserted_pieces.count);
    }
    insert_many(list, start_cursor, p_slice, slice_count);
    reset_cursor(list, &list->iter);
}

static void range_replace(piece_list *list, buffer_cursor c0, buffer_cursor c1, piece_slice p_slice)
{
    list->changed = true;

    base_iter start = find_cursor(list, &list->iter, c0);
    fix_iter(list, &start);
    base_iter end   = find_cursor(list, &list->iter, c1);
    fix_iter(list, &end);

    u32 not_boundary_end   = end.pos_in_piece > 0;
    u32 not_boundary_start = start.pos_in_piece > 0;

    u32 num_undo_pieces = (end.abs_idx + not_boundary_end) - start.abs_idx;

    u32 num_lines_deleted = line_number(&end) - line_number(&start);
    u32 num_chars_deleted = position(&end)    - position(&start);

    list->size -= num_chars_deleted;
    list->lcnt -= num_lines_deleted;

    undo_record *record = allocate_undo_record(&list->undo_records, num_undo_pieces);
    piece_slice undo_buffer = {};

    if (record)
    {
        record->abs_idx   = start.abs_idx;
        record->ins_count = p_slice.count + not_boundary_start + not_boundary_end;
        record->del_count = num_undo_pieces;

        undo_buffer = get_pieces_from_record(record);
    }
    replace_range(list, start, end, p_slice, undo_buffer);
}

static void initialize_piece_list(piece_list *list, u8 *original_text, u32 original_text_len)
{
    DLIST_INIT(&list->root_sentinel);
    list->size        = 0;
    list->lcnt        = 0;

    list->append   = allocate_append_buffer(&list->list_arena);
    list->original = allocate_original_buffer(&list->list_arena, original_text, original_text_len);

    list->first_free_node = NULL;
    list->root_sentinel.size = 0;
    list->root_sentinel.lcnt = 0;
    list->root_sentinel.count = 0;

#if TESTS
    initialize_arena_with_size(&list->undo_arena, DEBUG_UNDO_RECORDS_SIZE);
    initialize_arena_with_size(&list->redo_arena, DEBUG_UNDO_RECORDS_SIZE);
#else
    initialize_arena_with_size(&list->undo_arena, UNDO_RECORDS_SIZE);
    initialize_arena_with_size(&list->redo_arena, UNDO_RECORDS_SIZE);
#endif
    list->undo_records = create_undo_records(&list->undo_arena);
    list->redo_records = create_undo_records(&list->redo_arena);
    initialize_arena(&list->insert_state_arena);
    initialize_insert_state(&list->i_state);

    if (list->original.text_len)
    {
        piece piece = original_piece(&list->original);
        segmented_node *node = 
            PushStruct(&list->list_arena, segmented_node, NoClear());
        *node->pieces  = piece;
        list->size = node->size = piece.size;
        list->lcnt = node->lcnt = piece.lcnt;
        node->count = 1;
        DLIST_INSERT_TAIL(&list->root_sentinel, node);
    }

    list->changed = true;
    list->match_len = 0;
    list->num_matches = 0;
    list->first_match_line = 0;
    list->last_match_line = 0;
    list->current_match = 0;
    list->matches_capacity = 0;
    list->matches = NULL;
    list->changed_since_last_search = true;
    list->last_searched_string.len = 0;
    list->last_searched_string.buffer = NULL;
    list->replaced = false;
    list->replace_len = 0;

    base_init(list, Position, &list->iter);
    INIT_LIST_HEAD(&list->window_sentinel);
}

static inline void init_buffer(piece_list *list, char *filepath)
{
    u8 *original_text = 0;
    u32 original_text_len = 0;
    list->filepath = filepath;
    if (list->filepath)
    {
        platform_file_handle handle = Platform.OpenFile(filepath);
        if (handle.no_errors)
        {
            original_text_len = handle.size;
            original_text = (u8 *) PushArray(&list->list_arena, handle.size, u8, NoClear());
            Platform.ReadDataFromFile(&handle, 0, handle.size, original_text);
            Platform.CloseFile(handle);
        }
        else
        {
            perror("open 1");
            abort();
        }
    }

    initialize_piece_list(list, original_text, original_text_len);
}

static inline void write_buffer_to_file(piece_list *list)
{
    if (list->filepath)
    {
        platform_file_handle handle = Platform.OpenFile(list->filepath);
        if (handle.no_errors)
        {
            // This call may not be available, maybe have a query call that asks the platform.
            Platform.AllocateDiskSpace(&handle, 0, list->size);

            for (segmented_node *node = list->root_sentinel.next;
                node != &list->root_sentinel;
                node = node->next)
            {
                platform_scatter_gather_vector iov[node->count];

                for (u32 i = 0; i < node->count; ++i)
                {
                    piece piece = node->pieces[i];

                    const buffer *buffer = get_buffer(list, piece.type);
                    u32 offset = buffer->lines[piece.off.row] + piece.off.col;
                    iov[i].base = (void *) (buffer->text + offset);
                    iov[i].size = piece.size;
                }
                Platform.WriteGather(&handle, iov, node->count);
            }
            Platform.CloseFile(handle);
        }
        else
        {
            perror("open");
            abort();
        }
    }
}

// NOTE: THIS IS VERY UNOPTIMISED
static inline void search_str(piece_list *list, str search_string)
{
    Assert(search_string.len > 0);
    u32 current_match_cursor = 0;
    u32 prev_position = 0;
    u32 match_position = 0;
    u32 current_position = 0;

    // u32 current_position = 0;
    b32 in_match = false;

    segmented_node *curr_node = list->root_sentinel.next;
    segmented_node *prev_node_match = curr_node;

    u32 prev_piece_match_index = 0;
    u32 prev_piece_match_start = 0;

    for(;;)
    {
        if (curr_node == &list->root_sentinel)
        {
            break;
        }


        u32 i = 0;

        while (i < curr_node->count)
        {
            u32 piece_cursor = 0;
start:
            piece *curr_piece = curr_node->pieces + i;
            const buffer *buffer = get_buffer(list, curr_piece->type);
            u32 offset = buffer->lines[curr_piece->off.row] + curr_piece->off.col;
            u8 *piece_text_start = buffer->text + offset;

            while (piece_cursor < curr_piece->size)
            {
                u8 *text = piece_text_start + piece_cursor;

                if (in_match)
                {
                    Assert(current_match_cursor > 0);
                    u32 remaining_text_size = search_string.len - current_match_cursor;
                    u32 test_size = Minimum(remaining_text_size, curr_piece->size - piece_cursor);

                    if (!memcmp(search_string.buffer + current_match_cursor, text, test_size))
                    {
                        // first test_size bytes matched
                        if (remaining_text_size <= curr_piece->size - piece_cursor)
                        {
                            // found match.
                            list->match_len = search_string.len;
                            if (list->matches_capacity <= list->num_matches)
                            {
                                list->matches_capacity = Maximum(8, list->matches_capacity * 2);
                                list->matches = realloc(list->matches, sizeof(u32) * list->matches_capacity);
                            }
                            list->matches[list->num_matches++] = match_position;
                            curr_node = prev_node_match;
                            i = prev_piece_match_index;
                            piece_cursor = prev_piece_match_start;
                            current_position = prev_position;
                            in_match = false;
                            current_match_cursor = 0;
                            goto start;
                        }
                        else
                        {
                            // must keep scanning
                            current_match_cursor += test_size;
                            piece_cursor += remaining_text_size;
                        }
                        // if i know the searched string
                    }
                    else
                    {
                        // No match. must return to previous match start;
                        curr_node = prev_node_match;
                        i = prev_piece_match_index;
                        piece_cursor = prev_piece_match_start;
                        current_position = prev_position;
                        in_match = false;
                        current_match_cursor = 0;
                        goto start;
                    }
                }
                else
                {
                    u8 *match_start = 
                        (u8 *) memchr(text, search_string.buffer[0], curr_piece->size - piece_cursor);
                    if (match_start)
                    {
                        piece_cursor += match_start - text;
                        match_position = current_position + piece_cursor;
                        current_match_cursor++;
                        in_match = true;
                        piece_cursor++;
                    }
                    else
                    {
                        piece_cursor = curr_piece->size;
                    }
                    prev_piece_match_index = i;
                    prev_piece_match_start = piece_cursor;
                    prev_node_match = curr_node;
                    prev_position = current_position;

                }
            }
            current_position += curr_piece->size;
            i++;
        }
        curr_node = curr_node->next;
    }

    if (in_match)
    {
        list->match_len = search_string.len;
        if (list->matches_capacity <= list->num_matches)
        {
            list->matches_capacity = Maximum(8, list->matches_capacity * 2);
            list->matches = realloc(list->matches, sizeof(u32) * list->matches_capacity);
        }
        list->matches[list->num_matches++] = match_position;
    }
}

static inline piece_list *create_buffer(memory_arena *arena, char *filepath)
{
    piece_list *buffer = PushStruct(arena, piece_list, NoClear());
    init_buffer(buffer, filepath);
    return buffer;
}

static inline void initialize_piece_list_s(piece_list *list, string original)
{
    initialize_piece_list(list, original.buffer, original.len);
}

static inline void write_to_buffer(piece_list *list, u8 *buf, u32 len)
{
    Assert(len >= list->size);
    u32 cursor = 0;
    for (segmented_node *node = list->root_sentinel.next;
        node != &list->root_sentinel;
        node = node->next)
    {
        for (u32 i = 0; i < node->count; ++i)
        {
            piece piece = node->pieces[i];
            const buffer *buffer = get_buffer(list, piece.type);
            u32 offset = buffer->lines[piece.off.row] + piece.off.col;
            memcpy(buf + cursor, (buffer->text + offset), piece.size);
            cursor += piece.size;
        }
    }
}

#if TESTS

static b32 freelist_sanity_check(piece_list *list)
{
    b32 result = true;

    if (list)
    {
        segmented_node *free = list->first_free_node;
        if (free)
        {
            temporary_memory tmp = begin_temporary_memory(&list->list_arena);

            u32 num_nodes = 1;

            segmented_node **first_node = PushStruct(&list->list_arena, segmented_node *, NoClear());
            *first_node = &list->root_sentinel;

            for (segmented_node *node = list->root_sentinel.next; 
                node != &list->root_sentinel;
                node = node->next, num_nodes++)
            {
                segmented_node **new_node = PushStruct(&list->list_arena, segmented_node *, NoClear());
                *new_node = node;
            }

            u32 not_found = true;
            for (segmented_node *node = list->first_free_node; node && not_found; node = node->next)
            {
                for (u32 i = 0; i < num_nodes; ++i)
                {
                    segmented_node *n = first_node[i];
                    if (n == node)
                    {
                        result = false;
                        not_found = false;
                        break;
                    }
                }
            }
            end_temporary_memory(tmp);
        }
    }

    return result;
}

static inline b32 equal_offsets(const offset a, const offset b)
{
    b32 result = (a.row == b.row) && (a.col == b.col);
    return result;
}

static inline b32 pieces_are_equal(const piece a, const piece b)
{
    b32 result = (a.size == b.size) && 
                 (a.lcnt == b.lcnt) && 
                 (equal_offsets(a.off, b.off)) &&
                 (a.type == b.type);
    return result;
}

static inline b32 nodes_are_equal(const segmented_node *a, const segmented_node *b)
{
    b32 result = (a->count == b->count) && (a->size == b->size) && (a->lcnt == b->lcnt);
    for (u32 i = 0; i < a->count; ++i)
    {
        result = result && pieces_are_equal(a->pieces[i], b->pieces[i]);
    }
    return result;
}

static inline b32 semantic_equality(const segmented_node *sentinel_a, const segmented_node *sentinel_b)
{
    b32 result = true;

    segmented_node *node_a = sentinel_a->next;
    segmented_node *node_b = sentinel_b->next;
    u32 index_a = 0;
    u32 index_b = 0;

    for (;;)
    {
        piece *piece_a = 0;
        if (node_a != sentinel_a)
        {
            piece_a = node_a->pieces + index_a++;
            if (index_a == node_a->count)
            {
                index_a = 0;
                node_a = node_a->next;
            }
        }

        piece *piece_b = 0;
        if (node_b != sentinel_b)
        {
            piece_b = node_b->pieces + index_b++;
            if (index_b == node_b->count)
            {
                index_b = 0;
                node_b = node_b->next;
            }
        }

        if (!piece_a)
        {
            result = result && (!piece_b);
            break;
        }
        if (!piece_b)
        {
            result = result && (!piece_a);
            break;
        }

        result = result && pieces_are_equal(*piece_a, *piece_b);
    }

    return result;
}

static inline b32 strict_equality(const segmented_node *sentinel_a, const segmented_node *sentinel_b)
{
    b32 result = true;
    segmented_node *node_1 = sentinel_a->next;
    segmented_node *node_2 = sentinel_b->next;

    for (;;)
    {
        if (node_1 == sentinel_a)
        {
            Assert(node_2 == sentinel_b);
            result = result && (node_2 == sentinel_b);
            break;
        }

        result = result && (node_2 != sentinel_b);
        result = result && nodes_are_equal(node_1, node_2);

        node_1 = node_1->next;
        node_2 = node_2->next;
    }
    return result;
}


static inline b32 lists_are_equal(piece_list *a, piece_list *b)
{
    b32 result = (a->size == b->size) && (a->lcnt == b->lcnt);

    result = result && buffers_are_equal(&a->original, &b->original);
    result = result && buffers_are_equal(&a->append, &b->append);
    result = result && semantic_equality(&a->root_sentinel, &b->root_sentinel);
    result = result && are_all_records_equal(&a->undo_records, &b->undo_records);
    return result;
}


static inline void list_invariants(piece_list *list)
{
    Assert(list);
    Assert(strictly_increasing(list->append.lines,   list->append.num_lines));
    Assert(strictly_increasing(list->original.lines, list->original.num_lines));
    Assert(freelist_sanity_check(list));

    u32 size = 0;
    u32 lcnt = 0;
    for (segmented_node *node = list->root_sentinel.next; 
        node != &list->root_sentinel;
        node = node->next)
    {
        Assert(node->count > 0);
        Assert(node->size > 0);
        node_size_and_lcnt size_and_lcnt = node_size_lcnt(node);
        Assert(node->size == size_and_lcnt.size);
        Assert(node->lcnt == size_and_lcnt.lcnt);

        if (node->next != &list->root_sentinel)
        {
            Assert(node->count >= MAX_PIECES_PER_NODE / 2);
        }
        size += node->size;
        lcnt += node->lcnt;
    }
    Assert(size == list->size);
    Assert(lcnt == list->lcnt);
}

static buffer_cursor rand_buffer_cursor(piece_list *list, prng *prng)
{
    test_cursor result = rand_cursor(prng, MAX_LINE_LEN, list->lcnt);
    u32 line_len = get_line_len(list, &list->iter, result.y);
    result.x = Minimum(result.x, line_len);
    return result;
}

static buffer_range rand_buffer_range(piece_list *list, prng *prng)
{
    // Assert(list->size);
    buffer_range result = {};
    b32 found = false;
    for(;!found;)
    {
        result.first = rand_cursor(prng, MAX_LINE_LEN, list->lcnt);
        result.one_past_end = rand_cursor(prng, MAX_LINE_LEN, list->lcnt);

        switch (compare(result.first, result.one_past_end))
        {
            case LessThan:
            {
                found = true;
            } break;

            case GreaterThan:
            {
                test_cursor tmp = result.first;
                result.first = result.one_past_end;
                result.one_past_end = tmp;
                found = true;
            } break;

            case EqualTo:
            {
            } break;
        }
    }


    u32 line_len_a = get_line_len(list, &list->iter, result.first.y);
    u32 line_len_b = get_line_len(list, &list->iter, result.one_past_end.y);
    result.first.x = Minimum(result.first.x, line_len_a);
    result.one_past_end.x = Minimum(result.one_past_end.x, line_len_b);

    return result;
}

static inline b32 rand_replace(piece_list *list, prng *prng)
{
    if (list->size + num_insert_pieces == 0) 
    {
        return false;
    }
    u32 num_ins_pieces;

    test_cursor a;
    test_cursor b;
    b32 found = false;
    for(; !found;)
    {
        num_ins_pieces = rand_range_u32_inclusive(prng, 0, num_insert_pieces);

        a = rand_cursor(prng, MAX_LINE_LEN, list->lcnt);
        b = rand_cursor(prng, MAX_LINE_LEN, list->lcnt);

        switch (compare(a, b))
        {
            case LessThan:
            {
                found = true;
            } break;

            case GreaterThan:
            {
                test_cursor tmp = a;
                a = b;
                b = tmp;
                found = true;
            } break;

            case EqualTo:
            {
                if (num_ins_pieces > 0)
                {
                    found = true;
                    break;
                }
                else
                {
                    reset_cursor(list, &list->iter);
                }
            } break;
        }
    }

    u32 line_len_a = get_line_len(list, &list->iter, a.y);
    u32 line_len_b = get_line_len(list, &list->iter, b.y);
    a.x = Minimum(a.x, line_len_a);
    b.x = Minimum(b.x, line_len_b);

    piece *pieces = (piece *) malloc(num_ins_pieces * sizeof(piece));;

    for (u32 i = 0; i < num_ins_pieces; ++i)
    {
        INIT_STACK_STRING(s, MAX_STRING_LEN)
        rand_ascii_string(&s, prng, 1, MAX_STRING_LEN);
        if (s.len > 0)
        {
            pieces[i] = make_piece(list, from_string(s));
        }
    }

    piece_slice p_slice = { .base = pieces , .count = num_ins_pieces };
    if (compare(a, b) == EqualTo && num_ins_pieces == 0)
    {
        free(pieces);
        return false;
    }
    else
    {
        range_replace(list, a, b, p_slice);
    }
    free(pieces);
    return true;
}

static inline piece_list *rand_list(prng *prng)
{
    piece_list *list = BootstrapPushStruct(piece_list, list_arena, 4096);

    string original_text = rand_ascii_string_alloc(
        prng,
        &list->list_arena,
        0, MAX_ORIGINAL_STRING_LEN);
    initialize_piece_list_s(list, original_text);

    for (u32 i = 0; i < num_edits; ++i)
    {
        begin_undo_sequence(&list->undo_records, 0);
        rand_replace(list, prng);
        end_undo_sequence(&list->undo_records, &list->redo_records);
    }

    return list;
}

#endif

