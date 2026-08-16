
static inline Undo_Records create_undo_records(memory_arena *arena)
{
    Undo_Records result =
    {
        .limit = 0,
        .first = 0,
        .last  = 0,
        .last_top = 0,
        .last_bot = 0,
        .num_records = 0,
        .slices = PushArray(arena, UNDO_RECORDS_SIZE, u8, NoClear())
    };
    return result;
}

static inline void advance_first(Undo_Records *records) 
{
    Assert(!are_records_empty(records));
    Assert(records->first < records->limit);

    records->wrapped = true;
    records->num_records--;

    undo_record_header *header = get_first_record_header(records);
    u32 count = header->count;

    undo_record *record = get_first_record(records, header);

    while (count > 0)
    {
        record = get_next_record(records, record);
        count--;
    }

    records->first = (u8 *) record - records->slices;
}

static inline slice get_space_for_size(Undo_Records *records, u32 size)
{
    slice result = {};
    if (records->last == records->first)
    {
        u32 top_free = UNDO_RECORDS_SIZE - records->last_top;
        u32 bot_free = records->first;
        if (top_free >= size)
        {
            result.count = top_free;
            result.index = records->last_top;
        }
        else if (bot_free >= size)
        {
            result.count = bot_free;
            result.index = 0;
        }
    }
    // if (records->last_top == 0 && records->first == 0)
    // {
    //     Assert(records->last_top == 0);
    //     result.count = UNDO_RECORDS_SIZE;
    //     result.index = 0;
    // }
    else if (records->last_top > records->first)
    {
        u32 top_free = UNDO_RECORDS_SIZE - records->last_top;
        u32 bot_free = records->first;
        if (top_free >= size)
        {
            result.count = top_free;
            result.index = records->last_top;
        }
        else if (bot_free >= size)
        {
            result.count = bot_free;
            result.index = 0;
        }
    }
    else 
    {
        u32 middle_free = records->first - records->last_top;
        if (middle_free >= size)
        {
            result.count = middle_free;
            result.index = records->last_top;
        }
    }
    return result;
}

// Returns NULL if allocation failed:
// The only way for an allocation to fail is if a single undo sequence 
// can't fit inside the undo memory.
// If we set the undo memory to a reasonable amount (say 16 Mb) this is highly 
// unlikely. In any case, we must choose a policy to handle this case.
// For now we erase the undo history; 

static inline u8 *allocate(Undo_Records *records, u32 size)
{
    if (records->state == AllocationError)
    {
        return NULL;
    }

    if (size > UNDO_RECORDS_SIZE - sizeof(undo_record_header))
    {
        records->state = AllocationError;
        return NULL;
    }

    if (records->state == NoError)
    {
        b32 prev_direction = (records->first < records->last);
        u32 changed = 0;
        while (changed < 2)
        {
            slice available = get_space_for_size(records, size);
            if (available.count > 0)
            {
                u8 *result = records->slices + available.index;

                u32 prev_last_top = records->last_top;
                records->last_top = available.index + size;
                if (prev_last_top > records->last_top)
                {
                    records->limit = prev_last_top;
                }
                else
                {
                    records->limit = Maximum(records->limit, records->last_top);
                }
                return result;
            }
            else
            {
                if (are_records_empty(records))
                {
                    Assert(size > available.count);
                    break;
                }
                advance_first(records);
                b32 curr_direction = (records->first < records->last);
                changed += curr_direction != prev_direction;
                prev_direction = curr_direction;
            }
        }
    }

    records->state = AllocationError;
    return NULL;

}

static inline void begin_undo_sequence(Undo_Records *records, u32 position)
{
    u32 alloc_size = sizeof(undo_record_header);
    undo_record_header *header = (undo_record_header *) allocate(records, alloc_size);
    if (header)
    {
        records->num_records++;
        header->count = 0;
        header->position= position;
        header->prev_index = records->last;

        records->last = (u32) ((u8 *) header - records->slices);
        records->last_bot = records->last_top;
    }
}

static inline void begin_undo(window *win)
{
    u32 position = position_from_cursor(&win->buffer->iter, win->bc);
    begin_undo_sequence(&win->buffer->undo_records, position);
}

static inline void clear_records(Undo_Records *records)
{
    records->state = NoError;
    records->wrapped = 0;
    records->num_records = 0;
    records->limit = 0;
    records->last = records->first = records->last_top = 0;
}

static inline void end_undo_sequence(
    Undo_Records *undo_records,
    Redo_Records *redo_records)
{
    if (undo_records->state == AllocationError)
    {
        // TODO: Choose allocate error handling policy.
        // For now we erase all the history so far.
        clear_records(undo_records);
        if (redo_records)
        {
            clear_records(redo_records);
        }
    }
    else
    {
        undo_record_header *header = get_last_record_header(undo_records);
        // This may happen when we start undo sequence, and the subsequent 
        // operations are nops, 
        if (header->count == 0 && !(are_records_empty(undo_records)))
        {
            undo_records->last_top = undo_records->last;
            undo_records->last = header->prev_index;
            undo_records->num_records--;
        }
        else if (redo_records)
        {
            clear_records(redo_records);
        }
    }
}

static inline void end_undo(window *win)
{
    end_undo_sequence(&win->buffer->undo_records, &win->buffer->redo_records);
}

