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

#include "fe_pow22523.h"

void fe_pow22523(fe out, const fe z)
{
    fe t0;
    fe t1;
    fe t2;
    int i;

    fe_sq(t0, z);
    for (i = 1; i < 1; ++i)
        fe_sq(t0, t0);

    fe_sq(t1, t0);
    for (i = 1; i < 2; ++i)
        fe_sq(t1, t1);

    fe_mul(t1, z, t1);

    fe_mul(t0, t0, t1);

    fe_sq(t0, t0);
    for (i = 1; i < 1; ++i)
        fe_sq(t0, t0);

    fe_mul(t0, t1, t0);

    fe_sq(t1, t0);
    for (i = 1; i < 5; ++i)
        fe_sq(t1, t1);

    fe_mul(t0, t1, t0);

    fe_sq(t1, t0);
    for (i = 1; i < 10; ++i)
        fe_sq(t1, t1);

    fe_mul(t1, t1, t0);

    fe_sq(t2, t1);
    for (i = 1; i < 20; ++i)
        fe_sq(t2, t2);

    fe_mul(t1, t2, t1);

    fe_sq(t1, t1);
    for (i = 1; i < 10; ++i)
        fe_sq(t1, t1);

    fe_mul(t0, t1, t0);

    fe_sq(t1, t0);
    for (i = 1; i < 50; ++i)
        fe_sq(t1, t1);

    fe_mul(t1, t1, t0);

    fe_sq(t2, t1);
    for (i = 1; i < 100; ++i)
        fe_sq(t2, t2);

    fe_mul(t1, t2, t1);

    fe_sq(t1, t1);
    for (i = 1; i < 50; ++i)
        fe_sq(t1, t1);

    fe_mul(t0, t1, t0);

    fe_sq(t0, t0);
    for (i = 1; i < 2; ++i)
        fe_sq(t0, t0);

    fe_mul(out, t0, z);

    return;
}