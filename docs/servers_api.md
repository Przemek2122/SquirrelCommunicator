# Squirrel Communicator Server API Documentation

Version 1.9

This document describes all server endpoints and WebSocket message types available in Squirrel Communicator. The system uses two communication channels: REST API over HTTPS for authentication and account management, and WebSocket for real time messaging and server operations.

============================================
SECTION 1: REST API ENDPOINTS
============================================

All REST endpoints are prefixed with /api/v1/ and accept JSON request bodies. Responses are JSON with status and message fields. Authentication is handled via cookies after login.
REST endpoints are subject to two-tier global rate limiting: unauthenticated requests (no valid session token) are limited per-IP (default 300/hour), while authenticated requests (valid session token) are limited per-UserID (default 2000/hour). See Section 7 for details.

--------------------------------------------
1.1 USER AUTHENTICATION
--------------------------------------------

POST /api/v1/users/register

    Start a new account registration. Subject to per-IP registration rate limiting (default 10 registrations/hour, configurable via RegisterAccountLimitPerHour).

    The account is NOT created immediately. The server generates a 6 digit verification code, stores the registration as pending, and emails the code to the provided address. The email also contains a verification link (Brevo param LINK) that opens the registration-verification page pre-filled with the code and email. Call /api/v1/users/register/verify with the code to complete registration.

    Request body:
        username  string  Display name 4 to 109 characters
        password  string  Password 8 to 269 characters
        email     string  Valid email address

    Responses:
        429 Too Many Requests
            status  error
            message  Too many registrations. Try again later.

        200 OK
            status  success
            message  Registration started. Check your email for the verification code.
        400 Bad Request
            Registration failed. User may already exist or invalid input.
            Registration failed. Password too weak.

    Note: This endpoint (and the email it sends) is used only for database registration. Third party integrations (Google / Microsoft) do not send any email.

    The verification link (Brevo param LINK) points at the backend public base URL:
        Debug builds   http://<DebugDomain>:<port>/register/verify?code=<code>&email=<email>  (DebugDomain defaults to localhost)
        Release builds https://comm.sqrll.net/register/verify?code=<code>&email=<email>
    The registration email template (Brevo templateId 2) must render {{ params.LINK }}
    as the clickable link, alongside {{ params.TOKEN }} and {{ params.USERNAME }}.

POST /api/v1/users/register/verify

    Complete a pending registration by validating the code sent to the email address. On success the account is created.

    Request body:
        email  string  Email address used during registration
        code   string  6 digit code from the email

    Responses:
        200 OK
            status  success
            message  Registration completed. You can now log in.
        400 Bad Request
            Invalid or expired verification code.
        500 Internal Server Error
            Registration failed. Please try again.

POST /api/v1/users/login

    Login with email and password. Sets auth_token cookie on success.

    Request body:
        email     string  Registered email address
        password  string  Account password

    Responses:
        200 OK
            status  success
            message  User login successful.
            Sets cookie: auth_token
        403 Forbidden
            Wrong credentials.
            IncorrectInputLength.
        204 No Content
            Session already exists.

POST /api/v1/users/verify

    Verify if the current session token is valid. Reads auth_token from cookie.

    Request body: none required

    Responses:
        200 OK
            Token correct.
        401 Unauthorized
            Token incorrect.

POST /api/v1/users/refresh

    Refresh the session token to extend its lifetime. Reads auth_token from cookie.

    Request body: none required

    Responses:
        200 OK
            Token has new refreshed.
        401 Unauthorized
            Token not found.

POST /api/v1/users/logout

    Terminate the current session.

    Request body:
        token  string  Session token to invalidate

    Responses:
        200 OK
            status  success
            message  Session terminated.
        400 Bad Request
            Can not log out.

--------------------------------------------
1.2 ACCOUNT MANAGEMENT
--------------------------------------------

POST /api/v1/account/change_name

    Change the display name of the authenticated user.

    Request body:
        new_name  string  New display name 4 to 109 characters

    Authentication: Cookie auth_token required.

    Responses:
        200 OK
            status  success
            message  User name changed.
        401 Unauthorized
            Invalid token.
        500 Internal Server Error
            Error details.

POST /api/v1/account/change_password

    Change password. Requires old password for verification.

    Request body:
        old_password  string  Current password
        new_password  string  New password 8 to 269 characters

    Authentication: Cookie auth_token required.

    Responses:
        200 OK
            status  success
            message  User password changed.
        401 Unauthorized
            Invalid token.

POST /api/v1/account/reset_pass_by_mail

    Request a password reset email. Sends a 6 digit code via email template.

    Request body:
        target_mail  string  Email address to receive reset code

    Responses:
        200 OK
            status  success
            message  If such e mail exists, it was sent. Check you mailbox.
        400 Bad Request
            Invalid email.

POST /api/v1/account/reset_pass_by_mail_verify

    Verify the reset code and set a new password.

    Request body:
        target_mail  string  Email address
        reset_code   string  6 digit code from email
        new_password string  New password

    Responses:
        200 OK
            status  success
            message  Password reset successful.
        400 Bad Request
            Invalid reset token or user email.
        500 Internal Server Error
            Internal error.

--------------------------------------------
1.3 THIRD PARTY INTEGRATION
--------------------------------------------

POST /api/v1/integrate/google

    Login or register using a Google OAuth ID token. Requires verified email from Google.

    Request body:
        google_token  string  Google ID token from OAuth flow

    Responses:
        200 OK
            status  success
            message  User login successful.
            Sets cookie: auth_token
        400 Bad Request
            Google integration  E Mail not verified.
        401 Unauthorized
            Missing authorization.
        500 Internal Server Error
            Google integration error details.

POST /api/v1/integrate/microsoft

    Login or register using a Microsoft access token. Calls Microsoft Graph API to get profile.

    Request body:
        microsoft_token  string  Microsoft Bearer access token

    Responses:
        200 OK
            status  success
            message  User login successful.
            Sets cookie: auth_token
        401 Unauthorized
            Microsoft integration  missing authorization.
        500 Internal Server Error
            Microsoft integration error details.

--------------------------------------------
1.4 TRANSFER TOKEN
--------------------------------------------

POST /api/v1/transfer_token/create

    Create a one time use transfer token for cross device login.

    Authentication: Cookie auth_token required.

    Request body: none required

    Responses:
        200 OK
            status  success
            token  Transfer token string
        401 Unauthorized
            Missing or invalid session.
            Token incorrect.
        500 Internal Server Error
            Failed to create transfer token.

