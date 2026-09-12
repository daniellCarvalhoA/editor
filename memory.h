#include <sys/mman.h>

typedef struct memory_block_footer
{
    memory_index size;
    memory_index used;
    u8 *base;
} memory_block_footer;

typedef struct memory_arena 
{
    memory_index size;
    u8 *base;
    memory_index used;
    memory_index minimum_block_size;

    u32 block_count;
    u32 tmp_count;
} memory_arena;

static inline memory_block_footer *get_footer(
    memory_arena *arena)
{
    memory_block_footer *result = (memory_block_footer *) (arena->base + arena->size);
    return result;
}


enum arena_push_flag
{
    ArenaFlag_ClearToZero = 0x1,
};

typedef struct arena_push_params
{
    u32 flags;
    u32 alignment;
} arena_push_params;

typedef struct temporary_memory
{
    memory_arena *arena;
    u8 *base;
    memory_index used;
} temporary_memory;

static inline temporary_memory begin_temporary_memory(
    memory_arena *arena) 
{
    temporary_memory result;
    result.arena = arena;
    result.base = arena->base;
    result.used  = arena->used;

    ++arena->tmp_count;
    return result;
}

static inline u32 free_last_block(memory_arena *arena)
{
    void *free = arena->base; 
    memory_index block_size = arena->size + sizeof(memory_block_footer);
    memory_block_footer *footer = get_footer(arena);

    arena->base = footer->base;
    arena->size = footer->size;
    arena->used = footer->used;
    --arena->block_count;
    u32 block_count = arena->block_count;
    munmap(free, block_size);
    return block_count;
}

static inline void end_temporary_memory(
    temporary_memory tmp_mem) 
{
    memory_arena *arena = tmp_mem.arena;

    while (arena->base != tmp_mem.base)
    {
        free_last_block(arena);
    }

    Assert(arena->used >= tmp_mem.used);
    arena->used = tmp_mem.used;
    Assert(arena->tmp_count > 0);
    --arena->tmp_count; 
}
#define ZeroStruct(instance) ZeroSize(sizeof(instance), &(instance))
#define ZeroArray(count, pointer) ZeroSize(count * sizeof((pointer)[0]), pointer)

static inline void ZeroSize(
    memory_index size,
    void *ptr)
{
    memset(ptr, 0, size);
}

static inline void set_minimum_block_size(
    memory_arena *arena,
    memory_index minimum_block_size)
{
    arena->minimum_block_size = minimum_block_size;
}

static inline void initialize_arena_with_size(
    memory_arena *arena,
    memory_index size)
{
    arena->size = 0;
    arena->base = 0;
    arena->used = 0;
    arena->tmp_count = 0;
    arena->block_count = 0;
    arena->minimum_block_size = size;
}

static inline void initialize_arena(
    memory_arena *arena)
    // memory_index size,
    // void *base)
{
    arena->size = 0;
    arena->base = 0;
    arena->used = 0;
    arena->tmp_count = 0;
    arena->block_count = 0;
    arena->minimum_block_size = 4096; // TODO: Tune default block_size;
}

static inline arena_push_params default_arena_params()
{
    arena_push_params params;
    params.flags = ArenaFlag_ClearToZero;
    params.alignment = 4;
    return params;
}

static inline arena_push_params AlignClear(
    u32 alignment,
    b32 clear)
{
    arena_push_params params = {};
    params.alignment = alignment;
    params.flags = clear ? ArenaFlag_ClearToZero : 0;
    return params;
}

static inline arena_push_params AlignNoClear(
    u32 alignment)
{
    arena_push_params params = default_arena_params();
    params.flags &= ~ArenaFlag_ClearToZero;
    params.alignment = alignment;
    return params;
}

static inline arena_push_params NoClear()
{
    arena_push_params params = default_arena_params();
    params.flags &= ~ArenaFlag_ClearToZero;
    return params;
}

static inline memory_index get_alignment_offset(
    memory_arena *arena,
    arena_push_params params)
{
    memory_index alignment_offset = 0;
    memory_index result_pointer = (memory_index) arena->base + arena->used;
    memory_index alignment_mask = params.alignment - 1;

    if (result_pointer & alignment_mask)
    {
        alignment_offset = params.alignment - (result_pointer & alignment_mask);
    }
    return alignment_offset;
}

static inline memory_index get_arena_size_remaining(
    memory_arena *arena,
    arena_push_params params)
{
    memory_index result = arena->size - (arena->used + get_alignment_offset(arena, params));
    return result;
}

