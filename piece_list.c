#include "history.c"

// TODO: There is a lot of addition and subtraction of pairs (size, lcnt).
// => cast make this fields adjacent in all the structs they appear in and,
// then cast the pair to a u64 and add as u64.
// Theoretically a file could be bigger than 4Gb (Who the fuck edits 4Gb files).
// Anyway if a file is bigger than 4gb we may need to partition it into chunks,
// and the buffer type will no longer be an enum. Instead it will be an ordinal.

static b32 lists_are_equal(const piece_list *a, const piece_list *b)
{
    b32 result = (a->num_pieces == b->num_pieces) && 
                 (a->size == b->size) && 
                 (a->lcnt == b->lcnt);

    result = result && buffers_are_equal(&a->original, &b->original);
    result = result && buffers_are_equal(&a->append, &b->append);
    result = result && semantic_equality(&a->root_sentinel, &b->root_sentinel);
    result = result && histories_are_equal(a->undo_history, b->undo_history);
    return result;
}

static inline u32 num_logical_lines(piece_list *buffer) 
{
    u32 result = buffer->lcnt + (buffer->size > 0);
    return result;
}

static inline u32 num_logical_lines_(piece_list *buffer)
{

    base_iter iter = {};
    u32 result = buffer->lcnt;
    if (base_init_rev_(buffer, LineNumber, &iter))
    {

        // base_prev_line(&iter);
        u32 last_line_len = buffer->size - get_position(&iter);

        if (last_line_len > 0)
        {
            result++;
        }
    }
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
    u32 num_pieces = 0;
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
        num_pieces += node->count;
    }
    Assert(size == list->size);
    Assert(lcnt == list->lcnt);
    Assert(num_pieces == list->num_pieces);
    Assert((list->append.num_lines + list->original.num_lines) >= list->lcnt);
    Assert((list->append.text_len  + list->original.text_len) >= list->size);
}

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
    piece piece = { .lcnt = buffer->num_lines - 1, .size = buffer->text_len };
    return piece;
}