POST /api/v1/transfer_token/redeem

    Redeem a transfer token to get a new session. Token is consumed on use.

    Request body:
        token  string  Transfer token from /transfer_token/create

    Responses:
        200 OK
            status  success
            message  Transfer token redeemed.
            Sets cookie: auth_token
        400 Bad Request
            Invalid transfer token.
            Invalid user ID.

============================================
SECTION 2: WEBSOCKET API
============================================

The WebSocket connection uses a JSON based protocol. Every message must contain a type field identifying the action and a data field with payload. The connection requires prior authentication via REST login; the session cookie is used to identify the user.
All WebSocket messages are subject to the authenticated per-UserID rate limit (default 2000 messages/hour per user). Since WebSocket connections always require a valid session token, only the authenticated tier applies. See Section 7 for details.

Base message format:
    type  string  Message type identifier
    data  object  Payload data

    Optional fields:
    section  string  priv or servers message section

--------------------------------------------
2.1 PRIVATE MESSAGING
--------------------------------------------

These messages use the priv section and handle direct messages, conversations, friend lists and voice calling between users.

2.1.1 Client to Server Messages

    type: message
        Send a message in a conversation.

        data:
            conversation_id  string  Conversation ID
            content          string  Message text

    type: message_edit
        Edit an existing message in a conversation.

        data:
            conversation_id  string  Conversation ID
            message_id       string  Message ID to edit
            content          string  New message text

    type: typing
        Notify that the user is typing in a conversation.

        data:
            conversationId  number  Conversation ID

    type: message_read
        Mark all messages in a conversation as read.

        data:
            conversationId  number  Conversation ID

    type: search_user
        Search for users by ID or username pattern.

        data:
            search_target  string  User ID or username substring

        Server response type: search_user
            message contains JSON array of data with id and displayName fields.

    type: load_more_messages
        Load older messages from conversation history with pagination.

        data:
            conversation_id  string  Conversation ID
            offset           number  Starting offset
            limit            number  Number of messages to load

        Server response type: load_more_messages
            status   success or no_more_messages or unauthorized
            message  Array of message objects with message, sender_id fields

    type: get_conversations
        Get list of recent conversations for the current user.

        data:
            offset  number  Pagination offset
            limit   number  Number of conversations to return

        Server response type: get_conversations
            message contains JSON array of conversation objects with:
                id          Conversation ID
                users       Array of user objects id, name, status
                userids     Array of user IDs
                messages    Array of last 25 messages with message, message_id, sender_id, time, status

    type: add_conversation
        Create or open a conversation with another user. Only works if users are friends.

        data:
            user_id  string  Target user ID

        Server response type: add_conversation
            message contains conversation JSON object same format as get_conversations.

2.1.2 Friend List Messages

    type: get_friend_list
        Get paginated list of friends.

        data:
            offset  number  Pagination offset
            limit   number  Number of friends per page

        Server response type: get_friend_list
            data:
                friends  Array of user objects id, name, status
                offset   Current offset
                limit    Page limit

    type: get_friend_request_list
        Get paginated list of sent and incoming friend requests.

        data:
            offset  number  Pagination offset
            limit   number  Number of requests per page

        Server response type: get_friend_request_list
            data:
                sent      Array of sent request user objects id, name, status
                incoming  Array of incoming request user objects id, name, status
                offset    Current offset
                limit     Page limit

    type: create_friend_request
        Send a friend request to another user.

        data:
            other_id  string  Target user ID

        Server response type: create_friend_request
            message: friend request added or already friends or sent requests limit reached or target incoming requests limit reached

    type: accept_friend_request
        Accept an incoming friend request.

        data:
            other_id  string  Requester user ID

        Server response type: accept_friend_request
            message: friend request accepted or no friend request or friends limit reached

    type: reject_friend_request
        Reject an incoming friend request.

        data:
            other_id  string  Requester user ID

        Server response type: reject_friend_request
            message: friend request rejected or friend request not found

    type: cancel_friend_request
        Cancel a sent friend request.

        data:
            other_id  string  Target user ID

        Server response type: cancel_friend_request
            message: friend request canceled or friend request not found

    type: remove_friend
        Remove a user from the friend list.

        data:
            other_id  string  Target user ID

        Server response type: remove_friend
            message: friend removed or friend not found

    Server to Client Push Messages (Friend / Friend-Request Events)

    The server pushes the following events to the "other" user affected by a
    friend or friend-request change, so their UI updates in real time without
    re-fetching the friend list. Each push uses section: priv and includes the
    acting user's ID and display name in data.

    type: friend_request_received
        Sent to a user when someone sends them a friend request.

        data:
            user_id    number  ID of the user who sent the request
            user_name  string  Display name of the user who sent the request

    type: friend_request_accepted
        Sent to the original requester when their friend request is accepted.

        data:
            user_id    number  ID of the user who accepted the request
            user_name  string  Display name of the user who accepted the request

    type: friend_request_rejected
        Sent to the original requester when their friend request is rejected.

        data:
            user_id    number  ID of the user who rejected the request
            user_name  string  Display name of the user who rejected the request

    type: friend_request_canceled
        Sent to the target when the sender cancels their friend request.

        data:
            user_id    number  ID of the user who canceled the request
            user_name  string  Display name of the user who canceled the request

    type: friend_removed
        Sent to a user when they are removed from someone's friend list.

        data:
            user_id    number  ID of the user who removed them
            user_name  string  Display name of the user who removed them

2.1.3 Voice and Calling

    type: data_stream_channel
        Request a voice chat with another user. Both users must be friends. Creates or gets a voice room from the Go voice service.

        data:
            other_id  string  Target user ID

        Server response type: data_stream_channel
            data:
                name   Voice room name
                token  Voice room access token

    type: user_calling
        Notify another user about an incoming call.

        data:
            other_id  string  Target user ID

        Server pushes type: user_calling to the target user:
            data:
                user_id  Caller user ID

--------------------------------------------
2.2 SERVERS
--------------------------------------------

These messages handle the community server system with channels and voice chat. Messages use the servers section.

