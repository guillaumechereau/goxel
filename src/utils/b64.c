#include "b64.h"

#include <stdint.h>
#include <string.h>

/* Function: b64_decode
 * Decode a base64 string
 *
 * Parameters:
 *   src  - A base 64 encoded string.
 *   dest - Buffer that will receive the decoded value or NULL.  If set to
 *          NULL the function just returns the size of the decoded data.
 *
 * Return:
 *   The size of the decoded data.
 */
int b64_decode(const char *src, void *dest)
{
    const char TABLE[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    int len = strlen(src);
    uint8_t *p = dest;
    #define b64_value(c) ((strchr(TABLE, c) ?: TABLE) - TABLE)
    #define isbase64(c) (c && strchr(TABLE, c))

    if (*src == 0) return 0;
    if (!p) return ((len + 3) / 4) * 3;
    do {
        char a = b64_value(src[0]);
        char b = b64_value(src[1]);
        char c = b64_value(src[2]);
        char d = b64_value(src[3]);
        *p++ = (a << 2) | (b >> 4);
        *p++ = (b << 4) | (c >> 2);
        *p++ = (c << 6) | d;
        if (!isbase64(src[1])) {
            p -= 2;
            break;
        }
        else if (!isbase64(src[2])) {
            p -= 2;
            break;
        }
        else if (!isbase64(src[3])) {
            p--;
            break;
        }
        src += 4;
        while (*src && (*src == 13 || *src == 10)) src++;
    } while (len -= 4);
    return p - (uint8_t*)dest;
}
