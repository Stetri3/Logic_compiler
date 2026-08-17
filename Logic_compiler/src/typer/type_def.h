#pragma once
#include "typeflags.h"
#include "Alloc_optimized.h"
#include "std_types.h"

//file type_def.h (type related definitions)
namespace typer {
	using TypeID = uint32_t; //>= 0xF000... reserved for builtin types (ex. void = 0xF0000000)
	//>= 0xFFFF0000 reserved for sentinels and errors

	using TypeHash = uint64_t; //typehash has 32 typeId, 32 flags (decoration, values, etc.)

#define ATOMIC_ENUM void_type, \
		bool_type, \
		byte_type, \
		u8_type, u16_type, u32_type, u64_type, \
		i8_type, i16_type, i32_type, i64_type, \
		f32_type, f64_type, \
		first_type


	enum class Atomic {

		ATOMIC_ENUM, 

		__count
	};

	enum class TypeKind : uint8_t {
		ATOMIC_ENUM,  // i32, f64, bool, void, etc.
		Plain,      // from definitions, (type{}, struct {} etc.)
		Pointer,    // Constructor: Pointee TypeID
		Array       // Constructor: Element TypeID + Length
	};
#undef ATOMIC_ENUM
	static_assert(static_cast<uint8_t>(Atomic::f64_type) == static_cast<uint8_t>(TypeKind::f64_type),
		"Atomic enum discrepancy");
	static_assert(static_cast<uint8_t>(Atomic::__count) == static_cast<uint8_t>(TypeKind::Plain),
		"Atomic enum count must strictly match the start of composite TypeKind variants!");

	struct PointerBuilder {
		TypeID ID;
	};

	struct ArrayBuilder {
		uint64_t arraySize;
		TypeID ID;
	};

	namespace membermutators {
		using Flag = uint8_t;
		constexpr Flag EMPTYDC = 0;
		constexpr Flag CONSTEXPRDC = 1u;
		constexpr Flag STATICDC = 1 << 1;
		constexpr Flag CONSTDC = 1 << 2;
		constexpr Flag CONSTAC = 1 << 3;
		constexpr Flag PRIVATEAC = 1 << 4;
		constexpr Flag DEFAULTED = 1 << 5;
		constexpr Flag METHOD = 1 << 6;
	}

	struct TypeMember {
		TypeID typeID; //return type
		uint16_t memberID; //up to 65k members per struct
		membermutators::Flag decoration;
		uint32_t initVal; //pushed into a values arena. If it's a method (from decorators) it's an index of a method instead
		//If sizeof(member type) <= 4, initVal contains the actual bit Value in little endian

	};

	struct TypeLean {
		//Memory info
		TypeID ID;
		uint32_t arenaOffset;
		//Quick look type info
		uint64_t typeSize : 48;
		uint64_t kind : 8;
		uint64_t alignment : 8;

		[[nodiscard]] constexpr TypeKind getKind() const noexcept{
			return static_cast<TypeKind>(kind);
		}
	};

	namespace detail {

		consteval TypeLean make_atomic_typelean(Atomic kind) {
			TypeLean lean{};
			lean.ID = static_cast<TypeID>(kind);
			lean.arenaOffset = 0; // Atomics do not allocate in TypeArena
			lean.kind = static_cast<uint64_t>(kind);

			switch (kind) {
			case Atomic::void_type:
				lean.typeSize = 0;
				lean.alignment = 1; // 1-byte alignment avoids div-by-zero
				break;

			case Atomic::bool_type:
			case Atomic::byte_type:
			case Atomic::u8_type:
			case Atomic::i8_type:
				lean.typeSize = 1;
				lean.alignment = 1;
				break;

			case Atomic::u16_type:
			case Atomic::i16_type:
				lean.typeSize = 2;
				lean.alignment = 2;
				break;

			case Atomic::u32_type:
			case Atomic::i32_type:
			case Atomic::f32_type:
			case Atomic::first_type: // Meta 'type' (stores a 32-bit TypeID)
				lean.typeSize = 4;
				lean.alignment = 4;
				break;

			case Atomic::u64_type:
			case Atomic::i64_type:
			case Atomic::f64_type:
				lean.typeSize = 8;
				lean.alignment = 8;
				break;

			case Atomic::__count:
				// Unreachable sentinel
				break;
			}

			return lean;
		}

		consteval auto make_atomic_typelean_arr() {
			cc::CArray<TypeLean, static_cast<uint8_t>(Atomic::__count)> ret{};
			for (uint8_t i = 0; i < static_cast<uint8_t>(Atomic::__count); ++i) {
				ret[i] = make_atomic_typelean(static_cast<Atomic>(i));
			}
			return ret;
		}

		constexpr auto ATOMIC_TYPELEANS = make_atomic_typelean_arr();

	} // namespace detail

	
	struct TypeContent {
		TypeID   ID;
		uint32_t totalWords;   // Total size of this record in 32-bit words
		uint32_t membersWord;  // Word offset from `this` to start of TypeMember array

		static constexpr uint32_t HEADER_WORDS = 3;
		static constexpr uint32_t MEMBER_STRIDE_WORDS = sizeof(TypeMember) / sizeof(uint32_t); // 3

		// Flags Slice
		[[nodiscard]] constexpr uint32_t getFlagCount() const noexcept {
			return (membersWord > HEADER_WORDS) ? (membersWord - HEADER_WORDS) : 0;
		}

		[[nodiscard]] constexpr cc::CSpan<uint32_t> getFlags() const noexcept {
			const uint32_t* base = reinterpret_cast<const uint32_t*>(this);
			return cc::CSpan<uint32_t>(base + HEADER_WORDS, getFlagCount());
		}

		// Members Slice
		[[nodiscard]] constexpr uint32_t getMemberCount() const noexcept {
			return (totalWords > membersWord) ? ((totalWords - membersWord) / MEMBER_STRIDE_WORDS) : 0;
		}

		[[nodiscard]] constexpr cc::CSpan<TypeMember> getMembers() const noexcept {
			const uint32_t* base = reinterpret_cast<const uint32_t*>(this);
			const auto* membersPtr = reinterpret_cast<const TypeMember*>(base + membersWord);
			return cc::CSpan<TypeMember>(membersPtr, getMemberCount());
		}

		[[nodiscard]] constexpr const TypeMember& getMember(uint32_t index) const noexcept {
			const uint32_t* base = reinterpret_cast<const uint32_t*>(this);
			const auto* membersPtr = reinterpret_cast<const TypeMember*>(base + membersWord);
			return membersPtr[index];
		}
	};

	class TypeArena {
		//optimistically the language should probably have as consequence a behavior such that
		//types can only be pushed/popped, not erased outside of order (scopes preserve definition order)
		OsPagedVector<uint32_t> raw;
		
	};

	struct TypeList {
		//the vector that indexes the types. TypeId accesses this
		OsPagedVector<TypeLean> raw;

	};

}