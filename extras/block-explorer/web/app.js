"use strict";

const els = {
    healthStatus: document.getElementById("healthStatus"),
    networkCards: document.getElementById("networkCards"),
    indexCards: document.getElementById("indexCards"),
    searchForm: document.getElementById("searchForm"),
    searchInput: document.getElementById("searchInput"),
    resultBox: document.getElementById("resultBox"),
    refreshBtn: document.getElementById("refreshBtn")
};

let lastQuery = "";

function fmtNum(value) {
    if (value === null || value === undefined) {
        return "-";
    }
    return Number(value).toLocaleString();
}

function setResult(data) {
    els.resultBox.textContent = JSON.stringify(data, null, 2);
}

async function getJson(path) {
    const res = await fetch(path);
    const json = await res.json();
    if (!res.ok || json.success === false) {
        throw new Error(json.error ? json.error.message : "Request failed");
    }
    return json;
}

function setCards(container, items) {
    container.innerHTML = items.map((item) => (
        `<div class="card"><span>${item.label}</span><strong>${item.value}</strong></div>`
    )).join("");
}

async function loadOverview() {
    const [health, summary, idx] = await Promise.all([
        getJson("/api/v1/health"),
        getJson("/api/v1/network/summary"),
        getJson("/api/v1/index/status")
    ]);

    const hs = health.data && health.data.status ? health.data.status : "unknown";
    els.healthStatus.textContent = `Health: ${hs}`;

    const s = summary.data;
    setCards(els.networkCards, [
        { label: "Height", value: fmtNum(s.height) },
        { label: "Network Height", value: fmtNum(s.network_height) },
        { label: "Hashrate", value: fmtNum(s.hashrate) },
        { label: "Difficulty", value: fmtNum(s.difficulty) }
    ]);

    const i = idx.data;
    setCards(els.indexCards, [
        { label: "Indexed Tip", value: i.indexed_tip_height !== null ? fmtNum(i.indexed_tip_height) : "-" },
        { label: "Lag", value: i.lag !== null ? fmtNum(i.lag) : "-" },
        { label: "Payment IDs", value: fmtNum(i.indexed_payment_id_count) },
        { label: "Last Sync", value: i.last_sync_at ? new Date(i.last_sync_at).toLocaleTimeString() : "-" }
    ]);
}

async function runSearch(query) {
    const search = await getJson(`/api/v1/search?q=${encodeURIComponent(query)}`);
    const target = search.data.target;
    const result = await getJson(target);
    setResult({
        search: search.data,
        result: result.data
    });
}

async function refresh() {
    try {
        await loadOverview();
        if (lastQuery) {
            await runSearch(lastQuery);
        }
    } catch (err) {
        setResult({ error: err.message });
    }
}

els.searchForm.addEventListener("submit", async (event) => {
    event.preventDefault();
    const q = els.searchInput.value.trim();
    if (!q) {
        return;
    }
    lastQuery = q;
    try {
        await runSearch(q);
    } catch (err) {
        setResult({ error: err.message });
    }
});

els.refreshBtn.addEventListener("click", refresh);

refresh();

