#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <dlfcn.h>
#include <semaphore.h>

// #include "e.h"
#include "e_share.h"
#include "prng.c"
#include "string.c"

#define NUM_TESTS 1000

// This should be at least 64 bit aligned, but since this is allocated via mmap, it will be page aligned.
typedef struct 
{
    u64 test_index; 
    u64 seed;
    u64 pad[6]; 
} thread_log;

struct work_queue;
#define WORK_QUEUE_CALLBACK(name) void name(char *test_name, thread_log *log, u32 test_index)
typedef WORK_QUEUE_CALLBACK(work_queue_callback);

#define ADD_WORK_ENTRY(name) void name(struct work_queue *queue, char  *test_name, u32 test_index)
// typedef ADD_WORK_ENTRY(add_entry);

#define TEST_FUNCTION(name) void name(prng *prng)
typedef TEST_FUNCTION(function_test);

typedef struct work_queue_entry
{
    i32 volatile seq;
    u32 test_index;
    char *test_name;
} work_queue_entry;

typedef struct test_entry
{
    u64 seed;
    string test_name;
} test_entry;

typedef struct work_queue
{
    u32 volatile submitted;
    u32 volatile completed;
    u32 volatile read_cursor;
    u32 volatile write_cursor;
    sem_t sem_handle;
    work_queue_entry entries[64];
} work_queue;

static WORK_QUEUE_CALLBACK(do_test_work)
{
    void *code = dlopen("/home/daniel/c/e/build/tests.so", RTLD_LAZY);
    if (code)
    {
        function_test *test = (function_test *) dlsym(code, test_name);
        if (test)
        {
            log->test_index = test_index;

            entropy_src entropy;
            initialize_entropy(&entropy, true);
            for (u32 i = 0; i < NUM_TESTS; ++i)
            {
                log->seed = seed(&entropy);
                prng p;
                // Is there a need to go to the os here, 
                // maybe call next on the prng, reseed??
                // if (strcmp(test_name, "search_string") == 0)
                // {
                //     fprintf(stderr, "seed = %lu\n", log->seed);
                // }
                from_system_entropy(&entropy, &p);
                test(&p);
            }
            free_entropy(&entropy);
            fprintf(stderr, "%lu - TEST (pid: %lu), %s PASSED\n", log->test_index, (u64) getpid(), test_name);
        }
        else
        {
            // fprintf(stderr, "symbol : %s, not present in dll\n", test_name);
        }
    }
    else
    {
        fprintf(stderr, "unable to load dll\n");
    }
}

static ADD_WORK_ENTRY(add_entry)
{
    u32 write_cursor;
    work_queue_entry *slot;

    for (;;)
    {
        write_cursor = __atomic_load_n(&queue->write_cursor, __ATOMIC_RELAXED);
        slot = &queue->entries[write_cursor & (ArrayCount(queue->entries) - 1)];
        i32 seq = __atomic_load_n(&slot->seq, __ATOMIC_ACQUIRE);
        i32 diff = seq - (i32) write_cursor;
        if (diff == 0)
        {
            if (__atomic_compare_exchange_n(
                    &queue->write_cursor,
                    &write_cursor,
                    write_cursor + 1,
                    true,
                    __ATOMIC_RELAXED,
                    __ATOMIC_RELAXED))
            {
                break;
            }
            else
            {
                continue;
            }
        }
        else if (diff < 0)
        {
            continue;
        }
        else
        {
            continue;
        }
    }

    slot->test_index = test_index;
    slot->test_name = test_name;
    __atomic_store_n(&slot->seq, write_cursor + 1, __ATOMIC_RELEASE);
    __atomic_add_fetch(&queue->submitted, 1, __ATOMIC_RELEASE);
    sem_post(&queue->sem_handle);
    return;
}

