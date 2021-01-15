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

#include "sha3.h"

#include <chrono>
#include <iomanip>
#include <iostream>

#define PERFORMANCE_ITERATIONS 1000000

template<typename T>
void benchmark(T &&function, const std::string &functionName, uint64_t iterations = PERFORMANCE_ITERATIONS)
{
    std::cout << std::setw(40) << functionName << ": ";

    auto tenth = iterations / 10;

    auto startTimer = std::chrono::high_resolution_clock::now();

    for (uint64_t i = 0; i < iterations; ++i)
    {
        if (i % tenth == 0)
            std::cout << ".";

        function();
    }

    auto elapsedTime =
        std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - startTimer)
            .count();

    auto timePer = elapsedTime / iterations;

    std::cout << "  " << std::setprecision(5) << std::setw(10) << timePer / 1000.0 << " ms" << std::endl;
}

int main()
{
    std::cout << "Benchmark Timings" << std::endl << std::endl;

    const uint8_t seed[32] = {0x31, 0x3b, 0x08, 0x3f, 0x84, 0x28, 0x2b, 0x00, 0xb9, 0xc8, 0x4f,
                              0x4c, 0xf4, 0x39, 0x24, 0xf6, 0x61, 0x27, 0xf5, 0xd2, 0x77, 0x2f,
                              0xdf, 0x36, 0x11, 0x09, 0x56, 0xa8, 0xda, 0xd5, 0x98, 0x04};

    benchmark(
        [&seed]() {
            uint8_t message_digest[32] = {0};

            SHA3::hash(&seed, sizeof(seed), &message_digest);
        },
        "sha3");

    return 0;
}