2.2.1 Client to Server Messages

    type: create_server
        Create a new server. Rate limited per IP address (default 2 per hour, configurable via CreateServerRateLimitNumberPerIP).

        data:
            server_name  string  Name of the new server

        Server response type: server_created
            data: Full server object with members, channels, id, name, token, owner_id, created_at.

    type: join_server
        Rejoin a server by ID. Only works if the user is already a member.
        New members must use server_join_invite with a valid invite code.

        data:
            server_id  string  Server ID to rejoin

        Server response type: server_created
            data: Full server object.

        Server broadcasts type: server_user_joined to all server members with user_id and user_name.

    type: leave_server
        Leave a server.

        data:
            server_id  string  Server ID to leave

        Server response type: leave_server
            data:
                server_id  Server ID
                status   left

        Server broadcasts type: server_user_left to remaining server members.

    type: server_message
        Send a text message in a server channel.

        data:
            server_id     string  Server ID
            channel_id  string  Channel ID
            content     string  Message text

        Server broadcasts type: server_message to all server members with:
            server_id, channel_id, message_id, sender_id, sender_name, content, timestamp.

    type: create_channel
        Create a new channel in a server. The channel is auto-assigned the next
        available position (appears at the bottom of the channel list).

        data:
            server_id       string  Server ID
            channel_name  string  Channel name
            channel_type  string  text or voice

        Server broadcasts type: server_channel_created to all server members with:
            server_id, channel_id, channel_name, channel_type.

        New in channel data response: position field indicating the display order.

    type: move_channel
        Move a channel to a new position in the server's channel list.
        All channels are renumbered to eliminate gaps after the move.
        Requires CAN_MANAGE_CHANNELS permission or server owner.

        data:
            server_id      string  REQUIRED. Server ID.
            channel_id   string  REQUIRED. Channel ID to move.
            new_position number  REQUIRED. Target position (0-based). Clamped to valid range.

        Example request:
            {
                "type": "move_channel",
                "data": {
                    "server_id": "123456789",
                    "channel_id": "42",
                    "new_position": 0
                }
            }

        Server response type: server_channel_moved
            data:
                server_id      Server ID (number)
                channel_id   Moved channel ID (number)
                new_position New position (number)

        Server broadcasts type: server_channel_moved to all server members (same data).

        Error if user lacks permission:
            type: error
            message: permission denied: you lack CAN_MANAGE_CHANNELS permission

        Error if channel not found:
            type: error
            message: failed to move channel

    type: reorder_channels
        Reorder all channels at once by providing the complete ordered array of channel IDs.
        Designed for drag-and-drop UIs. The array must contain every channel in the server
        exactly once (no missing, no duplicates, no extras). All positions are updated
        atomically in a single DB transaction.
        Requires CAN_MANAGE_CHANNELS permission or server owner.

        data:
            server_id     string  REQUIRED. Server ID.
            channel_ids array   REQUIRED. Array of channel ID strings (or numbers) in the desired order.

        Example request:
            {
                "type": "reorder_channels",
                "data": {
                    "server_id": "123456789",
                    "channel_ids": ["42", "17", "5", "99"]
                }
            }

        Server response type: server_channels_reordered
            data:
                server_id     Server ID (number)
                channel_ids Array of channel IDs in the new order (array of numbers)

        Server broadcasts type: server_channels_reordered to all server members (same data).

        Error if user lacks permission:
            type: error
            message: permission denied: you lack CAN_MANAGE_CHANNELS permission

        Error if channel_ids is invalid (wrong count, missing IDs, duplicates):
            type: error
            message: failed to reorder channels

    type: delete_channel
        Permanently delete a channel and all its messages.
        Requires CAN_MANAGE_CHANNELS permission or server owner.

        data:
            server_id     string  REQUIRED. Server ID.
            channel_id  string  REQUIRED. Channel ID to delete.

        Example request:
            {
                "type": "delete_channel",
                "data": {
                    "server_id": "123456789",
                    "channel_id": "42"
                }
            }

        Server response type: server_channel_deleted
            data:
                server_id     Server ID (number)
                channel_id  Deleted channel ID (number)

        Server broadcasts type: server_channel_deleted to all server members (same data).

        Error if user lacks permission:
            type: error
            message: permission denied: you lack CAN_MANAGE_CHANNELS permission

        Error if channel not found:
            type: error
            message: failed to delete channel or channel not found

    type: rename_channel
        Rename a channel in a server. Updates both DB and in-memory cache.
        Requires CAN_MANAGE_CHANNELS permission or server owner.

        data:
            server_id     string  REQUIRED. Server ID.
            channel_id  string  REQUIRED. Channel ID to rename.
            new_name    string  REQUIRED. New name for the channel. Must not be empty.

        Example request:
            {
                "type": "rename_channel",
                "data": {
                    "server_id": "123456789",
                    "channel_id": "42",
                    "new_name": "general-chat"
                }
            }

        Server response type: server_channel_renamed
            data:
                server_id     Server ID (number)
                channel_id  Renamed channel ID (number)
                new_name    New channel name (string)

        Server broadcasts type: server_channel_renamed to all server members (same data).

        Error if user lacks permission:
            type: error
            message: permission denied: you lack CAN_MANAGE_CHANNELS permission

        Error if channel not found or name is empty:
            type: error
            message: failed to rename channel or channel not found

    type: server_invite
        Directly invite a user to a server by user ID. Sends a push notification to the target user.

        data:
            server_id  string  Server ID
            user_id  string  Target user ID to invite

        Server response type: server_invite
            data:
                status   sent
                server_id  Server ID
                user_id  Invited user ID

        Server pushes type: server_invite to the invited user with:
            server_id, server_name, inviter_id, inviter_name.

    type: server_join_voice
        Join a voice channel in a server. Creates Go voice service room if needed.

        data:
            server_id     string  Server ID
            channel_id  string  Voice channel ID

        Server response type: server_join_voice
            data:
                name         Voice room name for Go service
                token        Voice room access token
                user_name    Current user name
                participants Array of users currently connected to the channel
                             (including the joining user). Each entry:
                             { user_id, user_name }


        Server broadcasts type: server_voice_joined to server members with:
            server_id, channel_id, user_id, user_name.

    type: server_leave_voice
        Leave a voice channel.

        data:
            server_id     string  Server ID
            channel_id  string  Voice channel ID

        Server response type: server_leave_voice
            data:
                status  disconnected

    type: get_voice_channel_users
        Get the list of users currently connected to a voice channel WITHOUT joining it.
        The requesting user must be a member of the server. Unlike server_join_voice,
        this is a read-only query and does not add the user to the channel.

        data:
            server_id     string  Server ID
            channel_id  string  Voice channel ID

        Server response type: voice_channel_users
            data:
                server_id     Server ID (number)
                channel_id  Voice channel ID (number)
                participants Array of users currently connected to the channel.
                             Each entry: { user_id, user_name }

        The same participant list is also available in the channel object's
        connected_users field inside server data responses (see Section 3.3).

    type: get_server_list
        Get list of all servers the current user belongs to. Supports pagination.

        data:
            offset  number  Optional. Zero-based offset. Default 0.
            limit   number  Optional. Max servers to return. Default 50, capped at 200.

        Server response type: server_list
            data:
                servers    Array of server objects, each with full server data.
                total    Total number of servers the user belongs to.
                has_more Boolean. True if more servers exist beyond the returned page.

    type: get_server_messages
        Get channel message history with timestamp based pagination.

        data:
            server_id     string  Server ID
            channel_id  string  Channel ID
            before      string  Optional. Timestamp to fetch messages before.
            limit       string  Optional. Max 100, default 50.

        Server response type: server_messages
            data:
                messages  Array of message objects with message_id, channel_id, sender_id, sender_name, content, timestamp
                has_more  boolean

    type: server_create_invite
        Generate an invite code for a server. Invites are never permanent; they always have
        a configurable expiration (max 12 months) and a configurable usage limit.
        Requires CAN_CREATE_INVITES permission or server owner.
        Subject to hourly invite creation rate limit per IP (default 20/hour, configurable via InviteCreateLimitPerHour).

        data:
            server_id             string  REQUIRED. Server ID to create invite for.
            max_uses            number  Optional. Max times invite can be used. Default 1000 from config. Set 0 to use default.
            expires_in_seconds  number  Optional. Lifetime in seconds. Max 31536000 (12 months). Default 2592000 (30 days). Set 0 to use default.

        Example request:
            {
                "type": "server_create_invite",
                "data": {
                    "server_id": "123456789",
                    "max_uses": 50,
                    "expires_in_seconds": 86400
                }
            }

        Server response type: server_invite_created
            data:
                invite_code         Generated invite code string (random alphanumeric characters, max 16)
                invite_url          Full invite URL <base>/invite/code. <base> is the backend public base URL: https://comm.sqrll.net in release, or http://<DebugDomain>:<port> in debug (DebugDomain defaults to localhost).
                max_uses            Actual max uses value applied
                expires_at          Unix timestamp (seconds) when invite expires
                expires_in_seconds  Actual expiration duration in seconds applied

        Error if user lacks permission:
            type: error
            message: permission denied: you lack CAN_CREATE_INVITES permission

        Error if IP exceeds hourly invite creation limit:
            type: error
            message: invite create rate limit exceeded

    type: server_join_invite
        Join a server using an invite code. New members via invite get zero special permissions
        (can chat freely but cannot create invites, manage channels, etc.).
        Subject to hourly invite use rate limit per IP (default 30/hour, configurable via InviteUseLimitPerHour).

        Two-layer invite protection is active on this endpoint:
        Layer 1 — Hourly rate limit: max 30 invite use attempts per IP per hour.
        Layer 2 — Abuse detection: too many failed attempts from the same IP within a
        rolling window will result in a temporary ban. Successful joins reset the counter.

        data:
            invite_code  string  Invite code from server_invite_created

        Server response type: server_joined
            data: Full server object.

        Server broadcasts type: server_user_joined to server members.

        Error if IP exceeds hourly invite use limit:
            type: error
            message: invite use rate limit exceeded

        Error if banned for too many failed attempts:
            type: error
            message: abuse ban: too many failed invite attempts. Try again later.

        Error if invite is invalid or expired:
            type: error
            message: invalid or expired invite code

    type: server_delete_invite
        Delete an invite by its code. The invite is permanently removed and can no longer be used.
        Requires CAN_CREATE_INVITES permission or server owner.

        data:
            server_id     string  REQUIRED. Server ID the invite belongs to.
            invite_code string  REQUIRED. The invite code to delete.

        Example request:
            {
                "type": "server_delete_invite",
                "data": {
                    "server_id": "123456789",
                    "invite_code": "aB3xK7mQ2p"
                }
            }

        Server response type: server_invite_deleted
            data:
                server_id     Server ID (number)
                invite_code The deleted invite code (string)

        Error if invite not found:
            type: error
            message: invite not found or already deleted

        Error if user lacks permission:
            type: error
            message: permission denied: you lack CAN_CREATE_INVITES permission

    type: server_list_invites
        List invites for a server with pagination. Returns invites sorted by creation time
        (newest first). Requires CAN_CREATE_INVITES permission or server owner.

        data:
            server_id  string  REQUIRED. Server ID to list invites for.
            start    number  Optional. Zero based offset into the result set. Default 0.
            count    number  Optional. Max invites to return. Default 50, capped at 200.

        Example request (first page of 20):
            {
                "type": "server_list_invites",
                "data": {
                    "server_id": "123456789",
                    "start": 0,
                    "count": 20
                }
            }

        Server response type: server_invites_list
            data:
                server_id  Server ID (number)
                start    Offset used (number)
                count    Number of invites in this page (number)
                total    Total number of invites for this server (number)
                invites  Array of invite objects, each containing:
                    invite_code     The alphanumeric invite token (string)
                    created_by      User ID who created the invite (string)
                    max_uses        Configured max usage count (number)
                    current_uses    How many times it has been used (number)
                    remaining_uses  How many uses remain = max_uses - current_uses (number)
                    created_at      Creation timestamp, MySQL TIMESTAMP (string)
                    expires_at      Expiration timestamp, MySQL TIMESTAMP (string)

        Example response:
            {
                "type": "server_invites_list",
                "data": {
                    "server_id": 123456789,
                    "start": 0,
                    "count": 2,
                    "total": 5,
                    "invites": [
                        {
                            "invite_code": "zY9xW8vU7t",
                            "created_by": "42",
                            "max_uses": 50,
                            "current_uses": 3,
                            "remaining_uses": 47,
                            "created_at": "2026-07-29 10:15:00",
                            "expires_at": "2026-08-05 10:15:00"
                        },
                        {
                            "invite_code": "aB3xK7mQ2p",
                            "created_by": "42",
                            "max_uses": 1000,
                            "current_uses": 0,
                            "remaining_uses": 1000,
                            "created_at": "2026-07-28 22:00:00",
                            "expires_at": "2026-08-27 22:00:00"
                        }
                    ]
                }
            }

        Error if user lacks permission:
            type: error
            message: permission denied: you lack CAN_CREATE_INVITES permission

    type: server_update_member_permissions
        Update a members permissions. Only the server owner can use this.

        data:
            server_id         string  Server ID
            target_user_id  string  User ID whose permissions to update
            permissions     string  New permission bitfield as decimal string

        Server response type: server_member_permissions_updated
            data:
                server_id     Server ID
                user_id     Updated user ID
                permissions New permission bitfield

        Server broadcasts type: server_member_permissions_updated to all server members.

        Error if not owner:
            type: error
            message: permission denied: only the server owner can manage member permissions

        Error if targeting owner:
            type: error
            message: cannot modify the server owners permissions

    type: kick_member
        Kick a member from a server. The kicked user is permanently removed from the server.
        Requires CAN_KICK_MEMBERS permission or server owner.
        The server owner cannot be kicked. Users cannot kick themselves.

        data:
            server_id        string  REQUIRED. Server ID.
            target_user_id string  REQUIRED. User ID to kick.

        Example request:
            {
                "type": "kick_member",
                "data": {
                    "server_id": "123456789",
                    "target_user_id": "42"
                }
            }

        Server response type: server_user_kicked
            data:
                server_id     Server ID (number)
                user_id     Kicked user ID (number)
                user_name   Kicked user name (string)
                status    "kicked"

        Server sends type: server_user_kicked to the kicked user with:
            server_id, server_name, message: "You have been kicked from the server"

        Server broadcasts type: server_user_kicked to remaining server members with:
            server_id, user_id, user_name, kicker_id, kicker_name

        Error if user lacks permission:
            type: error
            message: permission denied: you lack CAN_KICK_MEMBERS permission

        Error if trying to kick owner:
            type: error
            message: cannot kick the server owner

        Error if trying to kick self:
            type: error
            message: cannot kick yourself

        Error if target user is not a member:
            type: error
            message: target user is not a member of this server

        Error if kick fails:
            type: error
            message: failed to kick user from server

