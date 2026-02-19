"use strict";

function isNumeric(value) {
    return /^[0-9]+$/.test(value);
}

function isHash64(value) {
    return /^[0-9a-fA-F]{64}$/.test(value);
}

function isPaymentIdShort(value) {
    return /^[0-9a-fA-F]{16}$/.test(value);
}

function isPaymentIdLong(value) {
    return /^[0-9a-fA-F]{64}$/.test(value);
}

function normalizePaymentId(value) {
    const raw = toLowerHex(value);
    if (isPaymentIdShort(raw)) {
        return { short: raw, long: null };
    }
    if (isPaymentIdLong(raw)) {
        return { short: null, long: raw };
    }
    return null;
}

function toLowerHex(value) {
    return value.toLowerCase();
}

module.exports = {
    isNumeric,
    isHash64,
    isPaymentIdShort,
    isPaymentIdLong,
    normalizePaymentId,
    toLowerHex
};
