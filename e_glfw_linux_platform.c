#include <stdio.h>
#include "gl.h"
#include <GLFW/glfw3.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/fcntl.h>
#include <sys/uio.h>
// #include "e_share.h"

#include "e_share.h"
#include "e_platform.h"
#include "linux_e.h"
#include "opengl_renderer.h"
#include "opengl_renderer.c"
#include "string.c"


#include <ft2build.h>
#include FT_FREETYPE_H

static platform_api Platform;

typedef struct 
{
    GLFWwindow *window;
    atlas *atlas;
} window_handle;


PLATFORM_GET_WINDOW_DIM(LinuxGetTerminalDim)
{
    struct window_size {
        i32 ws_row;
        i32 ws_col;
        i32 xpixel;
        i32 ypixel;
    } win;

    window_handle *w_handle = (window_handle *) handle.handle;
    glfwGetFramebufferSize(w_handle->window, &win.ws_col, &win.ws_row);
    atlas *atlas = w_handle->atlas;


    Assert(win.ws_col > 0);
    Assert(win.ws_row > 0);
    platform_window_dim dim = { 
        .width = (u16) win.ws_col / (u16) atlas->max_width,
        .height = (u16) win.ws_row / (u16) atlas->max_height};
    return dim;
}

int utf8_encode(uint32_t codepoint, char out[5]);
void framebuffer_size_callback(GLFWwindow* window, int width, int height);

static void key_callback(
    GLFWwindow* window,
    int key,
    int scancode,
    int action, 
    int mods);

static void char_callback(GLFWwindow *window, unsigned int codepoint);

static editor_memory allocate_editor_memory()
{
    editor_memory memory = {};
    return memory;
}

typedef struct
{
    renderer renderer;
    u8 count_text_input;
    u8 text_input[16];
} state;

int main(int argc, char **argv)
{
    linux_state linux_state = {};
    linux_get_exe_filename(&linux_state);

    char src_code_dll_fullpath[LINUX_FILENAME_COUNT];
    build_exe_path_name(&linux_state, "e.so", src_code_dll_fullpath);

    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    int win_w = 800, win_h = 600;
    GLFWwindow* window = glfwCreateWindow( win_w, win_h, "Hello Triangle", NULL, NULL );
    if (!window)
    {
        printf("Failed to create GLFW window\n");
        return -1;
    }

    glfwSetCharCallback(window, char_callback);
    glfwSetKeyCallback(window, key_callback);
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    int version_glad = gladLoadGL(glfwGetProcAddress);
    if (version_glad == 0) 
    {
      fprintf( stderr, "ERROR: Failed to initialize OpenGL context.\n" );
      return -1;
    }



    glDisable(GL_DEPTH_TEST);
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_SCISSOR_TEST);

    // TODO: For some reason that as to do with the compositor, i have to disable
    // vsync.
    glfwSwapInterval(0);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_TEXTURE_2D);

    i32 width, height;
    glfwGetFramebufferSize(window, &width, &height);
    glViewport(0, 0, width, height);

    memory_arena render_arena;
    initialize_arena_with_size(&render_arena, Megabytes(1));

    memory_arena atlas_arena;
    initialize_arena_with_size(&atlas_arena, Megabytes(1));

    render_commands r_commands = allocate_render_commands(&render_arena);



    renderer renderer = {};
    renderer_initialize(window, &atlas_arena, &renderer, 32);

    state state = { .renderer = renderer};



    glfwSetWindowUserPointer(window, &state);

    f32 prev_s = glfwGetTime();

    editor_memory memory = allocate_editor_memory();
    linux_e_code code    = load_code(src_code_dll_fullpath);

    window_handle w_handle = {
        .window = window,
        .atlas = renderer.atlas
    };

    memory.Platform.OpenFile          = LinuxOpenFile;
    memory.Platform.ReadDataFromFile  = LinuxReadFromFile;
    memory.Platform.CloseFile         = LinuxCloseFile;
    memory.Platform.AllocateDiskSpace = LinuxAllocateDiskSpace;
    memory.Platform.WriteGather       = LinuxWriteGather;
    memory.Platform.GetWindowDim    = LinuxGetTerminalDim;
    memory.Platform.WindowHandle.handle = (void *) &w_handle;

    Platform = memory.Platform;

    while ( !glfwWindowShouldClose( window ) )
    {

        glfwWaitEventsTimeout(1.0 / 120.0);
        // printf("input_count = %d\n", state.count_text_input);
        f32 curr_s = glfwGetTime();

        f32 dt = curr_s - prev_s;
        prev_s = curr_s;

        keyboard_input input =
        {
            .supports_physical_layout = true,
            .utf8_str = Str(state.text_input, state.count_text_input)
        };
        // printf("cou

        // This must be derived from the camera position  
        render_view view = {
            .top_margin =  0,
            .bot_margin =  0,
            .left_margin =  0,
            .right_margin =  0,
        };

        // if (state.count_text_input > 0)
        // {

        if (code.update_and_render(&memory, input, &render_arena, &r_commands, view, argc, (void **) argv))
        {
            break;
        }
        state.count_text_input = 0;
        // printf("input_count = %d\n", state.count_text_input);
        // }

        opengl_renderer(&renderer, &r_commands, dt);


        clear_arena(&render_arena);

        r_commands = allocate_render_commands(&render_arena);

        glfwSwapBuffers( window );
        // printf("input_count = %d\n", state.count_text_input);

    }

}