static void do_work(work_queue *queue, thread_log *log)
{
    u32 read_cursor;
    work_queue_entry *slot;
    for (;;)
    {
        for (;;)
        {
            read_cursor = __atomic_load_n(&queue->read_cursor, __ATOMIC_RELAXED);
            slot = &queue->entries[read_cursor & (ArrayCount(queue->entries) - 1)];
            i32 seq = __atomic_load_n(&slot->seq, __ATOMIC_ACQUIRE);
            i32 diff = seq - (i32)(read_cursor + 1);

            if (diff == 0)
            {
               if (__atomic_compare_exchange_n(
                        &queue->read_cursor,
                        &read_cursor,
                        read_cursor + 1,
                        true,
                        __ATOMIC_ACQUIRE,
                        __ATOMIC_RELAXED))
                {
                    work_queue_entry entry = queue->entries[read_cursor];
                    do_test_work(entry.test_name, log, entry.test_index);
                    break;
                }
                else
                {
                    continue;
                }

            }
            else if (diff < 0)
            {
                sem_wait(&queue->sem_handle);
                u32 prev = __atomic_load_n(&queue->completed, __ATOMIC_ACQUIRE);
                u32 s    = __atomic_load_n(&queue->submitted, __ATOMIC_ACQUIRE);
                if (prev == s)
                {
                    sem_post(&queue->sem_handle);
                    return;
                } 
                continue;
            }
            else
            {
                continue;
            }
        }

        u32 prev = __atomic_fetch_add(&queue->completed, 1, __ATOMIC_ACQ_REL) + 1;
        u32 s    = __atomic_load_n(&queue->submitted, __ATOMIC_ACQUIRE);
        sem_post(&queue->sem_handle);
        if (prev == s)
        {
            break;
        } 
    }
}

typedef struct test_code
{
    void *code;
} test_code;

static thread_log *make_shared_thread_log(u32 num_threads)
{
    int log_fd = shm_open("log", O_CREAT | O_RDWR, S_IRUSR| S_IWUSR);

    ssize_t size = sizeof(thread_log) * num_threads;
    if (log_fd == -1)
    {
        perror("shm_open");
        return 0;
    }
    if (ftruncate(log_fd, size) == -1)
    {
        perror("ftruncate");
        return 0;
    }

    void *addr = mmap(0, size, PROT_READ | PROT_WRITE, MAP_SHARED, log_fd, 0);

    if (addr == MAP_FAILED)
    {
        perror("mmap");
        return 0;
    }

    thread_log *logs = (thread_log *) addr;
    return logs;
}

typedef struct test_function
{
    u32 len;
    char *name;
} test_function;

static char *copy_test_functions_to_shared_space(test_function *functions) 
{
    u32 size = 0;
    for (u32 i = 0; i < g_len(functions); ++i)
    {
        size += 1 + functions[i].len;
    }

    int names_fd = shm_open("names", O_CREAT | O_RDWR , S_IWUSR|S_IRUSR);
    if (names_fd == -1)
    {
        perror("shm_open");
        return 0;
    }
    if (ftruncate(names_fd, size) == -1)
    {
        perror("ftruncate");
        return 0;
    }

    void *addr = mmap(0, size, PROT_READ | PROT_WRITE, MAP_SHARED, names_fd, 0);

    if (addr == MAP_FAILED)
    {
        perror("mmap");
        return 0;
    }

    char *names = (char *) addr;

    char *tmp = names;
    for (u32 i = 0; i < g_len(functions); ++i)
    {
        memcpy(tmp, functions[i].name, sizeof(u8) *(functions[i].len + 1));
        tmp += functions[i].len + 1;
    }
    return names;
}

static work_queue *make_shared_work_queue()
{
    int work_queue_fd = shm_open("queue", O_CREAT | O_RDWR , S_IWUSR|S_IRUSR);

    ssize_t size = sizeof(work_queue);
    if (work_queue_fd == -1)
    {
        perror("shm_open");
        return 0;
    }
    if (ftruncate(work_queue_fd, size) == -1)
    {
        perror("ftruncate");
        return 0;
    }

    void *addr = mmap(0, size, PROT_READ | PROT_WRITE, MAP_SHARED, work_queue_fd, 0);

    if (addr == MAP_FAILED)
    {
        perror("mmap");
        return 0;
    }

    work_queue *queue = (work_queue *) addr;
    for (u32 i = 0; i < ArrayCount(queue->entries); ++i)
    {
        queue->entries[i].seq = i;
    }

    queue->submitted = 0;
    queue->completed = 0;
    queue->read_cursor = 0;
    queue->write_cursor = 0;
    sem_init(&queue->sem_handle, 1, 0);
    return queue;
}

typedef struct seed_file
{
    u32 num_entries;
    test_entry *entries;
} seed_file;


