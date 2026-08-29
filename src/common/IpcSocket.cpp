// Copyright (c) 2018-2026, The WrkzCoin developers
//
// Please see the included LICENSE file for more information.

#include "IpcSocket.h"

#include "httplib.h"

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <stdexcept>

#if WRKZ_IPC_SOCKET_SUPPORTED
#include <fcntl.h>
#include <grp.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>
#endif

namespace
{
#if WRKZ_IPC_SOCKET_SUPPORTED
    std::string errnoMessage()
    {
        return std::strerror(errno);
    }

    /* Longest name the kernel will accept, minus room for the terminator. */
    size_t maxPathLength()
    {
        sockaddr_un probe {};
        return sizeof(probe.sun_path) - 1;
    }

    /* Distinguishes "a previous run crashed and left the file behind" from
       "another daemon is running right now". Unlinking the latter would leave
       two processes each convinced it owns the endpoint, with clients silently
       landing on whichever bound last. */
    bool socketIsLive(const std::string &path)
    {
        /* Callers reach this through validatePath, but the memcpy below is
           only safe because of this bound, so it does not rely on that. */
        if (path.size() > maxPathLength())
        {
            return false;
        }

        const int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);

        if (fd == -1)
        {
            return false;
        }

        const int flags = ::fcntl(fd, F_GETFL, 0);

        if (flags != -1)
        {
            ::fcntl(fd, F_SETFL, flags | O_NONBLOCK);
        }

        sockaddr_un addr {};
        addr.sun_family = AF_UNIX;
        std::memcpy(addr.sun_path, path.data(), path.size());

        const bool connected = ::connect(fd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) == 0;

        /* A full accept queue still means somebody is listening. */
        const bool busy = !connected && (errno == EAGAIN || errno == EINPROGRESS);

        ::close(fd);

        return connected || busy;
    }
#endif
} // namespace

namespace Common
{
    namespace Ipc
    {
        bool supported()
        {
#if WRKZ_IPC_SOCKET_SUPPORTED
            return true;
#else
            return false;
#endif
        }

        std::string unsupportedReason()
        {
#if WRKZ_IPC_SOCKET_SUPPORTED
            return "";
#elif defined(_WIN32)
            return "IPC sockets are not available on Windows builds: the socket file carries no "
                   "enforceable permissions there, so the endpoint could not be restricted to its owner";
#else
            return "IPC sockets are not available in this build";
#endif
        }

        bool isAbstract(const std::string &path)
        {
            return path.size() > 1 && path.front() == '@';
        }

        bool parseMode(const std::string &text, uint32_t &mode)
        {
            if (text.empty())
            {
                return false;
            }

            std::string digits = text;

            if (digits.size() > 2 && digits[0] == '0' && (digits[1] == 'o' || digits[1] == 'O'))
            {
                digits = digits.substr(2);
            }

            if (digits.empty())
            {
                return false;
            }

            size_t consumed = 0;
            unsigned long parsed = 0;

            try
            {
                parsed = std::stoul(digits, &consumed, 8);
            }
            catch (const std::exception &)
            {
                return false;
            }

            if (consumed != digits.size() || parsed > 0777)
            {
                return false;
            }

            mode = static_cast<uint32_t>(parsed);

            return true;
        }

        std::string formatMode(const uint32_t mode)
        {
            char buffer[8] = {};
            std::snprintf(buffer, sizeof(buffer), "0%o", mode & 0777);
            return std::string(buffer);
        }

        std::string describe(const std::string &path)
        {
            if (isAbstract(path))
            {
                return "abstract socket " + path;
            }

            return "socket " + path;
        }

#if !WRKZ_IPC_SOCKET_SUPPORTED

        bool validatePath(const std::string &, std::string &error)
        {
            error = unsupportedReason();
            return false;
        }

        bool removeStaleSocket(const std::string &, std::string &error)
        {
            error = unsupportedReason();
            return false;
        }

        bool applyPermissions(const std::string &, const uint32_t, const std::string &, std::string &error)
        {
            error = unsupportedReason();
            return false;
        }

        void cleanup(const std::string &) {}

        bool bindServer(
            httplib::Server &,
            const std::string &,
            const uint32_t,
            const std::string &,
            std::string &error)
        {
            error = unsupportedReason();
            return false;
        }

        void configureClient(httplib::Client &) {}

#else

