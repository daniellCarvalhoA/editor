#include <utf8proc.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <unicode/uchar.h>
#include <sys/types.h>
#include <stdlib.h>
#include <assert.h>


const uint8_t utf8_len_table[] = {
    // 1  2  3  4  5  6  7  8  9  A  B  C  D  E  F
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // 0
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // 1
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // 2
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // 3
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // 4
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // 5
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // 6
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // 7
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 8
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 9
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // A
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // B
    0, 0, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, // C
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, // D
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, // E
    4, 4, 4, 4, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // F
};

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <utf8proc.h>

/* Decode previous UTF-8 codepoint starting before byte index `pos`.
   Returns codepoint and sets *start to the byte index of that codepoint,
   or returns -1 on error. */
static int32_t prev_codepoint(
    const uint8_t *s,
    size_t len,
    size_t pos,
    size_t *start
) {
    if (pos == 0) return -1;
    ssize_t i = (ssize_t)pos - 1;
    while (i >= 0 && (s[i] & 0xC0) == 0x80) i--;
    // if (i < 0) return -1;
    int32_t cp;
    int rc = utf8proc_iterate(s + i, (int)(len - i), &cp);
    // if (rc <= 0) return -1;
    *start = (size_t)i;
    return cp;
}

/* Find the previous grapheme cluster boundary before byte index `index`.
   This uses utf8proc_grapheme_break_stateful by building a forward sequence
   from each candidate start to index and checking whether the first cluster
   boundary reported by the state machine is at the start of that sequence.
*/
size_t prev_grapheme_boundary_stateful(const uint8_t *s, size_t len, size_t index) {
    if (index == 0) return 0;

    /* We'll collect codepoints (in forward order) into a small buffer.
       To avoid reallocating for each candidate, store recent codepoints up to a reasonable limit.
       Most grapheme clusters are short; use a limit like 32 codepoints. */
    const size_t MAX_CPS = 64;
    int32_t cps[MAX_CPS];
    size_t cps_count = 0;

    /* Starting from index, walk leftwards decoding codepoints and prepend them into cps buffer.
       After each prepend, run the stateful grapheme breaker forward on the cps array to see where
       the first cluster boundary (from the left) lies. If that boundary is at position 0, we've found
       the previous grapheme start (the current candidate). */
    size_t scan_pos = index;
    while (scan_pos > 0 && cps_count < MAX_CPS) {
        size_t cp_start;
        int32_t cp = prev_codepoint(s, len, scan_pos, &cp_start);
        if (cp < 0) break;

        /* Prepend cp into cps array (we maintain cps in forward order from cp_start..index) */
        if (cps_count == 0) {
            cps[0] = cp;
            cps_count = 1;
        } else {
            /* shift right to make room at front */
            memmove(cps + 1, cps, cps_count * sizeof(int32_t));
            cps[0] = cp;
            cps_count++;
        }

        /* Build a UTF-32-like buffer that utf8proc_grapheme_break_stateful can accept.
           utf8proc_grapheme_break_stateful expects utf8proc_int32_t code points and a state object. */
        utf8proc_int32_t cp_buf[MAX_CPS];
        for (size_t i = 0; i < cps_count; ++i) cp_buf[i] = (utf8proc_int32_t)cps[i];

        /* Run the stateful grapheme breaker forward over cp_buf and find the first boundary.
           The stateful API returns 1 when a boundary is seen between previous and current codepoint.
           We'll iterate through cp_buf advancing the state until we hit a boundary between codepoints.
           If the first boundary encountered is after the first codepoint (i.e., the cluster starting at
           cp_buf[0] is exactly the suffix from cp_start to index), then the start is cp_start.
           Specifically: if there's no boundary between cp_buf[0] and cp_buf[1], then cluster continues.
           We need to find if there is a boundary at position >0; if the first boundary appears at offset k,
           it means cluster [0..k-1] is a grapheme; we want the previous grapheme start, which is cp_start
           if k == cps_count (i.e., no boundary inside the buffer) — but that's not what we want.
           Simpler approach: run the breaker from the left and stop when it reports a boundary; if that boundary
           occurs at index == cps_count (i.e., boundary at the end), then the entire buffer is a single cluster and
           we must continue extending left. If the breaker reports a boundary at some pos > 0 and that boundary
           separates cp_buf[b-1] and cp_buf[b], then the start of the last cluster (which ends at buffer end)
           is at b. We want to know whether the last cluster's start equals 0 (meaning our candidate start is a boundary).
        */

        /* Use the stateful API to find cluster boundaries across the buffer,
           and record the start of the final cluster that ends at the buffer end. */
        utf8proc_grapheme_break_state_t state;
        utf8proc_grapheme_break_stateful_init(&state);

        int last_boundary_index = 0; /* index in cp_buf where the current cluster starts (0-based) */
        for (size_t i = 0; i < cps_count; ++i) {
            int is_boundary = utf8proc_grapheme_break_stateful(&state, cp_buf[i]);
            if (is_boundary) {
                /* boundary between previous and cp_buf[i] — new cluster starts at i */
                last_boundary_index = (int)i;
            }
        }
        /* After processing the whole buffer, the last cluster in the buffer starts at last_boundary_index.
           If last_boundary_index == 0, then the last cluster starts at the buffer start, i.e., cp_start is a grapheme boundary.
           That means we've found the previous grapheme boundary at cp_start. */
        if (last_boundary_index == 0) {
            return cp_start;
        }

        /* Not a boundary yet — continue scanning left */
        scan_pos = cp_start;
    }

    /* If we exhausted the string or buffer limit, return 0 as the previous boundary. */
    return 0;
}