static buffer allocate_append_buffer(memory_arena *arena)
{
    buffer buffer;
    buffer.text_len = 0;
    buffer.text_capacity = Megabytes(16);
    buffer.text = PushArray(arena, buffer.text_capacity, u8, NoClear());

    buffer.num_lines = 1;
    buffer.lines_capacity = Megabytes(1);
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

static piece make_piece(piece_list *list, u8 *text, u32 text_len)
{
    u32 append_len    = list->append.text_len;
    u32 start_row_idx = list->append.num_lines - 1;
    u32 start_row     = list->append.lines[start_row_idx];
    u32 start_col     = list->append.text_len - start_row; 

    Assert(list->append.text_capacity > list->append.text_len + text_len);
    memcpy(list->append.text + list->append.text_len, text, text_len);
    list->append.text_len += text_len;

    for (u32 i = 0; i < text_len; i++)
    {
        if (text[i] == '\n')
        {
            Assert(list->append.lines_capacity > list->append.num_lines + 1);
            list->append.lines[list->append.num_lines++] = append_len + i + 1;
        }
    }

    u32 end_row_idx = list->append.num_lines - 1;
    piece new_piece;
    new_piece.size    = text_len;
    new_piece.lcnt    = end_row_idx - start_row_idx;
    new_piece.off.row = start_row_idx;
    new_piece.off.col = start_col;

    return new_piece;
}

static inline piece make_piece_s(piece_list *list, string s)
{
    piece result = make_piece(list, s.buffer, s.len);
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

static void move_to_line(piece_list *list, u32 line)
{
    list->iter.type &= ~Position;
    u32 current_line = get_line_number(&list->iter);

    if (line < current_line)
    {
        base_advance_rev_by_line(&list->iter, line - current_line);
    } else if (line > current_line)
    {
        base_advance_by_line(&list->iter, current_line - line);
    }

    normalize(&list->iter);
}

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
    else
    {
    }

    normalize(&iter);
    *last_location = iter;

    return iter;
}

static base_iter find_cursor(base_iter *last_location, u32 cy, u32 cx )
{
    base_iter iter = find_line(last_location, cy);
    base_advance_by_cell(&iter, cx);
    return iter;
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

static u32 get_line_len(base_iter *last_location, u32 line)
{
    base_iter line_start = find_line(last_location, line);
    if (line == last_location->list->lcnt)
    {
        return last_location->list->size - get_position(&line_start);
    }
    else
    {
        base_iter line_end = find_line(last_location, line + 1);
        return get_position(&line_end) - get_position(&line_start) - 1;
    }
}

static void move_to(piece_list *list, u32 position)
{
    list->iter.type &= ~LineNumber;
    u32 last_position = get_position(&list->iter);

    if (position < last_position)
    {
        base_advance_pos_rev_by(&list->iter, last_position - position);
    }
    else if (position > last_position)
    {
        base_advance_pos_by(&list->iter, position - last_position);
    }

    // list->cy = get_line_number(&list->iter);

    // base_iter start_line_iter = find_line(&list->iter, list->cy);
    // u32 start_line_pos        = get_position(&start_line_iter);

    
    normalize(&list->iter);
}

static void maybe_merge_with_next(piece_list *list, segmented_node *node)
{
    segmented_node *next_node = node->next;
    if (next_node != &list->root_sentinel)
    {
        piece *src_pieces = next_node->pieces;
        piece *dst_pieces = node->pieces + node->count;
        buffer_type *src_types = next_node->b_types;
        buffer_type *dst_types = node->b_types + node->count;

        if (node->count + next_node->count <= MAX_PIECES_PER_NODE)
        {
            memcpy(dst_pieces, src_pieces, sizeof(piece) * next_node->count);
            memcpy(dst_types,  src_types,  sizeof(buffer_type) * next_node->count);

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
            memcpy(dst_types,  src_types,  sizeof(buffer_type) * right_to_left_transfer_count);

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

            src_types = next_node->b_types + right_to_left_transfer_count;
            dst_types = next_node->b_types;
            memmove(dst_types, src_types, sizeof(buffer_type) * next_node->count);
        }
    }
}

static void append(piece_list  *list, piece *pieces, buffer_type *types, u32 num_pieces)
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
        memcpy(new_node->b_types, types  + at, sizeof(buffer_type) * pieces_per_this_node);

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
        piece       *pieces = node->pieces;
        buffer_type *types  = node->b_types;

        u32 num = node->count - start;
        node->count += count;

        memmove(pieces + start + count, pieces + start, sizeof(piece)       * num);
        memmove(types  + start + count, types  + start, sizeof(buffer_type) * num);
        result = at;
    }
    else
    {
        u32 num_nodes       = 1 + num_alloc;
        u32 pieces_per_node = total / num_nodes;
        u32 rem             = total % num_nodes;

        u32 left_len = start;

        piece *left_pieces      = node->pieces;
        buffer_type *left_types = node->b_types;

        u32 right_len = node->count - at.piece_index;

        piece *right_pieces      = node->pieces  + at.piece_index;
        buffer_type *right_types = node->b_types + at.piece_index;

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

            buffer_type *dst_types = new_node->b_types + right_start;
            buffer_type *src_types = right_types + right_end - right_to_new_count;

            memcpy(dst_pieces, src_pieces, sizeof(piece)       * right_to_new_count);
            memcpy(dst_types,  src_types,  sizeof(buffer_type) * right_to_new_count);

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

            dst_types = new_node->b_types;
            src_types = left_types + from_left;

            memcpy(dst_pieces, src_pieces, sizeof(piece)       * pieces_start);
            memcpy(dst_types,  src_types,  sizeof(buffer_type) * pieces_start);

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

        buffer_type *src_types = node->b_types + at.piece_index;
        buffer_type *dst_types = node->b_types + at.piece_index + pieces_remaining;

        u32 count = (node->count - right_count) - at.piece_index;

        memmove(dst_pieces, src_pieces, sizeof(piece)       * count);
        memmove(dst_types,  src_types,  sizeof(buffer_type) * count);
        
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
    buffer_type *types_start = start.node->b_types + start.piece_index;
    if (start.node == end.node)
    {
        Assert(count == (end.piece_index - start.piece_index));
        copy_slice(slice_cursor, piece_start, types_start, count);
            // start.node->pieces + start.piece_index,
            // start.node->b_types + start.piece_index,
            // count);
    }
    else
    {
        u32 len = start.node->count - start.piece_index;
        copy_slice(slice_cursor, piece_start, types_start, len);
            // start.node->pieces + start.piece_index,
            // start.node->b_types + start.piece_index,
            // len);

        for (segmented_node *node = start.node->next;
            node != end.node;
            node = node->next)
        {
            copy_slice(slice_cursor, node->pieces, node->b_types, node->count);

        }
        copy_slice(slice_cursor, end.node->pieces, end.node->b_types, end.piece_index);
    }
}

static void replace(
    piece_list *list,
    cursor start,
    cursor end,
    piece *pieces,
    buffer_type *types,
    u32 num_pieces)
{
    if (start.node == &list->root_sentinel)
    {
        append(list, pieces, types, num_pieces);
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

            piece *src_piece      = node->pieces + end.piece_index;
            piece *dst_piece      = node->pieces + start.piece_index + num_pieces;

            buffer_type *src_type = node->b_types + end.piece_index;
            buffer_type *dst_type = node->b_types + start.piece_index + num_pieces;

            u32 num_moved = node->count - end.piece_index;

            if (start.piece_index + num_pieces < total)
            {
                memmove(dst_piece, src_piece, sizeof(piece) * num_moved);
                memmove(dst_type,  src_type,  sizeof(buffer_type) * num_moved);
            }

            src_piece = node->pieces  + start.piece_index;
            src_type  = node->b_types + start.piece_index;

            memcpy(src_piece, pieces, sizeof(piece) * num_pieces);
            memcpy(src_type,  types,  sizeof(buffer_type) * num_pieces);

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

            piece *left_pieces      = node->pieces;
            buffer_type *left_types = node->b_types;

            u32 delete_end = start.piece_index + distance;

            u32 right_len = node->count - delete_end;

            piece *right_pieces      = node->pieces  + delete_end;
            buffer_type *right_types = node->b_types + delete_end;

            u32 left_count   = 0; // # pieces copied so far from left part of split to newly allocated nodes
            u32 right_count  = 0; // # pieces copied so far from right part of split to newly allocated nodes
            u32 pieces_count = 0; // # pieces copied so far from *pieces* to newly allocated nodes

            Assert(num_alloc > 0);
            for (u32 i = 0; i < num_alloc; ++i)
            {
                segmented_node *new_node;
                FREELIST_ALLOCATE(
                    new_node,
                    list->first_free_node,
                    PushStruct(&list->list_arena, segmented_node, NoClear())
                );
                // Copy pieces from the right part of the split to the newly allocated node
                u32 pieces_per_this_node = (rem > 0) ? pieces_per_node + 1 : pieces_per_node;
                u32 right_remaining = (right_count > right_len) ? 0 : right_len - right_count;
                u32 right_to_new_count = Minimum(pieces_per_this_node, right_remaining);
                u32 right_start = pieces_per_this_node - right_to_new_count;
                u32 right_end   = right_len - right_count;

                piece *dst_pieces = new_node->pieces + right_start;
                piece *src_pieces = right_pieces + right_end - right_to_new_count;

                buffer_type *dst_types = new_node->b_types + right_start;
                buffer_type *src_types = right_types + right_end - right_to_new_count;

                memcpy(dst_pieces, src_pieces, sizeof(piece) * right_to_new_count);
                memcpy(dst_types, src_types, sizeof(buffer_type) * right_to_new_count);

                right_count += right_to_new_count;
                // Copy pieces from the new pieces to new_node
                u32 pieces_end = num_pieces - pieces_count;
                u32 pieces_to_new_count = Minimum(pieces_end, right_start);
                u32 pieces_start = right_start - pieces_to_new_count;

                u32 rest_pieces = pieces_end - pieces_to_new_count;

                dst_pieces = new_node->pieces + pieces_start;
                src_pieces = pieces + rest_pieces;

                dst_types = new_node->b_types + pieces_start;
                src_types = types + rest_pieces;

                memcpy(dst_pieces, src_pieces, sizeof(piece) * pieces_to_new_count);
                memcpy(dst_types, src_types, sizeof(buffer_type) * pieces_to_new_count);

                // Copy pieces from the left part of the split to the newly allocated node
                pieces_count += pieces_to_new_count;

                u32 from_left = left_len - pieces_start;
                dst_pieces = new_node->pieces;
                src_pieces = left_pieces + from_left;

                dst_types = new_node->b_types;
                src_types = left_types + from_left;

                memcpy(dst_pieces, src_pieces, sizeof(piece)       * pieces_start);
                memcpy(dst_types,  src_types,  sizeof(buffer_type) * pieces_start);

                left_count += pieces_start;
                new_node->count = pieces_per_this_node;

                fix_size_and_lines(new_node);

                rem = (rem > 0) ? (rem - 1) : 0;

                DLIST_INSERT_AFTER(node, new_node);
            }
            u32 pieces_remaining = num_pieces - pieces_count;

            piece *src_pieces = node->pieces + delete_end;
            piece *dst_pieces = node->pieces + start.piece_index + pieces_remaining;

            buffer_type *src_types = node->b_types + delete_end;
            buffer_type *dst_types = node->b_types + start.piece_index + pieces_remaining;

            u32 count = (node->count - right_count) - delete_end;

            memmove(dst_pieces,src_pieces, sizeof(piece)       * count);
            memmove(dst_types, src_types,  sizeof(buffer_type) * count);

            u32 left_split = start.piece_index - left_count;
            dst_pieces = node->pieces  + left_split;
            dst_types  = node->b_types + left_split;
            
            memcpy(dst_pieces, pieces, sizeof(piece)       * pieces_remaining); 
            memcpy(dst_types,  types,  sizeof(buffer_type) * pieces_remaining);

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

            piece *dst_pieces      = left->pieces  + start.piece_index;
            buffer_type *dst_types = left->b_types + start.piece_index;

            memcpy(dst_pieces, pieces, sizeof(piece) * num_pieces);
            memcpy(dst_types,  types,  sizeof(buffer_type) * num_pieces);

            dst_pieces += num_pieces;
            dst_types  += num_pieces;

            memcpy(dst_pieces, right->pieces  + end.piece_index, sizeof(piece) * right_len);
            memcpy(dst_types,  right->b_types + end.piece_index, sizeof(buffer_type) * right_len);

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
                FREELIST_ALLOCATE(
                    new_node,
                    list->first_free_node,
                    PushStruct(&list->list_arena, segmented_node, NoClear())
                );

                u32 pieces_per_this_node = pieces_per_node + (rem > 0);
                u32 node_cursor = pieces_per_this_node - right_to_left;

                memcpy(new_node->pieces + node_cursor, right->pieces + end.piece_index, sizeof(piece) * right_to_left);
                memcpy(new_node->b_types + node_cursor, right->b_types + end.piece_index, sizeof(buffer_type) * right_to_left);

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

                memmove(
                    right->pieces + right_to_left_rev,
                    right->pieces + end.piece_index + right_to_left,
                    sizeof(piece) * (right_len - right_to_left));
                memmove(
                    right->b_types + right_to_left_rev,
                    right->b_types + end.piece_index + right_to_left,
                    sizeof(buffer_type) * (right_len - right_to_left));

                // Copy from *pieces* to *right*

                memcpy(right->pieces, pieces + pieces_cursor - right_to_left_rev, sizeof(piece) * right_to_left_rev);
                memcpy(right->b_types, types + pieces_cursor - right_to_left_rev, sizeof(buffer_type) * right_to_left_rev);

                pieces_cursor -= right_to_left_rev;

                // Copy from *pieces* to *new_node*
                u32 const rest_pieces_node = Minimum(pieces_cursor, node_cursor);

                memcpy(
                    new_node->pieces + node_cursor - rest_pieces_node,
                    pieces + pieces_cursor - rest_pieces_node,
                    sizeof(piece) * rest_pieces_node);
                memcpy(
                    new_node->b_types + node_cursor - rest_pieces_node,
                    types + pieces_cursor - rest_pieces_node,
                    sizeof(buffer_type) * rest_pieces_node);

                node_cursor -= rest_pieces_node;
                pieces_cursor -= rest_pieces_node;

                u32 const left_to_node = Minimum(left_to_right, node_cursor);

                memcpy(new_node->pieces, left->pieces + start.piece_index - left_to_node, sizeof(piece) * left_to_node);
                memcpy(new_node->b_types, left->b_types + start.piece_index - left_to_node, sizeof(buffer_type) * left_to_node);

                Assert(pieces_cursor >= left_to_right_rev);

                // Copy from *pieces* to *left*
                u32 const pieces_to_left = (pieces_cursor == left_to_right_rev) * left_to_right_rev;

                memcpy(left->pieces  + start.piece_index, pieces, sizeof(piece)       * pieces_to_left);
                memcpy(left->b_types + start.piece_index, types , sizeof(buffer_type) * pieces_to_left);

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
            memcpy(left->b_types + start.piece_index, types, sizeof(buffer_type) * left_to_right_rev);

            // Copy *right* to *left*
            memcpy(
                left->pieces + start.piece_index + left_to_right_rev,
                right->pieces + end.piece_index,
                sizeof(piece) * right_to_left);
            memcpy(
                left->b_types + start.piece_index + left_to_right_rev,
                right->b_types + end.piece_index,
                sizeof(buffer_type) * right_to_left);

            // Adjust *right*
            memmove(
                right->pieces + left_to_right + right_to_left_rev,
                right->pieces + end.piece_index + right_to_left,
                sizeof(piece) * (right_len - right_to_left));
            memmove(
                right->b_types + left_to_right + right_to_left_rev,
                right->b_types + end.piece_index + right_to_left,
                sizeof(buffer_type) * (right_len - right_to_left));

            // Copy *pieces* to *right*
            memcpy(
                right->pieces + left_to_right,
                pieces + num_pieces - right_to_left_rev,
                sizeof(piece) * right_to_left_rev);
            memcpy(
                right->b_types + left_to_right,
                types + num_pieces - right_to_left_rev,
                sizeof(buffer_type) * right_to_left_rev);
            // Copy *left* to *right*
            u32 copy_offset = start.piece_index - left_to_right;
            memcpy(right->pieces,  left->pieces  + copy_offset, sizeof(piece) * left_to_right);
            memcpy(right->b_types, left->b_types + copy_offset, sizeof(buffer_type) * left_to_right);

            left->count  = left_pieces_per_node;
            right->count = right_pieces_per_node;
            fix_size_and_lines(left);
            fix_size_and_lines(right);
        }
    }
}

