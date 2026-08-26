# Squirrel Communicator Image Service API

Version 1.0

This document describes the SquirrelCommunicatorImage microservice — a
content-addressable file storage service with a built-in GIF provider proxy.
Media files (images, GIFs, videos) are NOT stored by the main backend; they live
here. The backend only issues and revokes per-session API keys (see the
`image_api_key` WebSocket event in `servers_api.md`); the client talks to this
service directly using that key.

No database. No external dependencies. Just a filesystem and RAM.

--------------------------------------------
AUTHENTICATION & KEY MANAGEMENT
--------------------------------------------

Keys are long random strings (64 hex chars, up to 1024). Only their SHA-256
hashes are kept in memory — the plaintext is never stored or logged.

Uploads require an API key (the per-session key issued at login), sent via the
`X-SQRLL-API-KEY` header. Key management endpoints additionally require the
master/admin key, sent via `X-SQRLL-IMAGE-API-KEY`.

POST /api/key

    Login (register a key) — admin endpoint, requires the master key.
    The backend calls this when a user logs in.

    Headers:
        X-SQRLL-IMAGE-API-KEY  string  Master/admin key

    Response 201:
        { "key": "f3b0c44298fc1c149afbf4c8996fb924..." }

DELETE /api/key

    Logout (revoke a key) — admin endpoint, requires the master key and the key
    to revoke. The backend calls this when a user logs out or their session
    expires.

    Headers:
        X-SQRLL-IMAGE-API-KEY  string  Master/admin key
        X-SQRLL-API-KEY        string  Key to revoke

    Response 200:
        { "status": "revoked" }

    Bootstrap keys can be preloaded at startup via SQRLL_AUTH_KEYS
    (comma-separated).

--------------------------------------------
UPLOAD
--------------------------------------------

POST /api/image/upload

    Upload a file. Requires a registered API key.

    Request:
        Headers: X-SQRLL-API-KEY: <key>
        Body: raw file bytes

    Response 201 (new file):
        {
            "id": "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
            "status": "ok",
            "type": "image/png",
            "size": 1024
        }

    Response 200 (duplicate):
        { "id": "...", "status": "duplicate", "type": "image/png", "size": 1024 }

    Errors:
        403 - Missing/invalid API key
        413 - Body exceeds max upload size
        415 - Unsupported file type
        429 - Rate limit exceeded (with Retry-After)
        500 - Storage write failure

--------------------------------------------
DOWNLOAD
--------------------------------------------

GET /api/image/{sha256-hash}

    Download a file by its SHA-256 hash. Public (no key required). This is how
    media referenced by a message's content hash is displayed.

    Request:
        GET /api/image/e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855

    Response 200:
        Headers:
            Content-Type: image/png
            Content-Length: 1024
            X-Cache: HIT | MISS
            X-Content-Type-Options: nosniff
            (SVG only) Content-Disposition: attachment
        Body: raw file bytes

    Errors:
        400 - Missing or invalid file ID
        404 - File not found

--------------------------------------------
GIF PROVIDER PROXY
--------------------------------------------

Proxy for KLIPY GIF search/trending and server-side GIF fetch-for-re-upload. The
KLIPY key is attached server-side and never returned to the browser.

All three endpoints return JSON, support OPTIONS preflight, and require auth
(the same API key issued on login, sent via X-SQRLL-API-KEY or X-API-Token).

GET /api/gifs/search

    Query: q (required, trimmed non-empty), limit (1..50, default 25),
           page (1..1000, default 1)

    Response 200:
        { "results": [ ... ], "has_next": true }

GET /api/gifs/trending

    Query: limit (1..50, default 25), page (1..1000, default 1)

    Response 200:
        { "results": [ ... ], "has_next": true }

GET /api/gifs/fetch

    Downloads a remote GIF server-side and re-uploads it into content-addressable
    storage, returning the same upload response as /api/image/upload.

    Query: url (required) — remote GIF to download for re-upload

    Response 201:
        { "id": "<sha256>", "status": "ok", "type": "image/gif", "size": 12345 }

    Guards:
        - SSRF guard (loopback/private/link-local/multicast/unspecified hosts rejected)
        - http/https scheme only, redirects disabled
        - 8 MB download cap, ~25s upstream timeout
        - magic-byte content validation (same as uploads)

    Errors:
        400 - Missing/invalid url, or non-public host
        401 - Missing/invalid API key
        413 - Remote content exceeds 8 MB
        415 - Remote content is not an allowed type
        429 - Rate limit exceeded (with Retry-After)
        502 - Upstream transport error / non-200 / malformed JSON
        503 - KLIPY key unset

    Normalized result item:

        {
            "id": 123, "slug": "...", "title": "...",
            "preview": {"url": "...", "width": 0, "height": 0},
            "gif":     {"url": "...", "width": 0, "height": 0},
            "mp4":     {"url": "...", "width": 0, "height": 0}
        }

    Rendition selection rules: ads are skipped; items without a usable GIF URL are
    skipped; preview prefers static formats (webp -> jpg -> png -> gif) from sizes
    sm -> md -> xs -> hd; gif and mp4 are picked from sizes md -> sm -> hd -> xs.