static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    state *state = glfwGetWindowUserPointer(window);
    switch (key)
    {
        case GLFW_KEY_ESCAPE:
        {
            if (action == GLFW_PRESS)
            {
                state->text_input[0] = '\x1b';
                state->count_text_input++;
            }
        } break;

        case GLFW_KEY_ENTER:
        {
            if (action == GLFW_PRESS)
            {
                state->text_input[0] = '\r';
                state->count_text_input++;
            }
        } break;

        case GLFW_KEY_BACKSPACE:
        {
            if (action == GLFW_PRESS)
            {
                state->text_input[0] = 127;
                state->count_text_input++;
            }

        }
    }
}

static void char_callback(GLFWwindow *window, unsigned int codepoint)
{

    state *state = glfwGetWindowUserPointer(window);

    // if (state->count_text_input > 0)
    // {
        int bytes = utf8_encode(codepoint, (char *) &state->text_input);
        state->count_text_input += bytes;
    // }

    // if (app->state == Insert)
    // {
    //     // fprintf(stderr, "key = %d\n", codepoint);
    //
    //     char utf8[5] = {};
    //
    //     int bytes = utf8_encode(codepoint, utf8);
    //
    //     if (bytes)
    //     {
    //         strcpy(app->text + app->text_size, utf8);
    //         app->text_size += strlen(utf8);
    //         app->cursor.x ++;//  app->rd.atlas.max_width;
    //
    //     }
    // }
}


int utf8_encode(uint32_t codepoint, char out[5])
{
    if (codepoint <= 0x7F)
    {
        out[0] = (char)codepoint;
        out[1] = '\0';
        return 1;
    }
    else if (codepoint <= 0x7FF)
    {
        out[0] = 0xC0 | (codepoint >> 6);
        out[1] = 0x80 | (codepoint & 0x3F);
        out[2] = '\0';
        return 2;
    }
    else if (codepoint <= 0xFFFF)
    {
        out[0] = 0xE0 | (codepoint >> 12);
        out[1] = 0x80 | ((codepoint >> 6) & 0x3F);
        out[2] = 0x80 | (codepoint & 0x3F);
        out[3] = '\0';
        return 3;
    }
    else if (codepoint <= 0x10FFFF)
    {
        out[0] = 0xF0 | (codepoint >> 18);
        out[1] = 0x80 | ((codepoint >> 12) & 0x3F);
        out[2] = 0x80 | ((codepoint >> 6) & 0x3F);
        out[3] = 0x80 | (codepoint & 0x3F);
        out[4] = '\0';
        return 4;
    }
    return 0; // invalid codepoint
}


void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    state *state = glfwGetWindowUserPointer(window);
    state->renderer.window_width = width;
    state->renderer.window_height = height;

    // make sure the viewport matches the new window dimensions; note that width and 
    // height will be significantly larger than specified on retina displays.
    glViewport(0, 0, width, height);
}