static void write_test_entry_to_file(i32 fd, test_entry entry);
static u64 parse_int(u8 **buf);
static u64 parse_seed(u8 *buf, u32 len); 
static string parse_test_name(u8 **buf, char separator);
static void parse_seed_file(u8 *file_memory, seed_file *file, u32 file_size);
static void parse_test_file(test_function **functions, u8 *file_memory, u32 file_size);
static void with_seed_entry(u64 seed, string s, void *shared_lib);

int main(int argc, char **argv) {

    int seed_file_fd = open("seed", O_APPEND | O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);

    if (seed_file_fd == -1)
    {
        perror("open");
        return 1;
    }

    if (argc > 1)
    {
        test_code test_code;
        test_code.code = dlopen("/home/daniel/c/e/build/tests.so", RTLD_LAZY);
        if (strings_are_equal(argv[1], "-p"))
        {
            struct stat stat_buf;
            fstat(seed_file_fd, &stat_buf);

            seed_file file;
            u8 *file_memory = (u8 *) mmap(0, stat_buf.st_size, PROT_READ, MAP_PRIVATE, seed_file_fd, 0);
            parse_seed_file(file_memory, &file,  stat_buf.st_size);

            for (u32 seed_index = 0; seed_index < file.num_entries; ++seed_index)
            {
                with_seed_entry(
                    file.entries[seed_index].seed,
                    file.entries[seed_index].test_name,
                    test_code.code);
                fprintf(stderr, "Test of seed: %lu passed\n", file.entries[seed_index].seed);
            }

            free(file.entries);
        } 
        else if ((argc > 2)  && strings_are_equal(argv[1], "-s"))
        {
            u32 seed_len = str_len(argv[2]);
            u64 seed = parse_seed((u8 *) argv[2], seed_len);



            string name = { .buffer = (u8 *) argv[3], .len = str_len(argv[3]) };;

            with_seed_entry(seed, name, test_code.code);
            fprintf(stderr, "TEST passed\n");
        }
    }
    else
    {
        struct stat stat_buf;
        stat("tests.c", &stat_buf);
        i32 fd = open("tests.c", O_RDONLY);
        u8 *file_memory = (u8 *) mmap(0, stat_buf.st_size, PROT_READ, MAP_PRIVATE, fd, 0);

        test_function *functions = 0;
        parse_test_file(&functions, file_memory, stat_buf.st_size);

        char *test_names = copy_test_functions_to_shared_space(functions);

        u32 NUM_WORKERS = 12;

        thread_log *logs  = make_shared_thread_log(NUM_WORKERS);
        work_queue *queue = make_shared_work_queue();

        pid_t child_pids[NUM_WORKERS];
        for (u32 i = 0; i < g_len(functions); ++i)
        {
            test_function *test = functions + i;
            add_entry(queue, test_names, i);
            test_names +=  test->len + 1;
        }

        for (u32 child_index = 0; child_index < NUM_WORKERS; ++child_index)
        {
            pid_t pid = fork();
            if (pid == -1)
            {
                perror("fork");
                return 1;
            }

            if (pid == 0)
            {
                thread_log *thread_log = logs + child_index; 
                memset(thread_log, 0, sizeof(*thread_log));
                do_work(queue, thread_log);
                _exit(0);
            }
            else
            {
                child_pids[child_index] = pid;
            }
        }

        u32 reaped_children = 0;
        while (reaped_children < NUM_WORKERS)
        {
            int status;
            pid_t pid = waitpid(-1, &status, 0);
            if (pid == 0)
            {
                continue;
            }
            reaped_children++;

            if (WIFSIGNALED(status))
            {
                if (WTERMSIG(status) == SIGSEGV || WTERMSIG(status) == SIGABRT)
                {
                    u32 child_index = 0;
                    while (child_pids[child_index] != pid) 
                    {
                        child_index++;
                    }
                    thread_log *log = logs + child_index;
                    test_function *test = functions + log->test_index;
                    __atomic_fetch_add(&queue->completed, 1, __ATOMIC_ACQ_REL);
                    sem_post(&queue->sem_handle);
                    fprintf(stderr, "process: %u, failed on seed: %lu, on function %s\n",
                            pid, log->seed, test->name);

                    test_entry entry = { .seed = log->seed, .test_name = char_str_to_string(test->name) };
                    write_test_entry_to_file(seed_file_fd, entry);
                }
            }
            else if (WIFSTOPPED(status))
            {
            } else if (WIFEXITED(status))
            {
                // fprintf(stderr, "fuck pid : %u\n, status = %d\n", pid, WTERMSIG(status));
            }
        }
        for (u32 i = 0; i < g_len(functions); ++i)
        {
            test_function *test = functions + i;
            free(test->name);
        }

        g_free(functions);
    }
}

