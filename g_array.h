#include <stdlib.h>

#define G_INIT_CAPACITY 256

typedef struct g_header
{
    u32 count;
    u32 capacity;
} g_header;

#define g_get_header(arr) ((g_header *) (arr) - 1)

#define g_len(arr) ((g_header *)(arr) - 1)->count

#define g_set_len(arr, len) ((g_header *)(arr) - 1)->count = (len);
#define g_push(arr, x)                                                                      \
    do {                                                                                    \
        if (!arr)                                                                           \
        {                                                                                   \
            g_header *header = malloc(sizeof(*arr) * G_INIT_CAPACITY + sizeof(g_header));   \
            header->count = 0;                                                              \
            header->capacity = G_INIT_CAPACITY;                                             \
            arr = (void *) (header + 1);                                                    \
        }                                                                                   \
        g_header *header = (g_header *) (arr) - 1;                                          \
        if (header->count >= header->capacity)                                              \
        {                                                                                   \
            header->capacity *= 2;                                                          \
            header = realloc(header, sizeof(*arr) * header->capacity + sizeof(g_header));   \
            if ((void *)(header + 1) != (arr))                                              \
            {                                                                               \
                memcpy((header + 1), arr, sizeof(*arr) * header->count);                    \
            }                                                                               \
            arr = (void *) (header + 1);                                                    \
        }                                                                                   \
        (arr)[header->count++] = (x);                                                       \
    } while (0);


#define g_append(arr, slice, len) \
    do {\
        if (!arr)\
        {\
            g_header *header = malloc(sizeof(*arr) * G_INIT_CAPACITY + sizeof(g_header));  \
            header->count = 0;\
            header->capacity = G_INIT_CAPACITY;\
            arr = (void *) (header + 1);\
        }\
        g_header *header = (g_header *) (arr) - 1;\
        if (header->count + len > header->capacity)\
        {\
            header->capacity = Maximum(header->count + len, header->capacity * 2);\
            header = realloc(header, sizeof(*arr) * header->capacity + sizeof(g_header));\
            if ((void *) (header + 1) != (arr))\
            {\
                memcpy(header + 1, arr, sizeof(*arr) * header->count);\
            }\
            arr = (void *) (header + 1);\
        }\
        memcpy(arr + header->count, slice, len); \
        header->count += len;\
    } while (0);

#define g_free(arr) free((g_header *)(arr) - 1)

