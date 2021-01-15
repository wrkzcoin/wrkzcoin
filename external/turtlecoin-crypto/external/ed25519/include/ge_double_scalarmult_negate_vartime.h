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

#ifndef ED25519_GE_DOUBLE_SCALARMULT_VARTIME_H
#define ED25519_GE_DOUBLE_SCALARMULT_VARTIME_H

#include "ge.h"
#include "ge_dsm_precomp.h"
#include "ge_p1p1_to_p2.h"
#include "ge_p2_0.h"
#include "ge_sub.h"
#include "slide.h"

void ge_double_scalarmult_negate_vartime(
    ge_p1p1 *t,
    const unsigned char *a,
    const ge_p3 *A,
    const unsigned char *b,
    const ge_dsmp Bi);

#endif // ED25519_GE_DOUBLE_SCALARMULT_VARTIME_H
