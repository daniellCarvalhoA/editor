typedef struct
{
    b32 valid;
    u32 cell;
    u32 len;
} cell_item;

typedef enum
{
    Position    = 1,       
    LineNumber  = 2,     
} iter_type;

typedef struct base_iter
{
    segmented_node *node;

    u32 node_pos;  // Pos offset of the current node;
    u32 node_line; // Line offset of the current node
    u32 piece_pos; // Pos offset of the current piece (relative to the current node);
    u32 piece_line; // Line offset of the current piece (relative to the current node);
    u32 pos_in_piece; // Pos offset of the current line (relative to the current piece);
    u32 line_in_piece; // Line offset of the current line 
                       // (relative to the current piece);

    u16 abs_idx; // absolute index of the first piece of the current node;
    u16 piece_idx; // index of the current piece relative to the current node;
    iter_type type; // iteration type.
} base_iter;

typedef struct {
    base_iter start;
    base_iter end;
} iter_range;

typedef b32 (*search_pred)(u8 *buf, u32 len, str s);

static b32 base_init(piece_list *list, iter_type type, base_iter *iter);
static inline piece *get_piece(base_iter *iter);
static inline u32 get_position(piece_list *list, base_iter *iter);
static inline u32 position(base_iter *iter);
static inline u32 line_number(base_iter *iter);
static inline u32 get_line_number(piece_list *list, base_iter *iter);
static inline offset get_offset(piece_list *list, base_iter *iter);
static cell_item base_next_cell(piece_list *list, base_iter *iter);
static b32 base_advance_pos_by(piece_list *list, base_iter *iter, u32 count);
static b32 base_advance_by_line(piece_list *list, base_iter *iter, u32 count);
static b32 base_advance_rev_by_line(piece_list *list, base_iter *iter, u32 count);
static inline void reset_cursor(piece_list *list, base_iter *iter);
static inline void normalize(piece_list *list, base_iter *iter);
static inline b32 base_next_cell_(piece_list *list, base_iter *iter);
static inline b32 base_prev_cell(piece_list *list, base_iter *iter);
static inline b32 base_advance_by_cell(piece_list *list, base_iter *iter, u32 count);
static inline b32 base_next_pos(piece_list *list, base_iter *iter);
static b32 base_advance_pos_rev_by(base_iter *iter, u32 count);
static inline b32 base_next_line(piece_list *list, base_iter *iter);
static inline str get_char_utf8(piece_list *list, base_iter *iter);
static inline b32 base_next_pred(
    piece_list *list,
    base_iter *iter,
    search_pred pred, str needle);