2.2.2 Server to Client Push Messages

    type: server_member_status
        Broadcast when a member status changes (online, offline, idle, do_not_disturb).

        data:
            server_id    Server ID
            user_id    User ID whose status changed
            user_name  User name
            status     New status string

    type: server_channel_moved
        Broadcast when a channel is reordered. All members receive this so
        their channel lists stay in sync.

        data:
            server_id      Server ID (number)
            channel_id   Moved channel ID (number)
            new_position New position (number)

    type: server_channels_reordered
        Broadcast when all channels are reordered at once via drag-and-drop.
        All members receive the complete new order so their channel lists stay in sync.

        data:
            server_id     Server ID (number)
            channel_ids Array of channel IDs in the new order (array of numbers)

    type: server_channel_deleted
        Broadcast when a channel is deleted. All members receive this so
        their channel lists stay in sync.

        data:
            server_id     Server ID (number)
            channel_id  Deleted channel ID (number)

    type: server_channel_renamed
        Broadcast when a channel is renamed. All members receive this so
        their channel names stay in sync.

        data:
            server_id     Server ID (number)
            channel_id  Renamed channel ID (number)
            new_name    New channel name (string)

    type: server_member_permissions_updated
        Broadcast when a members permissions have been updated.

        data:
            server_id     Server ID
            user_id     User ID
            permissions New permissions bitfield

    type: server_user_kicked
        Broadcast when a member is kicked from a server. Sent to the kicked user
        with a kick notification message, and broadcast to remaining members with
        kicker and kicked user details.

        To kicked user:
            data:
                server_id     Server ID
                server_name   Server display name
                message     "You have been kicked from the server"

        To remaining members:
            data:
                server_id     Server ID
                user_id     Kicked user ID
                user_name   Kicked user name
                kicker_id   ID of the user who performed the kick
                kicker_name Name of the user who performed the kick

    type: error
        Sent when an error occurs processing a request.

        data:
            message  Error description string


