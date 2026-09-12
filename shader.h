
typedef struct file_memory
{
    uint8_t *memory;
    u32 size;
} file_memory;

file_memory read_file(const char *path);
void free_file(file_memory f_memory);

typedef struct
{
    uint32_t id;
} shader;

GLuint create_shader_from_file(
    GLenum shader_type,
    const char *filename)
{
    GLuint s = glCreateShader( shader_type );

    file_memory memory = read_file(filename);
    const GLchar *shader[1] = { (const GLchar *) memory.memory };
    const GLint length[1] = { memory.size };
    glShaderSource( s, 1, shader, length );

    glCompileShader( s );

    int params = -1;
    glGetShaderiv( s, GL_COMPILE_STATUS, &params);

    // On error, capture the log and print it
    if ( GL_TRUE != params )
    {
        int max_length = 2048, actual_length = 0;
        char slog[2048];
        glGetShaderInfoLog ( s, max_length, &actual_length, slog );
        fprintf( stderr, "ERROR: Shader index %u did not compile.\n%s\n", s, slog);
        return 0;
    }

    return s;
}

shader create_shader_program_from_file(
    const char *vertex_shader_filename,
    const char *fragment_shader_filename)
{
    
    shader shader = {};
    GLuint vs = create_shader_from_file( GL_VERTEX_SHADER, vertex_shader_filename);;
    GLuint fs = create_shader_from_file( GL_FRAGMENT_SHADER, fragment_shader_filename );

    shader.id = glCreateProgram();
    glAttachShader( shader.id, fs );
    glAttachShader( shader.id, vs );
    glLinkProgram( shader.id );

    glDeleteShader(fs);
    glDeleteShader(vs);

    int params = -1;
    glGetProgramiv( shader.id, GL_LINK_STATUS, &params);

    if (GL_TRUE != params)
    {
        int max_length = 2048, actual_length = 0;
        char plog[2048];
        glGetProgramInfoLog( shader.id, max_length, &actual_length, plog);
        fprintf(stderr, "ERROR: Could not link shader program GL index %u.\n%s\n", 
                shader.id, plog); 
        return shader;
    }

    return shader;
}

void reload_shader_program_from_files(
    shader *old_shader,
    const char *vertex_shader_filename,
    const char *fragment_shader_filename)
{
    Assert(old_shader && vertex_shader_filename && fragment_shader_filename);

    shader reloaded_program = create_shader_program_from_file(
        vertex_shader_filename,
        fragment_shader_filename);

    if (reloaded_program.id)
    {
        glDeleteProgram(old_shader->id);
        *old_shader = reloaded_program;
    }
}

void set_bool(shader s, const char *name, bool value)
{
    glUniform1i(glGetUniformLocation(s.id, name), (int)value);
}

void set_int(shader s, const char *name, int value)
{
    glUniform1i(glGetUniformLocation(s.id, name), value);
}

void set_float(shader s, const char *name, float value)
{
    glUniform1f(glGetUniformLocation(s.id, name), value);
}


void free_file(file_memory f_memory)
{
    Assert(f_memory.size > 0);
    munmap(f_memory.memory, f_memory.size);
}

file_memory read_file(const char *path)
{

    file_memory result = {};
    int font_file_fd = open(path, O_RDONLY);
    if (font_file_fd == -1)
    {
        printf("unable to open file\n");
        return result;
    }

    struct stat stat_buf;
    int stat_ret = fstat(font_file_fd, &stat_buf);
    if (stat_ret == -1)
    {
        perror("unable to stat file\n");
        return result;
    }

    printf("Font data is %ld kb\n", stat_buf.st_size / 1024);

    void *memory = mmap(
        NULL,
        stat_buf.st_size,
        PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANONYMOUS,
        -1,
        0);

    if (memory == MAP_FAILED)
    {
        printf("mmap failed\n");
        return result;
    }

    ssize_t read_so_far = 0;
    while (read_so_far < stat_buf.st_size)
    {
        ssize_t read_ret = read(font_file_fd, (void *) ((char *) memory + read_so_far), stat_buf.st_size - read_so_far);
        if (read_ret == -1)
        {
            perror("error on read\n");
            break;
        }

        read_so_far += read_ret;
    }

    close(font_file_fd);

    result = (struct file_memory) { .memory = memory, .size = stat_buf.st_size };
    return result;
}


