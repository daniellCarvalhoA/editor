
/*-------------------------------- UTILITIES -----------------------------*/

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#define Assert(expression) if(!(expression))\
  { fprintf(stderr, "Assert failed in: %d of file: %s\n", __LINE__, __FILE__); *(int *)0 = 0; }

#define TEST(name) 

#define Minimum(A, B) (((A) < (B)) ? (A) : (B))
#define Maximum(A, B) (((A) > (B)) ? (A) : (B))

#define LIST_INSERT(head, node) \
    (node)->next = (head); \
    (head) = (node);

#define DLIST_INIT(sentinel) \
    (sentinel)->next = (sentinel);\
    (sentinel)->prev = (sentinel);

#define DLIST_INSERT_HEAD(sentinel, element) \
    (element)->next = (sentinel)->next;\
    (element)->prev = (sentinel);\
    (element)->next->prev = (element);\
    (element)->prev->next = (element);

#define DLIST_INSERT_AFTER(at, new) \
    (new)->next = (at->next); \
    (at)->next = (new); \
    (new)->prev = at; \
    (new)->next->prev = (new);

#define DLIST_INSERT_TAIL(sentinel, element) \
    (element)->next = (sentinel);\
    (element)->prev = (sentinel)->prev;\
    (element)->next->prev = (element);\
    (element)->prev->next = (element);

#define DLIST_REMOVE(node) \
    (node)->prev->next = (node)->next;\
    (node)->next->prev = (node)->prev;

#define LIST_REMOVE(prev, node, head) \
    if (prev) \
    { \
        (prev)->next = (node)->next; \
    } \
    else \
    { \
        Assert((node) == (head)); \
        (head) = (head)->next; \
    }

#define LIST_REPLACE(prev, old_node, new_node, head) \
    if (prev) \
    { \
        (prev)->next = (new_node); \
        (new_node)->next = (old_node)->next; \
    } \
    else \
    { \
        Assert((old_node) == (head)); \
        (new_node)->next = (old_node)->next; \
        (head) = (new_node); \
    }

#define FREELIST_ALLOCATE(Result, FreeListPointer, AllocationCode)\
    (Result) = (FreeListPointer);\
    if (Result)\
    {\
        FreeListPointer = (Result)->next;\
    }\
    else\
    {\
        Result = AllocationCode;\
    }

#define FREELIST_DEALLOCATE(Pointer, FreeListPointer)\
    if (Pointer)\
{\
    (Pointer)->next = (FreeListPointer);\
    (FreeListPointer) = (Pointer);  \
}

#define SET_BIT_TO(x,n,v) ((x) = ((x) & ~(1U << (n))) | (((v) & 1U) << (n)))

static u32 str_len(const char *str);

// Remember to implement memcpy, memset, memmove!!!.

typedef struct string
{
    u32 len;
    u32 capacity;
    u8 *buffer;
} string;

static string char_str_to_string(
    char *str)
{
    u32 len = str_len(str);
    string result = { .len = len, .buffer = (u8 *)str, .capacity = len };
    return result;
}

#define INIT_STACK_STRING(s_name, s_cap) \
    u8 buffer[(s_cap)]; \
    string (s_name) = { .len = 0, .capacity = (s_cap), .buffer = buffer };

static void push_string(string *dst, string *src)
{
    assert(dst->len + src->len <= dst->capacity);
    memcpy(dst->buffer + dst->len, src->buffer, src->len);
    dst->len += src->len;
}

static u32 count_token(string s, u8 token) 
{
    u32 result = 0;
    u8 *ptr = s.buffer;

    while (ptr < s.buffer + s.len)
    {
        result += (*ptr++ == token);
    }

    return result;
}

static u32 count_rev_until(string s, u32 token)
{
    u32 result = 0;
    u8 *ptr = s.buffer + s.len - 1;
    while (ptr - result >= s.buffer && *(ptr - result) != token)
    {
        ++result;
    }
    return result;
}

static inline void null_terminate(
    string *s)
{
    s->buffer[s->len] = '\0';
}

static u64 int_to_string(
    u64 value,
    u8 *buf)
{
    u32 len = 0;
    do 
    {
        u8 digit = (u8) (value % 10);
        buf[len++] = '0' + digit;
        value = value / 10;
    } while (value != 0);

    
    u32 i = 0;
    u32 j = len - 1;
    while (i < j)
    {
        u8 tmp = buf[j];
        buf[j] = buf[i];
        buf[i] = tmp;
        i++;
        j--;
    }
    return len;
}

static u32 str_len(
    const char *str)
{
    u32 count = 0;

    while (*str++)
    {
        ++count;
    }
    return count;
}

static void cat_strings(
    const size_t src_a_count,
    const char *src_a,
    const size_t src_b_count,
    const char *src_b,
    char *dst)
{
    for (u32 index = 0; index < src_a_count; ++index)
    {
        *dst++ = *src_a++;
    }

    for (u32 index = 0; index < src_b_count; ++index)
    {
        *dst++ = *src_b++;
    }

    *dst++ = 0;
}

static b32 strings_are_equal(
    const char *a,
    const char *b)
{
    Assert(a);
    Assert(b);
    while (*a && *b)
    {
        if (*a != *b)
        {
            return false;
        }
        a++;
        b++;
    }
    if (!*a && !*b)
    {
        return true;
    } 
    else
    {
        return false;
    }

}