static void redo(piece_list *list)
{
    undo_memory_header **first_header = undo_node_unpop(&list->undo_history);

    if (first_header && *first_header)
    {
        undo_memory_header *prev_header = 0;

        for (undo_memory_header *header = *first_header;
            header;
            header = header->next)
        {
            cursor start_cursor = abs_idx_to_cursor_2(list, header->abs_idx);
            cursor end_cursor   = abs_idx_to_cursor_2(list, header->abs_idx + header->ins_count);

            undo_memory_header *redo_header = allocate_undo_memory_block(
                &list->undo_history,
                &list->history_arena,
                header->ins_count);

            redo_header->ins_count = header->del_count;
            redo_header->del_count = header->ins_count;
            redo_header->abs_idx   = header->abs_idx;

            slice_cursor slice_cursor = { .cursor = 0 };
            get_slice_cursor_from_header(&slice_cursor, redo_header);

            copy_range(start_cursor, end_cursor, header->ins_count, &slice_cursor);

            piece *pieces = get_pieces_from_header(header);
            buffer_type *types = get_types_from_header(header);

            list->num_pieces += header->del_count;
            list->num_pieces -= slice_cursor.count;

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

            replace(list, start_cursor, end_cursor, pieces, types, header->del_count);

            redo_header->next = prev_header;
            prev_header = redo_header;
        }
        free_undo_memory_block(&list->undo_history, *first_header);
        *first_header = prev_header;
        reset_cursor_(&list->iter);
    }
}