        bool validatePath(const std::string &path, std::string &error)
        {
            if (path.empty())
            {
                error = "IPC socket path is empty";
                return false;
            }

            if (path.size() > maxPathLength())
            {
                error = "IPC socket path is " + std::to_string(path.size()) + " bytes; the kernel accepts at most "
                        + std::to_string(maxPathLength());
                return false;
            }

            if (isAbstract(path))
            {
                return true;
            }

            if (path.front() != '/')
            {
                error = "IPC socket path must be absolute, or \"@name\" for the Linux abstract namespace: " + path;
                return false;
            }

            const size_t slash = path.find_last_of('/');
            const std::string directory = slash == 0 ? "/" : path.substr(0, slash);

            struct stat info {};

            if (::stat(directory.c_str(), &info) != 0)
            {
                error = "IPC socket directory " + directory + " is not usable: " + errnoMessage();
                return false;
            }

            if (!S_ISDIR(info.st_mode))
            {
                error = "IPC socket directory " + directory + " is not a directory";
                return false;
            }

            return true;
        }

        bool removeStaleSocket(const std::string &path, std::string &error)
        {
            if (isAbstract(path))
            {
                return true;
            }

            struct stat info {};

            if (::lstat(path.c_str(), &info) != 0)
            {
                if (errno == ENOENT)
                {
                    return true;
                }

                error = "cannot inspect " + path + ": " + errnoMessage();
                return false;
            }

            /* A mistyped path must never cost somebody their wallet file. */
            if (!S_ISSOCK(info.st_mode))
            {
                error = path + " already exists and is not a socket; refusing to remove it";
                return false;
            }

            if (socketIsLive(path))
            {
                error = "another process is already listening on " + path;
                return false;
            }

            if (::unlink(path.c_str()) != 0)
            {
                error = "cannot remove stale socket " + path + ": " + errnoMessage();
                return false;
            }

            return true;
        }

        bool applyPermissions(
            const std::string &path,
            const uint32_t mode,
            const std::string &group,
            std::string &error)
        {
            if (isAbstract(path))
            {
                if (!group.empty())
                {
                    error = "abstract namespace sockets have no owner, so a group cannot be assigned to " + path;
                    return false;
                }

                return true;
            }

            if (::chmod(path.c_str(), static_cast<mode_t>(mode & 0777)) != 0)
            {
                error = "cannot set mode " + formatMode(mode) + " on " + path + ": " + errnoMessage();
                return false;
            }

            if (group.empty())
            {
                return true;
            }

            errno = 0;

            const struct group *entry = ::getgrnam(group.c_str());

            if (entry == nullptr)
            {
                error = "unknown group \"" + group + "\""
                        + (errno == 0 ? std::string() : std::string(": ") + errnoMessage());
                return false;
            }

            if (::chown(path.c_str(), static_cast<uid_t>(-1), entry->gr_gid) != 0)
            {
                error = "cannot assign " + path + " to group \"" + group + "\": " + errnoMessage();
                return false;
            }

            return true;
        }

        void cleanup(const std::string &path)
        {
            if (path.empty() || isAbstract(path))
            {
                return;
            }

            struct stat info {};

            if (::lstat(path.c_str(), &info) == 0 && S_ISSOCK(info.st_mode))
            {
                ::unlink(path.c_str());
            }
        }

        bool bindServer(
            httplib::Server &server,
            const std::string &path,
            const uint32_t mode,
            const std::string &group,
            std::string &error)
        {
            if (!validatePath(path, error))
            {
                return false;
            }

            if (!removeStaleSocket(path, error))
            {
                return false;
            }

            server.set_address_family(AF_UNIX);

            /* The socket only exists once bind() has run, so there is a moment
               before chmod() in which it carries whatever the process umask
               allowed. Closing that window means the umask itself has to
               produce the final mode; the chmod below then only makes it exact
               instead of dependent on how the platform combined the two.

               umask is process wide, so this must run on the startup thread
               before any other listener is spawned. Anything else created
               during the window comes out more restrictive, never less. */
            const mode_t previousUmask = ::umask(static_cast<mode_t>((~mode) & 0777));

            /* bind_to_port rather than listen, so that the permissions land
               before listen_after_bind() lets the first client in. The port is
               ignored for AF_UNIX but has to be non-zero: httplib reads the
               bound port back with getsockname() when it is zero, which no
               Unix socket can answer. */
            const bool bound = server.bind_to_port(path, 80);

            ::umask(previousUmask);

            if (!bound)
            {
                error = "cannot bind " + describe(path);
                return false;
            }

            if (!applyPermissions(path, mode, group, error))
            {
                /* httplib owns the listening descriptor now and will not give
                   it back before listen_after_bind(). Unlinking the path is
                   what actually matters: with the name gone nothing new can
                   reach the socket. */
                cleanup(path);
                return false;
            }

            return true;
        }

        void configureClient(httplib::Client &client)
        {
            client.set_address_family(AF_UNIX);
        }

#endif
    } // namespace Ipc
} // namespace Common
