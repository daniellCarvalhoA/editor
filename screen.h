#include <sys/ioctl.h>


typedef struct screen
{
    memory_arena render_arena;

    u16 rows;
    u16 cols;

    u32 text_count;
    u32 line_count;

    u16 left;
    u16 right;
    u16 top;
    u16 bot;

    multilevel_grid grid; 
    u32 cursor;
    u8 buffer[8192];

} screen;


// All writes must be full. No string representing an ansi sequence can 
// span multiple flush boundaries.

static void flush_buffer(screen *screen)
{
    ssize_t ret = write(1, screen->buffer, screen->cursor);
    if (ret < 0)
    {
        fprintf(stderr, "Error writint to terminal\n");
        exit(0);
    }
    if (ret < screen->cursor)
    {
        fprintf(stderr, "Wrote less than expected\n");
        exit(0);
    }
    screen->cursor = 0;
}

static void write_string(screen *screen, u8 *s, u32 len) 
{
    if (screen->cursor + len > ArrayCount(screen->buffer))
    {
        flush_buffer(screen);
    }
    memcpy(screen->buffer + screen->cursor, s, len);
    screen->cursor += len;

}

static void reset_color(screen *screen)
{
    char reset_string[] = "\x1b[0m";
    write_string(screen, (u8 *) reset_string, sizeof(reset_string) - 1);
}

static void set_color(screen *screen, u32 color)
{
    Assert(color < 10000);
    size_t max_size = (size_t) (ArrayCount(screen->buffer) - screen->cursor);

    char *buffer = (char *) (screen->buffer + screen->cursor);
    int len = snprintf(buffer, max_size, "\x1b[1;%um", color);
    Assert(len > 0);
    if ((size_t) len >= max_size)
    {
        // fprintf(stderr, "set color\n");
        // exit(3);
        flush_buffer(screen);
        set_color(screen, color);
        return;
    }

    screen->cursor += (u32) len;
}

static void place_cursor(screen *screen, u32 y, u32 x)
{
    Assert(x < 10000);
    Assert(y < 10000);
    size_t max_size = (size_t) (ArrayCount(screen->buffer) - screen->cursor);

    char *buffer = (char *) (screen->buffer + screen->cursor);
    int len = snprintf(buffer, max_size, "\x1b[%u;%uH", 1 + y, 1 + x);
    Assert(len > 0);
    if ((size_t) len >= max_size)
    {
        // fprintf(stderr, "place_cursor\n");
        // exit(4);
        flush_buffer(screen);
        place_cursor(screen, y, x);
        return;
    }

    screen->cursor += (u32) len;
}

static void move_cursor_right(screen *screen, u32 x)
{
    Assert(x < 10000);
    size_t max_size = (size_t) (ArrayCount(screen->buffer) - screen->cursor);

    char *buffer = (char *) (screen->buffer + screen->cursor);
    int len = snprintf(buffer, max_size, "\x1b[%uC", x);
    Assert(len > 0);
    if ((size_t) len >= max_size)
    {
        // fprintf(stderr, "move_cursor\n");
        // exit(5);
        flush_buffer(screen);
        move_cursor_right(screen, x);
        return;
    }
    Assert((u32) len < ArrayCount(screen->buffer));

    screen->cursor += (u32) len;
}

static void set_cursor_column(screen *screen, u32 x)
{
    Assert(x < 10000);
    size_t max_size = (size_t) (ArrayCount(screen->buffer) - screen->cursor);

    char *buffer = (char *) (screen->buffer + screen->cursor);
    int len = snprintf(buffer, max_size, "\x1b[%uG", 1 + x);
    Assert(len > 0);
    if ((size_t) len >= max_size)
    {
        // fprintf(stderr, "set_cursor_column\n");
        // exit(6);
        flush_buffer(screen);
        set_cursor_column(screen, x);
        return;
    }
    Assert((u32) len < ArrayCount(screen->buffer));

    screen->cursor += (u32) len;
}