/* Example usage */
int main(void) {
    const char *text = "A🇺🇸🏳️‍🌈é한"; /* mixed graphemes */
    const uint8_t *s = (const uint8_t *)text;
    size_t len = strlen(text);
    size_t idx = len;

    while (idx > 0) {
        size_t prev = prev_grapheme_boundary_stateful(s, len, idx);
        printf("Grapheme from %zu to %zu: '", prev, idx);
        fwrite(s + prev, 1, idx - prev, stdout);
        printf("'\n");
        idx = prev;
    }
    return 0;
}





// The caller must ensure that *str is a valid utf8 string.
// Doesn not check for illegal byte sequences;
uint32_t utf8_charlen_unchecked(const uint8_t *const str, uint32_t size)
{
    uint8_t c = (uint8_t)(*str);

    assert(c != '\0');

    if (c < 0x80)  //&& str[1] < 0x80)
    {
        return 1; // ASCII
    }

    assert(size > 1);

    // u32 len = (u32) utf8_len_table[p[0]];


    uint32_t prev_len = 0;
    utf8proc_int32_t state = 0;

    utf8proc_int32_t prev_codepoint;
    utf8proc_ssize_t len = utf8proc_iterate(str, size, &prev_codepoint);
    while (len < size)
    {
        utf8proc_int32_t next_codepoint;
        utf8proc_iterate(str + len, size - len, &next_codepoint);

        if (str[len] < 0x80 || 
            utf8proc_grapheme_break_stateful(prev_codepoint, next_codepoint, &state))
        {
            return len;
        }

        len += utf8_len_table[str[len]];
        prev_codepoint = next_codepoint;
    }
}

int main()
{
    // char str[] = "joão";
    char str[] = "👩🏽‍⚕️";

    int len = strlen(str);
    fprintf(stderr, "%s, byte_len: %d\n", str, len);

    int c = 0;

    while (c < len)
    {
        uint32_t g_len = utf8_charlen_unchecked(str + c, len - c);
        printf("g_len = %d\n", g_len);
        c += g_len;
        // str += g_len;
    }

     
}


