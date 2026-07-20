
typedef struct model
{
    memory_arena arena;
    u32 pos;
    u32 cx;
    u32 cy;
    u32 line_len;
    string s;
} model;

static void initialize_model(model *model, const string original)
{
    model->cx = model->cy = model->pos = model->line_len = 0;
    model->s.capacity = 8 * 4096;
    model->s.buffer = PushArray(&model->arena, model->s.capacity, u8, NoClear());
    memcpy(model->s.buffer, original.buffer, original.len);
    model->s.len = original.len;
}

static inline u32 num_lcnts(const model *model)
{
    u32 result = 0;
    for (u32 i = 0; i < model->s.len; ++i)
    {
        result += model->s.buffer[i] == '\n';
    }
    return result;
}

static inline b32 next_row(const model *model, u8 **row_pos, u32 *row_size)
{
    if (*row_pos >= model->s.buffer + model->s.len)
    {
        return false;
    }

    u8 *row_start = *row_pos;

    u32 cursor = 0;
    while ((row_start + cursor < model->s.buffer + model->s.len) &&
           (cursor < *row_size))
    {
        if (row_start[cursor] == '\n')
        {
            *row_size = cursor;
            *row_pos += cursor + 1;
            return true;
        }
        cursor++;
    }

    if (row_start[cursor] == '\n')
    {
        (*row_pos)++;
    }

    *row_size = cursor;
    *row_pos += cursor;

    return true;

}

static inline u32 line_position(const model *model, const u32 line)
{
    u32 result = 0;
    if (line > 0)
    {
        for (u32 i = 0, j = 0; i < model->s.len; ++i)
        {
            if (model->s.buffer[i] == '\n')
            {
                j++;
                if (j == line)
                {
                    result = i + 1;
                    break;
                }
            }
        }
    }
    return result;
}

static inline u32 line_len(const model *model, u32 line)
{
    u32 line_start = line_position(model, line);
    u32 line_end;
    if (line < num_lcnts(model))
    {
        line_end = line_position(model, line + 1) - 1;
    }
    else
    {
        line_end = model->s.len;
    }
    return line_end - line_start;
}


static inline u32 num_lines(const model *model)
{
    u32 result = num_lcnts(model);

    u32 size = model->s.len;
    u32 last_line_position = line_position(model, result);

    if (size - last_line_position > 0)
    {
        result++;
    }
    return result;
}

static inline u32 line_positions(const model *model, u32 start, u32 *buf, u32 len)
{
    u32 result = 0;
    if (len)
    {
        if (start == 0)
        {
            *buf++ = 0;
            result ++;

        }
        for (u32 i = 0, j = 0; i < model->s.len; ++i)
        {
            if (model->s.buffer[i] == '\n')
            {
                j++;
                if (j >= start)
                {
                    if (j >= start + len)
                    {
                        break;
                    }
                    *buf++ = i + 1;
                    result++;
                }
            }
        }
    }
    return result;
}

static inline void model_move_to_pos(model *model, u32 position)
{
    u32 num_line_positions = num_lcnts(model) + (model->s.len > 0);
    u32 *l_pos = malloc(num_line_positions * sizeof(u32));

    u32 num_len = line_positions(model, 0, l_pos, num_line_positions);

    if (num_len > 0)
    {
        u32 last_pos = 0;
        u32 i = 0;
        while (i < num_len && position >= l_pos[i])
        {
            last_pos = l_pos[i++];
        }

        model->pos = position;
        model->cy  = i - 1;
        model->cx  = position - last_pos;
        if (i < num_len)
        {
            model->line_len = l_pos[i] - last_pos - 1;
        }
        else
        {
            model->line_len = model->s.len - last_pos;
        }
    }

    free(l_pos);
}

static inline void model_move_to_line(model *model, u32 line)
{
    u32 num_line_positions = num_lcnts(model) + (model->s.len > 0);
    u32 *l_pos = malloc(num_line_positions * sizeof(u32));
    u32 num_len = line_positions(model, 0, l_pos, num_line_positions);
    if (num_len > 0)
    {
        model->cy       = line;
        model->cx       = 0;
        model->pos      = l_pos[line];
        if (num_lcnts(model) == line)
        {
            model->line_len = model->s.len - l_pos[line];
        }
        else
        {
            model->line_len = l_pos[line + 1] - (l_pos[line] + 1);
        }
    }

    free(l_pos);
}