static void undo_(window *win)
{
    piece_list *list = win->buffer;
    undo_node **curr_node = &list->undo_history.curr_node;

    if (*curr_node)
    {
        // swap cursor positions;
        u32 prev_cx = win->bcx;
        u32 prev_cy = win->bcy;
        win->bcx = (*curr_node)->cx;
        win->bcy = (*curr_node)->cy;
        (*curr_node)->cx = prev_cx;
        (*curr_node)->cy = prev_cy;

        undo_memory_header **first_header = &(*curr_node)->data;
        undo_memory_header *prev_header = 0;

        for (undo_memory_header *header = *first_header;
            header;
            header = header->next)
        {
            cursor start_cursor = abs_idx_to_cursor_2(list, header->abs_idx);
            cursor end_cursor   = abs_idx_to_cursor_2(list, header->abs_idx + header->ins_count);

            undo_memory_header *redo_header = allocate_undo_memory_block(&list->undo_history, &list->history_arena, header->ins_count);

            redo_header->ins_count = header->del_count;
            redo_header->del_count = header->ins_count;
            redo_header->abs_idx   = header->abs_idx;

            slice_cursor slice_cursor = {};
            get_slice_cursor_from_header(&slice_cursor, redo_header);

            copy_range(start_cursor, end_cursor, header->ins_count, &slice_cursor);

            piece *pieces = get_pieces_from_header(header);
            buffer_type *types = get_types_from_header(header);

            list->num_pieces += header->del_count;
            list->num_pieces -= slice_cursor.count;

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
            replace(list, start_cursor, end_cursor, pieces, types, header->del_count);

            redo_header->next = prev_header;
            prev_header = redo_header;
        }
        free_undo_memory_block(&list->undo_history, *first_header);
        *first_header = prev_header;
        reset_cursor_(&list->iter);
        *curr_node = (*curr_node)->parent;
    }
}

