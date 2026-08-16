
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
} iter_type;

typedef struct base_iter
{
    piece_list *list;
    segmented_node *node;

    u32 node_pos;  // Pos offset of the current node;
    u32 node_line; // Line offset of the current node
    u32 piece_pos; // Pos offset of the current piece (relative to the current node);
    u32 piece_line; // Line offset of the current piece (relative to the current node);
    u32 pos_in_piece; // Pos offset of the current line (relative to the current piece);
    u32 line_in_piece; // Line offset of the current line 
                       // (relative to the current piece);

    u32 abs_idx;       // absolute index of the first piece of the current node;
    u32 piece_idx;     // index of the current piece relative to the current node;
    iter_type type;    // iteration type.
} base_iter;

typedef struct {
    base_iter start;
    base_iter end;
} iter_range;