2.3 PING / PONG

    Application-level ping/pong for round-trip latency measurement and connection
    keep-alive verification. Available in both "priv" and "servers" sections.

    Ping/pong is a lightweight request-response that carries zero rate-limit cost
    and minimal server overhead. The server responds immediately with its current
    microsecond timestamp, allowing the client to calculate real-time latency.

    Note: uWebSockets also sends protocol-level WebSocket pings automatically
    (sendPingsAutomatically=true, idleTimeout=300s). The application-level
    ping/pong documented here provides higher-level latency data for UI display
    and client-side health monitoring.

2.3.1 Client Request

    type: ping
        Send a ping to measure latency. Works in either section (priv or servers).

        data:
            timestamp  number  Optional. Client's current time in microseconds
                                since epoch. If provided, the server echoes it
                                back so the client can calculate round-trip time.

        Example (priv section):
            {
                "section": "priv",
                "type": "ping",
                "data": {
                    "timestamp": 1753284000123456
                }
            }

        Example (servers section):
            {
                "section": "servers",
                "type": "ping",
                "data": {
                    "timestamp": 1753284000123456
                }
            }

2.3.2 Server Response

    type: pong
        Server responds immediately with echoed client timestamp and server time.

        data:
            client_timestamp  number  Echo of the client's timestamp (0 if not provided)
            server_timestamp  number  Server's current time in microseconds since epoch

        Example response:
            {
                "type": "pong",
                "data": {
                    "client_timestamp": 1753284000123456,
                    "server_timestamp": 1753284000156789
                }
            }

        Round-trip time calculation:
            RTT = (client_receive_time - client_send_time)
            or approximate using: server_timestamp - client_timestamp
            (accounts for one-way travel + server processing, typically < 1ms)

2.3.3 Protocol-Level Pong

    In addition to the application-level ping/pong, the server responds to
    WebSocket protocol-level pings (uWS::PING opcode) with protocol pongs
    (uWS::PONG) and updates the user's last-activity timestamp. This keeps the
    connection alive and prevents idle timeout disconnection.

    The idle timeout is 300 seconds (5 minutes). WebSocket protocol pings are
    sent automatically by uWebSockets at a lower level and require no client code.

============================================
SECTION 3: DATA STRUCTURES
============================================