// static void replace_range_(
//     piece_list *list,
//     base_iter start,
//     base_iter end,
//     piece *pieces,
//     buffer_type *types,
//     u32 num_pieces)
// {
//     list->num_pieces += num_pieces;
//
//     piece *start_piece = get_piece_(&start);
//     piece *end_piece   = get_piece_(&end);
//
//     buffer_type *start_type = get_type_(&start);
//     buffer_type *end_type   = get_type_(&end);
//
//     u32 start_size = start_piece ? start_piece->size : UINT32_MAX;
//
//     u32 num_undo_pieces = 
//         (end.abs_idx + (end.pos_in_piece > 0)) - 
//         (start.abs_idx + (start.pos_in_piece == start_size));
//
//     undo_memory_header *undo_header = allocate_undo_memory_block(
//         &list->undo_history,
//         &list->history_arena,
//         num_undo_pieces);
//
//     undo_header->abs_idx   = start.abs_idx + (start.pos_in_piece == start_size);
//     undo_header->ins_count = num_pieces;
//     undo_header->del_count = num_undo_pieces;
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
//     if (start_piece == end_piece && start.pos_in_piece > 0 && end.pos_in_piece < end_piece->size)
//     {
//         list->num_pieces++;
//
//         copy_slice(&slice_cursor, start_piece, start_type, 1);
//
//         offset offset = get_offset(&end);
//
//         piece right = { 
//             .size = start_piece->size - end.pos_in_piece,
//             .lcnt = start_piece->lcnt - end.line_in_piece,
//             .off  = offset
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
//         piece *all_pieces[2] =  { pieces, &right };
//         buffer_type *all_types[2]  =  { types, start_type };
//
//         u32 count[2] = { num_pieces, 1 };
//
//         for (u32 i = 0; i < ArrayCount(all_pieces); ++i)
//         {
//             u32 slice_count  = count[i];
//             u32 slice_cursor = 0;
//
//             piece *piece_slice      = all_pieces[i];
//             buffer_type *type_slice = all_types[i];
//
//             while (slice_cursor < slice_count)
//             {
//                 piece *src_pieces      = piece_slice + slice_cursor;
//                 buffer_type *src_types = type_slice  + slice_cursor;
//
//                 piece *dst_pieces      = start_cursor.node->pieces  + start_cursor.piece_index;
//                 buffer_type *dst_types = start_cursor.node->b_types + start_cursor.piece_index;
//
//                 u32 remaining = slice_count - slice_cursor;
//                 u32 copy_amount = Minimum(remaining, (start_cursor.node->count - start_cursor.piece_index));
//
//                 memcpy(dst_pieces, src_pieces, sizeof(piece)       * copy_amount);
//                 memcpy(dst_types,  src_types,  sizeof(buffer_type) * copy_amount);
//
//                 for (u32 j = 0; j < copy_amount; ++j)
//                 {
//                     piece piece = src_pieces[j];
//                     start_cursor.node->size += piece.size;
//                     start_cursor.node->lcnt += piece.lcnt;
//                 }
//
//                 if (start_cursor.node->count == start_cursor.piece_index + copy_amount)
//                 {
//                     start_cursor.node = start_cursor.node->next;
//                     start_cursor.piece_index = 0;
//                 }
//                 else
//                 {
//                     start_cursor.piece_index += copy_amount;
//                 }
//                 slice_cursor += copy_amount;
//             }
//         }
//         undo_header->ins_count += 2;
//     }
//     else
//     {
//         if (start.pos_in_piece > 0)
//         {
//             if (start.pos_in_piece < start_piece->size)
//             {
//                  write_start(&slice_cursor, *start_piece, *start_type);
//
//                  start_cursor.node->size -= start_piece->size - start.pos_in_piece;
//                  start_cursor.node->lcnt -= start_piece->lcnt - start.line_in_piece;
//
//                  start_piece->size = start.pos_in_piece;
//                  start_piece->lcnt = start.line_in_piece;
//
//                  undo_header->ins_count++;
//             }
//             add_to_cursor(list, &start_cursor, 1);
//             start.abs_idx++;
//         }
//
//         if (end.pos_in_piece > 0)
//         {
//             if (end.pos_in_piece < end_piece->size)
//             {
//                 write_last(&slice_cursor, *end_piece, *end_type);
//
//                 end_cursor.node->size -= end.pos_in_piece;
//                 end_cursor.node->lcnt -= end.line_in_piece;
//
//                 end_piece->off = get_offset(&end);
//                 end_piece->size -= end.pos_in_piece;
//                 end_piece->lcnt -= end.line_in_piece;
//                 undo_header->ins_count++;
//             }
//             else
//             {
//                 add_to_cursor(list, &end_cursor, 1);
//                 end.abs_idx++;
//             }
//         }
//
//         u32 num_pieces_deleted = end.abs_idx - start.abs_idx;
//         list->num_pieces -= num_pieces_deleted;
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
//             replace(list, start_cursor, end_cursor, pieces, types, num_pieces);
//         }
//     }
//
//     insert_at_current(
//         &list->undo_history,
//         &list->history_arena,
//         undo_header,
//         active_window->bcx,
//         active_window->bcy);
//
//     reset_cursor_(&list->iter);
// }

