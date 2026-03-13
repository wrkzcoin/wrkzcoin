/*
 * wasm_fs_bridge.h
 *
 * Emscripten filesystem bridge — stores wallet files in the browser's
 * IndexedDB via synchronous JavaScript calls from the WASM thread.
 *
 * The bridge uses Emscripten's IDBFS-like approach but with direct
 * synchronous XMLHttpRequest-style blocking (safe in a Web Worker).
 *
 * For simplicity and SharedArrayBuffer-free operation, we use an
 * in-memory std::map as the primary store and provide JS hooks to
 * persist/load from IndexedDB on demand (async, called from JS side).
 *
 * Architecture:
 *   C++ (WASM)  ←→  in-memory map  ←→  JS IndexedDB (persistence)
 *
 * The wallet_wasm_exports.cpp dispatcher adds "exportWalletData" and
 * "importWalletData" methods so JS can push/pull binary wallet files
 * to/from the in-memory map, backed by IndexedDB on the JS side.
 */

#pragma once

#ifdef __EMSCRIPTEN__

#include <cstring>
#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace WasmFs
{
    /* In-memory file store. Thread-safe with mutex. */
    inline std::mutex &mutex()
    {
        static std::mutex m;
        return m;
    }

    inline std::map<std::string, std::vector<char>> &store()
    {
        static std::map<std::string, std::vector<char>> s;
        return s;
    }

    /* Check if a file exists in the in-memory store. */
    inline bool exists(const std::string &filename)
    {
        std::lock_guard<std::mutex> lk(mutex());
        return store().count(filename) > 0;
    }

    /* Read a file from the in-memory store. Returns empty vector if not found. */
    inline std::vector<char> read(const std::string &filename)
    {
        std::lock_guard<std::mutex> lk(mutex());
        auto it = store().find(filename);
        if (it != store().end())
            return it->second;
        return {};
    }

    /* Write a file to the in-memory store. */
    inline void write(const std::string &filename, const char *data, size_t len)
    {
        std::lock_guard<std::mutex> lk(mutex());
        store()[filename] = std::vector<char>(data, data + len);
    }

    inline void write(const std::string &filename, const std::vector<char> &data)
    {
        std::lock_guard<std::mutex> lk(mutex());
        store()[filename] = data;
    }

    /* Remove a file from the in-memory store. Returns true if it existed. */
    inline bool remove(const std::string &filename)
    {
        std::lock_guard<std::mutex> lk(mutex());
        return store().erase(filename) > 0;
    }

    /* List all filenames in the store. */
    inline std::vector<std::string> list()
    {
        std::lock_guard<std::mutex> lk(mutex());
        std::vector<std::string> names;
        for (const auto &kv : store())
            names.push_back(kv.first);
        return names;
    }

    /* Get raw pointer + size for a file (for export to JS).
       Returns false if file not found. Pointer valid until next write/remove. */
    inline bool getRaw(const std::string &filename, const char **outData, size_t *outLen)
    {
        std::lock_guard<std::mutex> lk(mutex());
        auto it = store().find(filename);
        if (it == store().end())
            return false;
        *outData = it->second.data();
        *outLen = it->second.size();
        return true;
    }

} // namespace WasmFs

#endif /* __EMSCRIPTEN__ */
