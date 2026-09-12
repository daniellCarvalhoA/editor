
#include <dlfcn.h>
// #include "e_share.h"

#define LINUX_FILENAME_COUNT 4096

typedef struct linux_e_code
{
    void *code;
    char *exe_filename[LINUX_FILENAME_COUNT];
    time_t dll_last_write_time;
    char *one_past_last_exe_flineame_slash;
    UpdateAndRender *update_and_render;
    UpdateWindowDimension *update_window_dim;
    b32 is_valid;
} linux_e_code;

typedef struct linux_state
{
    char exe_filename[LINUX_FILENAME_COUNT];
    char *one_past_last_exe_flineame_slash;
} linux_state;

static void linux_get_exe_filename(linux_state *state)
{
    // This is necessary because readlink does not append a null byte to the bufffer.
    memset(state->exe_filename, 0, sizeof(state->exe_filename));
    //  "/proc/self/exe" is a symbolic link to the executable file containing the 
    //  current processe's code

    // readlink reads the contents of a symbolic link 
    // (the name of the file it points to) and places it in the buffer passed

    // NOTE: I believe this does not work on all unix systems, the procfd filesystem 
    // is specific to linux.
    int n = readlink(
        "/proc/self/exe",
        state->exe_filename,
        sizeof(state->exe_filename) - 1);

    if (n == -1)
    {
        perror("readlink");
        return;
    }

    state->one_past_last_exe_flineame_slash = state->exe_filename;

    for (char *scan = state->exe_filename; *scan; ++scan)
    {
        if (*scan == '/')
        {
            state->one_past_last_exe_flineame_slash = scan + 1;
        }
    }
}

static void build_exe_path_name(linux_state *state, char *filename, char *dst)
{
    cat_strings(
        state->one_past_last_exe_flineame_slash - state->exe_filename,
        state->exe_filename,
        str_len(filename),
        filename,
        dst);
}


static time_t linux_get_last_write_time(char *filename)
{
    time_t last_write_time = 0;
    struct stat file_status;

    if (stat(filename, &file_status) == 0)
    {
        last_write_time = file_status.st_mtime;
    }
    return last_write_time;
}

static bool wait_for_lockfile_release(const char *lockfile_path, i32 max_wait_ms)
{
    i32 waited = 0;

    const i32 interval_ms = 100;
    while (access(lockfile_path, F_OK) == 0)
    {
        if(waited >= max_wait_ms)
        {
            fprintf(
                stderr,
                "Timeout: lockfile still exists after %d ms\n",
                max_wait_ms);
            return false;
        }
        usleep(interval_ms * 1000);
        waited += interval_ms;
    }
    return true;
}

static linux_e_code load_code(char *src_dll_name)
{
    linux_e_code result = {};

    result.dll_last_write_time = linux_get_last_write_time(src_dll_name);

    if (result.dll_last_write_time)
    {
        if (!wait_for_lockfile_release("/tmp/e.lock", 300))
        {
            exit(1);
        }

        result.code = dlopen(src_dll_name, RTLD_LAZY);

        if (result.code)
        {
            result.update_and_render = 
                (UpdateAndRender *) dlsym(result.code, "update_and_render");
            result.update_window_dim = 
                (UpdateWindowDimension *) dlsym(result.code, "update_window_dim");
            result.is_valid = result.update_and_render != 0;
        }
        else
        {
            puts(dlerror());
        }
    }

    if (!result.is_valid)
    {
        fprintf(stderr, "failed to load dll: %s", dlerror());
        result.update_and_render = 0;
    }

    return result;
}

static void unload_code(linux_e_code *code)
{
    if (code->code)
    {
        dlclose(code->code);
        code->code = 0;
    }

    code->is_valid = false;
    code->update_and_render = 0;
}



PLATFORM_OPEN_FILE(LinuxOpenFile)
{
    platform_file_handle result = { .no_errors = true};

    i32 fd = open(filepath, O_RDWR| O_CREAT, S_IWUSR|S_IRUSR|S_IRGRP|S_IWGRP|S_IROTH);
    if (fd == -1)
    {
        result.no_errors = false;
    }
    else
    {
        struct stat file_status;
        fstat(fd, &file_status);
        result.Platform = (void *) (u64) fd;
        result.size = file_status.st_size;
    }

    return result;
}

PLATFORM_CLOSE_FILE(LinuxCloseFile)
{
    i32 fd = (i32) (u64) (handle.Platform);
    close(fd);
}

PLATFORM_READ_DATA_FROM_FILE(LinuxReadFromFile)
{
    u32 bytes_to_read = size;
    u8 *next_byte_location = (u8 *) dst;

    i32 fd = (i32) (u64) handle->Platform;
    lseek(fd, (off_t) offset, SEEK_SET);
    while (bytes_to_read)
    {
        ssize_t bytes_read = read(fd, next_byte_location, bytes_to_read);
        if (bytes_read == -1)
        {
            handle->no_errors = false;
            break;
        }
        bytes_to_read -= bytes_read;
        next_byte_location += bytes_read;
    }
}

PLATFORM_ALLOCATE_DISK_SPACE(LinuxAllocateDiskSpace)
{
    i32 fd = (i32) (u64) handle->Platform;
    i32 ret = posix_fallocate(fd, offset, len);

    if (ret != 0)
    {
        perror("posix_fallocate");
        handle->no_errors = false;
    }
}

PLATFORM_WRITE_GATHER(LinuxWriteGather)
{
    i32 fd = (i32) (u64) handle->Platform;
    const struct iovec *iov = (const struct iovec *) vecs;
    // i32 iov_flags = (i32) flags.flags;
    i32 ret = writev(fd, iov, count);

    if (ret == -1)
    {
        handle->no_errors = false;
    }
}