// typedef struct
// {
//     size_t len;
//     size_t capacity;
//     char *str;
//     size_t byte_index;
//     size_t code_point_index;
// } utf8_str;
//
// #define CONT_MASK 0b00111111
//
// static inline uint32_t utf8_first_byte(char byte, uint32_t width)
// {
//     uint32_t result = (byte & (0x7F >> width));
//     return result;
// }
//
// static inline uint32_t utf8_acc_cont_byte(uint32_t ch, char byte)
// {
//     uint32_t result = (ch << 6) | (byte & CONT_MASK);
//     return result;
// }
//
// static inline bool utf8_is_cont_byte(uint8_t byte)
// {
//     bool result = ((int8_t) byte) < - 64;
//     return result;
// }
//
// typedef struct 
// {
//     bool not_finished;
//     uint32_t value;
// } iter_result;
//
// static iter_result next_code_point(utf8_str *s)
// {
//     iter_result result = {};
//     if (s->byte_index < s->len)
//     {
//         uint8_t x = s->str[s->byte_index++];
//         result.not_finished = true;
//
//         if (x < 128)
//         {
//             result.value = (uint32_t) x;
//         }
//         else
//         {
//             uint32_t init = utf8_first_byte(x, 2);
//             // If ther first byte is not ascii than there must be at least 2 bytes 
//             // for this codepoint.
//             assert(s->byte_index < s->len);
//             uint32_t y = s->str[s->byte_index++];
//
//             uint32_t ch = utf8_acc_cont_byte(init, y);
//
//             if (x >= 0xE0)
//             {
//                 assert(s->byte_index < s->len);
//                 uint32_t z = s->str[s->byte_index++];
//
//                 uint32_t y_z = utf8_acc_cont_byte((uint32_t)(y & CONT_MASK) , z);
//                 ch = (init << 12) | y_z;
//                 if (x >= 0xF0)
//                 {
//                     assert(s->byte_index < s->len);
//                     uint32_t w = s->str[s->byte_index++];
//                     ch = (init & 7) << 18 | utf8_acc_cont_byte(y_z, w);
//                 }
//             }
//             result.value = ch;
//         }
//     }
//     return result;
//
// }
//
// // pub unsafe fn next_code_point<'a, I: Iterator<Item = &'a u8>>(bytes: &mut I) -> Option<u32> {
// //     // Decode UTF-8
// //     let x = *bytes.next()?;
// //     if x < 128 {
// //         return Some(x as u32);
// //     }
// //
// //     // Multibyte case follows
// //     // Decode from a byte combination out of: [[[x y] z] w]
// //     // NOTE: Performance is sensitive to the exact formulation here
// //     let init = utf8_first_byte(x, 2);
// //     // SAFETY: `bytes` produces an UTF-8-like string,
// //     // so the iterator must produce a value here.
// //     let y = unsafe { *bytes.next().unwrap_unchecked() };
// //     let mut ch = utf8_acc_cont_byte(init, y);
// //     if x >= 0xE0 {
// //         // [[x y z] w] case
// //         // 5th bit in 0xE0 .. 0xEF is always clear, so `init` is still valid
// //         // SAFETY: `bytes` produces an UTF-8-like string,
// //         // so the iterator must produce a value here.
// //         let z = unsafe { *bytes.next().unwrap_unchecked() };
// //         let y_z = utf8_acc_cont_byte((y & CONT_MASK) as u32, z);
// //         ch = init << 12 | y_z;
// //         if x >= 0xF0 {
// //             // [x y z w] case
// //             // use only the lower 3 bits of `init`
// //             // SAFETY: `bytes` produces an UTF-8-like string,
// //             // so the iterator must produce a value here.
// //             let w = unsafe { *bytes.next().unwrap_unchecked() };
// //             ch = (init & 7) << 18 | utf8_acc_cont_byte(y_z, w);
// //         }
// //     }
// //
// //     Some(ch)
// // }
//
//
//
// static utf8_str create_string(size_t capacity)
// {
//     utf8_str result = 
//     {
//         .len = 0,
//         .capacity = capacity,
//         .str = malloc(capacity),
//     };
//     return result;
// }
//
// static utf8_str from_null_terminated(char *str)
// {
//     size_t len = strlen(str);
//     utf8_str result = 
//     {
//         .len = len,
//         .capacity = len,
//         .str = str
//     };
//     return result;
// }
//
// static uint32_t codepoint_count(utf8_str *str)
// {
//     uint32_t result = 0;
//
//     for (;;)
//     {
//         iter_result iter = next_code_point(str);
//
//         if (iter.not_finished)
//         {
//             result++;
//         }
//         else
//         {
//             break;
//         }
//     }
//     return result;
// }
//
//
//
// int main()
// {
//
//     char s[] = "jo~ao";
//     printf("size of s: %lu bytes\n", sizeof(s));
//
//     for (char *str = s; *str; ++str)
//     {
//         printf("%x ", *str);
//     }
//     printf("\n");
//
//     uint32_t num_code_points = 0;
//
//     utf8proc_int32_t codepoint;
//
//     const utf8proc_uint8_t *str = s;
//
//
//     utf8proc_ssize_t max_read = sizeof(s) - 1;
//     
//
//     for (;;)
//     {
//
//         utf8proc_ssize_t ret = utf8proc_iterate(str, max_read, &codepoint);
//         if (ret < 0 || codepoint == -1)
//         {
//             break;
//         }
//         // if (codepoint == -1
//         printf("%x of size: %ld\n", codepoint, ret);
//         str += ret;
//         max_read -= ret;
//         num_code_points++;
//     }
//
//     str = s;
//
//     utf8proc_int32_t buf[16];
//     utf8proc_ssize_t written = utf8proc_decompose(str, 5, buf, 16, UTF8PROC_NULLTERM | UTF8PROC_COMPAT);
//
//
//     for (int i = 0; i < written; ++i)
//     {
//         printf("%x\n", buf[i]);
//     }
//
//     // while (ret =  > 0)
//     // {
//     //     codepoint++;
//     // }
//
//     printf("num_code_points = %u\n", num_code_points);
//
//     printf("size of decomposed = %lu, %s\n", written, utf8proc_errmsg(written));
//
//
//     utf8proc_uint8_t *nfc = utf8proc_NFC(str);
//     printf("%s\n", nfc);
//
//     max_read = strlen(nfc);
//     num_code_points = 0;
//
//     for (;;)
//     {
//         utf8proc_ssize_t ret = utf8proc_iterate(nfc, max_read, &codepoint);
//         if (ret < 0 || codepoint == -1)
//         {
//             break;
//         }
//
//
//         int width = utf8proc_charwidth(codepoint);
//         // if (codepoint == -1
//         printf("%x of size: %ld, width %d\n", codepoint, ret, width);
//         nfc += ret;
//         max_read -= ret;
//         num_code_points++;
//     }
//
//     printf("num_code_points = %u\n", num_code_points);
//
//     printf("#######################################\n");
//
//     utf8proc_uint8_t *nfd = utf8proc_NFD(str);
//     printf("%s\n", nfd);
//
//     max_read = strlen(nfd);
//     num_code_points = 0;
//
//     for (;;)
//     {
//         utf8proc_ssize_t ret = utf8proc_iterate(nfd, max_read, &codepoint);
//         if (ret < 0 || codepoint == -1)
//         {
//             break;
//         }
//         int width = utf8proc_charwidth(codepoint);
//         // if (codepoint == -1
//         printf("%x of size: %ld, width: %d\n", codepoint, ret, width);
//         nfd += ret;
//         max_read -= ret;
//         num_code_points++;
//     }
//     
//     printf("num_code_points = %u\n", num_code_points);
//
//     
//
//     // for (u32 
//
//
//     // char buf[4];
//     // setbuf(stdin, buf_in);
//     // setvbuf(stdout, 0, _IONBF, 0);
//     // size_t read_amount = read(0, buf, 4);
//
//     // utf8_str new = from_null_terminated("jo~ao");
//     //
//     // printf("%.*s\n", (int) new.len, new.str);
//     // printf("num_bytes = %d\n", (int) new.len);
//     // printf("num_code_points = %u\n", codepoint_count(&new));
//
//
//     // printf(
//
//
//     // printf("read %lu bytes, \"%s\"\n", read_amount, buf);
//     // for (int i = 0; i < read_amount; ++i)
//     // {
//     //     printf(" -- %02x - %d\n", buf[i], buf[i]);
//     // }
//
//
//         
// }
//
//
