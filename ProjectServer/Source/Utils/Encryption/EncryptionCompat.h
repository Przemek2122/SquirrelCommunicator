// Encryption compatibility layer
// Maps Engine encryption names to SQRLL standalone library names

#pragma once

#include "SQRLLEncryption.h"

// ============================================================================
// FEncryptionManager - trivial factory, replicated inline
// ============================================================================
class FEncryptionManager
{
public:
	template<typename TEncryptorClass>
	static std::unique_ptr<TEncryptorClass> CreateEncryptorForPassword()
	{
		return std::make_unique<TEncryptorClass>();
	}
};