#define PushStruct(arena, type, ...) (type *)push_size(arena, sizeof(type), ## __VA_ARGS__)
#define PushArray(arena, count, type, params, ...) \
    (type *) push_size(arena, (count) * sizeof(type), params, ## __VA_ARGS__)
#define Pushsize(arena, size, ...) push_size(arena, size, ## __VA_ARGS__)
#define PushCopy(arena, size, src, ...) copy(size, src, push_size(arena, size, ## __VA_ARGS__))


static inline memory_index get_effective_size_for(
    memory_arena *arena,
    memory_index size_init,
    arena_push_params params)
{
    memory_index size = size_init;
    memory_index alignment_offset = get_alignment_offset(arena, params);
    size += alignment_offset;
    return size;
}

static inline b32 arena_has_room_for(
    memory_arena *arena,
    memory_index size_init,
    arena_push_params params)
{
    memory_index size = get_effective_size_for(arena, size_init, params);
    b32 result = ((arena->used + size) <= arena->size);
    return result;
}

static inline void *push_size(
    memory_arena *arena,
    memory_index size_init,
    arena_push_params params)
{
    memory_index size = get_effective_size_for(arena, size_init, params);

    if ((arena->used + size) > arena->size)
    {
        if (!arena->minimum_block_size)
        {
            // TODO: Tune default minimum_block_size.
            arena->minimum_block_size = 64 * 4096;
        }

        memory_block_footer save;
        save.base = arena->base;
        save.size = arena->size;
        save.used = arena->used;

        size = size_init; // The base will be aligned (mmap returns page aligned addresses).
        memory_index block_size = Maximum(size + sizeof(memory_block_footer), arena->minimum_block_size);
        arena->size = block_size - sizeof(memory_block_footer);
        arena->base = (u8 *) mmap(0, block_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        arena->used = 0;
        ++arena->block_count;

        memory_block_footer *footer = get_footer(arena);
        *footer = save;

    }

    // Assert((arena->used + size) <= arena->size);

    memory_index alignment_offset = get_alignment_offset(arena, params);
    void *result = arena->base + arena->used + alignment_offset;

    arena->used += size;

    Assert(size >= size_init);

    if (params.flags & ArenaFlag_ClearToZero)
    {
        ZeroSize(size_init, result);
    }
    return result;
}

static inline void sub_arena(
    memory_arena *result,
    memory_arena *arena,
    memory_index size,
    arena_push_params params)
{
    result->size = size;
    result->base = (u8 *) push_size(arena, size, params);
    result->used = 0;
    result->tmp_count = 0;
}


static inline void clear_arena(memory_arena *arena)
{
    while (arena->block_count > 1)
    { 
        free_last_block(arena);
    }

    arena->used = 0;
    // initialize_arena(arena, arena->size, arena->base);
}

static inline void free_arena(memory_arena *arena)
{
    // while (arena->block_count > 0)
    // {
    //     free_last_block(arena);
    // }

    u32 block_count = arena->block_count;

    while (block_count > 0)
    {
        block_count = free_last_block(arena);
    }
    // for (;;)
    // {
    //     u32 block_count = free_last_block(arena);
    //     if (block_count == 0)
    //     {
    //         break;
    //     }
    // }
    // while (free_last_block(arena) > 0) {};
    // while ((arena) && (arena->block_count > 0))
    // {
    //     free_last_block(arena);
    //     // fprintf(stderr, "here\n");
    //
    // }

    // munmap(arena->base, arena->size);
}
        

#if 0
static void reset_used(
    memory_arena *arena,
    void *reset)
{
    memory_index new_used = ((memory_index) arena->base) - (memory_index) reset;
    arena->used = new_used;
}
static char *PushString(
    memory_arena *arena,
    char* src)
{
    u32 size = 1;
    for (char *at = src; *at; ++at)
    {
        ++size;
    }
    char* dst = (char *) push_size(arena, size, NoClear()); 
    for (u32 char_index = 0; char_index < size; ++char_index) {
        dst[char_index] = src[char_index];
    }

    return dst;
}

static char *PushAndNullTerminate(
    memory_arena *arena,
    u32 length,
    char* src)
{
    char* dst = (char *) push_size(arena, length + 1, NoClear()); 
    for (u32 char_index = 0; char_index < length; ++char_index) {
        dst[char_index] = src[char_index];
    }
    dst[length] = 0;

    return dst;
}
#endif

#define BootstrapPushStruct(type, member,...) \
    (type *) bootstrap_push_size(sizeof(type), offsetof(type, member), ## __VA_ARGS__)

static inline void *bootstrap_push_size(
    memory_index struct_size,
    memory_index offset_to_arena,
    memory_index minimum_block_size)
    // , arena_push_params params)
{
    memory_arena bootstrap = {};
    bootstrap.minimum_block_size = minimum_block_size;
    void *struct_ptr = push_size(&bootstrap, struct_size, NoClear());
    *(memory_arena *)((u8 *) struct_ptr + offset_to_arena) = bootstrap;
    return struct_ptr;
}

