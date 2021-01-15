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

// adapted from ge25519.c

#include "ge_double_scalarmult_negate_vartime.h"

/*
r = a * A + b * B
where a = a[0]+256*a[1]+...+256^31 a[31].
and b = b[0]+256*b[1]+...+256^31 b[31].
*/
void ge_double_scalarmult_negate_vartime(
    ge_p1p1 *t,
    const unsigned char *a,
    const ge_p3 *A,
    const unsigned char *b,
    const ge_dsmp Bi)
{
    signed char aslide[256];
    signed char bslide[256];
    ge_dsmp Ai; /* A, 3A, 5A, 7A, 9A, 11A, 13A, 15A */
    ge_p3 u;
    int i;

    slide(aslide, a);
    slide(bslide, b);

    ge_dsm_precomp(Ai, A);

    ge_p2 r;

    ge_p2_0(&r);

    for (i = 255; i >= 0; --i)
    {
        if (aslide[i] || bslide[i])
            break;
    }

    for (; i >= 0; --i)
    {
        ge_p2_dbl(t, &r);

        if (aslide[i] > 0)
        {
            ge_p1p1_to_p3(&u, t);
            ge_add(t, &u, &Ai[aslide[i] / 2]);
        }
        else if (aslide[i] < 0)
        {
            ge_p1p1_to_p3(&u, t);
            ge_sub(t, &u, &Ai[(-aslide[i]) / 2]);
        }

        if (bslide[i] > 0)
        {
            ge_p1p1_to_p3(&u, t);
            ge_add(t, &u, &Bi[bslide[i] / 2]);
        }
        else if (bslide[i] < 0)
        {
            ge_p1p1_to_p3(&u, t);
            ge_sub(t, &u, &Bi[(-bslide[i]) / 2]);
        }

        ge_p1p1_to_p2(&r, t);
    }
}
