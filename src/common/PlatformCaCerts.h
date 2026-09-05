// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

#pragma once

#include <httplib.h>

/* set_ca_cert_path() only exists on httplib::Client when cpp-httplib is built
   with OpenSSL. The Android CLI build turns OpenSSL off
   (WRKZ_ANDROID_DISABLE_OPENSSL), so there is no TLS to point at a trust store
   and no such member to call - the helper has to compile away there. */
#if defined(__ANDROID__) && defined(CPPHTTPLIB_OPENSSL_SUPPORT)
#define WRKZ_HAVE_ANDROID_CA_CERT_DIR 1
#include <sys/stat.h>
#endif

namespace Common
{
    /* Point an httplib client at the platform's trust store when it cannot
       find one by itself.
     *
     * Everywhere except Android, cpp-httplib already does the right thing: it
     * enumerates the Windows ROOT/CA stores through CryptoAPI, reads the macOS
     * keychain, and finds /etc/ssl/... on Linux. Android is the gap. Its trust
     * store lives at /system/etc/security/cacerts - hash-named files, exactly
     * the layout OpenSSL's hash-dir lookup wants - and that path is in none of
     * cpp-httplib's candidate lists.
     *
     * This does not fail loudly, which is what makes it worth a helper. The
     * fallback ends with SSL_CTX_load_verify_locations(ctx, nullptr, dir), and
     * for a *directory* OpenSSL registers the path without looking at it - the
     * lookup is lazy. So on Android the built-in fallback "succeeds" on
     * /etc/ssl/certs, loads zero certificates, and every https request then
     * fails verification with nothing to say why. Plain http keeps working,
     * which is the shape the bug reports arrive in.
     *
     * Setting the directory explicitly takes cpp-httplib down its
     * ca_cert_dir_path_ branch instead of the fallback, so the store we name
     * is the one that gets used. Call this on any client that may talk https.
     */
    inline void applySystemCaCertificates(httplib::Client &client)
    {
#if defined(WRKZ_HAVE_ANDROID_CA_CERT_DIR)
        /* Newest first: since Android 14 the store moved into the Conscrypt
           APEX, with the old path kept as a compatibility symlink on most
           devices but not guaranteed. */
        static const char *const candidates[] = {
            "/apex/com.android.conscrypt/cacerts",
            "/system/etc/security/cacerts",
        };

        for (const char *const dir : candidates)
        {
            struct stat info
            {
            };

            if (::stat(dir, &info) == 0 && S_ISDIR(info.st_mode))
            {
                client.set_ca_cert_path(std::string(), dir);
                return;
            }
        }
#else
        (void)client;
#endif
    }
} // namespace Common
