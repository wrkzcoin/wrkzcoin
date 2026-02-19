"use strict";

class TtlCache {
    constructor() {
        this.store = new Map();
    }

    get(key) {
        const item = this.store.get(key);
        if (!item) {
            return null;
        }

        if (item.expiresAt <= Date.now()) {
            this.store.delete(key);
            return null;
        }

        return item.value;
    }

    set(key, value, ttlMs) {
        this.store.set(key, {
            value,
            expiresAt: Date.now() + ttlMs
        });
    }
}

module.exports = TtlCache;

