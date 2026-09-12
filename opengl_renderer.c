
void renderer_initialize(GLFWwindow *window, memory_arena *arena, renderer *r, u32 pixel_size) {

    // set up window dimensions
    r->window = window;
    glfwGetFramebufferSize(r->window, &r->window_width, &r->window_height);

    glGenVertexArrays(1, &r->text_vao);
    glBindVertexArray(r->text_vao);

    glGenBuffers(1, &r->text_vbo);
    glGenBuffers(1, &r->text_ebo);

    glBindBuffer(GL_ARRAY_BUFFER, r->text_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(r->vertices), 0, GL_DYNAMIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, r->text_ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(r->indices), 0, GL_DYNAMIC_DRAW);

    // position
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(vertex), (void *) offsetof(vertex, position));

    // texture
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(vertex), (void *) offsetof(vertex, uv)); 


    shader text_shader_program = create_shader_program_from_file("shaders/text.vert", "shaders/text.frag");

    if (!text_shader_program.id)
    {
        fprintf(stderr, "unable to compile shaders\n");
        exit(1);
    }

    glGenVertexArrays(1, &r->solid_vao);
    glBindVertexArray(r->solid_vao);
    //
    glGenBuffers(1, &r->solid_vbo);
    //
    glBindBuffer(GL_ARRAY_BUFFER, r->solid_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(r->solid_vertices), 0, GL_DYNAMIC_DRAW);
    //
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(solid_vertex), (void *) 0);

    shader solid_shader_program = create_shader_program_from_file("shaders/solid.vert", "shaders/solid.frag");
    if (!solid_shader_program.id)
    {
        fprintf(stderr, "unable to compile shaders\n");
        exit(1);
    }

    r->shaders[Shader_Solid] = solid_shader_program;
    r->shaders[Shader_Text] = text_shader_program;
    r->atlas = create_atlas(arena, pixel_size);

    r->rows = r->window_height / r->atlas->ascent;
    r->cols = r->window_width  / r->atlas->max_width;

    // NOTE: just ascii for now
    r->text.buffer = PushArray(arena, r->rows * r->cols, u8, NoClear());
    r->text.len = r->rows * r->cols;
    memset(r->text.buffer, ' ', r->text.len);

    // set up texture

    GLuint texture_id;
    glGenTextures(1, &texture_id);
    glBindTexture(GL_TEXTURE_2D, texture_id);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    r->atlas->texture_id = texture_id;

    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RED,
        r->atlas->width, 
        r->atlas->height, 
        0,
        GL_RED,
        GL_UNSIGNED_BYTE,
        r->atlas->texture);

    renderer_set_shader(r, Shader_Text);
 
    // this has to be updated on resize

    glUniform1i(glGetUniformLocation(r->shaders[Shader_Text].id, "atlas"), 0);


}

void renderer_sync_text(renderer *r)
{
    glBindVertexArray(r->text_vao);

    glBindBuffer(GL_ARRAY_BUFFER, r->text_vbo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, r->text_ebo);

    glBufferSubData(GL_ARRAY_BUFFER, 0, r->vertex_count * sizeof(vertex), r->vertices);
    glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, r->index_count * sizeof(u32), r->indices);
}

void renderer_draw_text(renderer *r)
{
    // glDrawArrays(GL_TRIANGLE_STRIP, 0, r->vertex_count);
    glDrawElements(GL_TRIANGLES, r->index_count, GL_UNSIGNED_INT, 0);
}

void renderer_sync_cursor(renderer *r)
{
    glBindVertexArray(r->solid_vao);

    glBindBuffer(GL_ARRAY_BUFFER, r->solid_vbo);

    glBufferSubData(GL_ARRAY_BUFFER, 0, r->solid_vertex_count * sizeof(solid_vertex), r->solid_vertices);
}

void renderer_draw_cursor(renderer *r)
{
    glDrawArrays(GL_TRIANGLES, 0, r->solid_vertex_count);
}

void renderer_flush_text(renderer *r)
{
    renderer_sync_text(r);
    renderer_draw_text(r);
    r->vertex_count = 0;
    r->index_count = 0;
}

void renderer_flush_cursor(renderer *r)
{
    renderer_sync_cursor(r);
    renderer_draw_cursor(r);
    r->solid_vertex_count = 0;
}