--------------------------------------------
HEALTH
--------------------------------------------

GET /health

    Response 200:
        {"status": "ok"}

--------------------------------------------
INSTANCE IDENTIFICATION
--------------------------------------------

GET /instance

    Returns the running instance's unique identifier — a random 32-character
    Base62 hash, regenerated on every process start and never persisted. Like
    /health, this endpoint is unauthenticated (the ID is a non-secret
    operational identifier, not authentication material).

    Response 200:
        { "instanceId": "7k2QnXp4wR9vT0cL5mY3aB8dE1fG6hJ" }

    Because the image service's key registry is RAM-only, a restart silently
    orphans every issued per-session key. The backend polls this endpoint every
    ImageInstanceProbeIntervalSeconds and, when instanceId changes (i.e. the
    process restarted), invalidates all cached per-session keys and re-issues
    them on the next login / WebSocket reconnect.

    When the endpoint is absent (older service) or returns a non-200 / empty
    body, the backend falls back to time-based (TTL) invalidation only.

    Every HTTP call the backend makes to this service is additionally wrapped
    in a circuit breaker (ImageServiceCircuitBreakerThreshold /
    ImageServiceCircuitBreakerCooldownSeconds in BackendSettings.ini): after a
    few consecutive failures it fails fast without contacting the service, so a
    down image service cannot stall the WebSocket event loop or the login
    threads. It half-opens after the cooldown to probe whether the service has
    recovered.

--------------------------------------------
ENVIRONMENT VARIABLES
--------------------------------------------

    STORAGE_PATH          /var/data/sqrll/media   File storage directory
    SQRLL_IMAGE_API_KEY   (empty = disabled)      Admin key for key management endpoints
    SQRLL_AUTH_KEYS       (empty)                 Comma-separated bootstrap API keys
    SQRLL_KLIPY_API_KEY   (empty = disabled)      KLIPY app key for GIF endpoints (503 if empty)
    MAX_REQUESTS_PER_HOUR 100                     Per-key rate limit (hourly window)
    MAX_DISK_GB           100                     Disk quota in GB
    MAX_RAM_MB            1024                    RAM cache limit in MB
    MAX_UPLOAD_MB         8                       Max single upload size
    PORT                  8083                    HTTP listen port

--------------------------------------------
ALLOWED FILE TYPES
--------------------------------------------

    Images   image/jpeg, image/png, image/gif, image/webp, image/svg+xml,
             image/bmp, image/tiff
    Video    video/mp4, video/webm, video/ogg
    Audio    audio/mpeg, audio/ogg, audio/wav, audio/webm
    Document application/pdf

    Ogg and WebM are generic containers; they are labeled as video/* even when
    they carry audio-only streams. This does not affect upload acceptance.

--------------------------------------------
SECURITY MODEL
--------------------------------------------

    - S2S auth: uploads require an API key; keys are stored only as SHA-256
      hashes and compared in constant time.
    - Per-key rate limit: sliding one-hour window; exceeding it returns 429 with
      a Retry-After header.
    - Magic-byte validation: content is sniffed from real signatures, not from
      the client-provided filename or Content-Type. Disguised scripts are rejected.
    - Anti-injection headers: every download sends X-Content-Type-Options: nosniff.
      SVG (which can carry <script>) is always forced to download with a sandboxed CSP.
    - OOM shield: http.MaxBytesReader caps the body before any processing.

--------------------------------------------
KEY PROPERTIES
--------------------------------------------

    - Stateless. Restart rebuilds all state from disk in under a second.
    - Self-healing. Boot scan recovers the full index.
    - Dogpile-safe. 500 concurrent requests for the same file: 1 disk read,
      499 RAM hits, all under RLock.
    - Horizontally scalable. No shared state between instances.
      (Note: per-key rate limits are in-memory and therefore per-instance.)