static inline void model_move_to_cursor(model *model, u32 cy, u32 cx)
{
    model_move_to_line(model, cy);
    if (model->line_len > 0)
    {
        model_move_to_pos(
            model,
            Minimum(model->pos + cx, model->pos + model->line_len - 1));
    }
}

static inline void model_move_by_motion(model *model, motion motion, u32 quant)
{
    switch (motion)
    {
        case Motion_Vertical:
        {
            u32 cy = (quant > model->cy) ? 0 : (model->cy - quant);
            // if (
            model_move_to_cursor(model, cy, model->cx);
        } break;

        //case Down:
        //{
            //u32 count_lines = num_lines(model);
            //u32 max_lines = (count_lines) ? (count_lines - 1) : 0;
            //u32 cy = Minimum(quant + model->cy, max_lines);
            //model_move_to_cursor(model, cy, model->cx);
        //} break;

        case Motion_Horizontal:
        {
            u32 cx = Minimum(
                model->cx + quant,
                (model->line_len) ? (model->line_len - 1) : 0);
            model_move_to_pos(model, model->pos - model->cx + cx);
        } break;

        //case Left:
        //{
            //u32 x = (quant > model->cx) ? model->cx : quant;
            //model_move_to_pos(model, model->pos - x);
        //} break;

        default:
        {
        } break;
    }
}


static u32 curr_line_len(model *model)
{
    u32 result = line_position(model, model->cy) - line_position(model, model->cy + 1);
    return result;
}

// Length of line does not include LF
static inline void line_lens(const model *model, u32 *buf, const u32 start, const u32 end)
{
    u32 prev_line_position = 0;
    for (u32 i = 0, j = 0; i < model->s.len; ++i)
    {
        if (model->s.buffer[i] == '\n')
        {
            j++;
            if (j > start)
            {
                if (j > end)
                {
                    return;
                }
                *buf++ = i - prev_line_position;
            }
            prev_line_position = i + 1;
        }
    }
    *buf = model->s.len - prev_line_position;
}

// static inline lines_result model_num_lines_from(
    // const model *model,
    // u32 start,
    // u32 row_size,
    // u32 num_rows)
// {
    // u32 end = num_lcnts(model) + 1;
    // u32 len = end - start;
    // u32 *lens = malloc(sizeof(u32) * len);
    // line_lens(model, lens, start, end);
// 
    // lines_result result = {};
    // for (u32 i = 0; i < len; ++i)
    // {
        // u32 rows_size = num_rows * row_size;
        // u32 length = lens[i];
        // if (length > num_rows * row_size)
        // {
            // result.rows_in = rows_size;
            // result.rows_out  = length - result.rows_in;
            // break;
        // }
        // result.num_lines++;
        // num_rows -= 1 + length / row_size;
    // }
// 
    // free(lens);
    // return result;
// }

static void model_replace(model *model, u32 start, u32 end, string s)
{
    u32 delete_count = end - start;
    if (delete_count >= s.len)
    {
        memmove(
            model->s.buffer + start + s.len,
            model->s.buffer + start + delete_count,
            model->s.len - end);

        memcpy(model->s.buffer  + start, s.buffer, s.len);
        model->s.len -= delete_count - s.len;
    }
    else if (model->s.len + s.len - delete_count < model->s.capacity)
    {
        memmove(
            model->s.buffer + end + s.len - delete_count,
            model->s.buffer + end,
            model->s.len - end);
        memcpy(model->s.buffer + start, s.buffer, s.len);
        model->s.len += s.len - delete_count;
    }
    else
    {
        u32 size = model->s.len + s.len - delete_count;
        do 
        {
            model->s.capacity *= 2;
        } while (size > model->s.capacity);

        u8 *new_buffer = PushArray(&model->arena, model->s.capacity, u8, NoClear());
        memcpy(new_buffer, model->s.buffer, start);
        memcpy(new_buffer + start, s.buffer, s.len);
        memcpy(new_buffer + start + s.len, model->s.buffer + end, model->s.len - end);

        model->s.buffer = new_buffer;
        model->s.len = size;
    }
}

static void model_replace_(model *model, test_cursor a, test_cursor b, string s)
{
    u32 start = line_position(model, a.y) + a.x;
    u32 end   = line_position(model, b.y) + b.x;
    model_replace(model, start, end, s);
}



