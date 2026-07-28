# SquirrelCommunicator — Servers (Rooms) API Reference

> **Last updated:** 2026-07-27  
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
   - [3.7 room_join_voice](#37-room_join_voice) — Join a voice channel
   - [3.8 room_leave_voice](#38-room_leave_voice) — Leave a voice channel
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
   - [4.7 room_user_voice_join / room_user_voice_leave](#47-room_user_voice_join--room_user_voice_leave) — Voice presence
   - [4.8 server_list](#48-server_list) — Response: list of servers
   - [4.9 server_messages](#49-server_messages) — Response: paginated message history
   - [4.10 server_invite_created](#410-server_invite_created) — Response: generated invite code
   - [4.11 server_joined](#411-server_joined) — Response: full server data from invite join
5. [Error Responses](#5-error-responses)
6. [Voice Integration](#6-voice-integration)
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
| `type` | string | **yes** | One of the message types listed below. Strin gs matched via compile-time FNV-1a hash → O(1) dispatch. |
| `data` | object | **yes** | Payload specific to each message type (may be empty `{}`). |

### Dispatch flow (server-side)

```
FSocket::OnMessageReceived_TEXT()
  ├─ Parse JSON
  ├─ Read "section" → hash → ESocketMessageSection::Rooms
  └─ ServersSocketData.PrimarySwitch()
      ├─ Read "type" → hash → ESocketMessageServersType::<X>
      ├─ Extract UserId from WebSocket session data (pointer deref)
      ├─ [optional] Rate-limit check
      ├─ [optional] Membership check (Server->HasMember)
      └─ Handle → DB persist → Broadcast to room members
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

### 3.7 room_join_voice

Join a voice channel. This triggers the Go voice service to ensure a room exists there, and returns the connection credentials.

```json
{
  "section": "rooms",
  "type": "room_join_voice",
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

**Backend behavior:**
1. Verify membership + channel type
2. Add user to `ConnectedUsers` vector in-memory
3. Construct voice room name: `"server_<room_id>_<channel_id>"`
4. Check Go voice service via `CheckRoom()` → create if not exists
5. Fetch room token from voice service

**Response:** [`room_user_voice_join`](#47-room_user_voice_join--room_user_voice_leave) sent to the joining user with `name`, `token`, and `user_name`. The frontend then connects directly to:

```
wss://comm.sqrll.net/voice-ws/api/rooms/stream?room=<name>&token=<token>&userid=<userid>
```

**Broadcast:** [`room_user_voice_join`](#47-room_user_voice_join--room_user_voice_leave) sent to other members.

---

### 3.8 room_leave_voice

Leave a voice channel.

```json
{
  "section": "rooms",
  "type": "room_leave_voice",
  "data": {
    "room_id": "42",
    "channel_id": "6"
  }
}
```

**Response:** Confirmation `{ "type": "room_leave_voice", "data": { "status": "disconnected" } }`  
**Broadcast:** [`room_user_voice_leave`](#47-room_user_voice_join--room_user_voice_leave) to other members.

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
3. `BuildRoomDataJson()` — serialize members + channels

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
      { "channel_id": "5", "channel_name": "general", "channel_type": "text" },
      { "channel_id": "6", "channel_name": "General", "channel_type": "voice" }
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

Broadcast to all server members (including the leaver? no — broadcast to all remaining) when someone leaves.

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

### 4.7 room_user_voice_join / room_user_voice_leave

**To the joining user:**

```json
{
  "type": "room_user_voice_join",
  "section": "rooms",
  "data": {
    "name": "server_42_6",
    "token": "base64voiceToken...",
    "user_name": "Alice"
  }
}
```

**To other members (broadcast):**

```json
{
  "type": "room_user_voice_join",
  "section": "rooms",
  "data": {
    "room_id": "42",
    "channel_id": "6",
    "user_id": "7",
    "user_name": "Alice"
  }
}
```

`room_user_voice_leave` has the same broadcast format with `"room_id"`, `"channel_id"`, `"user_id"`, `"user_name"`.

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

Each element in `rooms[]` has the same format as [`room_created.data`](#41-room_created).

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
| `"invalid or expired invite code"` | Invite code not found in cache or DB |
| `"Unknown message type"` | `"type"` string didn't match any known handler |

---

## 6. Voice Integration

Voice streaming is handled by a separate **Go service**. The C++ backend acts as a broker:

### Voice room naming

```
server_<room_id>_<channel_id>
```

Example: server ID `42`, channel ID `6` → voice room name `"server_42_6"`

### Flow

```
1. Client sends: room_join_voice { room_id, channel_id }
2. Backend:
   a. Checks membership
   b. Checks channel type is "voice"
   c. Adds UserId to FServerChannel::ConnectedUsers
   d. Calls Go service: CreateRoom("server_42_6")  [idempotent]
   e. Fetches room token from Go service
   f. Sends: room_user_voice_join { name, token, user_name }
3. Client connects directly to Go service:
   wss://comm.sqrll.net/voice-ws/api/rooms/stream?room=<name>&token=<token>&userid=<userid>
4. Audio data: binary WebSocket frames
   [userIdLen:1 byte][userId:UTF-8][WebM/Opus audio data]
```

### Go service endpoints (internal)

| Method | Path | Description |
|---|---|---|
| POST | `/api/rooms/create` | Create voice room (idempotent). Auth: `X-API-Token` header. |
| GET | `/api/rooms/check?room=<name>` | Check if room exists. Returns `200` or `404`. |
| WSS | `/api/rooms/stream` | WebSocket for audio streaming. Query params: `room`, `token`, `userid`. |

The Go service token cache (`RoomNameToToken`) is local to each C++ server process. If a token is not cached, a new one is generated and shared with the Go service.

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
| `ConnectedUsers` | vector<Uint64> | Active voice participants |

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
