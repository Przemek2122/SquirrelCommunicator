-- ============================================================================
-- SquirrelCommunicator - Servers (Rooms) Feature Database Schema
-- ============================================================================
-- Run this against your MySQL/MariaDB database to add the tables needed
-- for the servers/rooms feature.
--
-- Tables created:
--   1. servers          - Core server/room definitions
--   2. server_channels  - Channels within servers (text + voice) with position ordering
--   3. server_members   - Server membership (user_id to server_id) with permissions
--   4. server_messages  - Messages in server text channels
--   5. server_invites   - Invite codes for joining servers
-- ============================================================================

-- 1. Servers table
CREATE TABLE IF NOT EXISTS servers (
    id          BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
    name        VARCHAR(128)    NOT NULL,
    owner_id    BIGINT UNSIGNED NOT NULL,
    token       VARCHAR(128)    NOT NULL,
    created_at  VARCHAR(32)     NOT NULL DEFAULT '',

    INDEX idx_servers_owner (owner_id),
    INDEX idx_servers_token (token),

    CONSTRAINT fk_servers_owner
        FOREIGN KEY (owner_id) REFERENCES users(id)
        ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- 2. Server channels table
CREATE TABLE IF NOT EXISTS server_channels (
    id          BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
    server_id   BIGINT UNSIGNED NOT NULL,
    name        VARCHAR(128)    NOT NULL,
    type        ENUM('text', 'voice') NOT NULL DEFAULT 'text',
    position    INT             NOT NULL DEFAULT 0,

    INDEX idx_channels_server (server_id),
    INDEX idx_channels_position (server_id, position),

    CONSTRAINT fk_channels_server
        FOREIGN KEY (server_id) REFERENCES servers(id)
        ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- 3. Server members table (with permissions bitfield)
CREATE TABLE IF NOT EXISTS server_members (
    server_id   BIGINT UNSIGNED NOT NULL,
    user_id     BIGINT UNSIGNED NOT NULL,
    permissions BIGINT UNSIGNED NOT NULL DEFAULT 0,
    joined_at   TIMESTAMP       NOT NULL DEFAULT CURRENT_TIMESTAMP,

    PRIMARY KEY (server_id, user_id),
    INDEX idx_members_user (user_id),

    CONSTRAINT fk_members_server
        FOREIGN KEY (server_id) REFERENCES servers(id)
        ON DELETE CASCADE,
    CONSTRAINT fk_members_user
        FOREIGN KEY (user_id) REFERENCES users(id)
        ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- 4. Server messages table
-- sender_id is NULLable with ON DELETE SET NULL so messages survive user deletion.
-- The application-layer SELECT uses COALESCE(u.username, 'Unknown') as fallback.
CREATE TABLE IF NOT EXISTS server_messages (
    id          BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
    channel_id  BIGINT UNSIGNED NOT NULL,
    sender_id   BIGINT UNSIGNED NULL DEFAULT NULL,
    content     TEXT            NOT NULL,
    created_at  VARCHAR(32)     NOT NULL DEFAULT '',

    INDEX idx_messages_channel (channel_id),
    INDEX idx_messages_sender (sender_id),
    INDEX idx_messages_created (channel_id, id),

    CONSTRAINT fk_messages_channel
        FOREIGN KEY (channel_id) REFERENCES server_channels(id)
        ON DELETE CASCADE,
    CONSTRAINT fk_messages_sender
        FOREIGN KEY (sender_id) REFERENCES users(id)
        ON DELETE SET NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE INDEX idx_server_messages_channel_id_id ON server_messages(channel_id, id DESC);

-- 5. Server invites table
-- max_uses: maximum number of times this invite can be consumed (default 1000)
-- current_uses: how many times it has been used so far
-- expires_at: invite expiration timestamp (never permanent, max 12 months)
CREATE TABLE IF NOT EXISTS server_invites (
    invite_code  VARCHAR(16)     NOT NULL PRIMARY KEY,
    server_id    BIGINT UNSIGNED NOT NULL,
    created_by   BIGINT UNSIGNED NOT NULL,
    max_uses     INT UNSIGNED    NOT NULL DEFAULT 1000,
    current_uses INT UNSIGNED    NOT NULL DEFAULT 0,
    created_at   TIMESTAMP       NOT NULL DEFAULT CURRENT_TIMESTAMP,
    expires_at   TIMESTAMP       NOT NULL DEFAULT (DATE_ADD(CURRENT_TIMESTAMP, INTERVAL 30 DAY)),

    INDEX idx_invites_server (server_id),
    INDEX idx_invites_expires (expires_at),

    CONSTRAINT fk_invites_server
        FOREIGN KEY (server_id) REFERENCES servers(id)
        ON DELETE CASCADE,
    CONSTRAINT fk_invites_creator
        FOREIGN KEY (created_by) REFERENCES users(id)
        ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
