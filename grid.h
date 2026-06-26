// Grid represents the set of screen cells occupied by the window.
//
// First idea i had was to make the grid fixed size.
// Whose elements represent screen cells.
//  The elements were for grapheme clusters less than sizeof(u32).
//  If a screen cell was some grapheme cluster bigger than sizeof(u32), then the u32 would be the hash of 
//  the utf8-string corresponding to the cluster. 
//  Additinally we would have to implement a hash table whose values would contain the actual utf8-string corresponding to the cluster.
//  
// 
// My new idea it to make the grid variable size (but logically it spans the screen cells exactly).
// The grid is made up of sequences. 
// Each logical row starts with a sequence.
// A sequence represents a "sequence" of grapheme clusters (cells) of the same byte size. 
// Each sequence starts with a sequence metadata spanning a u32 value.
//  The metadata contains the size of the cell in bytes, and the number of cells in the sequence.
//
//  For instance: "O rato roeu a rolha da garrafa do rei da russia"
//
//  Would have the metadata = (1, 47).
//
//  My reasoning for this approach is that 
//      1> For code (ascii), rows have only one sequence metadata. 
//      2> Most languages have the same byte size for their chararcters, 
//         since they span the same unicode plane, 
//         thus most rows will have the minimal amount of metadata. 
//      3> I can use simd (within the same sequence) when comparing the last grid row with the current grid row.
//
//  FORGET THAT.
//  Found a better idea.
//  We can make the grid fixed sized. And cells can be u32 values, 
//  but we don't need to cache grapheme clusters bigger than u32, 
//  We are already storing them in either the original buffer.
//  Or the append buffer, the indexes/pointers into those will always be valid.
//  So when a grapheme cluster is bigger then 4 bytes, we store the index into the buffer and the 
//  buffer identifier.
//
//  This is possible because grapheme clusters will be contiguous in the buffer.
//  Moreover we can be sure that if two cells that correspond to indexes are equal, than 
//  the corresponing utf8-strings are also equal, we do not need to check for 
//  collisions.
//
//  This works for windows associated with buffers.






typedef struct
{
    u32 value;
    // This can either be an utf8-string of max sizeof(u32) or and index into 
    // a buffer. In that case
} cell;

typedef struct
{
    u32 index;
    buffer_type type;
} buffer_index;

static inline b32 is_not_inlined(cell cell)
{
    b32 result = ((cell.value & 0x80) == 0x80);
    return result;
}

static inline cell cell_from_buffer_index(u32 index, buffer_type type)
{
    cell result = {
        .value = 0x80 | (type << 3) | (index << 4)
    };
    return result;
}

static inline buffer_index buffer_index_from_cell(cell cell)
{
    buffer_index result = {
        .index = cell.value >> 4,
        .type  = (cell.value & (1 << 3)) >> 3,
    };
    return result;
}





// Came up with a more complex idea:
// But first let state the desired properties of a grid.
//
//  1> A grid should hold the state of the rendered text currently on the screen.
//  2> It should be used to compare the state of the current frame with the old frame such that 
//  the minimal amount of text is sent to the terminal.
//  3> The grid should represent the cells on the screen.
//      3.1> Each cell may be variable sized as it represents a grapheme cluster.
//          Statistical observations:
//              1> the vast majority of grapheme clusters are in 1-1 correspondence with codepoints.
//              2> Most languages' codepoints and by extension most of its grapheme clusters, lie in 
//              the same unicode plane, meaning they most likely have the same byte length.
//              3> Most code is written in english (ascii).
//
//  The previous implementation of a grid consisted of a buffer of u32s, which either represented,
//  the grapheme directly, or was an index into the window's associated buffer, for graphemes bigger 
//  than 4 bytes.
//
//  Now i'm thinking, 4 buffers: 
//      .text_8    = contains only graphemes of byte len 1.
//      .text_16   = contains only graphemes of byte len 2.
//      .text_24   = contains only graphemes of byte lne 3.
//      .text_32_i = contains only graphemes of byte len 4 or an index into the windows associated buffer; 
//
//  Downsides:
//      1> In the worst case (random grapheme lengths), this approach takes more memory, 
//      a little less than double the memory footprint of the previous approach;
//      2> More complicated algorithmically.
//      3> These are the main ones, i'm sure there are others.
//
//  Advantages: 
//      1> Best case: ascii this approach is pretty fast because:
//              1.1> cells are packed contiguously, I can use simd to calculate the diff.
//              1.2> optimal amount of memory (1 byte per cell).
//              1.3> Memory is allocated on demand, if there is no cell bigger than size x,
//              then there is no need to allocate its corresponding grid
//
//
//  Another question: 
//      1> Would it be reasonable to make the grid span more than one screen length?
//         We could make the grid span 3 screen lengths, one for the screen itself
//         one for the text just above the screen, and another for the text below.
//         This would make 

