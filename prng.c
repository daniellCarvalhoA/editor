#include <sys/random.h>
#include <malloc.h>

typedef struct ratio
{
    u64 numerator;
    u64 denominator;
} ratio;

static ratio init_ratio(
    u64 numerator, u64 denominator)
{
    assert(denominator > 0);
    assert(numerator <= denominator);
    ratio result = { .numerator = numerator, .denominator = denominator };
    return result;
}

typedef struct entropy_src
{
    u32 size;
    u32 max;
    u32 next_free;
    u64 *buffer;
} entropy_src;

static void initialize_entropy(entropy_src *src, const b32 alloc)
{
    if (alloc)
    {
        src->buffer = (u64 *) malloc(64 * sizeof(u64));
    }
    src->size = getrandom((void *)src->buffer, 64 * sizeof(u64), 0);
    src->max  = src->size / 64;
    src->next_free = 0;
};

static inline void free_entropy(entropy_src *src)
{
    free(src->buffer);
}

typedef struct prng
{
    u64 state[4];
} prng;

static prng clone_prng(prng *p)
{
    prng new;
    memcpy(&new, p->state, ArrayCount(p->state) * sizeof(u64));
    return new;
}

static inline u64 splitmix64(u64 *s)
{
    *s += 0x9E3779B97F4A7C15ULL;

    u64 z = *s;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;

    return z ^ (z >> 31);
}

static void from_seed(u64 seed, prng *g)
{
    g->state[0] = splitmix64(&seed);
    g->state[1] = splitmix64(&seed);
    g->state[2] = splitmix64(&seed);
    g->state[3] = splitmix64(&seed);
}

static u64 seed(entropy_src *entropy)
{
    u64 seed = *(entropy->buffer + entropy->next_free);
    return seed;
}

static void from_system_entropy(entropy_src *entropy, prng *prng)
{
    if (entropy->next_free >= entropy->max)
    {
        initialize_entropy(entropy, false);
    }

    u64 seed = *(entropy->buffer + entropy->next_free++);

    from_seed(seed, prng);

}
static inline u64 rotl(const u64 x, i32 k)
{
    return (x << k) | (x >> (64 - k));
}

u64 next(prng *prng)
{
    u64 result = rotl(prng->state[1] * 5, 7) * 9;

    u64 t = prng->state[1] << 17;

    prng->state[2] ^= prng->state[0];
    prng->state[3] ^= prng->state[1];
    prng->state[1] ^= prng->state[2];
    prng->state[0] ^= prng->state[3];

    prng->state[2] ^= t;
    prng->state[3] = rotl(prng->state[3], 45);

    return result;
}

static void fill(prng *prng, u8 *target, const u32 target_len)
{
    u32 i = 0;
    const u32 aligned_len = target_len - (target_len & 7);

    while (i < aligned_len)
    {
        u64 n = next(prng);
        u32 j = 0;
        while (j < 8)
        {
            target[i + j] = (u8) n;
            n >>= 8;
            j++;
        }

        i += 8;
    }
}

static b32 rand_b32(prng *prng)
{
    b32 result = ((next(prng) & 1) == 1);
    return result;
}

static u64 rand_u64(prng *prng)
{
    u64 result = next(prng);
    return result;
}

static u32 rand_u32(prng *prng)
{
    u32 result = (u32) next(prng);
    return result;
}

static u8 rand_u8(prng *prng)
{
    u8 result = (u8) next(prng);
    return result;
}

static u64 rand_u64_inclusive(prng *prng, const u64 max)
{
    if (max == UINT64_MAX)
    {
        return rand_u64(prng);
    }

    u64 less_than = max + 1;

    u64 x = rand_u64(prng);
    u128 m = (u128) x * (u128) less_than;
    u64 l = (u64) m;

    if (l < less_than)
    {
        u128 threshold = - less_than;

        if (threshold >= less_than)
        {
            threshold -= less_than;
            if (threshold >= less_than)
            {
                threshold %= less_than;
            }
        }

        while (l < threshold)
        {
            x = rand_u64(prng);
            m = (u128) x * (u128) less_than;
            l = (u64) m;
        }
    }

    return (u64) (m >> 64);
}

