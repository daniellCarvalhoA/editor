#include <freetype/ftglyph.h>



typedef struct
{
    rect atlas_dim;

    i32 bearing_x;
    i32 bearing_y;

    i32 advance;
} glyph;

typedef struct
{
    unsigned int texture_id;
    u8 *texture;
    // v2i texture_dims;
    u32 pixel_size;
    glyph *glyphs;
    //i32 char_width;
    //i32 char_height;
    i32 width;
    i32 height;

    i32 max_height;
    i32 max_width;

    i32 ascent;
} atlas;

static atlas *create_atlas(memory_arena *arena, u32 pixel_size)
{
    atlas *atls = PushStruct(arena, atlas, NoClear());
    FT_Library ft;

    if (FT_Init_FreeType(&ft))
    {
        return atls;
    }

    FT_Face face;
    if (FT_New_Face(ft, "fonts/FreeMonoBold.ttf", 0, &face))
    {
        return atls;
    }

    FT_Set_Pixel_Sizes(face, pixel_size, pixel_size);

    u32 num_chars = 128;
    atls->pixel_size = pixel_size;
    atls->width = 1024;
    atls->height = 1024;
    atls->texture = PushArray(arena, atls->width * atls->height, u8, NoClear());
    atls->glyphs = PushArray(arena, num_chars, glyph, NoClear());
    atls->ascent = face->size->metrics.ascender >> 6;
    atls->max_height = face->size->metrics.height >> 6;

    // i32 cursor_x = 0;
    // i32 cursor_y = 0;
    i32 row_height = 0;
    i32 padding = 4;

    i32 atls_y = 0;
    i32 atls_x = 0;

    for (u32 c = 0; c < 128; ++c)
    {
        FT_Load_Char(face, c, FT_LOAD_RENDER);
        FT_Bitmap bmp  = face->glyph->bitmap;
        atls->max_width = Maximum(atls->max_width, face->glyph->advance.x >> 6);

        if (atls_x + (i32) bmp.width > atls->width)
        {
            atls_x = 0;
            atls_y += row_height;
            row_height = 0;
        }

        atls->glyphs[c].atlas_dim = (struct rect) {
            .x = atls_x,
            .y = atls_y,
            .w = bmp.width,
            .h = bmp.rows
        };
        atls->glyphs[c].bearing_x = face->glyph->bitmap_left;
        atls->glyphs[c].bearing_y = face->glyph->bitmap_top;
        atls->glyphs[c].advance = face->glyph->advance.x;
        
        for (u32 row = 0; row < bmp.rows; row++)
        {
            memcpy(
                atls->texture + (atls_y + row) * atls->width + atls_x,
                bmp.buffer + row * bmp.pitch,
                bmp.width);
        }

        atls_x += bmp.width + padding;
        row_height = Maximum(row_height, (i32) bmp.rows);
    }

    return atls;

}


//static atlas create_ascii_atlas(memory_arena *arena, u32 pixel_size)
//{
//    atlas atlas = {};
//    FT_Library ft;
//
//    if (FT_Init_FreeType(&ft))
//    {
//        return atlas;
//    }
//
//    FT_Face face;
//    if (FT_New_Face(ft, "fonts/FreeMonoBold.ttf", 0, &face))
//    {
//        return atlas;
//    }
//
//    FT_Set_Pixel_Sizes(face, pixel_size, pixel_size);
//
//    const u32 num_ascii_chars = 128;
//
//    u8 *char_to_bitmap[128];
//    FT_BBox char_to_bbox[128];
//
//    FT_BBox max_bbox = { .xMin = 0, .yMin = 0, .xMax = 0, .yMax = 0 };
//
//    for (u32 i = 0; i < num_ascii_chars; ++i)
//    {
//        FT_Load_Char(face, i, FT_LOAD_RENDER);
//
//        FT_Bitmap bmp  = face->glyph->bitmap;
//        u32 bmp_width  = bmp.width;
//        u32 bmp_height = bmp.rows;
//        u32 bmp_pitch  = bmp.pitch;
//        char_to_bitmap[i] = bmp.buffer;
//
//        FT_Glyph glyph;
//        FT_Get_Glyph(face->glyph, &glyph);
//        FT_BBox bbox;
//        FT_Glyph_Get_CBox(glyph, FT_GLYPH_BBOX_PIXELS, &bbox);
//        char_to_bbox[i] = bbox;
//
//        max_bbox.xMin = Minimum(max_bbox.xMin, bbox.xMin);
//        max_bbox.yMin = Minimum(max_bbox.yMin, bbox.yMin);
//        max_bbox.xMax = Maximum(max_bbox.xMax, bbox.xMax);
//        max_bbox.yMax = Maximum(max_bbox.yMax, bbox.yMax);
//    }
//
//    i32 char_width = max_bbox.xMax - max_bbox.xMin;
//    i32 char_height = max_bbox.yMax - max_bbox.yMin;
//
//    const u32 num_chars = num_ascii_chars + 1; // ascii + one white box
//    u8 *texture = PushArray(arena, num_chars * (char_width * char_height), u8, NoClear());
//    rect *char_to_rect = PushArray(arena, num_chars, rect, NoClear());
//
//    u32 cursor = 0;
//    for (u32 i = 0; i < 128; ++i)
//    {
//        FT_BBox c_box = char_to_bbox[i];
//
//        i32 bitmap_width = c_box.xMax - c_box.xMin;
//        i32 bitmap_height = c_box.yMax - c_box.yMin;
//
//        u8 *bmp = char_to_bitmap[i];
//
//        for (i32 by = 0; by <  bitmap_height; ++by)
//        {
//            for (i32 bx = 0; bx <  bitmap_width; ++bx)
//            {   
//                i32 tx = (i * char_width) + bx + (c_box.xMin - max_bbox.xMin);
//                i32 ty = by + (max_bbox.yMax - c_box.yMax);
//                i32 ti = ty * (i32) num_chars * char_width + tx;
//                i32 bi = by * bitmap_width + bx;
//                texture[ti] = bmp[bi];
//            }
//        }
//
//        char_to_rect[i] = (struct rect) { 
//            .x = i * char_width,
//            .y = 0,
//            .w = char_width,
//            .h = char_height 
//        };
//    }
//
//    atlas.texture = texture;
//    atlas.char_to_rect = char_to_rect;
//    atlas.pixel_size = pixel_size;
//    atlas.texture_dims.x = (i32) num_chars * char_width;
//    atlas.texture_dims.y = char_height;
//    atlas.char_width = char_width;
//    atlas.char_height = char_height;
//    return atlas;
//}
