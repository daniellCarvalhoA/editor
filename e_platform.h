#include <utf8proc.h>


typedef struct 
{
    b32 no_errors;
    void *Platform;
    u32 size;
} platform_file_handle;

typedef struct 
{
    void *handle;
} platform_terminal_handle;

typedef struct 
{
    void *base;
    u64 size;
} platform_scatter_gather_vector;

// typedef struct
// {
//     i32 flags;
// } platform_scatter_gather_flags;


typedef struct platform_file_group 
{
    u32 file_count;
    void *Platform;
} platform_file_group;

typedef enum platform_file_access_mode
{
    Read,
    Write,
    ReadWrite,
} platform_file_access_mode;

typedef struct
{
    u32 width; 
    u32 height; 
} platform_window_dim;



#define PLATFORM_OPEN_FILE(name) platform_file_handle name(char *filepath)
typedef PLATFORM_OPEN_FILE(platform_open_file);

#define PLATFORM_CLOSE_FILE(name) void name(platform_file_handle handle)
typedef PLATFORM_CLOSE_FILE(platform_close_file);

#define PLATFORM_READ_DATA_FROM_FILE(name) void name(platform_file_handle *handle, u64 offset, u64 size, void *dst)
typedef PLATFORM_READ_DATA_FROM_FILE(platform_read_data_from_file);

#define PLATFORM_WRITE_GATHER(name) void name(platform_file_handle *handle, platform_scatter_gather_vector *vecs, i32 count)
typedef PLATFORM_WRITE_GATHER(platform_write_gather);

#define PLATFORM_GET_TERMINAL_HANDLE(name) platform_terminal_handle name(void)
typedef PLATFORM_GET_TERMINAL_HANDLE(platform_get_terminal_handle);

#define PLATFORM_GET_TERMINAL_DIM(name) platform_window_dim name(platform_terminal_handle handle)
typedef PLATFORM_GET_TERMINAL_DIM(platform_get_terminal_dim);


#define PLATFORM_ALLOCATE_DISK_SPACE(name) void name(platform_file_handle *handle, u64 offset, u64 len) 
typedef PLATFORM_ALLOCATE_DISK_SPACE(platform_allocate_disk_space);

typedef struct platform_api 
{
    platform_open_file             *OpenFile;
    platform_read_data_from_file   *ReadDataFromFile;
    platform_close_file            *CloseFile;
    platform_allocate_disk_space   *AllocateDiskSpace;
    platform_write_gather          *WriteGather;
    platform_get_terminal_dim      *GetTerminalDim;
    platform_get_terminal_handle   *GetTerminalHandle;
} platform_api;


typedef struct editor_memory
{
    struct editor_state *editor;
    platform_api Platform;

} editor_memory;

#define PLATFORM_FILE_ERROR(name) void name(platform_file_handle *handle, char *message)
typedef PLATFORM_FILE_ERROR(platform_file_error);

#define UPDATE_AND_RENDER(name) b32 name(editor_memory *memory, u8 *input, i32 input_size, u32 cmdc, void **cmdl)
typedef UPDATE_AND_RENDER(UpdateAndRender);

#define UPDATE_WINDOW_DIM(name) void name(editor_memory *memory)
typedef UPDATE_WINDOW_DIM(UpdateWindowDimension);


