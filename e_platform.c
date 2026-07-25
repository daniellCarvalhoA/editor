#include <utf8proc.h>
#include <sys/mman.h>
#include <sys/inotify.h>
#include <sys/signalfd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/uio.h>

#include <signal.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <termios.h>

#include "e.h"
#include "linux_e.h"

static platform_api Platform;

static struct termios term;
static struct termios old_term;

PLATFORM_GET_TERMINAL_HANDLE(LinuxGetTerminalHandle)
{
    platform_terminal_handle handle = { (void *) 1 };
    return handle;
}

PLATFORM_GET_TERMINAL_DIM(LinuxGetTerminalDim)
{
    struct window_size {
        u16 ws_row;
        u16 ws_col;
        u16 xpixel;
        u16 ypixel;
    } win;

    int t_fd = (int)((u64) handle.handle); 
    i32 n = ioctl(t_fd, TIOCGWINSZ, &win);
    if (n == - 1)
    {
        perror("ioctl");
        abort();
    }

    platform_window_dim dim = { .width = win.ws_col, .height = win.ws_row };
    return dim;
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
    // writev(
}

static void reset_mode()
{
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &old_term);
    char buf[] = "\x1b[?1049l";
    char show_cursor[] = "\x1b[?25h";
    int written;
    written = write(STDOUT_FILENO, (void *) show_cursor, sizeof(show_cursor));
    Assert(written == ArrayCount(show_cursor));
    written = write(STDOUT_FILENO, (void *) buf, sizeof(buf));
    Assert(written == ArrayCount(buf));
}

static void set_raw_mode()
{
    // Switch to terminals alternate buffer
    char buf[] = "\x1b[?1049h";
    i32 written = write(STDOUT_FILENO, (void *) buf, sizeof(buf));
    Assert(written == ArrayCount(buf));

    tcgetattr(STDIN_FILENO, &term);

    old_term = term;

    term.c_iflag &= ~(BRKINT | IGNBRK | INPCK | PARMRK | IGNCR | IXON | ICRNL);
    term.c_oflag &= ~(OPOST);
    term.c_cflag |= CS8;
    term.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);
    term.c_cc[VMIN] = 0;
    term.c_cc[VTIME] = 1;

    tcsetattr(STDIN_FILENO, TCSAFLUSH, &term);

    atexit(reset_mode);
}

static b32 handle_events(
    int fd,
    // int *wd,
    char *arg)
{
    b32 result = false;
    char buf[4096] __attribute__ ((aligned(__alignof__(struct inotify_event))));

    const struct inotify_event *event;
    ssize_t len;

    for (;;)
    {
        len = read(fd, buf, sizeof(buf));
        if (len == -1 && errno != EAGAIN)
        {
            perror("read");
            exit(EXIT_FAILURE);
        }

        if (len <= 0)
        {
            break;
        }

        for (char *ptr = buf;
            ptr < buf + len;
            ptr += sizeof(struct inotify_event) + event->len)
        {
            event = (const struct inotify_event *) ptr;

            if (event->mask & IN_CLOSE_WRITE)
            {
                if (strings_are_equal(event->name, arg))
                {
                    result = true;
                }
            }
        }
    }
    return result;
}

typedef struct mapped_file 
{
    u64 size;
    char *filepath;
    u8 *memory;
} mapped_file;


static mapped_file map_file(
    char *pathname)
{
    mapped_file map = {};
    map.filepath = pathname;

    struct stat stat_buf;
    i32 ret = stat(map.filepath, &stat_buf);

    if (ret == - 1)
    {
        perror("stat");
        return map;
    }
    map.size = stat_buf.st_size;

    int fd = open(map.filepath, O_RDONLY);

    if (fd == -1)
    {
        perror("open");
        return map;
    }

    map.memory = (u8 *) mmap(0, stat_buf.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);

    return map;
}

static b32 remap_file(
    mapped_file *map)
{
    int fd = open(map->filepath, O_RDONLY);

    struct stat stat_buf;
    i32 ret = stat(map->filepath, &stat_buf);

    if (ret == - 1)
    {
        perror("stat");
        map->size = 0;
        return false;;
    }

    if (fd == -1)
    {
        return false;
    }

    munmap((void *) map->memory, map->size);
    map->memory = (u8 *) mmap(
        (void *) map->memory,
        stat_buf.st_size,
        PROT_READ,
        MAP_PRIVATE,
        fd,
        0);
    map->size = stat_buf.st_size;
    return true;
}

static editor_memory allocate_editor_memory()
{
    editor_memory memory = {};
    // memory.permanent_storage_size = Megabytes(256);
//     memory.transient_storage_size = Gigabytes(1);
// // #if DEBUG
//     void *base_address = (void *) Terabytes(2);
// // #else
// //     void *base_address = 0;
// // #endif
//
//     u64 total_storage_size = /* memory.permanent_storage_size + */ memory.transient_storage_size;
//     void *storage = mmap(
//         base_address,
//         total_storage_size,
//         PROT_READ | PROT_WRITE,
//         MAP_PRIVATE | MAP_ANONYMOUS,
//         -1, 0);
//     // memory.permanent_storage = storage;
//     memory.transient_storage = (u8 *) storage /* + memory.permanent_storage_size */;

    return memory;
}


