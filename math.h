
static inline u32 saturating_sub(u32 sub_from, u32 sub_amount)
{
    u32 result = (sub_amount > sub_from) ? 0 : (sub_from - sub_amount);
    return result;
}

static inline u32 clamped_add(u32 add_to, u32 add_amount, u32 max)
{
    u32 result = add_to + add_amount;
    if (result > max)
    {
        result = max;
    }
    return result;
}

