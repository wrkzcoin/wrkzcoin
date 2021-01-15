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

#ifndef ED25519_GE_SCALARMULT_H
#define ED25519_GE_SCALARMULT_H

#if defined __SIZEOF_INT128__ && defined __USE_64BIT__
#include "donna128_scalarmult.h"
#endif

#include "equal.h"
#include "fe_copy.h"
#include "fe_neg.h"
#include "ge.h"
#include "ge_add.h"
#include "ge_cached_0.h"
#include "ge_cached_cmov.h"
#include "ge_frombytes_negate_vartime.h"
#include "ge_p1p1_to_p2.h"
#include "ge_p1p1_to_p3.h"
#include "ge_p2_0.h"
#include "ge_p2_dbl.h"
#include "ge_p3_to_cached.h"
#include "ge_p3_tobytes.h"
#include "negative.h"

void ref10_scalarmult(ge_p1p1 *r, const unsigned char *a, const ge_p3 *A);

#if defined __SIZEOF_INT128__ && defined __USE_64BIT__
void donna128_scalarmult_wrapper(ge_p1p1 *r, const unsigned char *a, const ge_p3 *A);
#define ge_scalarmult(out, scalar, point) donna128_scalarmult_wrapper(out, scalar, point)
#else
#define ge_scalarmult(out, scalar, point) ref10_scalarmult(out, scalar, point)
#endif

#endif // ED25519_GE_SCALARMULT_H