int main(int argc, char **argv)
{
    linux_state linux_state = {};
    linux_get_exe_filename(&linux_state);

    char src_code_dll_fullpath[LINUX_FILENAME_COUNT];
    build_exe_path_name(&linux_state, "e.so", src_code_dll_fullpath);

    int ifd, i, poll_num;
    int *wd;
    nfds_t nfds;
    struct pollfd fds[3];

    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGWINCH);
    sigaddset(&mask, SIGSEGV);
    sigaddset(&mask, SIGTERM);

    sigprocmask(SIG_BLOCK, &mask, NULL);

    int sfd = signalfd(-1, &mask, 0);

    // Create the file descriptor for accessing the inotify API
    ifd = inotify_init1(IN_NONBLOCK);
    if (ifd == - 1)
    {
        perror("inotify_init1");
        exit(EXIT_FAILURE);
    }

    // Allocate memory for watch descriptors
    wd = calloc(1, sizeof(int));
    if (wd == NULL)
    {
        perror("calloc");
        exit(EXIT_FAILURE);
    }

    for (i = 0; i < 1; i++)
    {
        wd[i] = inotify_add_watch(ifd, ".", IN_CREATE | IN_MOVED_TO | IN_CLOSE_WRITE | IN_DELETE);
        if (wd[i] == -1)
        {
            fprintf(stderr, "Cannot watch 'e_copy': %s\n", strerror(errno));
            exit(EXIT_FAILURE);
        }
    }
    /* Prepare for polling */
    nfds = 3;

    /* Console Input */
    fds[0].fd = STDIN_FILENO;
    fds[0].events = POLLIN;

    /* Inotify input */
    fds[1].fd = ifd;
    fds[1].events = POLLIN;

    /* window dimension change signal */ 
    fds[2].fd = sfd;
    fds[2].events = POLLIN;

    /* Wait for events and/or terminal input */
    mapped_file map = map_file("e_copy");
    Assert(map.memory);

    set_raw_mode();

    char clear_screen[] = "\x1b[2J\x1b[H";
    int written = write(STDOUT_FILENO, clear_screen, sizeof(clear_screen));
    Assert(written == ArrayCount(clear_screen));
    char non_blink_cursor[] = "\x1b[2 q";
    written = write(STDOUT_FILENO, non_blink_cursor, sizeof(non_blink_cursor));
    Assert(written == ArrayCount(non_blink_cursor));

    editor_memory memory = allocate_editor_memory();
    linux_e_code code    = load_code(src_code_dll_fullpath);

    memory.Platform.OpenFile          = LinuxOpenFile;
    memory.Platform.ReadDataFromFile  = LinuxReadFromFile;
    memory.Platform.CloseFile         = LinuxCloseFile;
    memory.Platform.AllocateDiskSpace = LinuxAllocateDiskSpace;
    memory.Platform.WriteGather       = LinuxWriteGather;
    memory.Platform.GetTerminalDim    = LinuxGetTerminalDim;
    memory.Platform.GetTerminalHandle = LinuxGetTerminalHandle;

    Platform = memory.Platform;

    str s = {};
    if (code.update_and_render(&memory, s, argc, (void **) argv))
    {
        return 1;
    }

    while (true)
    {
        poll_num = poll(fds, nfds, -1);
        if (poll_num == -1)
        {
            if (errno == EINTR)
            {
                continue;
            }
            perror("poll");
            exit(EXIT_FAILURE);
        }

        if (poll_num > 0)
        {

            if (fds[0].revents & POLLIN)
            {
                /* Console input is available. Empty stdin and quit */

                u8 buffer[4];
                i32 num_read = read(STDIN_FILENO, buffer, 4);
                str s = { .buffer = buffer, .len = num_read };

                if (num_read > 0)
                {
                    if (code.update_and_render(&memory, s, argc, (void **) argv))
                    {
                        break;
                    }
                }
            }

            if (fds[1].revents & POLLIN)
            {
                /* Inotify events are available */
                if (handle_events(ifd, "e_copy"))
                {
                    remap_file(&map);
                }
            }

            if (fds[2].revents & POLLIN)
            {
                struct signalfd_siginfo si;


                int n = read(sfd, &si, sizeof(si));
                if (n == -1)
                {
                    perror("read");
                    return -1;
                }
                
                if (si.ssi_signo == SIGSEGV)
                {
                    break;
                }

                if (si.ssi_signo == SIGTERM)
                {
                    break;
                }

                if (si.ssi_signo == SIGWINCH)
                {
                    code.update_window_dim(&memory);
                }

            }
        }
    }
    reset_mode();

    close(ifd);
    free(wd);

    return 1;
}