static void model_rand_replace_2(model *model, prng *prng)
{
    if (model->s.len + num_insert_pieces == 0)
    {
        return;
    }

    test_cursor a;
    test_cursor b;

    b32 found = false;

    u32 num_lines = num_lcnts(model);
    u32 num_ins_pieces;
    for (; !found ;)
    {
        num_ins_pieces = rand_range_u32_inclusive(prng, 0, num_insert_pieces);

        a = rand_cursor(prng, MAX_LINE_LEN, num_lines);
        b = rand_cursor(prng, MAX_LINE_LEN, num_lines);

        switch (compare(a, b))
        {
            case LessThan:
            {
                found = true;
            } break;

            case GreaterThan:
            {
                test_cursor tmp = a;
                a = b;
                b = tmp;
                found = true;
            } break;

            case EqualTo:
            {
                if (num_ins_pieces > 0)
                {
                    found = true;
                }
            } break;
        }
    }

    u32 line_len_a = line_len(model, a.y);
    u32 line_len_b = line_len(model, b.y);
    a.x = Minimum(a.x, line_len_a);
    b.x = Minimum(b.x, line_len_b);

    INIT_STACK_STRING(s, MAX_STRING_LEN * num_ins_pieces);

    for (u32 i = 0; i < num_ins_pieces; ++i)
    {
        string cursor = { 
            .len = 0,
            .capacity = s.capacity - s.len,
            .buffer = buffer + s.len 
        };
        rand_ascii_string(&cursor, prng, 1, MAX_STRING_LEN);
        s.len += cursor.len;
    }

    model_replace_(model, a, b, s);

}


static void model_rand_replace(model *model, prng *prng)
{
    u32 begin, finish, num_ins_pieces;

    if (model->s.len + num_insert_pieces == 0)
    {
        return;
    }


    ratio r = init_ratio(2, 4);

    for(;;)
    {
        if (chance(prng, r))
        {
            begin   = rand_range_u32_inclusive(prng, 0, model->s.len);
            finish  = rand_range_u32_inclusive(prng, begin, model->s.len);
        }
        else
        {
            u32 num_lcnt = num_lcnts(model);
            begin   = rand_range_u32_inclusive(prng, 0, num_lcnt);
            finish  = rand_range_u32_inclusive(prng, begin, num_lcnt);

            begin  = line_position(model, begin);
            finish = line_position(model, finish);
        }

        num_ins_pieces = rand_range_u32_inclusive(prng, 0, num_insert_pieces);

        if ((num_ins_pieces > 0) || (begin != finish))
        {
            break;
        }
    }

    INIT_STACK_STRING(s, MAX_STRING_LEN * num_ins_pieces);

    for (u32 i = 0; i < num_ins_pieces; ++i)
    {
        string cursor = { 
            .len = 0,
            .capacity = s.capacity - s.len,
            .buffer = buffer + s.len 
        };
        rand_ascii_string(&cursor, prng, 1, MAX_STRING_LEN);
        s.len += cursor.len;
    }

    model_replace(model, begin, finish, s);
}

static model *rand_model(prng *prng)
{
    model *result = BootstrapPushStruct(model, arena, 8 * 4096);
    INIT_STACK_STRING(original_text, MAX_ORIGINAL_STRING_LEN);
    rand_ascii_string(&original_text, prng, 0, MAX_ORIGINAL_STRING_LEN);

    initialize_model(result, original_text);

    for (u32 i = 0; i < num_edits; ++i)
    {
        model_rand_replace(result, prng);
    }

    if (num_lcnts(result) == 0)
    {
        result->line_len = result->s.len;
    }
    else
    {
        result->line_len = line_position(result, 1) - 1;
    }
    return result;
}


static model *rand_model_2(prng *prng)
{
    model *result = BootstrapPushStruct(model, arena, 8 * 4096);
    INIT_STACK_STRING(original_text, MAX_ORIGINAL_STRING_LEN);
    rand_ascii_string(&original_text, prng, 0, MAX_ORIGINAL_STRING_LEN);

    initialize_model(result, original_text);

    for (u32 i = 0; i < num_edits; ++i)
    {
        model_rand_replace_2(result, prng);
    }

    if (num_lcnts(result) == 0)
    {
        result->line_len = result->s.len;
    }
    else
    {
        result->line_len = line_position(result, 1) - 1;
    }
    return result;
}

