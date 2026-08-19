
// NOTE: What if one uses the ring buffer virtual memory trick?

typedef struct 
{
    u32 count;
    u32 index;
} slice;

typedef struct undo_record
{
    u16 abs_idx;
    u16 ins_count;
    u16 del_count;
    u16 prev_record;
    piece pieces[];
} undo_record;

typedef struct undo_record redo_record;
typedef struct u8 undo_token;

typedef struct
{
    u32 prev_index; // NOTE: two bits are wasted, since both (undo_records) 
                    // and undo_record_header are at least 4 byte aligned.
    u32 position;
    u32 count;
    undo_record records[];
} undo_record_header;

typedef struct undo_record_header redo_record_slice;

#define UNDO_RECORDS_SIZE Kilobytes(128)

typedef enum
{
    NoError,
    AllocationError,
} undo_state;


typedef struct
{
    undo_state state;
    b32 wrapped;  // This is for debuging purposes;
    b32 in_process;
    u32 num_records; 
    u32 limit;    // Demarks the end of the slices. This is dynamic, 
    u32 first;    // index into the oldest undo_slice

    u32 last;     // index into the most recent undo_slice
    u32 last_top; // index into the next undo_header/undo_record to be allocated
    u32 last_bot; // index into the last undo_record allocated
    u8 *slices;
} Undo_Records;

typedef struct
{
    Undo_Records *records;
    undo_record *first_record;
    undo_record *last_record;
    u32 count;
    u32 position;
} record_iter;

typedef undo_state redo_state;
typedef Undo_Records Redo_Records;

//typedef enum
//{
    //EraseHistory,
//} allocation_error_policy;

typedef struct
{
    b32 error;
} UndoSequenceToken;

static Undo_Records create_undo_records(memory_arena *arena);
static void begin_undo_sequence(Undo_Records *records, u32 position);
static void begin_undo(window *win);
static void end_undo_sequence(Undo_Records *undo_records, Redo_Records *redo_records);
static void end_undo(window *win);
static undo_record *allocate_undo_record(Undo_Records *records, u32 del_count);
static record_iter get_records(Undo_Records *records);
static undo_record *get_record(record_iter *iter);
static void undo(window *win);
static void redo(window *win);

static inline b32 are_records_empty(Undo_Records *records) 
{
    b32 result = records->num_records == 0;
    return result;
}

static inline undo_record_header *get_record_header(Undo_Records *records, u32 i)
{
    undo_record_header *result = (undo_record_header *) (records->slices + i);
    return result;
}

static inline undo_record_header *get_last_record_header(Undo_Records *records)
{
    undo_record_header *result = get_record_header(records, records->last);
    return result;
}

static inline undo_record_header *get_first_record_header(Undo_Records *records)
{
    undo_record_header *result = get_record_header(records, records->first);
    return result;
}

static inline u32 record_size(undo_record *record)
{
    u32 result = sizeof(undo_record) + record->del_count * sizeof(piece);
    return result;
}

static inline u32 record_size_from_del_count(u32 del_count)
{
    u32 result = sizeof(undo_record) + del_count * sizeof(piece);
    return result;
}

static inline piece_slice get_pieces_from_record(undo_record *record)
{
    piece_slice result =
    {
        .base = record->pieces,
        .count = record->del_count,
    };
    return result;
}

static inline undo_record *get_first_record(Undo_Records *records, undo_record_header *header)
{
    undo_record *result = (undo_record *) ((u8*) header + sizeof(undo_record_header));
    if ((u8 *) result - records->slices >= records->limit)
    {
        result = (undo_record *) records->slices;
    }
    return result;
}


static inline undo_record *get_next_record(Undo_Records *records, undo_record *record)
{
    undo_record *result = (undo_record *) ((u8 *) record + record_size(record));
    if ((u8 *) result >= records->slices + records->limit)
    {
        result = (undo_record *) records->slices;
    }
    return result;
}

static inline undo_record *get_last_record(
    Undo_Records *records,
    undo_record_header *header)
{
    undo_record *result = get_first_record(records, header);
    Assert(header->count > 0);
    u32 count = header->count - 1;
    while (count)
    {
        result = get_next_record(records, result);
        count--;
    }
    return result;
}

static inline undo_record *get_prev_record(Undo_Records *records, undo_record *record)
{
    undo_record *result = NULL; 
    if (record->prev_record > 0)
    {
        u16 distance_to_bottom = (u16)((u8 *) record - records->slices);
        if (record->prev_record > distance_to_bottom)
        {
            u8 *top = records->slices + UNDO_RECORDS_SIZE;
            result = (undo_record *) (top - (record->prev_record - distance_to_bottom));
        }
        else
        {
            result = (undo_record *) ((u8 *) record - record->prev_record);
        }
    }
    return result;
}

static inline u32 get_last_record_top(Undo_Records *records)
{
    u32 result = records->last_top;
    return result;
}

// This tests strict equality.
static b32 are_all_records_equal(Undo_Records *a, Undo_Records *b)
{

    b32 result = (a->num_records == b->num_records) && 
                 (a->first == b->first) && 
                 (a->last == b->last) && 
                 (a->last_top == b->last_top) &&
                 (a->last_bot == b->last_bot) &&
                 (a->limit  == b->limit);

    if (result)
    {
        result = (memcmp(a->slices, b->slices, a->limit) == 0);
    }

    return result;
}