3.1 Server Object

    {
        server_id    string  Server ID
        server_name  string  Display name
        server_token string  Internal access token
        owner_id   string  Owner user ID
        created_at number  Creation timestamp
        members    array   Array of member objects
        channels   array   Array of channel objects
    }

3.2 Member Object

    {
        user_id      string  User ID
        user_name    string  Display name
        status       string  online, offline, idle, do_not_disturb
        permissions  string  Permission bitfield as decimal string
        is_owner     boolean True if this member is the server owner
    }

3.3 Channel Object

    {
        channel_id      string  Channel ID
        channel_name    string  Display name
        channel_type    string  text or voice
        position        number  Display order (0-based, lower = first). Channels are
                                always returned sorted by position ascending.
        connected_users array   Present only for voice channels. Array of user_id,
                                user_name objects currently connected.
    }

3.4 Invite Object (from server_invites_list)

    {
        invite_code     string  The alphanumeric invite token
        created_by      string  User ID who created this invite
        max_uses        number  Configured max usage count
        current_uses    number  Times the invite has been consumed
        remaining_uses  number  Uses remaining (max_uses - current_uses)
        created_at      string  MySQL TIMESTAMP when invite was created
        expires_at      string  MySQL TIMESTAMP when invite expires
    }

3.5 Conversation Object

    {
        id       number  Conversation ID
        users    array   Array of member user objects
        userids  array   Array of member user IDs
        messages array   Array of message objects (last 25)
    }

3.6 Message Object

    {
        message     string  Message content
        message_id  number  Unique message identifier
        sender_id   number  User ID of sender
        time        number  Creation timestamp
        status      number  Message status code
    }

    Note: When a message is encrypted at rest and the server cannot decrypt it
    (missing or mismatched encryption key, or corrupted data), the message field
    is set to "[ENCRYPTED - INVALID KEY]" instead of the raw ciphertext. See
    Section 9 for details.

============================================
SECTION 4: PERMISSION SYSTEM
============================================

Squirrel Communicator uses a Discord like permission system for server members. Permissions are stored as a bitfield (Uint64) on each member. The server owner always has all permissions.

4.1 Permission Bits

    Bit 0  0x01  CAN_CREATE_INVITES     Allow member to create invite codes
    Bit 1  0x02  CAN_KICK_MEMBERS       Allow member to kick other members
    Bit 2  0x04  CAN_BAN_MEMBERS        Allow member to ban others (future)
    Bit 3  0x08  CAN_MANAGE_CHANNELS    Allow member to create, delete, rename, and reorder channels
    Bit 4  0x10  CAN_MANAGE_PERMISSIONS Allow member to manage other members permissions (future)

    All bits set  0xFFFFFFFFFFFFFFFF  means all permissions (granted to owner).

4.2 Default Permissions

    Server owner              All permissions (0xFFFFFFFFFFFFFFFF)
    New member via invite     Zero permissions (0x0): can chat freely but no special actions
    New member added directly Configurable via API, defaults to 0

4.3 Checking Permissions

    Permissions are checked using bitwise AND:
        has_permission = (member_permissions & CAN_CREATE_INVITES) != 0

    Server owners bypass all permission checks and always have access.

4.4 Managing Permissions

    Only the server owner can update member permissions via:
        server_update_member_permissions

    The owner cannot modify their own permissions. Future releases will allow delegation via CAN_MANAGE_PERMISSIONS.

4.5 Migration

    For existing databases, run migration_member_permissions.sql to add the permissions column and grant all permissions to existing server owners.

============================================
SECTION 5: INVITE SYSTEM
============================================

Invites are never permanent. Every invite has a mandatory expiration time with a maximum of 12 months (31536000 seconds). The number of uses per invite is also configurable. Only members with CAN_CREATE_INVITES permission or the server owner can create invites.

5.1 Configuration

    BackendSettings.ini parameters:

    InviteDefaultMaxUses            Default 1000
        Number of times an invite can be used when not specified in the API call.

    InviteDefaultExpiresInSeconds   Default 2592000 (30 days)
        Default invite lifetime when not specified in the API call.

    InviteMaxExpiresInSeconds       Default 31536000 (12 months)
        Hard cap on invite lifetime. Values above this are clamped down to this maximum.

    MaxInvitesPerServer             Default 10
        Maximum number of active (non-expired) invites per server.

    InviteCreateLimitPerHour        Default 20
        Maximum number of invite codes an IP can create per hour.

    InviteUseLimitPerHour           Default 30
        Maximum number of invite use attempts (successful or failed) per IP per hour.

    InviteAbuseMaxAttempts          Default 10
        Number of failed invite attempts from a single IP before a temporary ban is applied.

    InviteAbuseWindowSeconds        Default 120
        Rolling window in seconds for counting failed invite attempts. Attempts older than this window are not counted.

    InviteAbuseBanDurationSeconds   Default 3600 (1 hour)
        Duration of the temporary ban when an IP exceeds the max attempts within the window.

5.2 Creating an Invite

    Use the server_create_invite WebSocket message. You can optionally specify:

        max_uses           Custom usage limit (max 1000 by default, or set higher via config)
        expires_in_seconds Custom lifetime in seconds (max 31536000 = 12 months)

    If not specified, backend defaults from BackendSettings.ini are applied.

    Creating invites is also subject to an hourly per-IP rate limit (default 20/hour)
    to prevent flooding. See Section 5.6.

    Example: Create an invite valid for 7 days with 50 max uses:
        {
            "type": "server_create_invite",
            "data": {
                "server_id": "123456789",
                "max_uses": 50,
                "expires_in_seconds": 604800
            }
        }

5.3 Invite Lifecycle

    1. Authorized server member with CAN_CREATE_INVITES calls server_create_invite with optional max_uses and expires_in_seconds.
    2. Server generates a random invite code and stores it in the database with expiration and usage limits.
    3. New user calls server_join_invite with the code.
    4. Server atomically checks: code exists, not expired (expires_at is in the future), usage count below max_uses.
    5. If valid, user is added to the server with zero special permissions and usage count increments.
    6. If invalid (expired or max used), an error is returned.

5.4 Listing Invites

    Use server_list_invites to view all invites for a server. Results are paginated with start/count
    and include each invite's code, creator, usage stats, and timestamps. Requires CAN_CREATE_INVITES
    permission or server owner.

    Example: Get first page of 20 invites:
        {
            "type": "server_list_invites",
            "data": {
                "server_id": "123456789",
                "start": 0,
                "count": 20
            }
        }

    Response includes total count for building pagination UI:
        {
            "type": "server_invites_list",
            "data": {
                "server_id": 123456789,
                "start": 0,
                "count": 20,
                "total": 47,
                "invites": [ ... ]
            }
        }