static inline undo_record *allocate_undo_record(Undo_Records *records, u32 del_count)
{
    u32 alloc_size = record_size_from_del_count(del_count);


    u32 distance = records->last_top - records->last_bot;

    records->last_bot = records->last_top;
    undo_record *result = (undo_record *) allocate(records, alloc_size);
    if (result)
    {
        Assert((((u64)((u8 *) result)) & 3) == 0);

        Assert(distance <= UINT16_MAX);
        result->prev_record = (u16) distance;
        if (records->last_bot > records->last_top)
        {
            if (distance)
            {
                result->prev_record += UNDO_RECORDS_SIZE - records->last_bot;
            }
            records->last_bot = 0;
        }

        undo_record_header *header = get_last_record_header(records);
        header->count++;
    }

    return result;
}

static inline record_iter get_records(Undo_Records *records)
{
    record_iter result = {};

    if (!are_records_empty(records))
    {
        records->num_records--;
        undo_record_header *header = get_last_record_header(records);

        result.records  = records;
        result.count    = header->count;
        result.position = header->position;

        result.first_record = get_first_record(records, header);
        result.last_record  = get_last_record(records, header);

        records->last_top = records->last;
        if (records->num_records > 0)
        {
            records->last = header->prev_index;
            if (records->last_top == 0)
            {
                Assert(records->last + sizeof(undo_record_header) < records->limit);
                records->last_top = records->limit;
            }
        }
        records->last_bot = records->last_top;

    }
    return result;
}

// NOTE: Most record slices will have small sizes (between 1 and 2), 
// with the exception of large search and replace operations.
// When we make an undo we must iterate from the last undo record to 
// the most recent. Right now this operation will be O (n²) where 
// n is the number of records in a slice.
// This is because we don't have back pointers inside the records themselvs, 
// so to reach the last record we must find it starting with the first.
// This takes O (n + (n - 1) + (n - 2) ...) = O ((n * (n + 1)) / 2);
//
// If this becomes a problem we can either put back pointers in the 
// records, making iterating over n records take O (n) at the cost 
// of more memory, or splitting iteration over two passes.
//
//  1> first allocate temporary memory to hold slice->count indexes, 
//  and iterate over the slice to save thoses indexes.
//  2> Use the the saved indexes to iterate backwards.
//
//  This would take O (2 * n) and some temporary memory for each iteration.


static inline undo_record *get_record(record_iter *iter)
{
    undo_record *result = iter->last_record;
    if (result)
    {
        iter->last_record = get_prev_record(iter->records, result);
    }

    // iter.last_record = get_prev_record
    // undo_record *result = NULL;
    // 
    // if (iter->count > 0)
    // {
    //     result = iter->first_record;
    //
    //     for (u32 i = 0; i < iter->count - 1; ++i)
    //     {
    //         result = get_next_record(iter->records, result);
    //     }
    //
    //     iter->count--;
    // }
    return result;
}

static inline void undo_once(
    piece_list *list,
    undo_record *undo_record,
    redo_record *redo_record)
{
    Assert(undo_record);

    base_iter start = find_abs_idx(&list->iter, undo_record->abs_idx);
    base_iter end = find_abs_idx(&list->iter, undo_record->abs_idx + undo_record->ins_count);

    // redo_record *record = allocate_undo_record(&list->redo

    cursor start_cursor = { .node = start.node, .piece_index = start.piece_idx };
    cursor end_cursor   = { .node = end.node, .piece_index = end.piece_idx };

    u32 del_size = position(&end) - position(&start);
    u32 del_lcnt = line_number(&end) - line_number(&start); 

    list->size -= del_size;
    list->lcnt -= del_lcnt;

    piece_slice p_slice = get_pieces_from_record(undo_record);
    if (redo_record)
    {
        redo_record->abs_idx   = undo_record->abs_idx;
        redo_record->del_count = undo_record->ins_count;
        redo_record->ins_count = undo_record->del_count;
        piece_slice undo_buffer = get_pieces_from_record(redo_record);
        copy_range_2(start_cursor, end_cursor, end.abs_idx - start.abs_idx, undo_buffer);
    }

    replace(list, start_cursor, end_cursor, p_slice.base, p_slice.count);
    reset_cursor_(&list->iter);
}

static inline void undo(window *win)
{
    piece_list *list = win->buffer;
    record_iter iter = get_records(&list->undo_records);

    u32 old_position = position_from_cursor(&list->iter, win->bc);

    u32 new_position = old_position;
    if (iter.count != 0)
    {
        new_position = iter.position;
    }


    begin_undo_sequence(&list->redo_records, old_position);

    undo_record *record;
    while ((record = get_record(&iter)))
    {
        redo_record *redo_record = allocate_undo_record(&list->redo_records, record->ins_count);
        undo_once(list, record, redo_record);
    }

    buffer_cursor new_cursor = cursor_from_position(&list->iter, new_position);
    win->bc = new_cursor;

    end_undo_sequence(&list->redo_records, NULL);


    list_invariants(win->buffer);
}

static inline void redo(window *win)
{
    piece_list *list = win->buffer;
    record_iter iter = get_records(&list->redo_records);

    u32 old_position = position_from_cursor(&list->iter, win->bc);
    u32 new_position = old_position;
    if (iter.count != 0)
    {
        new_position = iter.position;
    }

    begin_undo_sequence(&list->undo_records, old_position);

    undo_record *record;
    while ((record = get_record(&iter)))
    {
        undo_record *undo_record = allocate_undo_record(&list->undo_records, record->del_count);
        undo_once(list, record, undo_record);
    }

    buffer_cursor new_cursor = cursor_from_position(&list->iter, new_position);
    win->bc = new_cursor;

    end_undo_sequence(&list->undo_records, NULL);
    list_invariants(win->buffer);

}

// static inline void redo(window *win)

