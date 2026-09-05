# Local IPC and Console

Every listener that speaks HTTP can also be served over an **AF_UNIX socket**
instead of, or alongside, a TCP port. Nothing is bound unless a path is given:
IPC is off by default in all binaries.

The point is not to avoid loopback but to change who decides. A TCP listener is
reachable by every process on the machine and defends itself with a shared
secret; a socket file is reachable only by whoever the mode on that file admits,
and the kernel enforces it.

!!! warning "POSIX only"
    Windows has had AF_UNIX since Windows 10 1803, but the socket file carries
    no permissions the OS enforces and there is no `SO_PEERCRED`, so the access
    control this rests on would not exist. The flags are accepted and ignored
    with a warning rather than opening an unrestricted endpoint.

## Serving

| Binary | Option | Default | Meaning |
| --- | --- | --- | --- |
| `Wrkzd` | `--rpc-ipc-path` | empty | Also serve RPC on this socket, **alongside** the TCP listeners |
| `Wrkzd` | `--rpc-ipc-mode` | `0600` | Octal permissions for the socket file |
| `Wrkzd` | `--rpc-ipc-group` | empty | Group to own the socket file, for a `0660` shared setup |
| `Wrkzd` | `--rpc-ipc-require-token` | `false` | Also demand `--rpc-access-token` from IPC callers |
| `wrkz-wallet-api` | `--rpc-ipc-path` | empty | Also serve the API on this socket |
| `wrkz-wallet-api` | `--rpc-ipc-mode` | `0600` | Octal permissions |
| `wrkz-wallet-api` | `--rpc-ipc-group` | empty | Owning group |
| `wrkz-service` | `--bind-ipc-path` | empty | Serve JSON-RPC on this socket **instead of** the TCP port |
| `wrkz-service` | `--bind-ipc-mode` | `0600` | Octal permissions |
| `wrkz-service` | `--bind-ipc-group` | empty | Owning group |

A path must be absolute. Prefixing it with `@` uses the Linux **abstract
namespace**, which has no filesystem entry and therefore no permissions at all —
every process in the network namespace can connect, and the daemon warns when
you ask for one.

```bash
# Owner-only socket, the default
Wrkzd --rpc-ipc-path /run/wrkz/wrkzd.sock

# Shared with a group, so an unprivileged service account can reach the node
Wrkzd --rpc-ipc-path /run/wrkz/wrkzd.sock --rpc-ipc-mode 0660 --rpc-ipc-group wrkz
```

## Connecting

Anywhere a daemon address is accepted, an absolute path, an `@name`, or an
`ipc://path` means a local socket rather than a host. One spelling works across
every client:

```bash
Wrkzd --rpc-ipc-path /run/wrkz/wrkzd.sock

miner        --daemon-address /run/wrkz/wrkzd.sock --address WRKZ...
wrkz-wallet  --remote-daemon  /run/wrkz/wrkzd.sock
wrkz-service --daemon-address /run/wrkz/wrkzd.sock \
             --container-file wallet --container-password hunter2
```

A relative path has to be written as `ipc://./wrkzd.sock` and resolves against
the client's working directory. Without the prefix it is indistinguishable from
a hostname and is dialled as one.

The daemon's own console uses the IPC socket automatically whenever one is
bound, so `status`, `print_cn` and friends stop making loopback TCP connections
to their own process.

## Authentication

On the **daemon's** IPC socket `--rpc-access-token` is *not* required. The mode
on the socket file already decided who may connect and the kernel enforced it,
so the token adds nothing except an obligation to hand the secret to every local
integration. Set `--rpc-ipc-require-token` to demand both. Rate limiting is
skipped on IPC for the same reason it is skipped on loopback: the caller is
neither anonymous nor remote.

`wrkz-wallet-api` and `wrkz-service` **still require their password on IPC**.
Those endpoints move money, and silently dropping an authentication step that
was previously unconditional is not a change worth making by default.

## Attaching a console to a running daemon

A daemon under a process manager — pm2, systemd, a container — has no terminal,
so it runs with `--no-console` and its commands are out of reach. `attach` puts
them back: it opens an interactive console against a daemon that is already
running, over its IPC socket. Every line typed runs inside that daemon through
the same command handler the local console uses, and whatever it prints comes
back.

```bash
pm2 start Wrkzd -- --no-console --rpc-ipc-path /run/wrkz/wrkzd.sock
Wrkzd attach /run/wrkz/wrkzd.sock
```

`Wrkzd --attach /run/wrkz/wrkzd.sock` is the same thing.

**The socket is the only endpoint accepted.** Console commands change log
levels, ban peers, start compactions and stop the node, so they are served on
the IPC socket alone and never on a TCP listener, with or without a token.
Attach therefore runs as a user the socket admits, which by default means the
daemon's own.

| In an attached session | Effect |
| --- | --- |
| `exit`, `quit` | Leave the session. The daemon carries on |
| `stop` | Shut the daemon down. Same command as the local console's `exit` |

`stop` exists under a second name precisely so that leaving an attached session
is never confused with stopping the node. Commands run one at a time across all
consoles, local and attached, and a command that blocks — `compact_db wait` —
holds its connection until it finishes.

### The route underneath

One route: `POST /console` with `{"command": "..."}`, answering
`{"output": "...", "status": "OK"}`. It is registered on the IPC listener only.
Scripts can call it directly, or pipe commands into `attach`, which exits when
its input ends.

```bash
echo "status" | Wrkzd attach /run/wrkz/wrkzd.sock
```

## Behaviour and guarantees

- **Permissions are in place before the first client can connect.** The socket
  is created under a umask derived from the requested mode rather than
  `chmod`'ed afterwards, so there is no window in which it is reachable more
  widely than asked for.
- **A stale socket from a crashed run is cleared automatically.** A path that
  exists but is not a socket is never removed, and a path another process is
  still listening on is refused rather than stolen.
- **The socket file is unlinked on shutdown.**
- **A failed IPC bind is not fatal** for `Wrkzd` or `wrkz-wallet-api` — they
  warn and keep serving on TCP, matching how an IPv6 bind failure is handled.
  For `wrkz-service`, where IPC *replaces* the TCP port, a failed bind stops
  startup.
- Socket paths are limited to 107 bytes by the kernel, not by us.
- P2P is not offered over IPC: a local socket cannot carry a peer-to-peer
  network.