typedef struct
{
    u32 num_copied_pieces;
    piece       *copied_pieces;
    buffer_type *copied_types;
} copied;


static inline u32 num_deleted_pieces(base_iter start, base_iter end)
{
    Assert(position(&start) <= position(&end));
    u32 result = 0;

    if (position(&start) != position(&end))
    {
        u32 end_idx   = end.abs_idx + (end.pos_in_piece != 0);
        u32 start_idx = start.abs_idx + (start.pos_in_piece != get_piece_(&start)->size);
        result = end_idx - start_idx;

    }
    return result;
}

// static inline copied get_copied_from_undo(undo_memory_header *header, base_iter start, base_iter end)
// {
    // copied result = {};
    // result.num_copied_pieces = 
    //
    // u32 copied_size  = get_data_size(header);
    //
    // void *data_start = get_data_start(header);
    //
    // void *copied_data = malloc(copied_size);

    // memcpy(copied_data, 



    // piece *piecs = get_pieces_from_header(

//
    // return result;

// }

static undo_memory_header *replace_range(
    piece_list *list,
    base_iter start,
    base_iter end,
    piece *pieces, 
    buffer_type *types,
    u32 num_pieces)
{
    list->num_pieces += num_pieces;

    piece *start_piece = get_piece_(&start);
    piece *end_piece   = get_piece_(&end);

    buffer_type *start_type = get_type_(&start);
    buffer_type *end_type   = get_type_(&end);

    u32 start_size = start_piece ? start_piece->size : UINT32_MAX;

    u32 num_undo_pieces = (end.abs_idx + (end.pos_in_piece > 0)) - 
                          (start.abs_idx + (start.pos_in_piece == start_size));

    undo_memory_header *undo_header = 
        allocate_undo_memory_block(&list->undo_history, &list->history_arena, num_undo_pieces);

    undo_header->abs_idx   = start.abs_idx + (start.pos_in_piece == start_size);
    undo_header->ins_count = num_pieces;
    undo_header->del_count = num_undo_pieces;

    slice_cursor slice_cursor = {};
    get_slice_cursor_from_header(&slice_cursor, undo_header);

    for (u32 i = 0; i < num_pieces; ++i)
    {
        piece piece = pieces[i];
        list->size += piece.size;
        list->lcnt += piece.lcnt;
    }

    u32 num_lines_deleted = line_number(&end) - line_number(&start);
    u32 num_chars_deleted = position(&end) - position(&start);

    list->size -= num_chars_deleted;
    list->lcnt -= num_lines_deleted;

    cursor start_cursor = { start.node, start.piece_idx };
    cursor end_cursor   = { end.node, end.piece_idx };

    // u32 deleted_count = num_deleted_pieces(start, end);

    if (start_piece == end_piece && start.pos_in_piece > 0 && end.pos_in_piece < end_piece->size)
    {
        // if (deleted_count > 0)
        // {
        //     piece erased = {
        //         .size = end.pos_in_piece - start.pos_in_piece,
        //         .lcnt = end.line_in_piece - start.line_in_piece,
        //         .off  = get_offset(&start)
        //     };
        // }
        list->num_pieces++;

        copy_slice(&slice_cursor, start_piece, start_type, 1);

        offset offset = get_offset(&end);

        piece right = { 
            .size = start_piece->size - end.pos_in_piece,
            .lcnt = start_piece->lcnt - end.line_in_piece,
            .off  = offset
        };

        start_cursor.node->size -= start_piece->size - start.pos_in_piece;
        start_cursor.node->lcnt -= start_piece->lcnt - start.line_in_piece;

        start_piece->size = start.pos_in_piece;
        start_piece->lcnt = start.line_in_piece;

        add_to_cursor(list, &start_cursor, 1);

        start_cursor = make_space_for_at(list, start_cursor, num_pieces + 1);

        piece *all_pieces[2] =  { pieces, &right };
        buffer_type *all_types[2]  =  { types, start_type };

        u32 count[2] = { num_pieces, 1 };

        for (u32 i = 0; i < ArrayCount(all_pieces); ++i)
        {
            u32 slice_count  = count[i];
            u32 slice_cursor = 0;

            piece *piece_slice      = all_pieces[i];
            buffer_type *type_slice = all_types[i];

            while (slice_cursor < slice_count)
            {
                piece *src_pieces      = piece_slice + slice_cursor;
                buffer_type *src_types = type_slice  + slice_cursor;

                piece *dst_pieces      = start_cursor.node->pieces  + start_cursor.piece_index;
                buffer_type *dst_types = start_cursor.node->b_types + start_cursor.piece_index;

                u32 remaining = slice_count - slice_cursor;
                u32 copy_amount = Minimum(remaining, (start_cursor.node->count - start_cursor.piece_index));

                memcpy(dst_pieces, src_pieces, sizeof(piece)       * copy_amount);
                memcpy(dst_types,  src_types,  sizeof(buffer_type) * copy_amount);

                for (u32 j = 0; j < copy_amount; ++j)
                {
                    piece piece = src_pieces[j];
                    start_cursor.node->size += piece.size;
                    start_cursor.node->lcnt += piece.lcnt;
                }

                if (start_cursor.node->count == start_cursor.piece_index + copy_amount)
                {
                    start_cursor.node = start_cursor.node->next;
                    start_cursor.piece_index = 0;
                }
                else
                {
                    start_cursor.piece_index += copy_amount;
                }
                slice_cursor += copy_amount;
            }
        }
        undo_header->ins_count += 2;
    }
    else
    {
        if (start.pos_in_piece > 0)
        {
            if (start.pos_in_piece < start_piece->size)
            {
                 write_start(&slice_cursor, *start_piece, *start_type);
                 start_cursor.node->size -= start_piece->size - start.pos_in_piece;
                 start_cursor.node->lcnt -= start_piece->lcnt - start.line_in_piece;
                 start_piece->size = start.pos_in_piece;
                 start_piece->lcnt = start.line_in_piece;
                 undo_header->ins_count++;
            }
            add_to_cursor(list, &start_cursor, 1);
            start.abs_idx++;
        }

        if (end.pos_in_piece > 0)
        {
            if (end.pos_in_piece < end_piece->size)
            {
                write_last(&slice_cursor, *end_piece, *end_type);
                end_cursor.node->size -= end.pos_in_piece;
                end_cursor.node->lcnt -= end.line_in_piece;
                end_piece->off = get_offset(&end);
                end_piece->size -= end.pos_in_piece;
                end_piece->lcnt -= end.line_in_piece;
                undo_header->ins_count++;
            }
            else
            {
                add_to_cursor(list, &end_cursor, 1);
                end.abs_idx++;
            }
        }

        u32 num_pieces_deleted = end.abs_idx - start.abs_idx;
        list->num_pieces -= num_pieces_deleted;

        u32 count = end.abs_idx - start.abs_idx;

        if (count)
        {
            copy_range(start_cursor, end_cursor, count, &slice_cursor);
        }

        if (count + num_pieces > 0)
        {
            replace(list, start_cursor, end_cursor, pieces, types, num_pieces);
        }
    }
    reset_cursor_(&list->iter);

    return undo_header;
}

