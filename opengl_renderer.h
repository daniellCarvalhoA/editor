
#include "atlas.h"
#include "shader.h"
#define MAX_VERTICES 1024 * 64
#define MAX_INDICES 1024 * 64

typedef struct vertex
{
    v2f position;
    v2f uv;
} vertex;

typedef struct solid_vertex
{
    v2f position;
} solid_vertex;

typedef enum
{
    Shader_Solid = 0,
    Shader_Text,
    Shader_Count,
} shader_index;

typedef struct 
{
    v2f position;
    v2f velocity;
    f32 damping;
    f32 stiffness;
} camera;

void camera_update(camera *cam, v2f target, f32 dt)
{
    //cam->position.x -= 1.0f;
    //cam->position.y -= 1.0f;
    f32 stiffness = 60.0f;
    f32 damping = 20.0f;
    v2f error = sub_v2f(target, cam->position);
    cam->velocity = add_v2f(cam->velocity, scalar_mulv2f(stiffness * dt, error));
    cam->velocity = scalar_mulv2f(expf(- damping * dt), cam->velocity);


    if ((fabs(cam->velocity.x) > 1.0f) || (fabs(cam->velocity.y) > 1.0f))
    {
        cam->position = add_v2f(cam->position, scalar_mulv2f(dt, cam->velocity));
    }
}


typedef struct
{
    GLFWwindow *window;
    atlas *atlas;
    GLuint text_vao;
    GLuint text_vbo;
    GLuint text_ebo;

    u32 rows;
    u32 cols;


    i32 window_width;
    i32 window_height;

    GLuint solid_vao;
    GLuint solid_vbo;
    // GLuint solid_ebo;

    shader shaders[Shader_Count];

    u32 vertex_count;
    vertex vertices[MAX_VERTICES];

    u32 index_count;
    u32 indices[MAX_INDICES]; 

    u32 solid_vertex_count;
    solid_vertex solid_vertices[16];

    str text;

    camera camera;

    f32 accumulator;

    f32 x;
    f32 y;

} renderer;

void renderer_initialize(GLFWwindow *window, memory_arena *arena, renderer *r, u32 pixel_size);
void renderer_sync_text(renderer *r);
void renderer_draw_text(renderer *r);
void renderer_flush_text(renderer *r);
void renderer_sync_solid(renderer *r);
void renderer_draw_solid(renderer *r);
void renderer_flush_solid(renderer *r);
void renderer_set_shader(renderer *r, shader_index i);
void renderer_queue_quad(renderer *r, rect src, rect dst);
void renderer_queue_solid_quad(renderer *r, rect src, rect dst);
