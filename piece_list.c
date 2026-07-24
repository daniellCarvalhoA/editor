#include "history.c"

typedef struct
{
    u32 count;
    union {
        undo_memory_header *undo_header;
        piece *pieces;
    };
    u32 start;
    u32 end;
    edit_flags flags;
} replace_result;

typedef struct
{
    const piece *base;
    u32 count;
} piece_slice;

static b32 lists_are_equal(const piece_list *a, const piece_list *b)
{
    b32 result = (a->size == b->size) && (a->lcnt == b->lcnt);

    result = result && buffers_are_equal(&a->original, &b->original);
    result = result && buffers_are_equal(&a->append, &b->append);
    result = result && semantic_equality(&a->root_sentinel, &b->root_sentinel);
    result = result && histories_are_equal(a->history, b->history);
    return result;
}

static inline u32 num_logical_lines(piece_list *buffer) 
{
    u32 result = buffer->lcnt + (buffer->size > 0);
    return result;
}

static void list_invariants(piece_list *list)
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

static piece original_piece(buffer *buffer)
{
    piece piece = { .lcnt = buffer->num_lines - 1, .size = buffer->text_len, .type = BufferType_Original };
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
    buffer.lines = PushArray(arena, (buffer.lines_capacity) / sizeof(u32), u32, NoClear());
    buffer.lines[0] = 0;
    return buffer;
}

static buffer allocate_original_buffer(memory_arena *arena, u8 *original_text, u32 original_text_len)
{
    buffer buffer = original_from_text(arena, original_text, original_text_len);
    buffer.text_capacity = original_text_len;
    buffer.lines_capacity = buffer.num_lines;
    return buffer;
}

static piece make_piece(piece_list *list, str s)//  u8 *text, u32 text_len)
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

static inline piece make_piece_s(piece_list *list, str s)
{
    piece result = make_piece(list, s);
    return result;
}

static piece make_piece_from_char(piece_list *list, char c)
{
    u32 start_row_index = list->append.num_lines - 1;
    u32 start_row = list->append.lines[start_row_index];
    u32 start_col = list->append.text_len - start_row;

    if (c == '\n')
    {
        Assert(list->append.lines_capacity > list->append.num_lines + 1);
        list->append.lines[list->append.num_lines++] = list->append.text_len + 1;
    }

    Assert(list->append.text_capacity > list->append.text_len + 1);
    list->append.text[list->append.text_len++] = c;
    u32 end_row_index = list->append.num_lines - 1;

    piece result;
    result.size = 1;
    result.lcnt = end_row_index - start_row_index;
    result.off.row = start_row_index;
    result.off.col = start_col;
    return result;
}

// #if 0
static base_iter find_position(base_iter *last_location, u32 position)  
{
    base_iter iter = *last_location;
    u32 last_position = get_position(&iter);
    iter.type = Position;

    if (position < last_position)
    {
        base_advance_pos_rev_by(&iter, last_position - position);
    }
    else if (position > last_position)
    {
        base_advance_pos_by(&iter, position - last_position);
    }

    normalize(&iter);
    *last_location = iter;

    return iter;
}

static buffer_cursor cursor_from_position(base_iter *last_location, u32 position)
{
    buffer_cursor result = {};
    base_iter iter = find_position(last_location, position);
    result.y = line_number(&iter);

    // for (;;)
    // {
    //     if (base_prev_cell(&iter))
    //     {
    //         if (result.y == line_number(&iter))
    //         {
    //             result.x++;
    //         }
    //     }
    //     // if (result.y == line_number)
    // }

    while (base_prev_cell(&iter) && result.y == line_number(&iter))
    {
        // if (result.y > line_number(&iter))
        // {
        //     result.x--;
        //     break;
        // }
        result.x++;
    }
    return result;
}


// #endif

static base_iter find_line(base_iter *last_location, u32 line)
{
    base_iter iter = *last_location;
    u32 last_line = get_line_number(&iter);
    iter.type = LineNumber;

    if (line <= last_line)
    {
        base_advance_rev_by_line(&iter, last_line - line);
    }
    else if (line > last_line)
    {
        base_advance_by_line(&iter, line - last_line);
    } 

    normalize(&iter);
    *last_location = iter;

    return iter;
}

static base_iter find_cursor(base_iter *last_location, buffer_cursor cursor)
{
    base_iter iter = find_line(last_location, cursor.y);
    // base_advance_by_cell(&iter, cursor.x);
    u32 count = cursor.x;

    str s = { .buffer = (u8 *) "\n", .len = 1 };
    while ((count > 0) && base_next_pred(&iter, not_equal_to, s))
    {
        count--;
    }
    return iter;
}

static u32 skip_space(base_iter *last_location, u32 cy)
{
    u32 result = 0;
    base_iter iter = find_line(last_location, cy);
    str s = {};
    while (base_next_pred(&iter, is_white_space, s)) 
    {
        result++;
    }
    return result;
}

static u32 get_line_len_(base_iter *last_location, u32 line)
{
    base_iter iter = find_line(last_location, line);
    u32 len = 0;

    while (base_next_cell_(&iter) && line_number(&iter) == line)
    {
        len++;
    }

    return len;
}

