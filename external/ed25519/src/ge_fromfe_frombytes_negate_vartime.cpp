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

#include "ge_fromfe_frombytes_negate_vartime.h"

/* sqrt(x) is such an integer y that 0 <= y <= p - 1, y % 2 = 0, and y^2 = x (mod p). */
/* d = -121665 / 121666 */
static const fe fe_d =
    {-10913610, 13857413, -15372611, 6949391, 114729, -8787816, -6275908, -3247719, -18696448, -12055116}; /* d */

static const fe fe_sqrtm1 =
    {-32595792, -7943725, 9377950, 3500415, 12389472, -272473, -25146209, -2005654, 326686, 11406482}; /* sqrt(-1) */

static const fe fe_ma = {-486662, 0, 0, 0, 0, 0, 0, 0, 0, 0}; /* -A */

static const fe fe_ma2 = {-12721188, -3529, 0, 0, 0, 0, 0, 0, 0, 0}; /* -A^2 */

static const fe fe_fffb1 = {
    -31702527,
    -2466483,
    -26106795,
    -12203692,
    -12169197,
    -321052,
    14850977,
    -10296299,
    -16929438,
    -407568}; /* sqrt(-2 * A * (A + 2)) */

static const fe fe_fffb2 = {
    8166131,
    -6741800,
    -17040804,
    3154616,
    21461005,
    1466302,
    -30876704,
    -6368709,
    10503587,
    -13363080}; /* sqrt(2 * A * (A + 2)) */

static const fe fe_fffb3 = {
    -13620103,
    14639558,
    4532995,
    7679154,
    16815101,
    -15883539,
    -22863840,
    -14813421,
    13716513,
    -6477756}; /* sqrt(-sqrt(-1) * A * (A + 2)) */

static const fe fe_fffb4 = {
    -21786234,
    -12173074,
    21573800,
    4524538,
    -4645904,
    16204591,
    8012863,
    -8444712,
    3212926,
    6885324}; /* sqrt(sqrt(-1) * A * (A + 2)) */

void ge_fromfe_frombytes_negate_vartime(ge_p2 *r, const unsigned char *s)
{
    fe u, v, w, x, y, z;
    unsigned char sign;

    fe_frombytes(u, s);

    fe_sq2(v, u); /* 2 * u^2 */
    fe_1(w);
    fe_add(w, v, w); /* w = 2 * u^2 + 1 */

    fe_sq(x, w); /* w^2 */
    fe_mul(y, fe_ma2, v); /* -2 * A^2 * u^2 */
    fe_add(x, x, y); /* x = w^2 - 2 * A^2 * u^2 */

    fe_divpowm1(r->X, w, x); /* (w / x)^(m + 1) */

    fe_sq(y, r->X);
    fe_mul(x, y, x);
    fe_sub(y, w, x);
    fe_copy(z, fe_ma);
    if (fe_isnonzero(y))
    {
        fe_add(y, w, x);
        if (fe_isnonzero(y))
        {
            goto negative;
        }
        else
        {
            fe_mul(r->X, r->X, fe_fffb1);
        }
    }
    else
    {
        fe_mul(r->X, r->X, fe_fffb2);
    }
    fe_mul(r->X, r->X, u); /* u * sqrt(2 * A * (A + 2) * w / x) */
    fe_mul(z, z, v); /* -2 * A * u^2 */
    sign = 0;
    goto setsign;
negative:
    fe_mul(x, x, fe_sqrtm1);
    fe_sub(y, w, x);
    if (fe_isnonzero(y))
    {
        assert((fe_add(y, w, x), !fe_isnonzero(y)));
        fe_mul(r->X, r->X, fe_fffb3);
    }
    else
    {
        fe_mul(r->X, r->X, fe_fffb4);
    }
    /* r->X = sqrt(A * (A + 2) * w / x) */
    /* z = -A */
    sign = 1;
setsign:
    if (fe_isnegative(r->X) != sign)
    {
        assert(fe_isnonzero(r->X));
        fe_neg(r->X, r->X);
    }
    fe_add(r->Z, z, w);
    fe_sub(r->Y, z, w);
    fe_mul(r->X, r->X, r->Z);

    fe check_x, check_y, check_iz, check_v;
    fe_invert(check_iz, r->Z);
    fe_mul(check_x, r->X, check_iz);
    fe_mul(check_y, r->Y, check_iz);
    fe_sq(check_x, check_x);
    fe_sq(check_y, check_y);
    fe_mul(check_v, check_x, check_y);
    fe_mul(check_v, fe_d, check_v);
    fe_add(check_v, check_v, check_x);
    fe_sub(check_v, check_v, check_y);
    fe_1(check_x);
    fe_add(check_v, check_v, check_x);
    assert(!fe_isnonzero(check_v));
}