static void append_to_buffer(screen *screen, u8 *s, u32 len)
{
    if (screen->cursor + len > ArrayCount(screen->buffer))
    {
        flush_buffer(screen);
    }
    memcpy(screen->buffer + screen->cursor, s, len);
    screen->cursor += len;
}



static void rotate(u32 left, u16 *mid, u32 right)
{
    if ((left == 0) || (right == 0))
    {
        return;
    }
    
    if (Minimum(left, right) <= ((sizeof(u64) * 32) / sizeof(u16)))
    {
        u16 buf[sizeof(u16) * 32];

        u16 *dim = mid - left + right;

        if (left <= right)
        {
            memcpy(buf, mid - left, sizeof(u16) * left);
            memmove(mid - left, mid, sizeof(u16) * right);
            memcpy(dim, buf, sizeof(u16) * left);
        }
        else
        {
            memcpy(buf, mid, sizeof(u16) * right);
            memmove(dim, mid - left, sizeof(u16) * left);
            memcpy(mid - left, buf, sizeof(u16) * right);
        }
    }
    else
    {
        Assert(false);
    }
}

// #[inline]
// const unsafe fn ptr_rotate_memmove<T>(left: usize, mid: *mut T, right: usize) {
//     // The `[T; 0]` here is to ensure this is appropriately aligned for T
//     let mut rawarray = MaybeUninit::<(BufType, [T; 0])>::uninit();
//     let buf = rawarray.as_mut_ptr() as *mut T;
//     // SAFETY: `mid-left <= mid-left+right < mid+right`
//     let dim = unsafe { mid.sub(left).add(right) };
//     if left <= right {
//         // SAFETY:
//         //
//         // 1) The `if` condition about the sizes ensures `[mid-left; left]` will fit in
//         //    `buf` without overflow and `buf` was created just above and so cannot be
//         //    overlapped with any value of `[mid-left; left]`
//         // 2) [mid-left, mid+right) are all valid for reading and writing and we don't care
//         //    about overlaps here.
//         // 3) The `if` condition about `left <= right` ensures writing `left` elements to
//         //    `dim = mid-left+right` is valid because:
//         //    - `buf` is valid and `left` elements were written in it in 1)
//         //    - `dim+left = mid-left+right+left = mid+right` and we write `[dim, dim+left)`
//         unsafe {
//             // 1)
//             ptr::copy_nonoverlapping(mid.sub(left), buf, left);
//             // 2)
//             ptr::copy(mid, mid.sub(left), right);
//             // 3)
//             ptr::copy_nonoverlapping(buf, dim, left);
//         }
//     } else {
//         // SAFETY: same reasoning as above but with `left` and `right` reversed
//         unsafe {
//             ptr::copy_nonoverlapping(mid, buf, right);
//             ptr::copy(mid.sub(left), dim, left);
//             ptr::copy_nonoverlapping(buf, mid.sub(left), right);
//         }
//     }
// }
//
//
//
// pub(super) const unsafe fn ptr_rotate<T>(left: usize, mid: *mut T, right: usize) {
//     if T::IS_ZST {
//         return;
//     }
//     // abort early if the rotate is a no-op
//     if (left == 0) || (right == 0) {
//         return;
//     }
//     // `T` is not a zero-sized type, so it's okay to divide by its size.
//     if !cfg!(feature = "optimize_for_size")
//         // FIXME(const-hack): Use cmp::min when available in const
//         && const_min(left, right) <= size_of::<BufType>() / size_of::<T>()
//     {
//         // SAFETY: guaranteed by the caller
//         unsafe { ptr_rotate_memmove(left, mid, right) };
//     } else if !cfg!(feature = "optimize_for_size")
//         && ((left + right < 24) || (size_of::<T>() > size_of::<[usize; 4]>()))
//     {
//         // SAFETY: guaranteed by the caller
//         unsafe { ptr_rotate_gcd(left, mid, right) }
//     } else {
//         // SAFETY: guaranteed by the caller
//         unsafe { ptr_rotate_swap(left, mid, right) }
//     }
// }