5.5 Deleting Invites

    Use server_delete_invite to permanently remove an invite by its code. Once deleted, the code
    can no longer be used to join the server. Requires CAN_CREATE_INVITES permission or server owner.

    Example:
        {
            "type": "server_delete_invite",
            "data": {
                "server_id": "123456789",
                "invite_code": "aB3xK7mQ2p"
            }
        }

5.6 Invite Rate Limiting (Two Layers)

    Invite operations are protected by two independent rate limiting layers:

    Layer 1 — Hourly Rate Limits (simple per-IP counters):

        Creating invites:  max 20 invites created per IP per hour.
        Using invites:     max 30 invite use attempts per IP per hour (both successful and failed).

        These limits reset every RateLimitTimeToClearInMins (default 60 minutes).
        Exceeding the limit returns "invite create rate limit exceeded" or
        "invite use rate limit exceeded".

        Configuration:
            InviteCreateLimitPerHour = 20
            InviteUseLimitPerHour    = 30

    Layer 2 — Abuse Detection (rolling-window + IP ban):

        To prevent brute force guessing of invite codes, the server tracks failed
        invite attempts per IP address:

        1. When a user submits an invalid, expired, or depleted invite code:
           a. The failure is recorded against the clients IP address.
           b. Failures are counted within a rolling window (default 120 seconds).
           c. If the count reaches the maximum allowed (default 10), the IP is banned.
        2. When banned, all subsequent invite attempts from that IP return an abuse ban error.
        3. The ban lasts for the configured duration (default 3600 seconds = 1 hour).
        4. On a successful invite join, the failure counter for that IP is reset.
        5. Periodic cleanup removes expired bans and stale records every 5 minutes.

        Configuration:
            InviteAbuseMaxAttempts = 10
            InviteAbuseWindowSeconds = 120
            InviteAbuseBanDurationSeconds = 3600

    Both layers coexist. Layer 1 stops broad flooding; Layer 2 catches aggressive
    brute-forcing patterns.

5.7 Migration

    For existing databases, run migration_invite_limits.sql to add the required columns (max_uses, current_uses) and ensure expires_at is NOT NULL with a 30 day default. Also run migration_member_permissions.sql to add the permissions column.

============================================
SECTION 6: ERROR HANDLING
============================================

REST API errors return HTTP status codes:

    200  Success
    204  No content
    400  Bad request, invalid input
    401  Unauthorized, invalid token
    403  Forbidden, wrong credentials
    429  Too many requests, rate limited
    500  Internal server error

WebSocket errors return:

    type  error
    data:
        message  Error description

Common WebSocket error messages:

    missing type              Message has no type field
    missing data              Message has no data field
    not authenticated         Session token invalid or missing
    not a member of this server User is not in the specified server
    message too large         Content exceeds maximum message size
    invalid or expired invite code  Invite code is not valid
    abuse ban: too many failed invite attempts. Try again later.  IP banned for too many wrong invite codes
    invite create rate limit exceeded  IP has exceeded the hourly invite creation limit (default 20/hour)
    invite use rate limit exceeded     IP has exceeded the hourly invite use limit (default 30/hour)
    service abuse             Specific operation rate limit exceeded (login, register, server creation)
    Global rate limit exceeded    Two-tier: unauthenticated IP has exceeded 300 req/hr (Tier 1) OR authenticated UserID has exceeded 2000 req/hr (Tier 2)
    failed to create server     Server creation failed
    failed to join server       Server join operation failed
    failed to leave server      Server leave operation failed
    failed to move channel    Channel reorder operation failed (channel not found or DB error)
    failed to reorder channels  Batch channel reorder failed (invalid channel_ids array, wrong count, or DB error)
    failed to delete channel or channel not found  Channel deletion failed
    failed to rename channel or channel not found  Channel rename failed (channel not found, empty name, or DB error)
    permission denied: you lack CAN_CREATE_INVITES permission            User needs invite creation permission
    permission denied: you lack CAN_MANAGE_CHANNELS permission           User needs channel management permission
    permission denied: you lack CAN_KICK_MEMBERS permission              User needs kick permission
    permission denied: only the server owner can manage member permissions  Not the server owner
    cannot modify the server owners permissions  Owner permissions are immutable
    cannot kick the server owner               Owner cannot be kicked
    cannot kick yourself                       Self-kick not allowed
    target user is not a member of this server  Target user not found in server
    failed to kick user from server            Kick operation failed
    invite not found or already deleted  Invite code does not exist or was already deleted

============================================
SECTION 7: RATE LIMITING
============================================

Squirrel Communicator uses a layered rate limiting strategy to protect against abuse.

7.1 Two-Tier Global Rate Limiting

    All requests are classified as either unauthenticated (no valid session token) or
    authenticated (valid session token). Each tier has its own rate limit:

    Tier 1 — Unauthenticated Requests (per-IP):
        Applied to requests without a valid auth_token cookie (e.g., /login, /register,
        static assets, any REST call without a valid session).
        Default:  300 requests per hour per IP
        Config:   UnauthenticatedRequestsPerHour in BackendSettings.ini
        Reset:    Counters reset every RateLimitTimeToClearInMins (default 60 minutes)
        Scope:    Applied in CrowAppMiddleware::before_handle (REST) for requests
                  where the auth_token cookie is missing or invalid.
        Error:    HTTP 429 "Global rate limit exceeded"

    Tier 2 — Authenticated Requests (per-UserID):
        Applied to REST requests with a valid auth_token cookie AND all WebSocket
        messages. Each authenticated user gets their own independent quota.
        50 users behind the same IP address get 50 separate quotas,
        totaling 50 × 2000 = 100,000 req/h for that IP.
        Default:  2000 requests per hour per UserID
        Config:   AuthenticatedRequestsPerHour in BackendSettings.ini
        Reset:    Counters reset every RateLimitTimeToClearInMins (default 60 minutes)
        Scope:    Applied in CrowAppMiddleware::before_handle (REST with valid token)
                  and FSocket::OnMessageReceived_TEXT (all WebSocket messages).
        Error:    HTTP 429 or WebSocket error "Global rate limit exceeded"

    This two-tier design ensures that:
    - Unauthenticated endpoints are protected from port scanning and brute-force.
    - Authenticated users behind shared IPs (NAT, office, school) are never unfairly
      rate-limited by other users on the same network.
    - WebSocket traffic has a generous per-user quota since connections are always
      authenticated.