static undo_memory_header *range_replace(
    piece_list *list, 
    u32 cy_0,
    u32 cx_0,
    u32 cy_1,
    u32 cx_1,
    piece *pieces,
    buffer_type *types,
    u32 num_pieces)
{
    base_iter start = find_cursor(&list->iter, cy_0, cx_0);
    base_iter end   = find_cursor(&list->iter, cy_1, cx_1);

    undo_memory_header *result = replace_range(list, start, end, pieces, types, num_pieces);
    return result;
}

static void initialize_piece_list(piece_list *list, u8 *original_text, u32 original_text_len)
{
    DLIST_INIT(&list->root_sentinel);
    // list->pos         = 0;
    list->num_pieces  = 0;
    list->size        = 0;
    list->lcnt        = 0;

    list->append   = allocate_append_buffer(&list->list_arena);
    list->original = allocate_original_buffer(&list->list_arena, original_text, original_text_len);

    list->first_free_node = NULL;
    list->root_sentinel.size = 0;
    list->root_sentinel.lcnt = 0;
    list->root_sentinel.count = 0;

    initialize_arena_with_size(&list->history_arena, 64 * 4094);
    initialize_arena(&list->insert_state_arena);
    initialize_undo_history(&list->undo_history);
    initialize_insert_state(&list->i_state);

    if (list->original.text_len)
    {
        piece piece = original_piece(&list->original);
        segmented_node *node = PushStruct(&list->list_arena, segmented_node, NoClear());
        *node->pieces  = piece;
        *node->b_types = BufferType_Original;
        list->size = node->size = piece.size;
        list->lcnt = node->lcnt = piece.lcnt;
        list->num_pieces = node->count = 1;
        DLIST_INSERT_TAIL(&list->root_sentinel, node);
    }

    list->top_changed = 0;
    list->bot_changed = num_logical_lines(list);
    list->changed = original_text != 0;
    list->lines_inserted = list->lcnt;
    list->lines_deleted  = 0;
    list->wrapped = false;

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
                    buffer_type type = node->b_types[i];

                    const buffer *buffer = get_buffer_2(list, type);
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
    iter iter;
    pieces(&list->root_sentinel, iter, 
    {
        const buffer *buffer = get_buffer_2(list, iter.type);
        u32 start = buffer->lines[iter.piece.off.row] + iter.piece.off.col;
        u32 end = start + iter.piece.size;
        memcpy(buf + cursor, (buffer->text + start), end - start);
        cursor += end - start;
    });
}
