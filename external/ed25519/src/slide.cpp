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

#include "slide.h"

void slide(signed char *r, const unsigned char *a)
{
    int i;
    int b;
    int k;

    for (i = 0; i < 256; ++i)
    {
        r[i] = 1 & (a[i >> 3] >> (i & 7));
    }

    for (i = 0; i < 256; ++i)
    {
        if (r[i])
        {
            for (b = 1; b <= 6 && i + b < 256; ++b)
            {
                if (r[i + b])
                {
                    if (r[i] + (r[i + b] << b) <= 15)
                    {
                        r[i] += r[i + b] << b;
                        r[i + b] = 0;
                    }
                    else if (r[i] - (r[i + b] << b) >= -15)
                    {
                        r[i] -= r[i + b] << b;
                        for (k = i + b; k < 256; ++k)
                        {
                            if (!r[k])
                            {
                                r[k] = 1;
                                break;
                            }
                            r[k] = 0;
                        }
                    }
                    else
                        break;
                }
            }
        }
    }
}