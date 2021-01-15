/**
This is free and unencumbered software released into the public domain.

Anyone is free to copy, modify, publish, use, compile, sell, or
distribute this software, either in source code form or as a compiled
binary, for any purpose, commercial or non-commercial, and by any
means.

In jurisdictions that recognize copyright laws, the author or authors
of this software dedicate any and all copyright interest in the
software to the public domain. We make this dedication for the benefit
of the public at large and to the detriment of our heirs and
successors. We intend this dedication to be an overt act of
relinquishment in perpetuity of all present and future rights to this
software under copyright law.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

For more information, please refer to <http://unlicense.org/>
*/

#include "ge_frombytes_negate_vartime.h"

/* sqrt(x) is such an integer y that 0 <= y <= p - 1, y % 2 = 0, and y^2 = x (mod p). */
/* d = -121665 / 121666 */
static const fe fe_d =
    {-10913610, 13857413, -15372611, 6949391, 114729, -8787816, -6275908, -3247719, -18696448, -12055116}; /* d */

static const fe fe_sqrtm1 =
    {-32595792, -7943725, 9377950, 3500415, 12389472, -272473, -25146209, -2005654, 326686, 11406482}; /* sqrt(-1) */

int ge_frombytes_negate_vartime(ge_p3 *h, const unsigned char *s)
{
    fe u;
    fe v;
    fe vxx;
    fe check;

    fe_frombytes(h->Y, s);
    fe_1(h->Z);
    fe_sq(u, h->Y);
    fe_mul(v, u, fe_d);
    fe_sub(u, u, h->Z); /* u = y^2-1 */
    fe_add(v, v, h->Z); /* v = dy^2+1 */

    fe_divpowm1(h->X, u, v); /* x = uv^3(uv^7)^((q-5)/8) */

    fe_sq(vxx, h->X);
    fe_mul(vxx, vxx, v);
    fe_sub(check, vxx, u); /* vx^2-u */
    if (fe_isnonzero(check))
    {
        fe_add(check, vxx, u); /* vx^2+u */
        if (fe_isnonzero(check))
        {
            return -1;
        }
        fe_mul(h->X, h->X, fe_sqrtm1);
    }

    if (fe_isnegative(h->X) != (s[31] >> 7))
    {
        /* If x = 0, the sign must be positive */
        if (!fe_isnonzero(h->X))
        {
            return -1;
        }
        fe_neg(h->X, h->X);
    }

    fe_mul(h->T, h->X, h->Y);
    return 0;
}