static u64 parse_int(u8 **buf)
{
    u64 value = 0;
    for (;;)
    {
        if ((**buf == ' ') || (**buf == '\n'))
        {
            (*buf)++;
            break;
        }
        value = 10 * value + (*(*buf)++ - '0');
    }
    return value;
}

static u64 parse_seed(u8 *buf, u32 len)
{
    u32 j = len - 1;
    u64 i = 1;
    u64 seed = 0;
    for(;;)
    {
        seed += i * (u64)(buf[j] - '0');
        if (j == 0)
        {
            break;
        }
        j--;
        i *= 10; 
    } 
    return seed;
}

static string parse_test_name(u8 **buf, char separator)
{
    string test_name;
    test_name.buffer = *buf;
    test_name.len = 0;

    while (*(*buf)++ != separator) 
    {
        test_name.len++;
    }
    return test_name;
}

static void parse_seed_file(u8 *file_memory, seed_file *file, u32 file_size)
{
    file->num_entries = 0;
    u8 *file_start = file_memory;
    for (u32 i = 0; i < file_size; ++i)
    {
        if (*file_start++ == '\n')
        {
            file->num_entries++;
        }
    }
    file->entries = malloc(sizeof(test_entry) * file->num_entries);
    u32 i = 0;
    while (*file_memory)
    {
        file->entries[i].seed = parse_int(&file_memory);
        file->entries[i].test_name = parse_test_name(&file_memory, '\n');
        i++;
    }
}

// This is very hacky; for now it serves its purpose
static void parse_test_file(test_function **functions, u8 *file_memory, u32 file_size)
{
    u32 match_count = 0;
    for (u32 i = 0; i < file_size; ++i)
    {
        switch (file_memory[i])
        {
            case 'T':
            {
                if (match_count == 0)
                {
                    match_count++;
                }
                else if (match_count == 3)
                {
                    i++;
                    if (file_memory[i] == '(')
                    {
                        i++;

                        u8 *start = file_memory + i;
                        u32 len = 0;

                        while (file_memory[i++] != ')')
                        {
                            len++;
                        }

                        char *name = malloc(sizeof(char) * (len + 1));
                        memcpy(name, start, sizeof(char) * len);
                        name[len] = '\0';
                       
                        test_function new_function = { .len = len, .name = name };

                        g_push(*functions, new_function);
                    }
                    match_count = 0;
                }
            } break;

            case 'E':
            {
                if (match_count == 1)
                {
                    match_count++;
                }
                else
                {
                    match_count = 0;
                }
            } break;

            case 'S':
            {
                if (match_count == 2)
                {
                    match_count++;
                }
                else
                {
                    match_count = 0;
                }
            } break;

            default: 
            {
                match_count = 0;
            } break;
        }
    }
}

static void write_test_entry_to_file(i32 fd, test_entry entry)
{
    int written;
    u8 buf[64];
    u32 seed_len = int_to_string(entry.seed, buf);
    written = write(fd, buf, seed_len);
    Assert(written < (i32) ArrayCount(buf));
    u8 space = ' ';
    written = write(fd, &space, 1);
    Assert(written == 1);
    written = write(fd, entry.test_name.buffer, entry.test_name.len);
    Assert(written = entry.test_name.len);
    u8 new_line = '\n';
    written = write(fd, &new_line, 1);
    Assert(written == 1);
}

static void with_seed_entry(u64 seed, string s, void *shared_lib)
{
    char path[4096];
    memcpy(path, s.buffer, s.len);
    path[s.len] = '\0';
    function_test *test_function = (function_test *) dlsym(shared_lib, path);

    if (test_function)
    {
        prng p;
        from_seed(seed, &p);
        test_function(&p);
    }
    else
    {
        fprintf(stderr, "Unable to load symbol\n");
        fprintf(stderr, "%s\n", dlerror());
    }
}