static u32 find_char_back(base_iter *last_location, str match, u32 count, buffer_cursor cursor)
{
    u32 result = 0;
    base_iter iter = find_cursor(last_location, cursor);

    for (; count > 0;)
    {
        if (!base_prev_cell(&iter))
        {
            result = 0;
            break;
        }

        str s = get_char_utf8(&iter);
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

static buffer_range find_boundary(base_iter *last_location, u8 open, u8 close, buffer_cursor cursor)
{
    base_iter copy_iter = find_cursor(last_location, cursor);
    base_iter iter = copy_iter;


    buffer_cursor back = cursor;

    // Find back cursor, 

    i32 back_score = 0;

    while (back_score < 1)
    {
        if (!base_prev_cell(&iter))
        {
            break;
        }

        str s = get_char_utf8(&iter);

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
            back.x = get_line_len_(&copy_iter, back.y);
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
        str s = get_char_utf8(&iter);
        if (!base_next_cell_(&iter))
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

static u32 find_char(base_iter *last_location, str match, u32 count, buffer_cursor cursor)
{
    u32 result = 0;
    base_iter iter = find_cursor(last_location, cursor);

    for (; count > 0;)
    {
        if (!base_next_cell_(&iter))
        {
            result = 0;
            break;
        }

        str s = get_char_utf8(&iter);

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

static buffer_cursor find_word_back(base_iter *last_location, u32 count, buffer_cursor cursor)
{
    buffer_cursor result = cursor;
    base_iter iter = find_cursor(last_location, cursor);

    for (; count > 0; )
    {
        // skip non_space;
        while (base_prev_cell(&iter))
        {
            str s = get_char_utf8(&iter);
            Assert(s.len);
            // NOTE: IMCOMPLETE/WRONG
            if (!(s.buffer[0] == ' ' || s.buffer[0] == '\t' || s.buffer[0] == '\n'))
            {
                break;
            }
            while (result.x == 0)
            {
                result.y--;
                base_iter copy_iter = *last_location;
                result.x = get_line_len_(&copy_iter, result.y) + 1;
            }
            result.x--;
        }
        // skip space
        while (base_prev_cell(&iter))
        {
            str s = get_char_utf8(&iter);
            Assert(s.len);
            if (s.buffer[0] == ' ' || s.buffer[0] == '\t' || s.buffer[0] == '\n')
            {
                while (result.x == 0)
                {
                    result.y--;
                    base_iter copy_iter = *last_location;
                    result.x = get_line_len_(&copy_iter, result.y);
                }
                result.x--;
                break;
            }
            result.x--;
        }

        count--;
    }
    return result;


}

static buffer_cursor find_word(base_iter *last_location, u32 count, buffer_cursor cursor)
{
    buffer_cursor result = cursor;
    base_iter iter = find_cursor(last_location, cursor);

    for(; count > 0;)
    {
        // skip non_space
        while (base_next_cell_(&iter))
        {
            str s = get_char_utf8(&iter);
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
                else
                {
                    result.x++;
                }
                break;
            }
        }
        // skip space;
        while (base_next_cell_(&iter))
        {
            str s = get_char_utf8(&iter);
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


static void maybe_merge_with_next(piece_list *list, segmented_node *node)
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

static void append(piece_list  *list, piece *pieces, u32 num_pieces)
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

static cursor make_space_for_at(piece_list *list, cursor at, u32 count)
{
    cursor result;

    if (at.node == &list->root_sentinel)
    {
        Assert(at.piece_index == 0);
        Assert(count > 0);

        segmented_node *new_node = allocate_node(list);

        DLIST_INSERT_TAIL(&list->root_sentinel, new_node);
        at.node = new_node;
        at.piece_index = 0;
    }

    u32 const total     = count + at.node->count;
    u32 const num_alloc = (total == 0) ? 0 : (total - 1) / MAX_PIECES_PER_NODE;

    segmented_node *node = at.node;
    u32 start = at.piece_index;

    if (total <= MAX_PIECES_PER_NODE)
    {
        Assert(num_alloc == 0);
        piece *pieces = node->pieces;

        u32 num = node->count - start;
        node->count += count;

        memmove(pieces + start + count, pieces + start, sizeof(piece) * num);
        result = at;
    }
    else
    {
        u32 num_nodes       = 1 + num_alloc;
        u32 pieces_per_node = total / num_nodes;
        u32 rem             = total % num_nodes;

        u32 left_len = start;

        piece *left_pieces = node->pieces;

        u32 right_len = node->count - at.piece_index;

        piece *right_pieces = node->pieces  + at.piece_index;

        u32 left_count   = 0; // pieces copied so far from left part of split to newly allocated nodes
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

            u32 pieces_end          = count - pieces_count;
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
            left_count += pieces_start;
            new_node->count = pieces_per_this_node;
            rem = (rem > 0) ? (rem - 1) : 0;
            DLIST_INSERT_AFTER(node, new_node);
        }
        u32 pieces_remaining = count - pieces_count;

        piece *src_pieces = node->pieces + at.piece_index;
        piece *dst_pieces = node->pieces + at.piece_index + pieces_remaining;

        u32 count = (node->count - right_count) - at.piece_index;

        memmove(dst_pieces, src_pieces, sizeof(piece) * count);
        
        node->count = pieces_per_node;

        if (at.piece_index >= node->count)
        {
            at.piece_index = at.piece_index % node->count;
            at.node = at.node->next;
        }
        result = at;
    }
    return result;
}

static void copy_range(cursor start, cursor end, u32 count, slice_cursor *slice_cursor)
{
    piece *piece_start = start.node->pieces + start.piece_index;
    if (start.node == end.node)
    {
        Assert(count == (end.piece_index - start.piece_index));
        copy_slice(slice_cursor, piece_start, count);
    }
    else
    {
        u32 len = start.node->count - start.piece_index;
        copy_slice(slice_cursor, piece_start, len);

        for (segmented_node *node = start.node->next;
            node != end.node;
            node = node->next)
        {
            copy_slice(slice_cursor, node->pieces, node->count);

        }
        copy_slice(slice_cursor, end.node->pieces, end.piece_index);
    }
}

static void replace(piece_list *list, cursor start, cursor end, piece *pieces, u32 num_pieces)
{
    if (start.node == &list->root_sentinel)
    {
        append(list, pieces, num_pieces);
        return;
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
        u32 total    = num_pieces + node->count - distance;

        if (total <= MAX_PIECES_PER_NODE)
        {
            // FIX NODE SIZES AND LCNTS: DELETED PIECES
            for (u32 i = start.piece_index; i < end.piece_index; ++i)
            {
                piece piece = node->pieces[i];
                node->size -= piece.size;
                node->lcnt -= piece.lcnt;
            }

            piece *src_piece = node->pieces + end.piece_index;
            piece *dst_piece = node->pieces + start.piece_index + num_pieces;


            u32 num_moved = node->count - end.piece_index;

            if (start.piece_index + num_pieces < total)
            {
                memmove(dst_piece, src_piece, sizeof(piece) * num_moved);
            }

            src_piece = node->pieces  + start.piece_index;

            memcpy(src_piece, pieces, sizeof(piece) * num_pieces);

            node->count = total;

            // FIX NODE SIZES AND LCNTS: INSERTED PIECES
            for (u32 i = 0; i < num_pieces; ++i)
            {
                piece piece = pieces[i];
                node->size += piece.size;
                node->lcnt += piece.lcnt;
            }

            if (total == 0)
            {
                DLIST_REMOVE(node);
                LIST_INSERT(list->first_free_node, node);
            } 
            else if (total < MAX_PIECES_PER_NODE / 2)
            {
                maybe_merge_with_next(list, node);
            }
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

            u32 left_count   = 0; // # pieces copied so far from left part of split to newly allocated nodes
            u32 right_count  = 0; // # pieces copied so far from right part of split to newly allocated nodes
            u32 pieces_count = 0; // # pieces copied so far from *pieces* to newly allocated nodes

            Assert(num_alloc > 0);
            for (u32 i = 0; i < num_alloc; ++i)
            {
                segmented_node *new_node;
                FREELIST_ALLOCATE(new_node, list->first_free_node, PushStruct(&list->list_arena, segmented_node, NoClear()));
                // Copy pieces from the right part of the split to the newly allocated node
                u32 pieces_per_this_node = (rem > 0) ? pieces_per_node + 1 : pieces_per_node;
                u32 right_remaining = (right_count > right_len) ? 0 : right_len - right_count;
                u32 right_to_new_count = Minimum(pieces_per_this_node, right_remaining);
                u32 right_start = pieces_per_this_node - right_to_new_count;
                u32 right_end   = right_len - right_count;

                piece *dst_pieces = new_node->pieces + right_start;
                piece *src_pieces = right_pieces + right_end - right_to_new_count;

                memcpy(dst_pieces, src_pieces, sizeof(piece) * right_to_new_count);

                right_count += right_to_new_count;
                // Copy pieces from the new pieces to new_node
                u32 pieces_end = num_pieces - pieces_count;
                u32 pieces_to_new_count = Minimum(pieces_end, right_start);
                u32 pieces_start = right_start - pieces_to_new_count;

                u32 rest_pieces = pieces_end - pieces_to_new_count;

                dst_pieces = new_node->pieces + pieces_start;
                src_pieces = pieces + rest_pieces;

                memcpy(dst_pieces, src_pieces, sizeof(piece) * pieces_to_new_count);

                // Copy pieces from the left part of the split to the newly allocated node
                pieces_count += pieces_to_new_count;

                u32 from_left = left_len - pieces_start;
                dst_pieces = new_node->pieces;
                src_pieces = left_pieces + from_left;

                memcpy(dst_pieces, src_pieces, sizeof(piece) * pieces_start);

                left_count += pieces_start;
                new_node->count = pieces_per_this_node;

                fix_size_and_lines(new_node);

                rem = (rem > 0) ? (rem - 1) : 0;

                DLIST_INSERT_AFTER(node, new_node);
            }
            u32 pieces_remaining = num_pieces - pieces_count;

            piece *src_pieces = node->pieces + delete_end;
            piece *dst_pieces = node->pieces + start.piece_index + pieces_remaining;

            u32 count = (node->count - right_count) - delete_end;

            memmove(dst_pieces,src_pieces, sizeof(piece)* count);

            u32 left_split = start.piece_index - left_count;
            dst_pieces = node->pieces + left_split;
            
            memcpy(dst_pieces, pieces, sizeof(piece) * pieces_remaining); 

            node->count = pieces_per_node;
            fix_size_and_lines(node);
        }
    }
    else
    {
        u32 right_len = end.node->count - end.piece_index;
        u32 total = num_pieces + start.piece_index + right_len;
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

            piece *dst_pieces = left->pieces + start.piece_index;

            memcpy(dst_pieces, pieces, sizeof(piece) * num_pieces);

            dst_pieces += num_pieces;

            memcpy(dst_pieces, right->pieces + end.piece_index, sizeof(piece) * right_len);

            left->count = total;
            fix_size_and_lines(left);

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
                // Force the "at least half full invariant"
                maybe_merge_with_next(list, left);
            }
        }
        else
        {
            // Number of nodes that will be updated (some of which might need to be allocated)
            u32 const num_used_nodes = 1 + (total - 1) / MAX_PIECES_PER_NODE;
            // Number off allocated nodes
            u32 num_alloc = (num_used_nodes > 2) ? (num_used_nodes - 2) : 0;

            u32 const pieces_per_node = total / num_used_nodes;
            u32 rem = total % num_used_nodes;
            u32 pieces_cursor = num_pieces;

            u32 const left_pieces_per_node = pieces_per_node + (rem > 0);
            rem = (rem > 0) ? (rem - 1) : 0;
            u32 const right_pieces_per_node = pieces_per_node + (rem > 0);
            rem = (rem > 0) ? (rem - 1) : 0;

            // Number of pieces that must be moved from *left* to *new_nodes* and/or *right*.
            u32 left_to_right = (start.piece_index > left_pieces_per_node) ? (start.piece_index - left_pieces_per_node) : 0;
            // Number of pieces that must be moved from *pieces* and/or *right* to *left*.
            u32 left_to_right_rev = (left_pieces_per_node > start.piece_index) ? (left_pieces_per_node - start.piece_index) : 0;
            // Number of pieces that must be moved from *right* to *new_nodes* and/or *left*.
            u32 right_to_left = (right_len > right_pieces_per_node) ? (right_len - right_pieces_per_node) : 0;
            // Number of pieces that must be moved from *pieces* and/or *left* to *right*.
            u32 right_to_left_rev = (right_pieces_per_node > right_len) ? (right_pieces_per_node - right_len) : 0;

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
                FREELIST_ALLOCATE(new_node, list->first_free_node, PushStruct(&list->list_arena, segmented_node, NoClear()));

                u32 pieces_per_this_node = pieces_per_node + (rem > 0);
                u32 node_cursor = pieces_per_this_node - right_to_left;

                memcpy(new_node->pieces + node_cursor, right->pieces + end.piece_index, sizeof(piece) * right_to_left);

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

                memmove(right->pieces + right_to_left_rev, right->pieces + end.piece_index + right_to_left, sizeof(piece) * (right_len - right_to_left));

                // Copy from *pieces* to *right*
                memcpy(right->pieces, pieces + pieces_cursor - right_to_left_rev, sizeof(piece) * right_to_left_rev);

                pieces_cursor -= right_to_left_rev;

                // Copy from *pieces* to *new_node*
                u32 const rest_pieces_node = Minimum(pieces_cursor, node_cursor);

                memcpy(new_node->pieces + node_cursor - rest_pieces_node, pieces + pieces_cursor - rest_pieces_node, sizeof(piece) * rest_pieces_node);

                node_cursor -= rest_pieces_node;
                pieces_cursor -= rest_pieces_node;

                u32 const left_to_node = Minimum(left_to_right, node_cursor);

                memcpy(new_node->pieces, left->pieces + start.piece_index - left_to_node, sizeof(piece) * left_to_node);

                Assert(pieces_cursor >= left_to_right_rev);

                // Copy from *pieces* to *left*
                u32 const pieces_to_left = (pieces_cursor == left_to_right_rev) * left_to_right_rev;

                memcpy(left->pieces + start.piece_index, pieces, sizeof(piece) * pieces_to_left);

                new_node->count = pieces_per_this_node;
                fix_size_and_lines(new_node);

                right_to_left      = 0;
                right_to_left_rev  = 0;
                left_to_right     -= left_to_node;
                left_to_right_rev -= pieces_to_left;

                rem = (rem > 0) ? (rem - 1) :0;
                end.piece_index = 0;

                DLIST_INSERT_AFTER(left, new_node);
            }

            left_to_right_rev = Minimum(left_to_right_rev, num_pieces);
            right_to_left_rev = Minimum(right_to_left_rev, num_pieces);

            // Copy *pieces* to *left*
            memcpy(left->pieces + start.piece_index, pieces, sizeof(piece) * left_to_right_rev);
            // Copy *right* to *left*
            memcpy(left->pieces + start.piece_index + left_to_right_rev, right->pieces + end.piece_index, sizeof(piece) * right_to_left);
            // Adjust *right*
            memmove(right->pieces + left_to_right + right_to_left_rev, right->pieces + end.piece_index + right_to_left, sizeof(piece) * (right_len - right_to_left));
            // Copy *pieces* to *right*
            memcpy(right->pieces + left_to_right, pieces + num_pieces - right_to_left_rev, sizeof(piece) * right_to_left_rev);
            // Copy *left* to *right*
            u32 copy_offset = start.piece_index - left_to_right;
            memcpy(right->pieces,  left->pieces  + copy_offset, sizeof(piece) * left_to_right);

            left->count  = left_pieces_per_node;
            right->count = right_pieces_per_node;
            fix_size_and_lines(left);
            fix_size_and_lines(right);
        }
    }
}

static cursor make_space(piece_list *list, cursor start, cursor end, u32 size)
{
    cursor result;

    if (start.node == &list->root_sentinel)
    {
        Assert(start.piece_index == 0);
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
                // fix_size_and_lines(new_node);
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

            // fix_size_and_lines(node);

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
            // fix_size_and_lines(left);

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
                FREELIST_ALLOCATE(new_node,
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

            memcpy(right->pieces,  left->pieces + copy_offset, sizeof(piece) * left_to_right);

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

static void redo(window *win)
{
    piece_list *list = win->buffer;
    undo_node *curr_node = undo_node_unpop(&list->history);

    if (curr_node)
    {
        buffer_cursor prev_bc = win->bc;
        win->bc = curr_node->bc;
        curr_node->bc = prev_bc;

        undo_memory_header **first_header = &(curr_node)->data;
        undo_memory_header *prev_header = 0;

        for (undo_memory_header *header = *first_header; header; header = header->next)
        {
            cursor start_cursor = abs_idx_to_cursor_2(list, header->abs_idx);
            cursor end_cursor   = abs_idx_to_cursor_2(list, header->abs_idx + header->ins_count);

            undo_memory_header *redo_header = 
                allocate_undo_memory_block(&list->history, &list->history_arena, header->ins_count);

            redo_header->ins_count = header->del_count;
            redo_header->del_count = header->ins_count;
            redo_header->abs_idx   = header->abs_idx;
            redo_header->ref_count = header->ref_count;

            slice_cursor slice_cursor = {};
            get_slice_cursor_from_header(&slice_cursor, redo_header);

            copy_range(start_cursor, end_cursor, header->ins_count, &slice_cursor);

            piece *pieces = get_pieces_from_header(header);

            for (u32 i = 0; i < slice_cursor.count; ++i)
            {
                piece piece = slice_cursor.pieces[i];
                list->size -= piece.size;
                list->lcnt -= piece.lcnt;
            }

            for (u32 i = 0; i < header->del_count; ++i)
            {
                piece piece = pieces[i];
                list->size += piece.size;
                list->lcnt += piece.lcnt;
            }

            replace(list, start_cursor, end_cursor, pieces, header->del_count);

            redo_header->next = prev_header;
            prev_header = redo_header;
        }

        free_undo_memory_block(&list->history, *first_header);
        *first_header = prev_header;
        reset_cursor_(&list->iter);
    }
}

static void undo_(window *win)
{
    piece_list *list = win->buffer;
    undo_node *curr_node = undo_node_pop(&list->history);

    if (curr_node)
    {
        buffer_cursor prev_bc = win->bc;
        win->bc = curr_node->bc;
        curr_node->bc = prev_bc;

        undo_memory_header **first_header = &(curr_node)->data;
        undo_memory_header *prev_header = 0;

        for (undo_memory_header *header = *first_header; header; header = header->next)
        {
            cursor start_cursor = abs_idx_to_cursor_2(list, header->abs_idx);
            cursor end_cursor   = abs_idx_to_cursor_2(list, header->abs_idx + header->ins_count);

            undo_memory_header *redo_header = allocate_undo_memory_block(&list->history, &list->history_arena, header->ins_count);

            redo_header->ins_count = header->del_count;
            redo_header->del_count = header->ins_count;
            redo_header->abs_idx   = header->abs_idx;
            redo_header->ref_count = header->ref_count;

            slice_cursor slice_cursor = {};
            get_slice_cursor_from_header(&slice_cursor, redo_header);

            copy_range(start_cursor, end_cursor, header->ins_count, &slice_cursor);

            piece *pieces = get_pieces_from_header(header);

            for (u32 i = 0; i < slice_cursor.count; ++i)
            {
                piece piece = slice_cursor.pieces[i];
                list->size -= piece.size;
                list->lcnt -= piece.lcnt;
            }

            for (u32 i = 0; i < header->del_count; ++i)
            {
                piece piece = pieces[i];
                list->size += piece.size;
                list->lcnt += piece.lcnt;
            }

            replace(list, start_cursor, end_cursor, pieces, header->del_count);

            redo_header->next = prev_header;
            prev_header = redo_header;
        }

        free_undo_memory_block(&list->history, *first_header);
        *first_header = prev_header;
        reset_cursor_(&list->iter);
    }
}

static void insert_many(cursor at, piece_slice *slices, u32 num_slices)
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

// static void replace_range_(
//     piece_list *list,
//     base_iter start,
//     base_iter end,
//     piece *pieces,
//     u32 num_pieces,
//     slice_cursor s_cursor)
// {
//     cursor start_cursor = { start.node, start.piece_idx };
//     cursor end_cursor   = { end.node, end.piece_idx };
//     cursor cursor = end_cursor;
//     u32 copy_amount = end.abs_idx - start.abs_idx;
//
//     if (end.pos_in_piece > 0)
//     {
//         add_to_cursor_(&cursor, 1);
//         copy_amount++;
//
//     }
//     copy_range(start_cursor, cursor, copy_amount, &s_cursor);
//
//     piece_slice p_slice[2] = { { .base = pieces, .count = num_pieces } };
//     u32 slice_count = 1;
//
//     piece *start_piece  = get_piece_(&start);
//     if (start.abs_idx == end.abs_idx && start.pos_in_piece > 0) 
//     {
//         offset offset = get_offset(&end);
//
//         piece right = { 
//             .size = start_piece->size - end.pos_in_piece,
//             .lcnt = start_piece->lcnt - end.line_in_piece,
//             .off  = offset,
//             .type = start_piece->type,
//         };
//
//         start_cursor.node->size -= start_piece->size - start.pos_in_piece;
//         start_cursor.node->lcnt -= start_piece->lcnt - start.line_in_piece;
//
//         start_piece->size = start.pos_in_piece;
//         start_piece->lcnt = start.line_in_piece;
//
//         add_to_cursor(list, &start_cursor, 1);
//
//         start_cursor = make_space_for_at(list, start_cursor, num_pieces + 1);
//
//         p_slice[1].base  = &right;
//         p_slice[1].count = 1;
//         slice_count++;
//     }
//     else
//     {
//         if (start.pos_in_piece > 0)
//         {
//             start_cursor.node->size -= start_piece->size - start.pos_in_piece;
//             start_cursor.node->lcnt -= start_piece->lcnt - start.line_in_piece;
//             start_piece->size = start.pos_in_piece;
//             start_piece->lcnt = start.line_in_piece;
//             add_to_cursor_(&start_cursor, 1);
//             start.abs_idx++;
//         }
//
//         if (end.pos_in_piece > 0)
//         {
//             piece *end_piece = get_piece_(&end);
//             end_cursor.node->size -= end.pos_in_piece;
//             end_cursor.node->lcnt -= end.line_in_piece;
//             end_piece->off = get_offset(&end);
//             end_piece->size -= end.pos_in_piece;
//             end_piece->lcnt -= end.line_in_piece;
//         }
//
//         u32 count = end.abs_idx - start.abs_idx;
//
//         if (count + num_pieces > 0)
//         {
//             start_cursor = make_space(list, start_cursor, end_cursor, num_pieces);
//         }
//     }
//     insert_many(start_cursor, p_slice, slice_count);
//     reset_cursor_(&list->iter);
// }

static piece serialize_piece_range_to(piece_list *a, piece_list *b, piece_range p_range_a)
{
    piece result = {};
    result.type = BufferType_Append;
    result.off.row = b->append.num_lines - 1;
    result.off.col = b->append.text_len - b->append.lines[result.off.row];

    for (u32 i = 0; i < p_range_a.count; ++i)
    {
        u32 start = 0;
        const piece *a_piece = p_range_a.pieces + i;
        u32 end   = a_piece->size;
        if (i == 0)
        {
            start = p_range_a.start;
        } 

        if (i == p_range_a.count - 1)
        {
            end = p_range_a.end;
        }

        const buffer *buffer = get_buffer(a, a_piece->type);

        offset start_offset = (start) ? (search_piece(buffer, *a_piece, start)) : a_piece->off;
        u32 size_len = end - start;
        u32 line_len = (end == a_piece->size) ? (a_piece->lcnt - (start_offset.row - a_piece->off.row)) :
            search_piece(buffer, *a_piece, end).row - start_offset.row;

        result.size += size_len;
        result.lcnt += line_len;

        u8 *src_text = buffer->text + buffer->lines[start_offset.row] + start_offset.col;
        u8 *dst_text = b->append.text + b->append.text_len;
        memcpy(dst_text, src_text, size_len);
        b->append.text_len += size_len;

        u32 *src_lines = buffer->lines + start_offset.row;
        u32 *dst_lines = a->append.lines + a->append.num_lines - 1;
        memcpy(dst_lines, src_lines, line_len);

        for (u32 i = 0; i < line_len; ++i)
        {
            dst_lines[i] = dst_lines[i] - buffer->num_lines + b->append.num_lines;
        }
        b->append.num_lines += line_len;

    }
    return result;
}

static replace_result  yank_(piece_list *list, buffer_cursor c0, buffer_cursor c1)
{
    replace_result result = {};
    base_iter start = find_cursor(&list->iter, c0);
    fix_iter(&start);
    base_iter end   = find_cursor(&list->iter, c1);
    fix_iter(&end);

    result.start = start.pos_in_piece;
    result.end   = end.pos_in_piece;

    u32 not_boundary_end = end.pos_in_piece > 0;
    u32 not_boundary_start = start.pos_in_piece > 0;

    u32 num_pieces = (end.abs_idx + not_boundary_end) - start.abs_idx;
    result.count = num_pieces;

    if (not_boundary_start)
    {
        result.flags |= Edit_Left;
    }

    if (not_boundary_end)
    {
        result.flags |= Edit_Right;
    }

    if (num_pieces == 1)
    {
        if ((result.flags & Edit_Left) && (result.flags & Edit_Right))
        {
            result.flags = Edit_Both;
        }
    }

    piece *copied_pieces = allocate_memory_block(
        &list->history,
        &list->history_arena,
        num_pieces * sizeof(piece));

    result.pieces = copied_pieces;

    slice_cursor s_cursor =  {
        .pieces = copied_pieces,
        .count = num_pieces
    };

    cursor start_cursor = { start.node, start.piece_idx };
    cursor end_cursor = { end.node, end.piece_idx };

    u32 copy_amount = end.abs_idx - start.abs_idx;

    if (not_boundary_end)
    {
        add_to_cursor_(&end_cursor, 1);
        copy_amount++;
    }

    copy_range(start_cursor, end_cursor, copy_amount, &s_cursor);

    return result;
}

static edit_flags replace_range(
    piece_list *list,
    base_iter start,
    base_iter end,
    piece_range p_range,
    slice_cursor s_cursor)
{
    edit_flags flags = Edit_None;
    cursor start_cursor = { start.node, start.piece_idx };
    cursor end_cursor   = { end.node, end.piece_idx };
    cursor cursor = end_cursor;
    u32 copy_amount = end.abs_idx - start.abs_idx;

    if (end.pos_in_piece > 0)
    {
        add_to_cursor_(&cursor, 1);
        copy_amount++;

    }
    copy_range(start_cursor, cursor, copy_amount, &s_cursor);

    piece_slice p_slice[4] = {};
    u32 slice_count = 0;
    u32 piece_cursor = 0;
    u32 piece_count = p_range.count;
    //
    piece left_piece;
    piece right_piece;

    if (p_range.flags == Edit_Both)
    {
        Assert(p_range.count == 1);
        left_piece = *(p_range.pieces);
        const buffer *buffer = get_buffer(list, left_piece.type);
        offset off_start = search_piece(buffer, left_piece, p_range.start);
        offset off_end   = search_piece(buffer, left_piece, p_range.end);
        left_piece.size = p_range.end - p_range.start;
        left_piece.lcnt = off_end.row - off_start.row;
        left_piece.off = off_start;
        p_slice[0].base = &left_piece;
        p_slice[0].count = 1;
        slice_count++;

        list->size -= p_range.pieces[0].size - left_piece.size;
        list->lcnt -= p_range.pieces[0].lcnt - left_piece.lcnt;

    }
    else
    {
        if (p_range.flags & Edit_Left)
        {
            left_piece = *(p_range.pieces);
            const buffer *buffer = get_buffer(list, left_piece.type);
            offset off = search_piece(buffer, left_piece, p_range.start);
            left_piece.size -= p_range.start;
            left_piece.lcnt -= off.row - left_piece.off.row;
            left_piece.off = off;
            p_slice[0].base = &left_piece;
            p_slice[0].count = 1;
            slice_count++;
            piece_cursor++;
            piece_count--;

            list->size -= p_range.pieces[0].size - left_piece.size;
            list->lcnt -= p_range.pieces[0].lcnt - left_piece.lcnt;
        }


        if (p_range.flags & Edit_Right)
        {
            right_piece = p_range.pieces[p_range.count - 1];
            const buffer *buffer = get_buffer(list, right_piece.type);
            offset off = search_piece(buffer, right_piece, p_range.end);
            right_piece.size = p_range.end;
            right_piece.lcnt = off.row - right_piece.off.row;

            piece_count--;
            p_slice[slice_count + (piece_count > 0)].base = &right_piece;
            p_slice[slice_count + (piece_count > 0)].count = 1;
            slice_count++;

            list->size -= p_range.pieces[p_range.count - 1].size - right_piece.size;
            list->lcnt -= p_range.pieces[p_range.count - 1].lcnt - right_piece.lcnt;
        }

        if (piece_count)
        {
            p_slice[piece_cursor].base  = p_range.pieces + piece_cursor;
            p_slice[piece_cursor].count = piece_count;
            slice_count++;
        }
    }

    piece *start_piece = get_piece_(&start);
    piece right;

    if (start.abs_idx == end.abs_idx && start.pos_in_piece > 0) 
    {
        offset offset = get_offset(&end);

        right.size = start_piece->size - end.pos_in_piece;
        right.lcnt = start_piece->lcnt - end.line_in_piece;
        right.off  = offset;
        right.type = start_piece->type;

        start_cursor.node->size -= start_piece->size - start.pos_in_piece;
        start_cursor.node->lcnt -= start_piece->lcnt - start.line_in_piece;

        start_piece->size = start.pos_in_piece;
        start_piece->lcnt = start.line_in_piece;

        add_to_cursor(list, &start_cursor, 1);

        start_cursor = make_space_for_at(list, start_cursor, p_range.count + 1);

        p_slice[slice_count].base  = &right;
        p_slice[slice_count].count = 1;
        slice_count++;

        flags = Edit_Both;
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
            flags |= Edit_Left;
        }

        if (end.pos_in_piece > 0)
        {
            piece *end_piece = get_piece_(&end);
            end_cursor.node->size -= end.pos_in_piece;
            end_cursor.node->lcnt -= end.line_in_piece;
            end_piece->off = get_offset(&end);
            end_piece->size -= end.pos_in_piece;
            end_piece->lcnt -= end.line_in_piece;
            flags |= Edit_Right;
        }

        u32 count = end.abs_idx - start.abs_idx;

        if (count + p_range.count > 0)
        {
            start_cursor = make_space(list, start_cursor, end_cursor, p_range.count);
        }
    }
    insert_many(start_cursor, p_slice, slice_count);
    reset_cursor_(&list->iter);

    return flags;

}


static replace_result range_replace(
    piece_list *list,
    buffer_cursor c0,
    buffer_cursor c1,
    piece_range p_range)
{
    list->changed_since_last_search = true;
    list->changed = true;
    replace_result result = {};
    base_iter start = find_cursor(&list->iter, c0);
    fix_iter(&start);
    base_iter end   = find_cursor(&list->iter, c1);
    fix_iter(&end);

    result.start = start.pos_in_piece;
    result.end   = end.pos_in_piece;

    u32 not_boundary_start = start.pos_in_piece > 0;
    u32 not_boundary_end   = end.pos_in_piece > 0;

    u32 num_undo_pieces = (end.abs_idx + not_boundary_end) - start.abs_idx;

    for (u32 i = 0; i < p_range.count; ++i)
    {
        piece piece = p_range.pieces[i];
        list->size += piece.size;
        list->lcnt += piece.lcnt;
    }

    u32 num_lines_deleted = line_number(&end) - line_number(&start);
    u32 num_chars_deleted = position(&end)    - position(&start);

    list->size -= num_chars_deleted;
    list->lcnt -= num_lines_deleted;

    undo_memory_header *header = allocate_undo_memory_block(&list->history, &list->history_arena, num_undo_pieces);

    header->abs_idx   = start.abs_idx;
    header->ins_count = p_range.count + not_boundary_start + not_boundary_end;
    header->del_count = num_undo_pieces;
    header->ref_count = 1;

    slice_cursor slice_cursor = {};
    get_slice_cursor_from_header(&slice_cursor, header);

    result.flags = replace_range(list, start, end, p_range, slice_cursor);
    result.undo_header = header;
    return result;
}

// static replace_result range_replace_(
//     piece_list *list,
//     buffer_cursor c0,
//     buffer_cursor c1,
//     piece *pieces,
//     u32 num_pieces)
// {
//     replace_result result = {};
//     base_iter start = find_cursor(&list->iter, c0);
//     fix_iter(&start);
//     base_iter end   = find_cursor(&list->iter, c1);
//     fix_iter(&end);
//
//     result.start = start.pos_in_piece;
//     result.end   = end.pos_in_piece;
//
//     u32 not_boundary_start = start.pos_in_piece > 0;
//     u32 not_boundary_end   = end.pos_in_piece > 0;
//
//     u32 num_undo_pieces = (end.abs_idx + not_boundary_end) - start.abs_idx;
//
//     for (u32 i = 0; i < num_pieces; ++i)
//     {
//         piece piece = pieces[i];
//         list->size += piece.size;
//         list->lcnt += piece.lcnt;
//     }
//
//     u32 num_lines_deleted = line_number(&end) - line_number(&start);
//     u32 num_chars_deleted = position(&end)    - position(&start);
//
//     list->size -= num_chars_deleted;
//     list->lcnt -= num_lines_deleted;
//
//     undo_memory_header *header = allocate_undo_memory_block(
//         &list->history,
//         &list->history_arena,
//         num_undo_pieces);
//
//     header->abs_idx   = start.abs_idx;
//     header->ins_count = num_pieces + not_boundary_start + not_boundary_end;
//     header->del_count = num_undo_pieces;
//     header->ref_count = 1;
//
//     slice_cursor slice_cursor = {};
//     get_slice_cursor_from_header(&slice_cursor, header);
//
//     replace_range_(list, start, end, pieces, num_pieces, slice_cursor);
//     result.undo_header = header;
//     return result;
// }

// static undo_memory_header *replace_range(
//     piece_list *list,
//     base_iter start,
//     base_iter end,
//     piece *pieces,
//     u32 num_pieces)
// {
//     piece *start_piece = get_piece_(&start);
//     piece *end_piece   = get_piece_(&end);
//
//     u32 start_size = start_piece ? start_piece->size : UINT32_MAX;
//
//     u32 num_undo_pieces = 
//         (end.abs_idx + (end.pos_in_piece > 0)) - 
//         (start.abs_idx + (start.pos_in_piece == start_size));
//
//     undo_memory_header *undo_header = allocate_undo_memory_block(
//         &list->history,
//         &list->history_arena,
//         num_undo_pieces);
//
//     undo_header->abs_idx   = start.abs_idx + (start.pos_in_piece == start_size);
//     undo_header->ins_count = num_pieces;
//     undo_header->del_count = num_undo_pieces;
//     undo_header->ref_count = 1;
//
//     slice_cursor slice_cursor = {};
//     get_slice_cursor_from_header(&slice_cursor, undo_header);
//
//     for (u32 i = 0; i < num_pieces; ++i)
//     {
//         piece piece = pieces[i];
//         list->size += piece.size;
//         list->lcnt += piece.lcnt;
//     }
//
//     u32 num_lines_deleted = line_number(&end) - line_number(&start);
//     u32 num_chars_deleted = position(&end) - position(&start);
//
//     list->size -= num_chars_deleted;
//     list->lcnt -= num_lines_deleted;
//
//     cursor start_cursor = { start.node, start.piece_idx };
//     cursor end_cursor   = { end.node, end.piece_idx };
//
//     if (start_piece == end_piece && start.pos_in_piece > 0) 
//     {
//         copy_slice(&slice_cursor, start_piece, 1);
//
//         offset offset = get_offset(&end);
//
//         piece right = { 
//             .size = start_piece->size - end.pos_in_piece,
//             .lcnt = start_piece->lcnt - end.line_in_piece,
//             .off  = offset,
//             .type = start_piece->type,
//         };
//
//         start_cursor.node->size -= start_piece->size - start.pos_in_piece;
//         start_cursor.node->lcnt -= start_piece->lcnt - start.line_in_piece;
//
//         start_piece->size = start.pos_in_piece;
//         start_piece->lcnt = start.line_in_piece;
//
//         add_to_cursor(list, &start_cursor, 1);
//
//         start_cursor = make_space_for_at(list, start_cursor, num_pieces + 1);
//
//         piece_slice p_slice[2];
//         p_slice[0].base = pieces;
//         p_slice[0].count = num_pieces;
//         p_slice[1].base = &right;
//         p_slice[1].count = 1;
//
//         insert_many(start_cursor, p_slice, ArrayCount(p_slice));
//         undo_header->ins_count += 2;
//     }
//     else
//     {
//         if (start.pos_in_piece > 0)
//         {
//             write_start(&slice_cursor, *start_piece);
//             start_cursor.node->size -= start_piece->size - start.pos_in_piece;
//             start_cursor.node->lcnt -= start_piece->lcnt - start.line_in_piece;
//             start_piece->size = start.pos_in_piece;
//             start_piece->lcnt = start.line_in_piece;
//             undo_header->ins_count++;
//             add_to_cursor_(&start_cursor, 1);
//             start.abs_idx++;
//         }
//
//         if (end.pos_in_piece > 0)
//         {
//             write_last(&slice_cursor, *end_piece);
//             end_cursor.node->size -= end.pos_in_piece;
//             end_cursor.node->lcnt -= end.line_in_piece;
//             end_piece->off = get_offset(&end);
//             end_piece->size -= end.pos_in_piece;
//             end_piece->lcnt -= end.line_in_piece;
//             undo_header->ins_count++;
//         }
//
//         u32 count = end.abs_idx - start.abs_idx;
//
//         if (count)
//         {
//             copy_range(start_cursor, end_cursor, count, &slice_cursor);
//         }
//
//         if (count + num_pieces > 0)
//         {
//             replace(list, start_cursor, end_cursor, pieces,  num_pieces);
//         }
//     }
//     reset_cursor_(&list->iter);
//
//     return undo_header;
// }
//
// static undo_memory_header *range_replace(
//     piece_list *list, 
//     buffer_cursor c0,
//     buffer_cursor c1,
//     piece *pieces,
//     u32 num_pieces)
// {
//     base_iter start = find_cursor(&list->iter, c0);
//     fix_iter(&start);
//     base_iter end   = find_cursor(&list->iter, c1);
//     fix_iter(&end);
//
//     undo_memory_header *result = replace_range(list, start, end, pieces, num_pieces);
//     return result;
// }

static void initialize_piece_list(piece_list *list, u8 *original_text, u32 original_text_len)
{
    DLIST_INIT(&list->root_sentinel);
    list->size        = 0;
    list->lcnt        = 0;

    list->append   = allocate_append_buffer(&list->list_arena);
    list->original = allocate_original_buffer(
        &list->list_arena, original_text, original_text_len);

    list->first_free_node = NULL;
    list->root_sentinel.size = 0;
    list->root_sentinel.lcnt = 0;
    list->root_sentinel.count = 0;

    initialize_arena_with_size(&list->history_arena, 64 * 4094);
    initialize_arena(&list->insert_state_arena);
    initialize_undo_history(&list->history);
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

    list->top_changed = 0;
    list->bot_changed = num_logical_lines(list);
    list->changed = original_text != 0;
    list->lines_inserted = list->lcnt;
    list->lines_deleted  = 0;
    list->wrapped = false;

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

    base_init_(list, Position, &list->iter);
    INIT_LIST_HEAD(&list->window_sentinel);
}

static void init_buffer(piece_list *list, char *filepath)
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

static void write_buffer_to_file(piece_list *list)
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

static void search_str(piece_list *list, u32 window_size, str search_string)
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

    for(;;)
    {
        if (curr_node == &list->root_sentinel)
        {
            break;
        }

        u32 prev_piece_match_index = 0;
        u32 prev_piece_match_start = 0;

        u32 i = 0;

        while (i < curr_node->count)
        {
            piece *curr_piece = curr_node->pieces + i;
            const buffer *buffer = get_buffer(list, curr_piece->type);
            u32 offset = buffer->lines[curr_piece->off.row] + curr_piece->off.col;
            u8 *piece_text_start = buffer->text + offset;

            u32 piece_cursor = 0;

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
                            // g_push(&list->matches, match_position);
                            in_match = false;
                            prev_piece_match_index = i;
                            prev_piece_match_start = piece_cursor + remaining_text_size;
                            prev_node_match = curr_node;
                            prev_position = current_position;
                            current_match_cursor = 0;

                        }
                        else
                        {
                            current_match_cursor += test_size;
                        }
                        piece_cursor += remaining_text_size;
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

    // for (segmented_node *node = list->root_sentinel.next;
    //     node != &list->root_sentinel;
    //     node = node->next)
    // {
    //     u32 piece_cursor = 0;
    //     for (u32 i = 0; i < node->count; ++i)
    //     {
    //         piece *piece = node->pieces + i;
    //         const buffer *buffer = get_buffer(list, piece->type);
    //         u32 offset = buffer->lines[piece->off.row] + piece->off.col;
    //         u8 *piece_text_start = buffer->text + offset;
    //
    //         while (piece_cursor < piece->size)
    //         {
    //             u8 *text = text_piece_start + piece_cursor;
    //
    //             if (in_match)
    //             {
    //                 Assert(current_match_cursor > 0);
    //                 u32 remaining_text_size = search_string.len - current_match_cursor;
    //                 u32 test_size = Minimum(remaining_text_size, piece->size - piece_cursor);
    //
    //                 if (!memcmp(search_string.buffer + current_match_cursor, text, test_size))
    //                 {
    //                     if (remaining_text_size <= piece->size - piece_cursor)
    //                     {
    //                         // found match.
    //                         // record position. 
    //                         list->match_len = search_string.len;
    //                         in_match = false;
    //                     }
    //                     piece_cursor += remaining_text_size;
    //                 }
    //                 else
    //                 {
    //                     // Must restart after the start of the current failed match;
    //                     // This may be in a previous piece or node;
    //
    //                 }
    //             }
    //             else
    //             {
    //
    //
    //             }
    //         }
    //     }
    // }
}

static piece_list *create_buffer(memory_arena *arena, char *filepath)
{
    piece_list *buffer = PushStruct(arena, piece_list, NoClear());
    init_buffer(buffer, filepath);
    return buffer;
}

static inline void initialize_piece_list_s(piece_list *list, string original)
{
    initialize_piece_list(list, original.buffer, original.len);
}

static void write_to_buffer(piece_list *list, u8 *buf, u32 len)
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

static replace_result rand_replace_(piece_list *list, prng *prng)
{
    replace_result result = {};
    if (list->size + num_insert_pieces == 0) 
    {
        return result;
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
                    // Is this necessary;
                    reset_cursor_(&list->iter);
                }
            } break;
        }
    }


    u32 line_len_a = get_line_len_(&list->iter, a.y);
    u32 line_len_b = get_line_len_(&list->iter, b.y);
    a.x = Minimum(a.x, line_len_a);
    b.x = Minimum(b.x, line_len_b);

    piece *pieces = (piece *) malloc(num_ins_pieces * sizeof(piece));;

    for (u32 i = 0; i < num_ins_pieces; ++i)
    {
        INIT_STACK_STRING(s, MAX_STRING_LEN)
        rand_ascii_string(&s, prng, 1, MAX_STRING_LEN);
        if (s.len > 0)
        {
            pieces[i] = make_piece_s(list, from_string(s));
        }
    }

    piece_range p_range = { .pieces = pieces, .count = num_ins_pieces };
    if (compare(a, b) == EqualTo && num_ins_pieces == 0)
    {
    }
    else
    {
        result = range_replace(list, a, b, p_range);
    }
    free(pieces);
    return result;
}


static piece_list *rand_list(prng *prng)
{
    piece_list *list = BootstrapPushStruct(piece_list, list_arena, 4096);

    string original_text = rand_ascii_string_alloc(prng, &list->list_arena, 0, MAX_ORIGINAL_STRING_LEN);
    initialize_piece_list_s(list, original_text);

    for (u32 i = 0; i < num_edits; ++i)
    {
        rand_replace_(list, prng);
    }

    return list;
}

#endif
