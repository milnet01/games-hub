/* Offline shell for the score book.
 *
 * The whole point of this file: a Friday evening at somebody's kitchen table
 * is exactly where the phone signal is worst, and a score book that needs the
 * internet is no replacement for one made of paper. Everything the app needs
 * is cached on the first visit and served from the cache thereafter.
 *
 * Bump CACHE when any file below changes, or the old copy is served forever.
 */
const CACHE = "canasta-scorebook-v3";
const SHELL = [
  "./",
  "./index.html",
  "./manifest.webmanifest",
  "./firebase-config.js",
  "./icon-192.png",
  "./icon-512.png"
];

self.addEventListener("install", (e) => {
  e.waitUntil(caches.open(CACHE).then((c) => c.addAll(SHELL)).then(() => self.skipWaiting()));
});

self.addEventListener("activate", (e) => {
  e.waitUntil(
    caches.keys()
      .then((keys) => Promise.all(keys.filter((k) => k !== CACHE).map((k) => caches.delete(k))))
      .then(() => self.clients.claim())
  );
});

/* The ONLY things worth caching: this app's own files, and the pinned Firebase
 * SDK that index.html loads from gstatic. Everything else goes straight to the
 * network and is never stored.
 *
 * That list is deliberately a whitelist rather than a "cache anything that
 * worked". The database client falls back to HTTP long-polling whenever the
 * network will not carry a WebSocket -- which is the restrictive-network case
 * this whole file is written for -- and those are ordinary GETs that come back
 * 200. Cached, and then served cache-first for ever, they replay one stale
 * answer at the live database and syncing never recovers: there is no
 * invalidation short of clearing the site's data on that phone. */
const SDK_PREFIX = "https://www.gstatic.com/firebasejs/";

function worthCaching(url) {
  if (url.origin === self.location.origin)
    return true;
  return url.href.startsWith(SDK_PREFIX);
}

/* Cache first, network second, for the shell. The scores live in localStorage
 * and never leave the phone, so there is nothing here that needs to be fresh —
 * only correct. */
self.addEventListener("fetch", (e) => {
  if (e.request.method !== "GET") return;

  const url = new URL(e.request.url);
  if (!worthCaching(url)) return;   // straight to the network, every time

  e.respondWith(
    caches.match(e.request).then((hit) => hit || fetch(e.request).then((res) => {
      if (res && res.ok && (res.type === "basic" || res.type === "cors")) {
        // waitUntil, so the write is not cut short when respondWith settles.
        // Without it the entry is written or not depending on timing, which is
        // the worst of both: sometimes cached, sometimes not, never diagnosable.
        const copy = res.clone();
        e.waitUntil(caches.open(CACHE).then((c) => c.put(e.request, copy)));
      }
      return res;
    }).catch(() => caches.match("./index.html")))
  );
});