static void rotate_left_u16(u16 *ptr, u32 size, u32 pivot)
{
    Assert(pivot <= size);
    u32 k = size - pivot;

    rotate(pivot, ptr + pivot, k);
}

static void rotate_right_u16(u16 *ptr, u32 size, u32 pivot)
{
    Assert(pivot <= size);
    u32 mid = size - pivot;
    rotate(mid, ptr + mid, pivot);
}
    // pub const fn rotate_right(&mut self, k: usize) {
    //     assert!(k <= self.len());
    //     let mid = self.len() - k;
    //     let p = self.as_mut_ptr();
    //
    //     // SAFETY: The range `[p.add(mid) - mid, p.add(mid) + k)` is trivially
    //     // valid for reading and writing, as required by `ptr_rotate`.
    //     unsafe {
    //         rotate::ptr_rotate(mid, p.add(mid), k);
    //     }
    // }



 // pub const fn rotate_left(&mut self, mid: usize) {
 //        assert!(mid <= self.len());
 //        let k = self.len() - mid;
 //        let p = self.as_mut_ptr();
 //
 //        // SAFETY: The range `[p.add(mid) - mid, p.add(mid) + k)` is trivially
 //        // valid for reading and writing, as required by `ptr_rotate`.
 //        unsafe {
 //            rotate::ptr_rotate(mid, p.add(mid), k);
 //        }
 //    }

// static void scroll_grid_down(screen *screen, u32 count)
// {
//     rotate_left_u16(grid_lines->lines, screen->rows, count);
//     // memmove(screen->lines, screen->lines + count, sizeof(u32) * (screen->rows - count));
//
//     for (u32 i = 0; i < count; ++i)
//     {
//         u32 line_start = screen->rows - (i + 1);
//         u32 start      = screen->lines[line_start];
//         memset(screen->text + start, ' ', screen->cols);
//     }
// }
//
// static void scroll_grid_up(screen *screen, u32 count)
// {
//     rotate_right_u16(screen->lines, screen->rows, count);
//
//     for (u32 i = 0; i < count; ++i)
//     {
//         u32 start = screen->lines[i];
//         memset(screen->text + start, ' ', screen->cols);
//     }
// }





static inline void reset_window_size(screen *screen)
{
    struct window_size {
        u16 ws_row;
        u16 ws_col;
        u16 xpixel;
        u16 ypixel;
    } win;

    i32 n = ioctl(1, TIOCGWINSZ, &win);
    if (n == - 1)
    {
        perror("ioctl");
        abort();
    }

    screen->rows = win.ws_row;
    screen->cols = win.ws_col;
}

static inline void reset_screen_diff_bounds(screen *screen)
{
    screen->left = UINT16_MAX;
    screen->top = UINT16_MAX;
    screen->right = 0; 
    screen->bot = 0;
}

static inline void initialize_screen(screen *screen)
{
    initialize_arena_with_size(&screen->render_arena, 8 * 4096);
    reset_window_size(screen);
    screen->text_count = screen->rows * screen->cols;
    screen->line_count = screen->rows;

    initialize_multilevel_grid(&screen->grid, screen->rows, screen->cols);
    reset_screen_diff_bounds(screen);

    // screen->lines = PushArray(&screen->render_arena, screen->line_count, u16, NoClear());
    // for (u32 i = 0; i < screen->rows; ++i)
    // {
    //     screen->lines[i] = screen->cols * i;
    // }
    // screen->text  = PushArray(&screen->render_arena, screen->text_count, u8, NoClear());
    // screen->diff_text = PushArray(&screen->render_arena, screen->text_count, u8, NoClear());
    // memset(screen->text, ' ', screen->text_count);
    // memset(screen->diff_text, ' ', screen->text_count);
    // INIT_LIST_HEAD(&screen->render_head);
}


