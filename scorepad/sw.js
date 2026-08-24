/* Offline shell for the score book.
 *
 * The whole point of this file: a Friday evening at somebody's kitchen table
 * is exactly where the phone signal is worst, and a score book that needs the
 * internet is no replacement for one made of paper. Everything the app needs
 * is cached on the first visit and served from the cache thereafter.
 *
 * Bump CACHE when any file below changes, or the old copy is served forever.
 */
const CACHE = "canasta-scorebook-v2";
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

/* Cache first, network second. The scores live in localStorage and never leave
 * the phone, so there is nothing here that needs to be fresh — only correct. */
self.addEventListener("fetch", (e) => {
  if (e.request.method !== "GET") return;
  e.respondWith(
    caches.match(e.request).then((hit) => hit || fetch(e.request).then((res) => {
      // "cors" as well as "basic": the Firebase SDK comes from gstatic, and
      // caching it is what lets a phone that has shared once still open the
      // book with no signal at all.
      if (res && res.ok && (res.type === "basic" || res.type === "cors"))
        caches.open(CACHE).then((c) => c.put(e.request, res.clone()));
      return res;
    }).catch(() => caches.match("./index.html")))
  );
});