7.2 Specific Operation Rate Limits

    Certain sensitive operations have stricter per-IP limits that apply
    independently of the two-tier global limits above:

    Operation         Limit (per hour)  Config key
    Authentication    55                RateLimitNumberPerIP
    Password reset    25                PasswordRateLimitNumberPerIP
    Server creation   2                 CreateServerRateLimitNumberPerIP
    Registration      10                RegisterAccountLimitPerHour

    These limits are much stricter and target specific attack vectors (credential
    stuffing, reset spam, server flooding). They apply per-IP regardless of
    authentication status.

7.3 Invite Hourly Rate Limits

    Invite operations have their own per-IP hourly caps, separate from the two-tier
    global limits and the specific operation limits:

    Operation                Limit (per hour)  Config key
    Invite creation          20                InviteCreateLimitPerHour
    Invite use attempts      30                InviteUseLimitPerHour

    See Section 5.6 for the two-layer invite protection system (hourly limits +
    rolling-window abuse detection with IP bans).

7.4 Invite Abuse Protection (Rolling Window + Ban)

    Separate from the hourly invite limits above. See Section 5.6 for the invite-specific
    IP ban system that triggers after 10 failed invite attempts in a 120 second rolling
    window, resulting in a 1 hour IP ban.

7.5 Configuration Summary

    BackendSettings.ini parameters:

    --- Two-tier global rate limiting ---
    UnauthenticatedRequestsPerHour  Default 300
        Maximum requests per IP per hour for unauthenticated REST calls.
        Protects /login, /register, and other endpoints without a session token.

    AuthenticatedRequestsPerHour    Default 2000
        Maximum requests per UserID per hour for authenticated REST calls
        and all WebSocket messages. Each user gets their own independent quota.

    RateLimitTimeToClearInMins      Default 60
        How often all rate limit counters reset (in minutes).

    --- Specific operation limits (per IP) ---
    RateLimitNumberPerIP            Default 55
        Max login/register attempts per IP per window.

    PasswordRateLimitNumberPerIP    Default 25
        Max password reset requests per IP per window.

    CreateServerRateLimitNumberPerIP  Default 2
        Max server creation requests per IP per window.

    RegisterAccountLimitPerHour      Default 10
        Max new account registrations per IP per hour.

    --- Invite hourly rate limits (per IP) ---
    InviteCreateLimitPerHour        Default 20
        Max invite codes an IP can create per hour.

    InviteUseLimitPerHour           Default 30
        Max invite use attempts per IP per hour.

    --- Invite abuse protection (rolling window + ban) ---
    InviteAbuseMaxAttempts          Default 10
        Failed invite attempts before temporary ban.

    InviteAbuseWindowSeconds        Default 120
        Rolling window for counting failed invite attempts.

    InviteAbuseBanDurationSeconds   Default 3600
        Duration of the invite abuse ban in seconds.

============================================
SECTION 8: CHANNEL MANAGEMENT
============================================

Channels within a server have a position field that determines their display order.
Lower position values appear first in the channel list.

8.1 Channel Position

    When channels are returned in any server object (via server_created, server_list,
    server_joined, etc.), they are always sorted by position ascending. Each channel
    object includes its position:

        {
            "channel_id": "42",
            "channel_name": "general",
            "channel_type": "text",
            "position": 0
        }

    The frontend should display channels in the order they are received — the server
    guarantees correct ordering.

8.2 Creating Channels

    New channels created via create_channel are auto-assigned the next available position
    (highest existing position + 1), so they appear at the bottom of the list by default.

8.3 Reordering Channels

    Use move_channel to change a channel's position. The new_position parameter is
    clamped to the valid range [0, channel_count - 1]. All channels are renumbered
    sequentially (0, 1, 2, ...) after the move to eliminate position gaps.

    Requires CAN_MANAGE_CHANNELS permission (or server owner).

    The move is broadcast to all server members via server_channel_moved so every
    connected client can update their channel list in real time.

8.4 Renaming Channels

    Use rename_channel to change a channel's name. The new name must not be empty.
    The change is persisted to the database and reflected in the in-memory cache
    immediately.

    Requires CAN_MANAGE_CHANNELS permission (or server owner).

    The rename is broadcast to all server members via server_channel_renamed so every
    connected client can update their channel list in real time.

    Example:
        {
            "type": "rename_channel",
            "data": {
                "server_id": "123456789",
                "channel_id": "42",
                "new_name": "general-chat"
            }
        }

8.5 Deleting Channels

    Use delete_channel to permanently remove a channel and all its messages.
    After deletion, remaining channels are renumbered to eliminate position gaps.

    Requires CAN_MANAGE_CHANNELS permission (or server owner).

    The deletion is broadcast to all server members via server_channel_deleted so every
    connected client can remove the channel from their UI.

8.6 Database Schema

    The server_channels table has a position column (INT NOT NULL DEFAULT 0)
    used for ordering. For existing databases, run migration_channel_position.sql
    to add this column and backfill positions for existing channels.

============================================
SECTION 9: MESSAGE ENCRYPTION AT REST
============================================

Messages (private conversation messages and server channel messages) may be
stored encrypted at rest. Each message row carries an is_encrypted flag
indicating whether its text/content field holds ciphertext.

When the server serves a message to a client, it attempts to decrypt any
encrypted message using the configured server-side key:

    - On success, the client receives the decrypted plaintext.
    - If the message is not encrypted, the plaintext is returned as-is.
    - If decryption fails (the key is missing, the key is mismatched, or the
      data is corrupted), the server returns the placeholder string:

          [ENCRYPTED - INVALID KEY]

      instead of the raw ciphertext. Exposing the raw ciphertext could aid
      breaking the encryption key, so it is never sent to the client.

The placeholder can appear in these message content fields:

    - Conversation messages:   the message field (Section 3.6 Message Object).
    - Server channel messages: the content field (server_message broadcast,
      server_messages and get_server_messages responses).

The placeholder is plain ASCII text and requires no special handling beyond
being displayed as-is by the client. Clients should treat it as an indication
that the message could not be decrypted (e.g., due to a server-side key
mismatch after a key rotation or database migration).