typedef struct line
{
    u16 start;
    u16 num_rows;
    u16 len_bytes;
    u16 len_grapheme;
} line;

typedef enum
{
    U8 = 0,
    U16 = 1,
    U24 = 2,
    U32 = 3,
} grid_type;

// 
typedef struct
{
    u8 *first_bit;
    u8 *second_bit;
} grid_bitmap;

// typedef enum
// {
//     Default,
//     Reversed,
// } attr;

typedef u8 attr;

#define Default 0
#define Reversed 1

typedef struct
{
    u16 rows;
    u16 cols;

    grid_type *types;

    u8 *text_8;
    u8 *text_16;
    u8 *text_24;
    u8 *text_32_i;

    attr *attr;

} multilevel_grid;

static inline void free_multilevel_grid(multilevel_grid *grid)
{
    free(grid->types);
    free(grid->text_8);
    free(grid->attr);

    if (grid->text_16)
    {
        free(grid->text_16);
    }

    if (grid->text_24)
    {
        free(grid->text_24);
    }

    if (grid->text_32_i)
    {
        free(grid->text_32_i);
    }
}

typedef struct 
{
    multilevel_grid *grid;
    u16 y_offset;
    u16 x_offset;
    u16 height;
    u16 width;

    // line *lines;
    // u16 *grid_lines;
} grid_view;

// static void free_view(grid_view *view)
// {
//     if (view->grid_lines)
//     {
//         free(view->grid_lines);
//     }
// }
//

typedef struct
{
    multilevel_grid *grid;
    u32 line_start;
    u16 width;
} grid_line;

static inline grid_view default_grid_view(
    multilevel_grid *grid,
    u16 y_offset,
    u16 x_offset,
    u16 height,
    u16 width)
{
    // if (
    // u16 *grid_lines = 
    // u16 *grid_lines = (u16 *) malloc(sizeof(u16)  * height);
    //
    // for (u32 i = 0; i < height; ++i)
    // {
    //     grid_lines[i] = i * grid->cols;
    // }
    
    grid_view result = {
        .grid  = grid,
        .y_offset = y_offset,
        .x_offset = x_offset,
        .height   = height,
        .width = width,
        // .grid_lines = grid_lines,
    };
    return result;
}


static inline void free_grid_view(grid_view view)
{
    free_multilevel_grid(view.grid);
    // free(view.grid_lines);
}

static inline grid_line get_grid_line(grid_view a, u16 line)
{
    // u32 line_start = a.grid_lines[line];
    // u32 line_start = a.grid->cols * (a.y_offset + line); // + a.x_offset;
    u32 line_start = a.grid->cols * line; // + a.x_offset;
    grid_line result = { .grid = a.grid, .line_start = line_start, .width = a.grid->cols };
    return result;
}

static inline void initialize_multilevel_grid(multilevel_grid *grid, u32 rows, u32 cols)
{
    grid->rows = (u16) rows;
    grid->cols = (u16) cols;

    u32 num_cells = rows * cols;
    // both of these may be statically allocated, they are never null;
    grid->types     = malloc(sizeof(grid_type) * num_cells);
    grid->text_8    = malloc(sizeof(u8) * num_cells);
    grid->attr      = malloc(sizeof(attr) * num_cells);

    memset(grid->types, U8, sizeof(grid_type) * num_cells);
    memset(grid->text_8, ' ', sizeof(u8) * num_cells);
    memset(grid->attr, Default, sizeof(attr) *  num_cells);

    grid->text_16   = 0;
    grid->text_24   = 0;
    grid->text_32_i = 0;
}

typedef struct
{
    u8 *data;
    grid_type *type; 
    attr *attr;
} s_cell;

static inline grid_type get_cell_type(grid_line line, u32 idx)
{
    grid_type result = line.grid->types[line.line_start + idx];
    return result;
}

static inline grid_type *get_cell_type_(grid_line line, u32 idx)
{
    grid_type *result = line.grid->types + line.line_start + idx;
    return result;
}

static inline attr get_cell_attr(grid_line line, u32 idx)
{
    attr attr = line.grid->attr[line.line_start + idx];
    return attr;
}

static inline attr *get_cell_attr_(grid_line line, u32 idx)
{
    attr *attr = line.grid->attr + line.line_start + idx;
    return attr;
}

static inline u8 *get_cell_data(grid_line line, u32 idx, grid_type type)
{
    // This is dangerous!! it relies on the fact that a.grid->text_8, a.grid->text_16, ...
    // are contiguous in memory (the pointers,  not the line.grid->cols it points to).
    u8 **start = &(line.grid->text_8) + type;
    if (*start == NULL)
    {
        u32 num_cell = line.grid->rows * line.grid->cols;
        *start = (u8 *) malloc(num_cell * sizeof(u8) * (type + 1));
    }
    u8 *result = *start + line.line_start * (type + 1) + idx * (type + 1);
    return result;
}

