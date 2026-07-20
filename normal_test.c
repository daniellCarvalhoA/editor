
static inline motion rand_motion(prng *prng)
{
    motion result = (motion) rand_range_u32_inclusive(prng, Motion_NoMotion, MotionCount);
    return result;
}

static inline action rand_action(prng *prng)
{
    action result = (action) rand_range_u32_inclusive(prng, NoAction, ActionCount);
    return result;
}

static inline position_modifier rand_position_modifier(prng *prng)
{
    position_modifier result = (position_modifier) rand_range_u32_inclusive(prng, Current, PositionModifierCount);
    return result;
}

static inline mode_modifier rand_mode_modifier(prng *prng)
{
    mode_modifier result = (mode_modifier) rand_range_u32_inclusive(prng, NoChange, ModeModifierCount);
    return result;
}
