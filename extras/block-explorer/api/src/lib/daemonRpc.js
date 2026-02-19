"use strict";

const { upstreamError } = require("./httpErrors");

class DaemonRpcClient {
    constructor({ baseUrl, token, timeoutMs }) {
        this.baseUrl = baseUrl;
        this.token = token;
        this.timeoutMs = timeoutMs;
    }

    buildHeaders(includeJsonContentType = false) {
        const headers = {};

        if (includeJsonContentType) {
            headers["Content-Type"] = "application/json";
        }

        if (this.token) {
            headers["X-API-Key"] = this.token;
        }

        return headers;
    }

    async request(path, options = {}) {
        const controller = new AbortController();
        const timer = setTimeout(() => controller.abort(), this.timeoutMs);

        try {
            const response = await fetch(`${this.baseUrl}${path}`, {
                ...options,
                signal: controller.signal
            });

            const text = await response.text();
            let json;

            try {
                json = text ? JSON.parse(text) : {};
            } catch (err) {
                throw upstreamError("Upstream daemon returned non-JSON response", {
                    path,
                    status: response.status,
                    body: text.slice(0, 300)
                });
            }

            if (!response.ok) {
                throw upstreamError("Upstream daemon returned HTTP error", {
                    path,
                    status: response.status,
                    body: json
                });
            }

            return json;
        } catch (err) {
            if (err.name === "AbortError") {
                throw upstreamError("Upstream daemon request timed out", { path });
            }

            if (err.statusCode) {
                throw err;
            }

            throw upstreamError("Upstream daemon request failed", {
                path,
                reason: err.message
            });
        } finally {
            clearTimeout(timer);
        }
    }

    async callJsonRpc(method, params = {}) {
        const payload = {
            jsonrpc: "2.0",
            id: "explorer-api",
            method,
            params
        };

        const json = await this.request("/json_rpc", {
            method: "POST",
            headers: this.buildHeaders(true),
            body: JSON.stringify(payload)
        });

        if (json.error) {
            throw upstreamError("Upstream JSON-RPC returned error", {
                method,
                params,
                rpcError: json.error
            });
        }

        if (!json.result) {
            throw upstreamError("Upstream JSON-RPC response missing result", {
                method,
                params,
                body: json
            });
        }

        return json.result;
    }

    async getInfo() {
        return this.request("/info", {
            method: "GET",
            headers: this.buildHeaders(false)
        });
    }

    async getLastBlockHeader() {
        const result = await this.callJsonRpc("getlastblockheader", {});
        return result.block_header;
    }

    async getBlockHeaderByHeight(height) {
        const result = await this.callJsonRpc("getblockheaderbyheight", { height });
        return result.block_header;
    }
}

module.exports = DaemonRpcClient;
