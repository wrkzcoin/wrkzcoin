// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

/* Releases a thread's CryptoNight scratchpad when the thread ends.
 *
 * slow-hash-x86.c keeps its scratchpad alive between hashes, because
 * allocating and freeing 128KB..2MB per hash cost more than hashing did. It
 * reaches the buffer through a C thread-local pointer, and a C thread-local
 * has no destructor - so a thread that hashed and then exited would take its
 * buffer with it. Several callers spawn a thread per unit of work (the miner
 * starts fresh workers for every block template, wrkz-txpow-server for every
 * job, stratum for every submitted share), which would turn that into a steady
 * leak.
 *
 * A C++ thread_local does have a destructor, and both MSVC and libstdc++ run
 * it on thread exit, so the release is hung off one of those. */

/* Only the x86 AES-NI implementation keeps a scratchpad; the ARM and portable
 * paths put theirs on the stack. The guard mirrors the one in
 * slow-hash-x86.c so this file does not reference a symbol that does not
 * exist on those targets. */
#if !defined NO_AES && (defined(__x86_64__) || (defined(_MSC_VER) && defined(_WIN64)))
#define WRKZ_SLOW_HASH_KEEPS_SCRATCHPAD 1
#endif

extern "C" void slow_hash_arm_state_release(void);

#if defined(WRKZ_SLOW_HASH_KEEPS_SCRATCHPAD)

extern "C" void slow_hash_release_state(void);

namespace
{
    struct ScratchpadOwner
    {
        bool armed = false;

        ~ScratchpadOwner()
        {
            if (armed)
            {
                slow_hash_release_state();
            }
        }
    };

    thread_local ScratchpadOwner owner;
} // namespace

extern "C" void slow_hash_arm_state_release(void)
{
    /* Writing to the object is what makes this thread construct it, and a
       constructed thread_local with a destructor is what the runtime registers
       to run at thread exit. Called after every successful allocation, which
       is cheap: it is one thread-local store once the flag is already set. */
    owner.armed = true;
}

#else

extern "C" void slow_hash_arm_state_release(void) {}

#endif
