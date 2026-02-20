"use strict";

class HttpError extends Error {
    constructor(statusCode, code, message, details = null) {
        super(message);
        this.name = "HttpError";
        this.statusCode = statusCode;
        this.code = code;
        this.details = details;
    }
}

function badRequest(message, details = null) {
    return new HttpError(400, "BAD_REQUEST", message, details);
}

function notFound(message, details = null) {
    return new HttpError(404, "NOT_FOUND", message, details);
}

function upstreamError(message, details = null) {
    return new HttpError(502, "UPSTREAM_ERROR", message, details);
}

module.exports = {
    HttpError,
    badRequest,
    notFound,
    upstreamError
};