static u32 rand_u32_inclusive(prng *prng, const u32 max)
{
    if (max == UINT32_MAX)
    {
        return rand_u32(prng);
    }

    u32 less_than = max + 1;

    u32 x = rand_u32(prng);
    u64 m = (u64) x * (u64) less_than;
    u32 l = (u32) m;

    if (l < less_than)
    {
        u64 threshold = - less_than;

        if (threshold >= less_than)
        {
            threshold -= less_than;
            if (threshold >= less_than)
            {
                threshold %= less_than;
            }
        }

        while (l < threshold)
        {
            x = rand_u32(prng);
            m = (u64) x * (u64) less_than;
            l = (u32) m;
        }
    }

    return (u32) (m >> 32);
}

static u32 rand_u8_inclusive(prng *prng, const u8 max)
{
    if (max == UINT8_MAX)
    {
        return rand_u8(prng);
    }

    u32 less_than = max + 1;

    u8 x = rand_u8(prng);
    u16 m = (u16) x * (u16) less_than;
    u8 l = (u8) m;

    if (l < less_than)
    {
        u16 threshold = - less_than;

        if (threshold >= less_than)
        {
            threshold -= less_than;
            if (threshold >= less_than)
            {
                threshold %= less_than;
            }
        }

        while (l < threshold)
        {
            x = rand_u8(prng);
            m = (u16) x * (u16) less_than;
            l = (u8) m;
        }
    }

    return (u8) (m >> 8);
}

static u64 rand_range_u64_inclusive(prng *prng, const u64 min, const u64 max)
{
    assert(min <= max);
    return min + rand_u64_inclusive(prng, max - min);
}

static u32 rand_range_u32_inclusive(prng *prng, const u32 min, const u32 max)
{
    assert(min <= max);
    return min + rand_u32_inclusive(prng, max - min);
}

static u8 rand_range_u8_inclusive(prng *prng, const u8 min, const u8 max)
{
    assert(min <= max);
    return min + rand_u8_inclusive(prng, max - min);
}

static b32 chance(prng *prng, const ratio probability)
{
    assert(probability.denominator > 0);
    assert(probability.numerator <= probability.denominator);
    b32 result = rand_u64_inclusive(prng, probability.denominator - 1) < probability.numerator;
    return result;
}

static u32 rand_power_of_two(prng *prng)
{
    u32 result = rand_range_u32_inclusive(prng, 2, 12);
    return 1 << result;
}

static string rand_ascii_string_alloc(prng *prng, memory_arena *arena, const u32 min_len, const u32 max_len)
{
    Assert(max_len >= min_len);
    string result = {};

    u32 length = rand_range_u8_inclusive(prng, min_len, max_len);

    if (length > 0)
    {
        result.len = length;
        result.capacity = length;
        result.buffer = PushArray(arena, result.capacity, u8, NoClear());

        ratio ratio = init_ratio(1, 10);
        // TODO: Do better than this / use fill?
        for (u32 i = 0; i < result.len; ++i)
        {
            if (chance(prng, ratio))
            {
                result.buffer[i] = '\n';
            }
            else
            {
                u8 c = rand_range_u8_inclusive(prng, 32, 126);
                result.buffer[i] = c;
            }
        }
    }
    return result;
}

static void rand_ascii_string(string *s, prng *prng, const u32 min_len, const u32 max_len)
{
    s->len = rand_range_u8_inclusive(prng, min_len, max_len);

    if (s->len > 0)
     {
        ratio ratio = init_ratio(1, 10);
        // TODO: Do better than this / use fill?
        for (u32 i = 0; i < s->len; ++i)
        {
            if (chance(prng, ratio))
            {
                s->buffer[i] = '\n';
            }
            else
            {
                u8 c = rand_range_u8_inclusive(prng, 32, 126);
                s->buffer[i] = c;
            }
        }
    }
}

static buffer rand_ascii_buffer(memory_arena *arena, prng *prng)
{
    buffer buf = {};
    buf.text_len = rand_u8_inclusive(prng, 40);
    buf.text     = PushArray(arena, buf.text_len, u8, default_arena_params());

    u32 lines[40] = {};
    u32 num_lines = 1;

    ratio ratio = init_ratio(1, 10);

    for (u32 i = 0; i < buf.text_len; ++i) 
    {
        if (chance(prng, ratio))
        {
            buf.text[i] = '\n';
            lines[num_lines++] = i + 1;
        }
        else
        {
            u8 c = rand_range_u8_inclusive(prng, 32, 126);
            buf.text[i] = c;
        }
    }

    buf.num_lines = num_lines;
    buf.lines = PushArray(arena, buf.num_lines, u32, default_arena_params());
    memcpy(buf.lines, lines, sizeof(u32) * num_lines);

    return buf;
}
