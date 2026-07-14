
typedef struct
{
    segmented_node *node;
    u32 pos;
    u32 line;
} node_iter;

typedef struct
{
    b32 valid;
    u32 cell;
    u32 len;
} cell_item;

typedef node_iter node_iter_rev;
typedef node_iter node_iter_line;

typedef struct 
{
    segmented_node *node;
    u32 pos;
    u32 line;
    piece *piece;
} piece_iter;

typedef piece_iter piece_iter_rev;

typedef enum
{
    Position    = 1,       
    LineNumber  = 2,     
    // AbsoluteIdx = 4,    
} iter_type;

typedef struct base_iter
{
    piece_list *list;
    segmented_node *node;
    u32 node_pos;      // Pos offset of the current node;
    u32 node_line;     // Line offset of the current node
    u32 piece_pos;     // Pos offset of the current piece  (relative to the current node);
    u32 piece_line;    // Line offset of the current piece (relative to the current node);
                       //
    u32 pos_in_piece;  // Pos offset of the current line   (relative to the current piece);
    u32 line_in_piece; // Line offset of the current line  (relative to the current piece);

    u32 abs_idx;       // absolute index of the first piece of the current node;
    u32 piece_idx;     // index of the current piece relative to the current node;

    iter_type type;    // iteration type.
} base_iter;

// typedef struct
// {
    // const piece_list *list;
    // const segmented_node *node;
    // u32 node_pos;      // Pos offset of the current node;
    // u32 piece_pos;     // Pos offset of the current piece (relative to the current node);
    // u32 pos_in_piece;  // Pos offset of the current line  (relative to the current piece);
    // u32 node_line;     // Line offset of the current node
    // u32 piece_line;    // Line offset of the current piece (relative to the current node);
    // u32 line_in_piece; // Line offset of the current line  (relative to the current piece);
                       // //
    // const piece *piece;
// } line_starts;



// static inline u32 current_line_(base_iter *l)
// {
//     u32 result = l->node_line + l->piece_line + l->line_in_piece;
//     return result;
// }
//
// static inline u32 current_position_(base_iter *l)
// {
//     u32 result = l->node_pos + l->piece_pos + l->pos_in_piece;
//     return result;
// }



// typedef base_iter line_starts;

// typedef struct 
// {
    // line_starts starts;
    // const buffer *buffer;
// 
// } render_iter;
// 
// typedef struct
// {
    // b8 valid; 
    // u8 item;
// } render_item;

// typedef struct 
// {
    // line_starts l_starts;
    // u32 last_line_pos;
// } line_lengths;
// 
// typedef struct
// {
    // b32 valid;
    // u32 len;
// } lengths_item;
// 
// static inline u32 current_line(line_starts *l)
// {
    // u32 result = l->node_line + l->piece_line + l->line_in_piece;
    // return result;
// }
// 
// static inline u32 current_position(line_starts *l)
// {
    // u32 result = l->node_pos + l->piece_pos + l->pos_in_piece;
    // return result;
// }
// 
// typedef struct
// {
    // u32 num_lines;
    // u32 rows_in;
    // u32 rows_out;
// } lines_result;
// 
// static inline b32 equal_lines_result(lines_result a, lines_result b)
// {
    // b32 result = a.num_lines == b.num_lines && 
                 // a.rows_in == b.rows_in && 
                 // a.rows_out == b.rows_out;
    // return result;
// }

