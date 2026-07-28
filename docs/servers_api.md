# SquirrelCommunicator — Servers (Rooms) API Reference

> **Last updated:** 2026-07-28
> **Backend version:** C++ / uWebSockets / MariaDB
> **Naming convention:** "room" on the wire (frontend language), "server" in the C++ codebase.
> **All IDs are 64-bit unsigned integers serialized as strings on the wire.**

---

## Table of Contents

1. [Connection](#1-connection)
2. [Wire Protocol Overview](#2-wire-protocol-overview)
3. [Client → Server Messages](#3-client--server-messages)
   - [3.1 create_room](#31-create_room) — Create a new server
   - [3.2 join_room](#32-join_room) — Join an existing server
   - [3.3 leave_room](#33-leave_room) — Leave a server
   - [3.4 room_message](#34-room_message) — Send a text message to a channel
   - [3.5 create_channel](#35-create_channel) — Create a channel inside a server
   - [3.6 room_invite](#36-room_invite) — Invite a user directly
   - [3.7 server_join_voice](#37-server_join_voice) — Join a voice channel
   - [3.8 server_leave_voice](#38-server_leave_voice) — Leave a voice channel
   - [3.9 get_server_list](#39-get_server_list) — List all servers you belong to
   - [3.10 get_server_messages](#310-get_server_messages) — Load message history (paginated)
   - [3.11 server_create_invite](#311-server_create_invite) — Generate an invite code
   - [3.12 server_join_invite](#312-server_join_invite) — Join a server via invite code
4. [Server → Client Messages](#4-server--client-messages)
   - [4.1 room_created](#41-room_created) — Full server data (creation or join confirmation)
   - [4.2 room_user_joined](#42-room_user_joined) — Another user joined
   - [4.3 room_user_left](#43-room_user_left) — Another user left
   - [4.4 room_message](#44-room_message) — Incoming text message
   - [4.5 room_channel_created](#45-room_channel_created) — New channel created
   - [4.6 room_member_status](#46-room_member_status) — Member went online/offline/away
   - [4.7 server_voice_joined / server_voice_left](#47-server_voice_joined--server_voice_left) — Voice presence
   - [4.8 server_list](#48-server_list) — Response: list of servers
   - [4.9 server_messages](#49-server_messages) — Response: paginated message history
   - [4.10 server_invite_created](#410-server_invite_created) — Response: generated invite code
   - [4.11 server_joined](#411-server_joined) — Response: full server data from invite join
5. [Error Responses](#5-error-responses)
6. [Voice Integration — Frontend Guide](#6-voice-integration--frontend-guide)
   - [6.1 Architecture Overview](#61-architecture-overview)
   - [6.2 Frontend Integration: Step-by-Step](#62-frontend-integration-step-by-step)
   - [6.3 WebSocket Audio Stream Protocol](#63-websocket-audio-stream-protocol)
   - [6.4 Token Lifecycle & Security](#64-token-lifecycle--security)
   - [6.5 Go Voice Service REST API Reference](#65-go-voice-service-rest-api-reference)
   - [6.6 Room Naming Convention](#66-room-naming-convention)
   - [6.7 Auto-Disconnect & Ghost Users](#67-auto-disconnect--ghost-users)
   - [6.8 Voice Presence in Room Data](#68-voice-presence-in-room-data)
   - [6.9 Error Handling for Voice](#69-error-handling-for-voice)
   - [6.10 Complete Voice Lifecycle Summary](#610-complete-voice-lifecycle-summary)
7. [Data Models](#7-data-models)
8. [Rate Limiting & Abuse Protection](#8-rate-limiting--abuse-protection)
9. [Database Schema](#9-database-schema)
10. [Deprecated REST Endpoints](#10-deprecated-rest-endpoints)

---

## 1. Connection

| Property | Value |
|---|---|
| **Protocol** | WebSocket (WSS) |
| **Endpoint** | `wss://comm.sqrll.net/ws/api/v1/ws` |
| **Authentication** | `auth_token` cookie (read during WebSocket upgrade handshake) |
| **Session lifetime** | Single persistent connection per user |
| **Idle timeout** | 5 minutes (pings sent automatically) |
| **Max payload** | 16 KB per frame |
| **Compression** | Disabled |

### How auth works

1. Client opens WebSocket to `wss://comm.sqrll.net/ws/api/v1/ws`
2. Browser sends `Cookie: auth_token=<token>` in the upgrade handshake
3. Server validates token against `users` table (no extra DB lookup after upgrade)
4. `UserId` is stored in the WebSocket session data (`FWebSocketSessionData.UserId`)
5. Every subsequent message reads UserId from session → **zero auth overhead per message**

If the token is invalid, the upgrade is rejected with `401 Unauthorized`.

---

## 2. Wire Protocol Overview

Every message (both directions) is a **JSON object** with this structure:

```json
{
  "section": "rooms",
  "type": "<message_type>",
  "data": { ... }
}
```

| Field | Type | Required | Description |
|---|---|---|---|
| `section` | string | **yes** | Must be `"rooms"` for servers feature. Other value: `"priv"` for private messages. |
| `type` | string | **yes** | One of the message types listed below. Strings matched via compile-time FNV-1a hash → O(1) dispatch. |
| `data` | object | **yes** | Payload specific to each message type (may be empty `{}`). |

### Dispatch flow (server-side)

```
FSocket::OnMessageReceived_TEXT()
  ├── Parse JSON
  ├── Read "section" → hash → ESocketMessageSection::Rooms
  └── ServersSocketData.PrimarySwitch()
      ├── Read "type" → hash → ESocketMessageServersType::<X>
      ├── Extract UserId from WebSocket session data (pointer deref)
      ├── [optional] Rate-limit check
      ├── [optional] Membership check (Server->HasMember)
      └── Handle → DB persist → Broadcast to room members
```

---

## 3. Client → Server Messages

### 3.1 create_room

Create a new server. You become the owner. Two default channels are auto-created: `"general"` (text) and `"General"` (voice).

```json
{
  "section": "rooms",
  "type": "create_room",
  "data": {
    "room_name": "My Awesome Server"
  }
}
```

| Field | Type | Required | Constraints |
|---|---|---|---|
| `room_name` | string | **yes** | 1–128 characters, non-empty |

**Rate limit:** Per-IP counter. Default ~10 creations/minute (configurable in `FRateLimiter`).

**Response:** [`room_created`](#41-room_created) sent to the creator.

**Error conditions:** Empty name, rate-limited, DB write failure.

---

### 3.2 join_room

Join a server you were invited to (or rejoin after leaving).

```json
{
  "section": "rooms",
  "type": "join_room",
  "data": {
    "room_id": "42"
  }
}
```

| Field | Type | Required | Description |
|---|---|---|---|
| `room_id` | string | **yes** | Server ID (64-bit, string-encoded) |

**Side effects:** Broadcasts [`room_user_joined`](#42-room_user_joined) to all other members.

**Response:** [`room_created`](#41-room_created) sent to the joining user (full room data).

---

### 3.3 leave_room

Leave a server permanently. You will no longer receive messages or appear as a member.

```json
{
  "section": "rooms",
  "type": "leave_room",
  "data": {
    "room_id": "42"
  }
}
```

| Field | Type | Required | Description |
|---|---|---|---|
| `room_id` | string | **yes** | Server ID |

**Side effects:** Broadcasts [`room_user_left`](#43-room_user_left) to all other members. DB member row is deleted.

**Response:** Confirmation with `"status": "left"` sent to the leaver.

---

### 3.4 room_message

Send a text message to a server channel. Message is persisted to DB and broadcast to all server members.

```json
{
  "section": "rooms",
  "type": "room_message",
  "data": {
    "room_id": "42",
    "channel_id": "5",
    "content": "Hello everyone!"
  }
}
```

| Field | Type | Required | Constraints |
|---|---|---|---|
| `room_id` | string | **yes** | Must be a member of this server |
| `channel_id` | string | **yes** | Channel must exist and be type `"text"` |
| `content` | string | **yes** | Max size: engine's `MaxMessageSize` (default 2000 chars) |

**Membership check:** `ServersManager->IsUserInServer(RoomId, UserId)` → O(1) `unordered_map::find` under `shared_lock`.

**Response:** Broadcast as [`room_message`](#44-room_message) to **all** members (including sender, for frontend consistency).

**Error conditions:** Not authenticated, not a member, message too large, DB write failure.

---

### 3.5 create_channel

Create a new channel inside a server.

```json
{
  "section": "rooms",
  "type": "create_channel",
  "data": {
    "room_id": "42",
    "channel_name": "memes",
    "channel_type": "text"
  }
}
```

| Field | Type | Required | Values |
|---|---|---|---|
| `room_id` | string | **yes** | Server ID |
| `channel_name` | string | **yes** | 1–128 characters |
| `channel_type` | string | **yes** | `"text"` or `"voice"` |

**Response:** Broadcast [`room_channel_created`](#45-room_channel_created) to all server members.

**Note:** Only members can create channels. All members see the broadcast.

---

### 3.6 room_invite

Send a direct invite from one member to another user (by user ID). Unlike invite codes, this is a direct push notification to the target.

```json
{
  "section": "rooms",
  "type": "room_invite",
  "data": {
    "room_id": "42",
    "user_id": "99"
  }
}
```

| Field | Type | Required | Description |
|---|---|---|---|
| `room_id` | string | **yes** | Server you're inviting to |
| `user_id` | string | **yes** | Target user's ID |

**Response:**
- Confirmation sent to the inviter: `{ "type": "room_invite", "data": { "status": "sent", "room_id": "42", "user_id": "99" } }`
- Notification sent to the target user via `SendToUser()` — delivered through uWS pub/sub topic `"user_<id>"`

**Note:** `SendToUser` checks if the target is online. If offline, the notification is silently dropped (no offline queue).

---

### 3.7 server_join_voice

Join a voice channel. This is the **single entry point** for voice — it triggers everything the frontend needs.

```json
{
  "section": "rooms",
  "type": "server_join_voice",
  "data": {
    "room_id": "42",
    "channel_id": "6"
  }
}
```

| Field | Type | Required | Description |
|---|---|---|---|
| `room_id` | string | **yes** | Server ID |
| `channel_id` | string | **yes** | Channel ID (must be type `"voice"`) |

**Backend behavior (all automatic):**
1. Verify membership + channel exists and is voice type
2. Add user to `FServerChannel::ConnectedUsers` in-memory
3. Construct voice room name: `"Server_<room_id>_<channel_id>"` (e.g. `"Server_42_6"`)
4. Check Go voice service via `CheckRoom()` → create if not exists (idempotent via `POST /api/rooms/create`)
5. Fetch room token from in-memory cache (or generate new 64-char base64 token)

**Response (direct, to the joining user only):**

```json
{
  "type": "server_join_voice",
  "section": "rooms",
  "data": {
    "name": "Server_42_6",
    "token": "dGhpcyBpcyBhIDY0LWNoYXJhY3RlciBiYXNlNjQgdG9rZW4gZm9yIHZvaWNlIHJvb20gYWNjZXNz...",
    "user_name": "Alice"
  }
}
```

| Field | Type | Description |
|---|---|---|
| `name` | string | Voice room name. Use this in the Go voice service WebSocket URL (`?room=` param). |
| `token` | string | 64-character base64 room access token. Use this in the Go voice service WebSocket URL (`?token=` param). |
| `user_name` | string | Your display name. Frontend usually already knows this, provided for convenience. |

**Broadcast:** [`server_voice_joined`](#47-server_voice_joined--server_voice_left) sent to other server members.

**Next step for frontend:** See [§6.2 — Frontend Integration](#62-frontend-integration-step-by-step) for the complete flow after receiving this response.

**Error conditions:** Not authenticated, not a member, channel not found, channel is not voice type.

---

### 3.8 server_leave_voice

Leave a voice channel. You should also close the Go voice WebSocket after receiving confirmation.

```json
{
  "section": "rooms",
  "type": "server_leave_voice",
  "data": {
    "room_id": "42",
    "channel_id": "6"
  }
}
```

**Response (to the leaver):**
```json
{
  "type": "server_leave_voice",
  "section": "rooms",
  "data": {
    "status": "disconnected"
  }
}
```

**Broadcast:** [`server_voice_left`](#47-server_voice_joined--server_voice_left) to other members.

**Frontend action:** After receiving `"status": "disconnected"`, close the Go voice WebSocket connection.

---

### 3.9 get_server_list

Fetch all servers you are a member of. Replaces the deprecated `GET /api/v1/rooms/list`.

```json
{
  "section": "rooms",
  "type": "get_server_list",
  "data": {}
}
```

`data` may be empty — no fields required. Authentication is implicit (from WebSocket session).

**Response:** [`server_list`](#48-server_list)

**Backend flow:**
1. `GetUserServerIds(UserId)` — lightweight `SELECT server_id FROM server_members WHERE user_id = ?`
2. For each ID: `GetServerById()` — cache hit or DB download
3. `BuildRoomDataJson()` — serialize members + channels (with `connected_users` for voice channels)

**Performance:** O(N) where N = number of servers you belong to. Each server is loaded on demand (lazy).

---

### 3.10 get_server_messages

Load paginated message history for a channel. Replaces `GET /api/v1/rooms/:id/messages?before=X&limit=Y`.

```json
{
  "section": "rooms",
  "type": "get_server_messages",
  "data": {
    "room_id": "42",
    "channel_id": "5",
    "before": "1722001234567890123",
    "limit": "50"
  }
}
```

| Field | Type | Required | Default | Description |
|---|---|---|---|---|
| `room_id` | string | **yes** | — | Server ID (must be a member) |
| `channel_id` | string | **yes** | — | Channel ID |
| `before` | string | no | `"0"` | Timestamp in **nanoseconds since epoch**. Messages older than this. `0` = newest messages. |
| `limit` | string | no | `"50"` | Number of messages to return. Clamped to **1–100**. |

**Response:** [`server_messages`](#49-server_messages)

**Pagination pattern:**
1. First call: `before: "0"`, `limit: "50"` → get newest 50 messages
2. If `has_more: true`, use oldest message's `timestamp` as `before` in next call
3. Repeat until `has_more: false`

**Storage:** Messages stored in-memory as `vector<FServerMessage>` sorted by `MessageId` **descending** (newest first). Timestamp comparison done via `std::stoull` on the string format.

**Note on message ordering:** DB query is always `ORDER BY id DESC`. The in-memory cache mirrors this. Timestamp filter applies `created_at < :before`.

---

### 3.11 server_create_invite

Generate a shareable invite code for a server. Replaces `POST /api/v1/rooms/:id/invite`.

```json
{
  "section": "rooms",
  "type": "server_create_invite",
  "data": {
    "room_id": "42"
  }
}
```

| Field | Type | Required | Description |
|---|---|---|---|
| `room_id` | string | **yes** | Server ID (must be a member) |

**Response:** [`server_invite_created`](#410-server_invite_created)

**Invite code format:** 10 random alphanumeric characters (a-z, A-Z, 0-9). Stored in `server_invites` table with `created_at` timestamp. Currently **no expiration** (`expires_at` is NULL).

---

### 3.12 server_join_invite

Join a server using an invite code. Replaces `POST /api/v1/rooms/join`.

```json
{
  "section": "rooms",
  "type": "server_join_invite",
  "data": {
    "invite_code": "aB3xK9mQz7"
  }
}
```

| Field | Type | Required | Description |
|---|---|---|---|
| `invite_code` | string | **yes** | 10-character code (case-sensitive) |

**Response:** [`server_joined`](#411-server_joined) — full server data if successful.

**Backend flow:**
1. Lookup `invite_code` → `server_id` (in-memory cache first, then DB)
2. `AddUserToServer()` — INSERT into `server_members`, add to in-memory `Members` map
3. Broadcast `room_user_joined` to other members
4. Return full room data to the joiner

**Error:** `"invalid or expired invite code"` if code not found.

---

## 4. Server → Client Messages

These are messages the server sends to clients (responses and broadcasts).

### 4.1 room_created

Sent to a user after they **create** a server or **join** an existing one. Contains the full server state.

```json
{
  "type": "room_created",
  "section": "rooms",
  "data": {
    "room_id": "42",
    "room_name": "My Server",
    "room_token": "a1b2c3...",
    "owner_id": "7",
    "created_at": "1722000000000000000",
    "members": [
      { "user_id": "7", "user_name": "Alice", "status": "online" }
    ],
    "channels": [
      {
        "channel_id": "5",
        "channel_name": "general",
        "channel_type": "text"
      },
      {
        "channel_id": "6",
        "channel_name": "General",
        "channel_type": "voice",
        "connected_users": [
          { "user_id": "7", "user_name": "Alice" }
        ]
      }
    ]
  }
}
```

| Field | Type | Description |
|---|---|---|
| `room_id` | string | Server ID |
| `room_name` | string | Display name |
| `room_token` | string | 48-char random token (used for voice service auth) |
| `owner_id` | string | Creator's user ID |
| `created_at` | string | Unix epoch **nanoseconds** as string |
| `members[]` | array | List of all members with status |
| `members[].user_id` | string | Member's user ID |
| `members[].user_name` | string | Member's display name (from `users` table JOIN) |
| `members[].status` | string | `"online"`, `"offline"`, or `"away"` |
| `channels[]` | array | List of all channels |
| `channels[].channel_id` | string | Channel ID |
| `channels[].channel_name` | string | Channel display name |
| `channels[].channel_type` | string | `"text"` or `"voice"` |
| `channels[].connected_users` | array | **(voice only)** Users currently in this voice channel. Omitted for text channels or when empty. |
| `channels[].connected_users[].user_id` | string | Connected user's ID |
| `channels[].connected_users[].user_name` | string | Connected user's display name |

---

### 4.2 room_user_joined

Broadcast to all server members (except the joiner) when someone joins.

```json
{
  "type": "room_user_joined",
  "section": "rooms",
  "data": {
    "room_id": "42",
    "user_id": "99",
    "user_name": "Bob"
  }
}
```

---

### 4.3 room_user_left

Broadcast to all server members when someone leaves. The leaving user gets a separate confirmation message.

```json
{
  "type": "room_user_left",
  "section": "rooms",
  "data": {
    "room_id": "42",
    "user_id": "99"
  }
}
```

---

### 4.4 room_message

Broadcast to **all** server members when a text message is sent. Includes the sender for frontend UI consistency (so the UI doesn't need to optimistically insert).

```json
{
  "type": "room_message",
  "section": "rooms",
  "data": {
    "room_id": "42",
    "channel_id": "5",
    "message_id": "1001",
    "sender_id": "7",
    "sender_name": "Alice",
    "content": "Hello everyone!",
    "timestamp": "1722001234567890123"
  }
}
```

| Field | Type | Description |
|---|---|---|
| `message_id` | string | Unique message ID (auto-increment) |
| `sender_name` | string | Resolved from `users` table at message creation time |
| `timestamp` | string | Unix epoch **nanoseconds** at time of server-side processing |

---

### 4.5 room_channel_created

Broadcast to all server members when a new channel is created.

```json
{
  "type": "room_channel_created",
  "section": "rooms",
  "data": {
    "room_id": "42",
    "channel_id": "8",
    "channel_name": "memes",
    "channel_type": "text"
  }
}
```

---

### 4.6 room_member_status

Broadcast to all members of **every server** the user belongs to when they connect or disconnect. Triggered by `OnClientConnected()` and `OnClientDisconnected()` in `Socket.cpp`.

```json
{
  "type": "room_member_status",
  "section": "rooms",
  "data": {
    "room_id": "42",
    "user_id": "7",
    "user_name": "Alice",
    "status": "offline"
  }
}
```

| Field | Type | Values |
|---|---|---|
| `status` | string | `"online"`, `"offline"`, `"away"` |

**Note:** This is not sent to the user themselves (excluded via `BroadcastToServerMembers` with `ExcludeUserId`).

---

### 4.7 server_voice_joined / server_voice_left

Voice presence events. Sent to all server members (except the user themselves) when someone joins or leaves a voice channel.

**To other members (broadcast when someone joins voice):**

```json
{
  "type": "server_voice_joined",
  "section": "rooms",
  "data": {
    "room_id": "42",
    "channel_id": "6",
    "user_id": "7",
    "user_name": "Alice"
  }
}
```

**To other members (broadcast when someone leaves voice):**

```json
{
  "type": "server_voice_left",
  "section": "rooms",
  "data": {
    "room_id": "42",
    "channel_id": "6",
    "user_id": "7",
    "user_name": "Alice"
  }
}
```

**Frontend action on `server_voice_joined`:** Update your voice channel UI to show this user as connected (add avatar indicator, etc.).

**Frontend action on `server_voice_left`:** Remove the user's voice indicator from the UI.

**Naming convention rationale:** `server_voice_joined` / `server_voice_left` use past-tense to indicate they are **events** (something that happened), distinct from the imperative C→S messages `server_join_voice` / `server_leave_voice`. The word "user" was removed — the `user_id` + `user_name` fields in the data payload already identify who.

**Note:** These broadcasts are also sent automatically when a user disconnects from WebSocket (see [§6.7](#67-auto-disconnect--ghost-users)).

---

### 4.8 server_list

Response to [`get_server_list`](#39-get_server_list). Contains all servers the user is a member of.

```json
{
  "type": "server_list",
  "section": "rooms",
  "data": {
    "rooms": [ ... ]
  }
}
```

Each element in `rooms[]` has the same format as [`room_created.data`](#41-room_created) (including `connected_users` for voice channels).

---

### 4.9 server_messages

Response to [`get_server_messages`](#310-get_server_messages). Paginated message history.

```json
{
  "type": "server_messages",
  "section": "rooms",
  "data": {
    "messages": [
      {
        "message_id": "1001",
        "channel_id": "5",
        "sender_id": "7",
        "sender_name": "Alice",
        "content": "Hello!",
        "timestamp": "1722001234567890123"
      }
    ],
    "has_more": true
  }
}
```

| Field | Type | Description |
|---|---|---|
| `has_more` | boolean | `true` if there are older messages. Use the oldest message's `timestamp` as the next `before` value. |

**Note:** `has_more` is determined by `messages.size() >= limit` — it's a simpler heuristic than making an extra COUNT query.

---

### 4.10 server_invite_created

Response to [`server_create_invite`](#311-server_create_invite).

```json
{
  "type": "server_invite_created",
  "section": "rooms",
  "data": {
    "invite_code": "aB3xK9mQz7",
    "invite_url": "https://comm.sqrll.net/invite/aB3xK9mQz7",
    "expires_at": null
  }
}
```

| Field | Type | Description |
|---|---|---|
| `invite_code` | string | 10-character code |
| `invite_url` | string | Shareable URL |
| `expires_at` | null | Not implemented yet — invites never expire |

---

### 4.11 server_joined

Response to [`server_join_invite`](#312-server_join_invite). Same format as [`room_created`](#41-room_created) but with type `"server_joined"`.

```json
{
  "type": "server_joined",
  "section": "rooms",
  "data": {
    "room_id": "42",
    "room_name": "Cool Server",
    ...
  }
}
```

Additionally, a [`room_user_joined`](#42-room_user_joined) broadcast is sent to other members.

---

## 5. Error Responses

When a message fails validation, the server sends an error frame with a short message:

```json
{
  "type": "error",
  "section": "rooms",
  "message": "not authenticated"
}
```

| `message` value | Meaning |
|---|---|
| `"missing type"` | Message does not contain `"type"` field |
| `"missing data"` | Message does not contain `"data"` field |
| `"missing room_name"` | `create_room` without `room_name` |
| `"empty room_name"` | `room_name` is empty string |
| `"not authenticated"` | UserId is 0 (WebSocket session has no valid auth) |
| `"service abuse"` | Rate-limited (too many requests) |
| `"failed to create room"` | DB insert failed |
| `"failed to join room"` | DB insert or server not found |
| `"not a member of this room"` | Membership check failed |
| `"message too large"` | Content exceeds `MaxMessageSize` |
| `"failed to save message"` | DB insert failed |
| `"invalid voice channel"` | Channel not found or not voice type |
| `"invalid or expired invite code"` | Invite code not found in cache or DB |
| `"Unknown message type"` | `"type"` string didn't match any known handler |

---

## 6. Voice Integration — Frontend Guide

Voice/audio streaming uses a **separate Go service** for real-time audio relay. The C++ backend acts as a **broker**: it manages who is in which voice channel, creates rooms on the Go service, manages access tokens, and broadcasts presence events. The frontend connects **directly to the Go service** for audio streaming.

This section is a complete frontend integration guide. Every step, every URL, every data format is documented below.

---

### 6.1 Architecture Overview

```
┌──────────────────────────────────────────────────────────────────────┐
│                        VOICE ARCHITECTURE                            │
│                                                                      │
│  ┌──────────┐                                                        │
│  │ FRONTEND │  Your browser/app                                      │
│  │          │                                                        │
│  │  Has TWO WebSocket connections:                                   │
│  │  ① Main WS ──── wss://comm.sqrll.net/ws/api/v1/ws                │
│  │     │  (auth_token cookie, JSON text frames for commands/events)  │
│  │     │                                                             │
│  │  ② Voice WS ─── wss://comm.sqrll.net/voice-ws/api/rooms/stream   │
│  │        (query params: room name + token, binary audio frames)     │
│  └──┬───────┬───────────────────────────────────────────────────────┘
│     │       │
│     │ ①     │ ②
│     ▼       ▼
│  ┌──────────┐    POST /api/rooms/create    ┌──────────────┐
│  │ C++ Back │◄────────────────────────────►│ Go Voice     │
│  │  (uWS)   │   GET /api/rooms/check       │ Service      │
│  │          │                               │              │
│  │ - Auth   │                               │ - WebRTC     │
│  │ - Member │                               │ - Audio relay│
│  │   checks │                               │ - WebM/Opus  │
│  │ - Token  │                               │   streaming  │
│  │   mgmt   │                               │              │
│  │ - Presence│                              └──────────────┘
│  │   broadcast│
│  └──────────┘
```

**Key points for frontend developers:**

1. **You always have exactly one main WebSocket** to the C++ backend. This is where auth, text messages, and voice commands flow.
2. **You open a second WebSocket** to the Go voice service whenever you join a voice channel. Close it when you leave.
3. The C++ backend tells you the Go room name and token via the `server_join_voice` response.
4. Audio data flows on the voice WebSocket as **binary frames**, not JSON.
5. Voice presence (`server_voice_joined`/`server_voice_left`) comes through the main WebSocket, not the voice one.

---

### 6.2 Frontend Integration: Step-by-Step

#### Step 1: User clicks "Join Voice" in a server channel

Your frontend already has the server and channel data from [`room_created`](#41-room_created) or [`server_list`](#48-server_list). Identify the voice channel by `channel_type: "voice"`.

#### Step 2: Send `server_join_voice` on the main WebSocket

```json
{
  "section": "rooms",
  "type": "server_join_voice",
  "data": {
    "room_id": "42",
    "channel_id": "6"
  }
}
```

The C++ backend will:
- Verify you are a member of server 42
- Verify channel 6 exists and is `"voice"` type
- Add you to the in-memory voice presence list
- Ensure a room exists on the Go voice service (creating it if needed)
- Generate/retrieve a room access token

#### Step 3: Receive the `server_join_voice` response

```json
{
  "type": "server_join_voice",
  "section": "rooms",
  "data": {
    "name": "Server_42_6",
    "token": "dGhpcyBpcyBhIDY0LWNoYXJhY3RlciBiYXNlNjQgdG9rZW4gZm9yIHZvaWNlIHJvb20gYWNjZXNz...",
    "user_name": "Alice"
  }
}
```

| Field | Use it for... |
|---|---|
| `name` | The Go voice room name. Pass as `?room=` query param. |
| `token` | Room access token. Pass as `?token=` query param. |
| `user_name` | Your display name (informational — already known by frontend). |

#### Step 4: Open a WebSocket to the Go voice service

Construct the URL using the `name` and `token` from step 3:

```
wss://comm.sqrll.net/voice-ws/api/rooms/stream?room=Server_42_6&token=dGhpcyBpcyBhIDY0LWNoYXJhY3RlciBiYXNlNjQgdG9rZW4gZm9yIHZvaWNlIHJvb20gYWNjZXNz...&userid=7
```

| Query Param | Source | Description |
|---|---|---|
| `room` | `data.name` from response | Voice room name (e.g. `Server_42_6`) |
| `token` | `data.token` from response | 64-char base64 access token |
| `userid` | Your own user ID (from `initial_client_data` or auth state) | Numeric user ID, used by Go service to tag audio frames |

> **Important:** The `token` is a **per-room password**. It is generated once when the first user joins a voice channel and cached in the C++ backend. All subsequent users joining the same voice channel receive the **same token**. The token persists until the C++ server restarts (at which point a new one is generated). Do NOT hardcode or store tokens permanently.

#### Step 5: Start streaming audio

Once the voice WebSocket is connected, you can send and receive **binary audio frames**. See [§6.3](#63-websocket-audio-stream-protocol) for the exact binary format.

#### Step 6: While connected, watch for voice presence events

On the **main WebSocket**, you will receive:

```json
// When someone else joins voice:
{ "type": "server_voice_joined", "section": "rooms", "data": { "room_id": "42", "channel_id": "6", "user_id": "99", "user_name": "Bob" } }

// When someone else leaves voice:
{ "type": "server_voice_left", "section": "rooms", "data": { "room_id": "42", "channel_id": "6", "user_id": "99", "user_name": "Bob" } }
```

**Frontend action:** Update the voice channel UI to show/hide user indicators accordingly.

> **Note:** You will NOT receive a `server_voice_joined` for yourself. The `server_join_voice` response already confirms you joined. Presence broadcasts always exclude the triggering user.

#### Step 7: To leave voice — send `server_leave_voice`

```json
{
  "section": "rooms",
  "type": "server_leave_voice",
  "data": {
    "room_id": "42",
    "channel_id": "6"
  }
}
```

#### Step 8: Receive leave confirmation, then close voice WebSocket

```json
{
  "type": "server_leave_voice",
  "section": "rooms",
  "data": {
    "status": "disconnected"
  }
}
```

After receiving this, call `.close()` on the Go voice WebSocket. The C++ backend has already removed you from the voice presence list; others will receive `server_voice_left`.

#### Pseudocode Summary

```javascript
// JOIN VOICE
function joinVoice(serverId, channelId) {
  mainWs.send(JSON.stringify({
    section: "rooms",
    type: "server_join_voice",
    data: { room_id: serverId, channel_id: channelId }
  }));
}

// Handle the response on mainWs
mainWs.onmessage = (event) => {
  const msg = JSON.parse(event.data);
  if (msg.type === "server_join_voice") {
    const { name, token } = msg.data;
    const myUserId = getMyUserId(); // from app auth state
    openVoiceWebSocket(name, token, myUserId);
  }
  if (msg.type === "server_voice_joined") {
    addVoiceIndicator(msg.data.user_id, msg.data.user_name);
  }
  if (msg.type === "server_voice_left") {
    removeVoiceIndicator(msg.data.user_id);
  }
};

function openVoiceWebSocket(roomName, token, myUserId) {
  const url = `wss://comm.sqrll.net/voice-ws/api/rooms/stream?room=${encodeURIComponent(roomName)}&token=${encodeURIComponent(token)}&userid=${myUserId}`;
  voiceWs = new WebSocket(url);
  voiceWs.binaryType = "arraybuffer";

  voiceWs.onmessage = (event) => {
    // event.data is an ArrayBuffer containing a binary audio frame
    // Format: [userIdLen:1 byte][userId:UTF-8 bytes][WebM/Opus audio data]
    playAudioFrame(event.data);
  };

  voiceWs.onerror = (err) => {
    console.error("Voice WebSocket error:", err);
    // The C++ backend still has you in the voice channel.
    // Send server_leave_voice to clean up, or the backend will auto-cleanup
    // if your main WebSocket also drops.
  };
}

// LEAVE VOICE
function leaveVoice(serverId, channelId) {
  mainWs.send(JSON.stringify({
    section: "rooms",
    type: "server_leave_voice",
    data: { room_id: serverId, channel_id: channelId }
  }));
  // Wait for confirmation, then:
  // voiceWs.close();
}
```

---

### 6.3 WebSocket Audio Stream Protocol

The Go voice service WebSocket streams **binary frames** (not JSON) in both directions.

#### Connection URL

```
wss://comm.sqrll.net/voice-ws/api/rooms/stream?room=<roomName>&token=<token>&userid=<userId>
```

| Param | Example | Description |
|---|---|---|
| `room` | `Server_42_6` | Voice room name. URL-encode if it contains special characters (the format `Server_<id>_<id>` does not, but encode defensively). |
| `token` | `dGhpcyBpcyBh...` | 64-char base64 room token from `server_join_voice` response. URL-encode it. |
| `userid` | `7` | Your numeric user ID (integer, not string). The Go service uses this to tag your audio frames so other peers know who is speaking. |

#### Binary Frame Format (Outbound: Frontend → Go)

When you send audio, each WebSocket binary frame should be:

```
┌──────────────────┬───────────────────────────────┐
│ userIdLen        │ userId                        │
│ 1 byte (uint8)   │ userIdLen bytes (UTF-8)       │
├──────────────────┴───────────────────────────────┤
│ WebM/Opus audio data                             │
│ (rest of the frame)                              │
└──────────────────────────────────────────────────┘
```

| Segment | Size | Description |
|---|---|---|
| `userIdLen` | 1 byte | Length of the userId string in bytes. For user ID `"7"`, this is `0x01`. For user ID `"123"`, this is `0x03`. |
| `userId` | `userIdLen` bytes | Your numeric user ID as a UTF-8 string (e.g., `"7"` → `0x37`). This is NOT a binary integer — it's the ASCII representation. |
| `audio data` | remaining bytes | WebM container with Opus-encoded audio. This is standard browser `MediaRecorder` output with MIME type `audio/webm;codecs=opus`. |

**Example (pseudocode for encoding):**

```javascript
// Using MediaRecorder API in the browser
const stream = await navigator.mediaDevices.getUserMedia({ audio: true });
const recorder = new MediaRecorder(stream, { mimeType: 'audio/webm;codecs=opus' });

recorder.ondataavailable = (event) => {
  if (event.data.size > 0 && voiceWs.readyState === WebSocket.OPEN) {
    const userId = String(myUserId);
    const userIdBytes = new TextEncoder().encode(userId);
    const userIdLen = userIdBytes.length;

    // Build frame: [1 byte len][userId bytes][audio blob bytes]
    const audioBytes = await event.data.arrayBuffer();
    const frame = new Uint8Array(1 + userIdLen + audioBytes.byteLength);
    frame[0] = userIdLen;
    frame.set(userIdBytes, 1);
    frame.set(new Uint8Array(audioBytes), 1 + userIdLen);

    voiceWs.send(frame.buffer);
  }
};

recorder.start(20); // 20ms chunks for low latency
```

#### Binary Frame Format (Inbound: Go → Frontend)

The Go service relays audio frames from other users to you. Each incoming binary frame has the **same format**:

```
[userIdLen: 1 byte][userId: variable][WebM/Opus audio data]
```

| Segment | Description |
|---|---|
| `userIdLen` | Length of the speaking user's ID string |
| `userId` | The speaking user's ID as a UTF-8 string |
| `audio data` | WebM/Opus audio from that user |

**Example (pseudocode for decoding):**

```javascript
voiceWs.onmessage = (event) => {
  const data = new Uint8Array(event.data);

  const userIdLen = data[0];
  const userIdBytes = data.slice(1, 1 + userIdLen);
  const userId = new TextDecoder().decode(userIdBytes);
  const audioData = data.slice(1 + userIdLen);

  // Route audio to the correct player for this userId
  playAudioForUser(userId, audioData.buffer);
};
```

#### Audio Codec Details

| Property | Value |
|---|---|
| **Container** | WebM (Matroska subset) |
| **Codec** | Opus |
| **Browser MIME type** | `audio/webm;codecs=opus` |
| **Sample rate** | 48 kHz (Opus default) |
| **Channels** | Mono (1 channel) |
| **Recommended chunk size** | 20 ms (browser `MediaRecorder.start(20)`) |
| **Bitrate** | Browser default (~32 kbps for mono voice) |

---

### 6.4 Token Lifecycle & Security

#### How tokens are generated

1. When the **first user** joins a voice channel (e.g., `Server_42_6`), the C++ backend generates a **64-character base64 random token**.
2. This token is sent to the Go voice service as part of the `POST /api/rooms/create` request.
3. The token is stored in an **in-memory cache** (`RoomNameToToken` map) in the C++ backend.
4. The token is used by the Go service to authenticate WebSocket connections to that room.

#### How tokens are reused

- **Subsequent users** joining the same voice channel receive the **same cached token**.
- The C++ backend checks `RoomNameToToken` → if found, returns it. If not, generates a new one.
- The Go service accepts connections using the token it received during room creation.

#### Token lifetime

| Event | What happens to the token |
|---|---|
| First user joins voice | Token generated, cached in C++ and sent to Go |
| Additional users join | Same token reused from cache |
| All users leave | Token remains cached (room stays on Go service) |
| C++ server restarts | Cache is lost. New token generated on next join. **Old tokens are invalidated.** |
| Go service restarts | Rooms are lost. C++ backend detects this via `CheckRoom()` and recreates with **same cached token**. |

#### Security considerations for frontend

- Tokens are **not JWTs** — they are opaque random strings. Do not parse them.
- Tokens are **room-scoped** — one token grants access to exactly one voice room.
- Tokens should be treated as **session-only** — never persist to localStorage or IndexedDB.
- If you receive a 401/403 from the Go voice WebSocket, re-send `server_join_voice` on the main WebSocket to get a fresh token.

---

### 6.5 Go Voice Service REST API Reference

The Go voice service exposes two REST endpoints (used internally by the C++ backend) and one WebSocket endpoint (used directly by the frontend).

All REST endpoints use `X-API-Token` header authentication. This is a **server-to-server secret** — the frontend never calls these directly.

#### POST /api/rooms/create

Create a voice room (idempotent — safe to call multiple times).

| Property | Value |
|---|---|
| **Method** | `POST` |
| **URL** | `http://<voice-service>:<port>/api/rooms/create` |
| **Auth** | `X-API-Token: <SQRLL_VOICE_API_KEY>` header |
| **Content-Type** | `application/json` |
| **Timeout** | 3 seconds |

**Request body:**

```json
{
  "RoomId": "Server_42_6",
  "Token": "dGhpcyBpcyBhIDY0LWNoYXJhY3RlciBiYXNlNjQgdG9rZW4gZm9yIHZvaWNlIHJvb20gYWNjZXNz..."
}
```

| Field | Type | Description |
|---|---|---|
| `RoomId` | string | Voice room name (e.g. `Server_42_6`) |
| `Token` | string | 64-char base64 access token for this room |

**Success response (201 Created):**

```json
{
  "created": true
}
```

**Error responses:** Non-201 status codes are treated as failures. The C++ backend logs the status and body.

#### GET /api/rooms/check

Check if a voice room exists on the Go service.

| Property | Value |
|---|---|
| **Method** | `GET` |
| **URL** | `http://<voice-service>:<port>/api/rooms/check?room=<RoomName>` |
| **Auth** | `X-API-Token: <SQRLL_VOICE_API_KEY>` header |
| **Timeout** | 3 seconds |

**Success response (200 OK):**

```json
// Room exists:
{ "exists": true }

// Room does NOT exist:
{ "exists": false }
```

#### WSS /api/rooms/stream

The WebSocket endpoint for audio streaming. This is what the frontend connects to directly.

| Property | Value |
|---|---|
| **Protocol** | WebSocket Secure (WSS) |
| **URL (production)** | `wss://comm.sqrll.net/voice-ws/api/rooms/stream` |
| **URL (local dev)** | Depends on Go service config; typically `ws://localhost:<port>/api/rooms/stream` |
| **Auth** | Query parameters (`token` and `userid`) |
| **Frame format** | Binary: `[userIdLen:1B][userId:variable][WebM/Opus audio]` |

**Query parameters:**

| Param | Required | Description |
|---|---|---|
| `room` | **yes** | Voice room name (e.g., `Server_42_6`) |
| `token` | **yes** | Room access token from `server_join_voice` response |
| `userid` | **yes** | Your numeric user ID |

**Connection lifecycle:**
- On connect: Go service validates the token against the room. If invalid → close with 4001.
- While connected: Binary frames are relayed to all other peers in the same room.
- On close: Peer is removed from the room. Go service has its own idle timeout.

---

### 6.6 Room Naming Convention

Voice rooms on the Go service follow a strict naming convention:

```
Server_<ServerId>_<ChannelId>
```

| Component | Example | Description |
|---|---|---|
| `Server` | `Server` | Literal prefix, capital S |
| `_` | `_` | Separator |
| `<ServerId>` | `42` | The server's numeric ID (from `room_id`) |
| `_` | `_` | Separator |
| `<ChannelId>` | `6` | The voice channel's numeric ID (from `channel_id`) |

**Examples:**

| Server ID | Channel ID | Voice Room Name |
|---|---|---|
| 42 | 6 | `Server_42_6` |
| 1 | 3 | `Server_1_3` |
| 9876543210 | 123 | `Server_9876543210_123` |

**Why this format:**

| Reason | Detail |
|---|---|
| **Capital `S` in `Server`** | Distinguishes server voice rooms from private conversation voice rooms which use `priv_voice_<sorted_ids>`. |
| **Server ID + Channel ID** | Guarantees global uniqueness — two different servers cannot have colliding room names even if they have channels with the same ID. |
| **Simple construction** | Frontend can predict the room name from `room_id` + `channel_id` without an API call. However, always use the `name` from the `server_join_voice` response — it's the authoritative source. |
| **No special characters** | Safe for URL query parameters without encoding overhead. |

**Private voice rooms** (for direct calls between users) use a different format:
```
priv_voice_<sorted_user_id_1><sorted_user_id_2>
```
Example: `priv_voice_799` for a call between users 7 and 99. These are managed by the `ConversationsManager`, not the servers system. The prefix `priv_voice_` vs `Server_` prevents any collision.

---

### 6.7 Auto-Disconnect & Ghost Users

When a user's **main WebSocket** connection drops (browser close, network loss, crash), the C++ backend automatically cleans up their voice state.

#### What happens on disconnect

1. `Socket::OnClientDisconnected()` is called in `Socket.cpp`
2. `ServersSocketData.CleanupUserVoiceChannels(UserId, UserName)` is invoked
3. The backend **scans all servers** the user belongs to, finds every voice channel they're in
4. For each voice channel: removes the user from `ConnectedUsers`, broadcasts `server_voice_left` to other members

#### What the frontend should do

- **Do NOT** rely on the Go voice WebSocket `onclose` as your sole cleanup mechanism. Always send `server_leave_voice` before closing the voice WebSocket.
- If the main WebSocket drops, assume voice state is cleaned up. On reconnect, you will need to re-join any voice channels.
- The `server_voice_left` event may arrive from the **auto-cleanup** path — treat it identically to a voluntary leave.

#### Go service idle timeout

The Go voice service has its own timeout for idle connections. If your voice WebSocket stays connected but you stop sending audio frames, the Go service may close it. This does NOT trigger a `server_voice_left` broadcast — only the C++ backend manages voice presence. If your voice WebSocket drops unexpectedly, send `server_leave_voice` on the main WebSocket to keep state consistent.

---

### 6.8 Voice Presence in Room Data

Voice channel presence (`connected_users`) is included in all room data responses:

- [`room_created`](#41-room_created) — when you create or join a server
- [`server_list`](#48-server_list) — when you request your server list
- [`server_joined`](#411-server_joined) — when you join via invite

Example voice channel in room data:

```json
{
  "channel_id": "6",
  "channel_name": "General",
  "channel_type": "voice",
  "connected_users": [
    { "user_id": "7", "user_name": "Alice" },
    { "user_id": "99", "user_name": "Bob" }
  ]
}
```

**Rules:**

| Rule | Detail |
|---|---|
| `connected_users` is **omitted** for text channels | Only voice channels have this field |
| `connected_users` is **omitted** when empty | If no one is connected, the field is simply absent (not `[]`) |
| User names are resolved from server members | The `user_name` comes from the same `JOIN` query used for `members[]` |
| This data is **transient** | Not persisted to DB. On server restart, all voice channels start empty |
| This is a **snapshot** | It reflects the state at the moment the room data was built. For real-time updates, listen to `server_voice_joined` / `server_voice_left` events |

**Frontend usage:** On initial load (after receiving `room_created` or `server_list`), iterate `channels[]`, find `channel_type: "voice"` entries, and render the `connected_users` as voice indicators in the UI. Then keep them updated via the real-time events.

---

### 6.9 Error Handling for Voice

#### Main WebSocket errors (from `server_join_voice` / `server_leave_voice`)

The C++ backend sends error frames on the main WebSocket:

```json
{
  "type": "error",
  "section": "rooms",
  "message": "<error description>"
}
```

| Error message | When it happens | Frontend action |
|---|---|---|
| `"not authenticated"` | UserId is 0 (session invalid) | Redirect to login |
| `"not a member of this room"` | You are not a member of the server | Don't retry. The user needs to join the server first. |
| `"room not found"` | Server ID doesn't exist | Don't retry. The server may have been deleted. |
| `"invalid voice channel"` | Channel ID doesn't exist, or is a text channel | Check your channel data. Only use channels with `channel_type: "voice"`. |
| (no response at all) | Main WebSocket is down | Reconnect main WebSocket first, then re-join voice. |

#### Go voice WebSocket errors

| Scenario | What happens | Frontend action |
|---|---|---|
| Invalid token | Go service closes with code 4001 | Re-send `server_join_voice` on main WS to get a fresh token |
| Room doesn't exist | Go service closes with error | Re-send `server_join_voice` to trigger room creation |
| Network error | `onerror` fires | Attempt reconnect to Go WS with same params (exponential backoff) |
| Go service down | Connection refused or timeout | Retry with backoff. Show "reconnecting..." indicator. |

#### Edge case: stale voice state

If you think you're in voice but the backend disagrees (e.g., after a crash or network blip), sending `server_join_voice` is always safe:
- It is **idempotent** — joining a voice channel you're already in does nothing harmful.
- It returns fresh credentials — always use the latest `name` and `token` from the response.

---

### 6.10 Complete Voice Lifecycle Summary

```
TIME ─────────────────────────────────────────────────────────────────►

JOIN:
  Frontend ──server_join_voice──► C++ Backend
                                      │
                                      ├─ Validate membership & channel
                                      ├─ Add to ConnectedUsers (in-memory)
                                      ├─ CheckRoom("Server_42_6") ──► Go Service
                                      │◄── { exists: false } ────────
                                      ├─ CreateRoom("Server_42_6") ──► Go Service
                                      │◄── { created: true } ───────
                                      ├─ GetRoomToken() → cache hit/miss
                                      │
  Frontend ◄──server_join_voice────── { name, token, user_name }
  Frontend ◄──server_voice_joined───► Other members (broadcast)

  Frontend ──WSS connect────────────► Go Voice Service
              ?room=Server_42_6
              &token=...
              &userid=7
  Frontend ◄══ binary audio ═══════► Go Service ◄═══► Other peers

LEAVE:
  Frontend ──server_leave_voice────► C++ Backend
                                      ├─ Remove from ConnectedUsers
                                      │
  Frontend ◄──server_leave_voice──── { status: "disconnected" }
  Frontend ◄──server_voice_left────► Other members (broadcast)

  Frontend ──WSS close──────────────► Go Voice Service

AUTO-CLEANUP (WebSocket drop/crash):
  C++ Backend detects close
      ├─ CleanupUserVoiceChannels(UserId, UserName)
      ├─ For each voice channel:
      │    ├─ LeaveVoiceChannel()
      │    └─ Broadcast server_voice_left
      └─ (Go service handles its own idle timeout separately)
```

---

## 7. Data Models

### FServer (in-memory)

| Field | Type | Description |
|---|---|---|
| `ServerId` | Uint64 | Primary key |
| `ServerName` | string | Display name (1–128 chars) |
| `OwnerId` | Uint64 | User ID of creator |
| `Token` | string | 48-char random alphanum (voice auth) |
| `CreatedAt` | string | Epoch nanoseconds as string |
| `Channels` | map<Uint64, FServerChannel> | Channel ID → channel |
| `Members` | map<Uint64, FServerMember> | User ID → member |
| `ChannelMessages` | map<Uint64, vector<FServerMessage>> | Channel ID → messages (newest first) |

**Thread safety:** `std::shared_mutex` — `shared_lock` for reads, `unique_lock` for writes.

### FServerChannel

| Field | Type | Description |
|---|---|---|
| `ChannelId` | Uint64 | Auto-increment PK |
| `ServerId` | Uint64 | Parent server |
| `ChannelName` | string | Display name |
| `ChannelType` | enum | `Text` or `Voice` |
| `ConnectedUsers` | vector<Uint64> | **(voice only, transient)** User IDs currently in this voice channel. Not persisted to DB. |

### FServerMessage

| Field | Type | Description |
|---|---|---|
| `MessageId` | Uint64 | Auto-increment PK |
| `ChannelId` | Uint64 | Parent channel |
| `SenderId` | Uint64 | Sender user ID |
| `SenderName` | string | Resolved at insert time (from users table) |
| `Content` | string | Message text |
| `CreatedAt` | string | Epoch nanoseconds as string |

### FServerMember

| Field | Type | Description |
|---|---|---|
| `UserId` | Uint64 | User PK |
| `UserName` | string | Display name (from users JOIN) |
| `Status` | string | `"online"`, `"offline"`, or `"away"` |

---

## 8. Rate Limiting & Abuse Protection

Server creation is rate-limited to prevent spam:

```
FAbuseProtection::CanAddressRequestCreateServer(ClientIP)
  → FRateLimiter::IsServerOperationAddressBlocked(ClientIP)
```

- Dedicated counter: `ServerOperationAddressToLimits` (separate from login/registration limits)
- Threshold: ~10 creations per minute per IP (configurable)
- When blocked: response `"service abuse"`
- IP source: `ws->getRemoteAddressAsText()` or `X-Forwarded-For` header (if behind proxy)

**For tests:** `FAbuseProtection::ResetRateLimits()` clears all counters. Call this between test batches.

---

## 9. Database Schema

5 tables, all in `utf8mb4` charset, InnoDB engine.

### servers

```sql
CREATE TABLE servers (
    id          BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
    name        VARCHAR(128) NOT NULL,
    owner_id    BIGINT UNSIGNED NOT NULL,      -- FK → users(id)
    token       VARCHAR(128) NOT NULL,          -- 48-char random token
    created_at  VARCHAR(32) NOT NULL DEFAULT '', -- epoch nanoseconds string
    INDEX idx_servers_owner (owner_id),
    INDEX idx_servers_token (token)
);
```

### server_channels

```sql
CREATE TABLE server_channels (
    id          BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
    server_id   BIGINT UNSIGNED NOT NULL,      -- FK → servers(id) ON DELETE CASCADE
    name        VARCHAR(128) NOT NULL,
    type        ENUM('text', 'voice') NOT NULL DEFAULT 'text',
    INDEX idx_channels_server (server_id)
);
```

### server_members

```sql
CREATE TABLE server_members (
    server_id   BIGINT UNSIGNED NOT NULL,      -- FK → servers(id)
    user_id     BIGINT UNSIGNED NOT NULL,       -- FK → users(id)
    joined_at   TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (server_id, user_id),
    INDEX idx_members_user (user_id)
);
```

### server_messages

```sql
CREATE TABLE server_messages (
    id          BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
    channel_id  BIGINT UNSIGNED NOT NULL,      -- FK → server_channels(id)
    sender_id   BIGINT UNSIGNED NOT NULL,       -- FK → users(id)
    content     TEXT NOT NULL,
    created_at  VARCHAR(32) NOT NULL DEFAULT '', -- epoch nanoseconds string
    INDEX idx_messages_channel (channel_id),
    INDEX idx_messages_sender (sender_id),
    INDEX idx_messages_created (channel_id, id)
);
```

### server_invites

```sql
CREATE TABLE server_invites (
    invite_code VARCHAR(16) NOT NULL PRIMARY KEY,  -- 10-char random alphanum
    server_id   BIGINT UNSIGNED NOT NULL,          -- FK → servers(id)
    created_by  BIGINT UNSIGNED NOT NULL,           -- FK → users(id)
    created_at  TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    expires_at  TIMESTAMP NULL DEFAULT NULL         -- NULL = never expires
);
```

**Key DB patterns:**
- Members query JOINs `users` table: `SELECT sm.user_id, u.username FROM server_members sm JOIN users u ON sm.user_id = u.id WHERE sm.server_id = ?`
- Messages query JOINs `users` table: `SELECT sm.id, sm.sender_id, u.username, sm.content, sm.created_at FROM server_messages sm JOIN users u ON sm.sender_id = u.id WHERE sm.channel_id = ? ... ORDER BY sm.id DESC`
- `INSERT IGNORE` used for members to prevent duplicate key errors
- Timestamp-based pagination: `WHERE ... AND sm.created_at < :before`
- Deletion cascade: deleting a server deletes its channels, members, messages, and invites

**Note:** Voice channel connected users are **not persisted** to DB — they live only in the in-memory `FServerChannel::ConnectedUsers` vector.

---

## 10. Deprecated REST Endpoints

The following REST endpoints previously existed under `/api/v1/rooms/*`. They have been **removed** — all functionality moved to the WebSocket protocol described above.

| Old endpoint | Replaced by WebSocket type |
|---|---|
| `GET /api/v1/rooms/list` | [`get_server_list`](#39-get_server_list) |
| `GET /api/v1/rooms/:id/messages` | [`get_server_messages`](#310-get_server_messages) |
| `POST /api/v1/rooms/:id/invite` | [`server_create_invite`](#311-server_create_invite) |
| `POST /api/v1/rooms/join` | [`server_join_invite`](#312-server_join_invite) |
| `POST /api/v1/rooms/create` | [`create_room`](#31-create_room) (was always WS) |

There are **zero** REST endpoints for servers in the current codebase. All server operations go through the persistent WebSocket connection.

---

## Design Decisions Summary

| Decision | Rationale |
|---|---|
| IDs as strings on wire | JavaScript cannot safely represent 64-bit unsigned integers. String encoding avoids precision loss. |
| Timestamps as nanoseconds in strings | Same reason — high precision, safe for JS as string comparison. |
| FNV-1a compile-time hashing | O(1) string→enum dispatch with zero runtime overhead. Compiler optimizes to integer switch. |
| Auth via WebSocket session data | Pointer deref, no DB lookup per message. Zero overhead. |
| `shared_mutex` per server | Read-heavy workload (broadcasts, membership checks). Multiple concurrent readers, exclusive writers. |
| Cache + lazy DB load | `GetServerById` tries cache → DB download → cache insert. Servers not accessed stay on disk. |
| Broadcast excludes sender | `BroadcastToServerMembers` takes `ExcludeUserId` param. Status broadcasts exclude the user themselves. |
| `SendToUser` skips offline | Checks `User->GetUserStatus() == Online` before enqueuing. No offline message queue (yet). |
| uWS pub/sub per user | Each user subscribes to topic `"user_<id>"`. Cross-thread message delivery via `EnqueueTaskForUserAtSocket`. |
| Invite codes not consumed | Currently not deleted after use — reusable. Can be changed to single-use by uncommenting the DELETE in `ConsumeInviteFromDB`. |
| Voice state is transient | `ConnectedUsers` in `FServerChannel` is in-memory only. On restart, all voice channels start empty. |
| Voice auto-cleanup on disconnect | `CleanupUserVoiceChannels` iterates all servers/channels on WebSocket close. Prevents ghost users. |
| Voice naming: `server_voice_joined` / `server_voice_left` | Past-tense event names (no "user") distinguish from imperative C→S actions `server_join_voice` / `server_leave_voice`. |
| Voice room naming: `Server_<ServerId>_<ChannelId>` | Capitalized `Server` prefix distinguishes server voice rooms from private voice rooms (`priv_voice_...`). Consistent with frontend's "servers" naming. |
| Two WebSocket architecture for voice | Main WS for commands/presence, Voice WS for audio. Separation keeps JSON parser off the audio hot path and allows independent scaling of the Go voice layer. |
| Token reuse across users | Same voice room token shared by all members. Generated once, cached, reused. Keeps Go room management simple — one room, one token. |
