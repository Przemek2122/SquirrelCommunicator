// Created by https://www.linkedin.com/in/przemek2122/ 2026

#pragma once

#include "EngineCompat.h"
#include <string_view>

/**
 * Discriminates what a chat message actually carries.
 *
 * Previously everything was treated as text (the message body held raw text or,
 * at best, a media URL as plain text). Dedicated types let the backend validate
 * that media messages reference verified content-addressable storage (the image
 * service) instead of arbitrary unverifiable URLs embedded in text.
 *
 * The numeric values are persisted to the `message_type` TINYINT column in both
 * `messages` and `server_messages` tables. Keep them stable: do NOT reorder
 * existing values once deployed.
 */
enum class EMessageType : Uint8
{
	/** Plain text message (backwards compatible default). */
	Text = 0,

	/** Image message; content holds a SHA-256 content hash from the image service. */
	Image = 1,

	/** Animated GIF message; content holds a SHA-256 content hash. */
	Gif = 2,

	/** Video message; content holds a SHA-256 content hash. */
	Video = 3,
};

/**
 * Validates that InValue is a well-formed SHA-256 content hash.
 *
 * Media messages must reference the content-addressable image service by hash.
 * Enforcing the exact 64 hex-char shape server-side is what makes media
 * messages "verified": arbitrary URLs or injected text can never masquerade as
 * a media reference.
 */
inline bool IsValidSha256Hex(const std::string_view InValue)
{
	if (InValue.size() != 64)
	{
		return false;
	}

	for (const char c : InValue)
	{
		const bool bDigit = (c >= '0' && c <= '9');
		const bool bLower = (c >= 'a' && c <= 'f');
		const bool bUpper = (c >= 'A' && c <= 'F');

		if (!bDigit && !bLower && !bUpper)
		{
			return false;
		}
	}

	return true;
}
