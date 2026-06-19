// Engine compatibility layer for standalone migration
// Provides type aliases matching Engine's naming conventions

#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>
#include <deque>
#include <unordered_map>
#include <map>
#include <string>
#include <functional>
#include <optional>
#include <algorithm>
#include <mutex>
#include <shared_mutex>
#include <chrono>
#include <cassert>

// ============================================================================
// Integer type aliases (Engine convention)
// ============================================================================
using Uint8 = uint8_t;
using Uint16 = uint16_t;
using Uint32 = uint32_t;
using Uint64 = uint64_t;
using int32 = int32_t;
using uint8 = uint8_t;
using uint16 = uint16_t;
using uint32 = uint32_t;

// ============================================================================
// Remove Engine export macros
// ============================================================================
#define ENGINE_API
#define EXPORT_API
#define PROJECT_API
#ifndef NO_DISCARD
#define NO_DISCARD [[nodiscard]]
#endif

// ============================================================================
// ENSURE_VALID macro (replaces Engine's assertion)
// ============================================================================
#define ENSURE_VALID(Expr) assert(Expr)

// ============================================================================
// CArray - std::vector with Engine-style methods
// ============================================================================
template<typename T>
class CArray : public std::vector<T>
{
public:
	using std::vector<T>::vector;

	// Engine's CArray.Vector member access - provides direct access to the underlying vector
	std::vector<T>& Vector() { return *this; }
	const std::vector<T>& Vector() const { return *this; }

	void Push(const T& Value) { this->push_back(Value); }
	void Push(T&& Value) { this->push_back(std::move(Value)); }

	void InsertAt(size_t Index, const T& Value)
	{
		this->insert(this->begin() + Index, Value);
	}

	void RemoveAt(size_t Index)
	{
		if (Index < this->size())
		{
			this->erase(this->begin() + Index);
		}
	}

	void Clear() { this->clear(); }
	size_t Size() const { return this->size(); }
	bool IsEmpty() const { return this->empty(); }

	bool Contains(const T& Value) const
	{
		return std::find(this->begin(), this->end(), Value) != this->end();
	}

	bool IsValidIndex(size_t Index) const
	{
		return Index < this->size();
	}

	void SetNum(size_t Num)
	{
		this->resize(Num);
	}
};

// ============================================================================
// CUnorderedMap - std::unordered_map with Engine-style methods
// The Engine version accepts an optional 3rd "Hash" template param
// which we ignore (use default hash)
// ============================================================================
template<typename K, typename V, typename... Extra>
class CUnorderedMap : public std::unordered_map<K, V>
{
public:
	using std::unordered_map<K, V>::unordered_map;

	// Engine's CUnorderedMap.Map member access - provides direct access
	std::unordered_map<K, V>& Map() { return *this; }
	const std::unordered_map<K, V>& Map() const { return *this; }

	bool ContainsKey(const K& Key) const
	{
		return this->find(Key) != this->end();
	}

	std::optional<V> FindValueByKey(const K& Key) const
	{
		auto It = this->find(Key);
		if (It != this->end())
		{
			return It->second;
		}
		return std::nullopt;
	}

	template<typename... Args>
	void Emplace(Args&&... args)
	{
		this->emplace(std::forward<Args>(args)...);
	}

	// Engine's InsertOrAssign
	void InsertOrAssign(const K& Key, const V& Value)
	{
		this->insert_or_assign(Key, Value);
	}

	void InsertOrAssign(const K& Key, V&& Value)
	{
		this->insert_or_assign(Key, std::move(Value));
	}

	bool Remove(const K& Key)
	{
		return this->erase(Key) > 0;
	}

	size_t Size() const { return this->size(); }
};

// ============================================================================
// CMap - std::map with Engine-style methods
// ============================================================================
template<typename K, typename V>
class CMap : public std::map<K, V>
{
public:
	using std::map<K, V>::map;
};

// ============================================================================
// CDeque - std::deque with Engine-style methods
// ============================================================================
template<typename T>
class CDeque : public std::deque<T>
{
public:
	using std::deque<T>::deque;

	// Engine's CDeque.Deque member access - provides direct access
	std::deque<T>& Deque() { return *this; }
	const std::deque<T>& Deque() const { return *this; }

	void PushBack(const T& Value) { this->push_back(Value); }
	void PushBack(T&& Value) { this->push_back(std::move(Value)); }
	void PushFront(const T& Value) { this->push_front(Value); }
	void PushFront(T&& Value) { this->push_front(std::move(Value)); }

	size_t Size() const { return this->size(); }
	void Clear() { this->clear(); }

	/** Get a sub-range of elements as a vector */
	std::vector<T> GetRange(size_t Offset, size_t Count) const
	{
		std::vector<T> Result;
		if (Offset >= this->size()) return Result;

		size_t End = std::min(Offset + Count, this->size());
		Result.reserve(End - Offset);
		for (size_t i = Offset; i < End; ++i)
		{
			Result.push_back((*this)[i]);
		}
		return Result;
	}

	/** Get the first N elements */
	std::vector<T> PeekFirst(size_t Count) const
	{
		return GetRange(0, Count);
	}
};

// ============================================================================
// FFunctorLambda - simplified to std::function wrapper
// The Engine version is a complex class hierarchy, but all usage in
// Communicator just needs callable semantics
// ============================================================================
template<typename TReturnType, typename... TInParams>
class FFunctorLambda
{
public:
	template<typename TTypeAuto>
	FFunctorLambda(TTypeAuto InFunction)
		: Function(std::move(InFunction))
	{
	}

	FFunctorLambda(std::function<TReturnType(TInParams ...)>& InFunction)
		: Function(std::move(InFunction))
	{
	}

	FFunctorLambda(std::function<TReturnType(TInParams ...)>&& InFunction)
		: Function(std::move(InFunction))
	{
	}

	TReturnType operator()(TInParams... Params)
	{
		return Function(Params ...);
	}

	bool IsValid() const { return static_cast<bool>(Function); }

protected:
	std::function<TReturnType(TInParams ...)> Function;
};

// ============================================================================
// FUtil - basic utility functions replacing Engine's FUtil
// ============================================================================
class FUtil
{
public:
	static Uint64 GetSeconds()
	{
		return static_cast<Uint64>(
			std::chrono::duration_cast<std::chrono::seconds>(
				std::chrono::system_clock::now().time_since_epoch()
			).count()
		);
	}
};

// ============================================================================
// FBitFlipping - from Engine's EncryptionUtil, used in SessionManager
// Maps to SQRLLBitFlipping from new Encryption library
// ============================================================================
// Forward declaration - actual impl comes from ThirdParty/Encryption
class SQRLLBitFlipping;
using FBitFlipping = SQRLLBitFlipping;

// ============================================================================
// IniReader type aliases - map Engine names to standalone SQRLL names
// ============================================================================
class SQRLLIniObject;
struct SQRLLIniField;
using FIniObject = SQRLLIniObject;
using FIniField = SQRLLIniField;

// ============================================================================
// Encryption type aliases - map Engine names to standalone SQRLL names
// ============================================================================
class SQRLLEncryption;
class SQRLLPredefinedCharsets;
using FEncryptionUtil = SQRLLEncryption;
using FPredefinedCharsets = SQRLLPredefinedCharsets;

// ============================================================================
// FGlobalDefines replacement
// Provides a global pointer to the ProjectEngine instance
// ============================================================================
class FProjectEngine;
struct FGlobalDefines
{
	static inline FProjectEngine* GEngine = nullptr;
};
