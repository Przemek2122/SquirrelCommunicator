-- ============================================================================
-- Migration 0001: add message_type discriminator to chat messages
-- ============================================================================
-- Adds dedicated message types (text / image / gif / video) so media messages
-- reference verified content-addressable storage instead of arbitrary URLs in
-- plain text.
--
-- Applies to BOTH direct messages (`messages`) and server channel messages
-- (`server_messages`). The media payload itself lives in the existing
-- `text` / `content` column (a 64-char SHA-256 content hash); `message_type`
-- just discriminates how the frontend must render it.
--
-- Idempotent: uses ADD COLUMN IF NOT EXISTS (MariaDB 10.0.2+, server is 11.x)
-- so it is safe to re-run.
-- ============================================================================

-- Direct / private messages
ALTER TABLE `messages`
  ADD COLUMN IF NOT EXISTS `message_type` TINYINT UNSIGNED NOT NULL DEFAULT 0 COMMENT '0=text, 1=image, 2=gif, 3=video';

-- Server text-channel messages
ALTER TABLE `server_messages`
  ADD COLUMN IF NOT EXISTS `message_type` TINYINT UNSIGNED NOT NULL DEFAULT 0 COMMENT '0=text, 1=image, 2=gif, 3=video';