void renderer_set_shader(renderer *r, shader_index i)
{
    glUseProgram(r->shaders[i].id);
}

static void queue_quad(renderer *r, rect dst, rect src) 
{
    f32 tx = (f32) (src.x) / (f32) r->atlas->width;
    f32 ty = (f32) (src.y) / (f32) r->atlas->height;
    f32 tw = (f32) (src.w) / (f32) r->atlas->width;
    f32 th = (f32) (src.h) / (f32) r->atlas->height;

    f32 vx = (f32) dst.x;
    f32 vy = (f32) dst.y;
    f32 vw = (f32) dst.w;
    f32 vh = (f32) dst.h;

    u32 vertex_ix = r->vertex_count;

    vertex vertices[4] = 
    {
        {{vx, vy}, {tx, ty}},
        {{vx + vw, vy}, {tx + tw, ty}},
        {{vx, vy + vh}, {tx, ty + th}},
        {{vx + vw, vy + vh}, {tx + tw, ty + th}},
    };

    memcpy(r->vertices + r->vertex_count, vertices, sizeof(vertices));

    r->vertex_count += 4;

    u32 indexes[6] = {
        vertex_ix + 0,
        vertex_ix + 1,
        vertex_ix + 3,

        vertex_ix + 0,
        vertex_ix + 3,
        vertex_ix + 2
    };

    memcpy(r->indices + r->index_count, indexes, sizeof(indexes));
    r->index_count += 6;
}

static void queue_solid_quad(renderer *r, u32 row, u32 col)
{

    f32 vx = (f32) (col * r->atlas->max_width);
    f32 vy = (f32) (row * r->atlas->ascent);
    f32 vw = (f32) (r->atlas->max_width);
    f32 vh = (f32) (r->atlas->max_height);

    // solid_vertex vertices[6] = {
    //     {{100,100}},
    //     {{110,100}},
    //     {{100,110}},
    //     {{100,110}},
    //     {{110,100}},
    //     {{110,110}}
    // };
    solid_vertex vertices[6] =
    {
        {{vx, vy}},
        {{vx + vw, vy}},
        {{vx, vy + vh}},
//
        {{vx, vy + vh}},
        {{vx + vw, vy}},
        {{vx + vw, vy + vh}},
    };

    memcpy(
        r->solid_vertices + r->solid_vertex_count,
        vertices,
        sizeof(vertices)
    );

    r->solid_vertex_count += 6;
}

// static void queue_solid_quad(renderer *r, u32 row, u32 col)
// // static void queue_solid_quad(renderer *r, rect dst)
// {
//     f32 vx = (f32) (col * r->atlas->max_width);
//     f32 vy = (f32) (row * r->atlas->ascent);
//     f32 vw = (f32) (r->atlas->max_width);
//     f32 vh = (f32) (r->atlas->max_height);
//
//     solid_vertex vertices[4] = 
//     {
//         {{vx, vy}},
//         {{vx + vw, vy }},
//         {{vx, vy + vh }},
//         {{vx + vw, vy + vh}},
//     };
//
//     memcpy(r->solid_vertices + r->solid_vertex_count, vertices, sizeof(vertices));
//
//     r->solid_vertex_count += 4;
// }


void queue_text(renderer *rd, i32 row, i32 col, str text)
{
    // u32 row = 0; 
    // i32 pen_x = col * rd->atlas->max_width;
    // i32 pen_y = row * rd->atlas->ascent + rd->atlas->ascent;
    for (u32 i = 0; i < text.len; ++i)
    {
        char c = text.buffer[i];
        if (c == ' ')
        {
            col++;
            // glyph glyph = rd->atlas.glyphs[(i32) c];
            // pen_x += rd->atlas->max_width;
            // pen_x += (app.advance >> 6);
        }
        else if (c == '\n')
        {
            // pen_x = 0;
            // pen_y += rd->atlas->ascent;
            row++;
            col = 0;
            // app->cursor.y += app->atlas.ascent;
        }
        else
        {
            // f32 distance = row - app->curosr.x;
            // f32 scale = 1.0f / (1.0f + fabs(distance) * 0.05f);
            // assert(*c < 128);
            i32 pen_x = col * rd->atlas->max_width;
            i32 pen_y = row * rd->atlas->ascent + rd->atlas->ascent;
            glyph glyph = rd->atlas->glyphs[(i32) c];
            rect src = glyph.atlas_dim;

            rect dst = {
                .x = pen_x + glyph.bearing_x,
                .y = pen_y - glyph.bearing_y,
                .w = src.w,
                .h = src.h,
            };

            queue_quad(rd, dst, src);
            // pen_x += rd->atlas->max_width;
            col++;
            // app->camera.x += app->atlas.max_width;
            // pen_x += (glyph.advance >> 6);
        }
    }
    // app->cursor.x = pen_x;
    // app->cursor.y = pen_y;
}

