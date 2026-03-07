# WrkzCoin Explorer

This folder contains a static block explorer UI for WrkzCoin.

It is intended to be served by a normal web server such as nginx while the
WrkzCoin daemon runs separately with explorer RPC enabled.

## Requirements

- A built and running WrkzCoin daemon
- Explorer RPC enabled on the daemon
- A web server to serve the static files in this folder

## Daemon

Run the daemon with explorer mode enabled.

Example:

```powershell
.\Wrkzd.exe --daemon-mode explorer --enable-cors
```

Notes:

- `--daemon-mode explorer` is required for the explorer-specific RPC methods
- `--enable-cors` is useful for direct browser access
- If you use nginx as a reverse proxy, browser CORS issues are usually avoided

## Files

- `index.html`: main explorer page
- `app.js`: frontend logic
- `style.css`: explorer styling
- `vendor/TurtleCoinUtils.js`: vendored client-side crypto helper used by Check Transaction

## Local Development

Serve this folder with any static file server.

Example with Python:

```powershell
cd extras\explorer
python -m http.server 8080
```

Then open:

```text
http://127.0.0.1:8080/
```

By default, the explorer expects the daemon RPC to be reachable through `/api`.

## Production Deployment

Recommended layout:

- nginx serves the static files in `extras/explorer`
- nginx proxies `/api` to the daemon RPC port

This keeps the browser on one origin and avoids cross-origin issues.

## nginx Sample

Example server block:

```nginx
server {
    listen 80;
    server_name explorer.example.com;

    root /var/www/wrkz-explorer;
    index index.html;

    location / {
        try_files $uri $uri/ /index.html;
    }

    location /api/ {
        proxy_pass http://127.0.0.1:17856/;
        proxy_http_version 1.1;

        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
        proxy_set_header X-Forwarded-Proto $scheme;
    }
}
```

## Deploy Steps

1. Build the daemon with explorer support.
2. Restart the daemon with `--daemon-mode explorer`.
3. Copy the contents of this folder to your web root.
4. Configure nginx to serve the static files and proxy `/api/` to the daemon.
5. Open the explorer in a browser and load a block or transaction page.

## Check Transaction

The `Check Transaction` feature runs entirely in the browser.

It does not send private keys to the server.

It relies on:

- transaction data already loaded on the page
- the recipient address
- a 64-character private key entered by the user
- `vendor/TurtleCoinUtils.js`

For view-key based checking to work, the transaction JSON exposed by the daemon
must include the transaction public key.

## Troubleshooting

### Explorer page loads but data fails

Check:

- daemon is running
- daemon was started with `--daemon-mode explorer`
- nginx `/api/` proxy points to the correct daemon port

### Check Transaction shows no matches

Check:

- the address is correct
- the private key is correct
- the transaction page JSON includes the transaction public key
- the daemon was restarted after any RPC changes

### Browser shows RPC or CORS errors

If you access the daemon directly from the browser, confirm:

- `--enable-cors` is set

If you use nginx reverse proxy, prefer routing through `/api` instead.
