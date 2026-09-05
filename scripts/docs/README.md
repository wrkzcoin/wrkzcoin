# Publishing docs.wrkz.work

The documentation sources are in [`docs/`](../../docs); this directory holds
what is needed to get them onto the server.

## Layout

The live docs stay at the web root — the URLs that are published today do not
move — and each archived release sits beside them:

```
/var/www/docs.wrkz.work/
    index.html  guides/  daemon-rpc/  wallet-api/  ...   <- current release
    llms.txt  llms-full.txt                              <- for AI agents
    versions.json                                        <- version selector
    v0.4.8/                                              <- archived releases
    v0.4.7/
```

Nothing in the existing site moves, so every bookmark, external link and search
result keeps working. That is the reason for this layout rather than the usual
`mike` one, which puts everything under `/latest/`.

## Setup, once

```bash
cd /path/to/wrkzcoin
python3 -m venv .venv-docs
.venv-docs/bin/pip install -r docs/requirements.txt
```

Use a virtualenv rather than a system-wide install. `apt install mkdocs` gives
you base mkdocs, whose only themes are `mkdocs` and `readthedocs`, so the build
dies with `Unrecognised theme name: 'material'`; and a bare `pip install` is
refused outright on Debian 12+ and Ubuntu 23.04+ as an externally-managed
environment.

## Publishing

```bash
git pull
source .venv-docs/bin/activate

# routine docs update
scripts/docs/publish.sh

# at release time - also snapshots the build as v<version>
scripts/docs/publish.sh --archive

# see what would change without touching anything
scripts/docs/publish.sh --dry-run
```

| Variable | Default |
| --- | --- |
| `DOCS_WEB_ROOT` | `/var/www/docs.wrkz.work` |
| `DOCS_BASE_URL` | `https://docs.wrkz.work` |

If the web root is not writable by your user, run it as one that owns the
directory, or keep the virtualenv on `PATH` through sudo:

```bash
sudo -E env "PATH=$PATH" scripts/docs/publish.sh
```

Plain `sudo scripts/docs/publish.sh` fails with `mkdocs is not installed` —
root has a different `PATH` and cannot see the virtualenv.

The version comes from `src/config/version.h.in`, the same header the binaries
report, so the docs and `Wrkzd --version` cannot disagree about which release
is current.

### Two things the script is careful about

- **It never builds into the web root.** `mkdocs build` wipes its output
  directory first, which would delete every archive. The build goes to a
  temporary directory and is then rsynced.
- **`rsync --delete` excludes `/v*/` and `/versions.json`.** Those are not part
  of any single build, so an unprotected sync would remove them.

## The version selector

`versions.json` is regenerated on every run from the directories that actually
exist on disk, so it cannot advertise a version that is not being served.

Its ordering matters. Material finds the current version by matching the last
path segment of the base URL against the file:

```js
let [,i] = t.base.match(/([^/]+)\/?$/);
return n.find(({version, aliases}) => version === i || aliases.includes(i)) || n[0]
```

The live site is served from the root, which has no path segment to match, so
Material falls back to `n[0]`. **The current release must be the first entry**,
which is what the generator does. Archived builds under `/v0.4.8/` match on
their directory name normally.

Sibling links are built as `new URL("../<version>/", base)`, which from the
root resolves to `https://docs.wrkz.work/<version>/` — the archive directories.

## nginx

```nginx
server {
    listen 443 ssl http2;
    server_name docs.wrkz.work;

    root /var/www/docs.wrkz.work;
    index index.html;

    # MkDocs uses directory URLs: /guides/lite-node/ -> .../index.html
    location / {
        try_files $uri $uri/ $uri/index.html =404;
    }

    # The AI artefacts are markdown and JSON, not HTML.
    location = /versions.json {
        add_header Cache-Control "no-cache";
        types { } default_type application/json;
    }

    location ~ \.md$ {
        types { } default_type "text/markdown; charset=utf-8";
    }

    location ~ ^/llms(-full)?\.txt$ {
        types { } default_type "text/plain; charset=utf-8";
    }

    # Archived releases are immutable once published.
    location ~ ^/v[0-9]+\.[0-9]+\.[0-9]+/ {
        expires 30d;
        add_header Cache-Control "public, immutable";
    }

    gzip on;
    gzip_types text/plain text/css text/markdown application/javascript application/json image/svg+xml;
}
```

## When to archive

**Start at 0.4.8.** Older tags do carry a `docs/` tree, so an archive for 0.4.7
could be built — but that content is the version this release corrected, and it
states several things that were never true (a `/fee` route that does not exist,
wallet API port 8070, fusion endpoints removed in 0.4.7). Republishing it under
a version label would give those errors a permanent home. Archive going
forward, not backward.

An archive is only worth making when the documented behaviour actually diverges
— a fork height, a removed endpoint, a renamed flag. A release that only fixes
wording does not need one.
