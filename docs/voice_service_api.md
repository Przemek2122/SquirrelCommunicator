# Squirrel Communicator Voice Service API Documentation

Version 1.0

This document describes the Go Voice Service (SquirrelCommunicatorVoice), which
handles audio/video rooms, voice chat and screen sharing. It runs as a separate
container on port `8082` and is reached from the outside through the reverse
proxy under the `/voice-ws` path.

The C++ backend does **not** proxy the voice WebSocket connection. It only
provisions rooms over REST (see below) and hands the client a room name and
token. The browser/Tauri client then connects to the voice WebSocket directly.
(Verified in `FRoomsServiceManager` — `@Note: connection is done by users browser.`)

The media/image service is a different component; see docs/image_service_api.md.
The backend REST + WebSocket API is documented in docs/servers_api.md.

============================================
SECTION 1: CONFIGURATION
============================================

The voice service is configured via docker/.env.voice:

    SQRLL_VOICE_ADDRESS  127.0.0.1  Address the backend uses to reach the service
    SQRLL_VOICE_PORT     8082       Port the Go service listens on (HTTP + WebSocket)
    SQRLL_VOICE_API_KEY  <secret>   Shared password so random callers cannot create
                                    rooms. Sent by the backend as the X-API-Token
                                    header on every REST call.

Exposure through the reverse proxy (Apache example):

    # GO voice service (HTTP + WebSocket) — rooms, voice, screenshare
    RewriteCond %{HTTP:Upgrade} =websocket [NC]
    RewriteRule ^/voice-ws/(.*) ws://localhost:8082/$1 [P,L]
    ProxyPass        /voice-ws  http://localhost:8082
    ProxyPassReverse /voice-ws  http://localhost:8082

============================================
SECTION 2: REST API (backend <-> voice service)
============================================

All REST calls from the backend authenticate with:

    X-API-Token: <SQRLL_VOICE_API_KEY>

--------------------------------------------
2.1 POST /api/rooms/create
--------------------------------------------

Create a voice room, or return the existing room.

    Headers:
        Content-Type: application/json
        X-API-Token:  <SQRLL_VOICE_API_KEY>

    Body:
        {
            "RoomId": "<room name>",
            "Token":  "<room access token>"
        }

    Responses:
        201 Created
            Brand new room was created.
            Body: {"created": true}

        200 OK
            Room already existed with the SAME token (harmless).

        409 Conflict
            Room already existed with a DIFFERENT token (token drift).
            The backend treats this as "room exists, different token" rather
            than a hard error (see ERoomCreateStatus::AlreadyExistsDifferentToken).

--------------------------------------------
2.2 GET /api/rooms/check
--------------------------------------------

Check whether a room already exists.

    Headers:
        X-API-Token: <SQRLL_VOICE_API_KEY>

    Query:
        room  Room name to check

    Responses:
        200 OK
            Body: {"exists": true|false}

--------------------------------------------
2.3 Availability / fail-fast behaviour
--------------------------------------------

The backend treats the voice service as an optional dependency and never
crashes when it is down or misconfigured:

- Every REST call is bounded by a 3-second timeout.
- A connection failure / timeout yields HTTP status 0, which the backend maps
  to `ERoomCreateStatus::Failed` (create) or `ERoomExistenceStatus::Unknown`
  (check). Neither is an error path that can throw or crash.
- If `SQRLL_VOICE_API_KEY` / `SQRLL_VOICE_ADDRESS` are unset, room calls fail
  fast (no HTTP round-trip).
- A global circuit breaker (see `VoiceServiceCircuitBreakerThreshold` /
  `VoiceServiceCircuitBreakerCooldownSeconds` in BackendSettings.ini) opens
  after N consecutive failures and short-circuits further room checks / creates
  until it half-opens after a cooldown. This prevents a down service from
  stalling the WebSocket event loop (voice joins are served synchronously).

When the service is unreachable, the backend responds to the client with
`"voice room unavailable"` (create failure) or hands back an empty room token
(check returned Unknown) rather than crashing.

============================================
SECTION 3: WEBSOCKET API (client <-> voice service)
============================================

The browser/Tauri client opens a WebSocket directly to the voice service
through the proxy (the `wss://comm.sqrll.net/voice-ws` endpoint). The
connection is authorized by a per-room token, which the backend issues and
returns to the client in the `data_stream_channel` / `server_join_voice`
responses (see docs/servers_api.md, Section 2.1.3 / 2.2.1).

--------------------------------------------
3.1 WebSocket handshake parameters (query string or headers)
--------------------------------------------

Every WebSocket endpoint below accepts its handshake parameters in two ways:
the traditional query string (backward compatible) or an HTTP header.
The header is checked first and wins if both are present.

Headers are the private option: `token` is a credential and `userid` is PII,
and passing them in the URL leaks them into reverse-proxy access logs, browser
history, and the `Referer` header. Passing them as headers avoids that.

| Query param | HTTP header        | Used by         |
|-------------|--------------------|-----------------|
| `room`      | `X-Room-Id`        | audio, screen   |
| `userid`    | `X-User-Id`        | audio, screen   |
| `token`     | `X-Room-Token`     | audio, screen   |
| `role`      | `X-Role`           | screen          |
| `target`    | `X-Target-User-Id` | screen (viewer) |

> **Browser clients note:** the native browser `WebSocket` API **cannot** set
> custom headers, so browser clients must keep using the query string (or pass
> the token as the first WebSocket message after connecting). The header
> alternative is intended for server-side / non-browser clients — e.g. your
> backend proxying a connection for a user.