static void opengl_renderer(renderer *rd, render_commands *r_commands, f32 dt)
{
    glfwGetFramebufferSize(rd->window, &rd->window_width, &rd->window_height);
    glViewport(0, 0, rd->window_width, rd->window_height);
    mat4f p = orthographic(0.0f, rd->window_width, rd->window_height, 0.0f, -1.0f, 1.0f);

    renderer_set_shader(rd, Shader_Text);
    u32 p_loc_text = glGetUniformLocation(rd->shaders[Shader_Text].id, "proj");
    glUniformMatrix4fv(p_loc_text, 1, GL_FALSE, (void *) p.mat);

    renderer_set_shader(rd, Shader_Solid);
    u32 p_loc_solid = glGetUniformLocation(rd->shaders[Shader_Solid].id, "proj");
    glUniformMatrix4fv(p_loc_solid, 1, GL_FALSE, (void *) p.mat);

    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // bind textures
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, rd->atlas->texture_id);


    v2i cursor = {};
    for (u32 i = 0; i < r_commands->count; ++i)
    {
        render_command command = r_commands->commands[i];

        switch (command.type)
        {
            case RenderCommand_Text:
            {
                u32 row = command.text.row;
                u32 col = command.text.col;
                if (col + command.text.text.len < rd->cols && row < rd->rows)
                {
                    memcpy( rd->text.buffer + row * rd->cols + col, command.text.text.buffer, command.text.text.len);
                }
                // rd->text[row * rd->rows + col]
                // queue_text(rd, command.text.row, command.text.col, command.text.text);
            } break;

            case RenderCommand_Rect:
            {
                cursor.y = command.cursor.row;
                cursor.x = command.cursor.col;
            } break;

            default:
            {
                // TODO: Handle lines
            } break;
        }
    }


    v2f screen_cursor = { 
        .x = (f32) (cursor.x * rd->atlas->max_width),
        .y = (f32) (cursor.y * rd->atlas->max_height),
    };

    v2f target = {
        .x = (screen_cursor.x - (f32) rd->window_width) * 0.25f,
        .y = (screen_cursor.y - (f32) rd->window_height) * 0.25f 
    };

    camera_update(&rd->camera, target, dt);
    // rd->camera.position = target;
    v3f trans_vector = { 
        .x = - (rd->camera.position.x) ,
        .y = - (rd->camera.position.y) , 
        .z = 0.0f
    };

    mat4f view = translation(trans_vector);

    renderer_set_shader(rd, Shader_Text);
    u32 view_loc_text = glGetUniformLocation(rd->shaders[Shader_Text].id, "view");
    glUniformMatrix4fv(view_loc_text, 1, GL_FALSE, (void *) view.mat);

    renderer_set_shader(rd, Shader_Solid);
    u32 view_loc_solid = glGetUniformLocation(rd->shaders[Shader_Solid].id, "view");
    glUniformMatrix4fv(view_loc_solid, 1, GL_FALSE, (void *) view.mat);

    renderer_set_shader(rd, Shader_Text);
    for (u32 i = 0; i < rd->rows; ++i)
    {
        str s = { 
            .buffer = rd->text.buffer + i * rd->cols,
            .len = rd->cols
        };
        queue_text(rd, i, 0, s);
    }

    // queue_text(rd, 0, 0, rd->text, 
    renderer_flush_text(rd);

    renderer_set_shader(rd, Shader_Solid);
    queue_solid_quad(rd, cursor.y, cursor.x);
    renderer_flush_cursor(rd);


    // };




}


