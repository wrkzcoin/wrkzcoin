"use strict";

const { HttpError } = require("./httpErrors");

function requireAdmin(request, adminApiKey) {
    if (!adminApiKey) {
        throw new HttpError(
            403,
            "FORBIDDEN",
            "Admin API key is not configured on server",
            null
        );
    }

    const provided = request.headers["x-admin-key"];
    if (!provided || provided !== adminApiKey) {
        throw new HttpError(403, "FORBIDDEN", "Admin API key is invalid", null);
    }
}

module.exports = {
    requireAdmin
};