static inline s_cell get_cell(grid_line line, u32 idx)
{
    grid_type *type = get_cell_type_(line, idx);
    u8 *data = get_cell_data(line, idx, *type);
    attr *attr = get_cell_attr_(line, idx);

    s_cell result = { .data = data, .type = type, .attr = attr };
    return result;
}

static inline u32 get_cell_len(grid_line line, u32 idx)
{
    grid_type type = get_cell_type(line, idx);
    u32 result = (type + 1);
    if (type == U32)
    {
    }
    return result; 
}

static inline void copy_cell(grid_line dst, grid_line src, u32 idx)
{
    s_cell src_cell = get_cell(src, idx);
    dst.grid->types[dst.line_start + idx] = *(src_cell.type);
    u8 *dst_data = get_cell_data(dst, idx, *(src_cell.type));
    memcpy((void *) dst_data, (const void *) src_cell.data, sizeof(u8) * (*(src_cell.type) + 1));
}

static inline void copy_cells(grid_line dst, grid_line src, u32 idx, u32 len)
{
    s_cell src_cell = get_cell(src, idx);
    dst.grid->types[dst.line_start + idx] = *src_cell.type;
    u8 *dst_data          = get_cell_data(dst, idx, *src_cell.type);
    grid_type *dst_types = get_cell_type_(dst, idx);
    attr *dst_attr        = get_cell_attr_(dst, idx);
    memcpy((void *) dst_data, (const void *) src_cell.data, len * sizeof(u8) * (*src_cell.type + 1));
    memcpy((void *) dst_types, (const void *) src_cell.type, len * sizeof(grid_type));
    memcpy((void *) dst_attr, (const void *) src_cell.attr, len * sizeof(attr));
}

static inline void copy_cell_(grid_line dst, grid_line src, u32 dst_idx, u32 src_idx)
{
    s_cell src_cell = get_cell(src, src_idx);
    u8 *dst_data = get_cell_data(dst, dst_idx, *src_cell.type);
    memcpy((void *) dst_data, (const void *) src_cell.data, sizeof(u8) * (*src_cell.type + 1));
    dst.grid->types[dst.line_start + dst_idx] = *src_cell.type;
    dst.grid->attr[dst.line_start + dst_idx] = *src_cell.attr;
}

static inline b32 cells_are_equal_(grid_line a, grid_line b, u32 a_index, u32 b_index)
{
    grid_type a_type = get_cell_type(a, a_index);
    grid_type b_type = get_cell_type(b, b_index);;

    attr a_attr = get_cell_attr(a, a_index);
    attr b_attr = get_cell_attr(b, b_index);

    b32 result = false;
    if (a_type == b_type && a_attr == b_attr)
    {
        u8 *s1 = get_cell_data(a, a_index, a_type);
        u8 *s2 = get_cell_data(b, b_index, b_type);

        result =  (memcmp((void *) s1, (const void *) s2, sizeof(u8) * (a_type + 1)) == 0);
    }
    return result;
}

static inline b32 cells_are_equal(grid_line a, grid_line b, u32 index)
{
    grid_type a_type = get_cell_type(a, index);
    grid_type b_type = get_cell_type(b, index);

    attr a_attr = get_cell_attr(a, index);
    attr b_attr = get_cell_attr(b, index);

    b32 result = false;
    if (a_type == b_type && a_attr == b_attr)
    {
        u8 *s1 = get_cell_data(a, index, a_type);
        u8 *s2 = get_cell_data(b, index, b_type);

        result =  (memcmp((void *) s1, (const void *) s2, sizeof(u8) * (a_type + 1)) == 0);
    }
    return result;
}

// static void prepend(u16 *line_offsets, u16 offset, u32 count) 
// {
//     Assert(line_offsets);
//     Assert(count > 0)
//     memmove((void *) (line_offsets + 1), (const void *)line_offsets, (count - 1) * sizeof(u16));
//     line_offsets[0] = offset;
// }
//
// static void insert_at_g(void *buf, u32 at, void *data, u32 data_size, u32 count)
// {
//     Assert(buf);
//     Assert(at <= count);
//     void *dst = (void *) ((u8 *) buf + data_size * (at + 1));
//     void *src = (void *) ((u8 *) buf + data_size * (at));
//
//     memmove(dst, src, (count - at) * data_size);
//     memcpy(src, data, data_size);
// }
//
// static void insert_at(u16 *line_offsets, u16 at, u16 value, u32 count)
// {
//     Assert(line_offsets);
//     Assert(at <= count);
//     memmove(line_offsets + at + 1, line_offsets + at, (count - at) * sizeof(u16));
//     line_offsets[at] = value;
// }
//
// static void push(u16 *line_offsets, u16 value, u32 count)
// {
//     line_offsets[count] = value;
// }

