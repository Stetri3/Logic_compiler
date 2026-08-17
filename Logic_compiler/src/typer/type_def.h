#pragma once
#include <cstdint>
#include <cstring>
#include "typeflags.h"
#include "Alloc_optimized.h"
#include "std_types.h"

// file type_def.h (type related definitions)
namespace typer {

    using TypeID = uint32_t;
    using TypeHash = uint64_t; // 32-bit TypeID | 32-bit decoration/mutator flags

    // --- Reserved TypeID Ranges ---
    namespace reserved {
        constexpr TypeID ATOMIC_BASE = 0x00000000u; // Builtin primitives: 0 .. Atomic::__count - 1
        constexpr TypeID USER_BASE = 0x00000020u; // User-defined types start here
        constexpr TypeID SENTINEL = 0xFFFF0000u; // Sentinel / placeholder
        constexpr TypeID ERROR_TYPE = 0xFFFFFFFFu; // Type error / poison
    }

#define ATOMIC_ENUM \
    void_type, \
    bool_type, \
    byte_type, \
    u8_type, u16_type, u32_type, u64_type, \
    i8_type, i16_type, i32_type, i64_type, \
    f32_type, f64_type, \
    first_type

    enum class Atomic : uint8_t {
        ATOMIC_ENUM,
        __count
    };

    enum class TypeKind : uint8_t {
        ATOMIC_ENUM, // 0 .. Atomic::__count - 1
        Plain,       // User-defined structs / types (from definitions)
        Pointer,     // Constructor: Pointee TypeID
        Array        // Constructor: Element TypeID + Length
    };
#undef ATOMIC_ENUM

    static_assert(static_cast<uint8_t>(Atomic::f64_type) == static_cast<uint8_t>(TypeKind::f64_type),
        "Atomic enum discrepancy: element order mismatch!");
    static_assert(static_cast<uint8_t>(Atomic::__count) == static_cast<uint8_t>(TypeKind::Plain),
        "Atomic enum count must strictly match the start of composite TypeKind variants!");

    [[nodiscard]] constexpr bool isAtomic(TypeKind kind) noexcept {
        return static_cast<uint8_t>(kind) < static_cast<uint8_t>(Atomic::__count);
    }

    struct PointerBuilder {
        TypeID pointeeID;
    };

    struct ArrayBuilder {
        uint64_t arraySize;
        TypeID   elementID;
    };

    namespace membermutators {
        using Flag = uint8_t;
        constexpr Flag EMPTYDC = 0;
        constexpr Flag CONSTEXPRDC = 1 << 0;
        constexpr Flag STATICDC = 1 << 1;
        constexpr Flag CONSTDC = 1 << 2;
        constexpr Flag CONSTAC = 1 << 3;
        constexpr Flag PRIVATEAC = 1 << 4;
        constexpr Flag DEFAULTED = 1 << 5;
        constexpr Flag METHOD = 1 << 6;
        constexpr Flag HAS_INIT = 1 << 7;
    }

    // --- Uniform Member Representation (12 Bytes / 3 Words) ---
    struct TypeMember {
        TypeID               typeID;     // 4B: Member type or method return type
        uint16_t             memberID;   // 2B: Symbol / Name identifier (up to 65k unique member names)
        membermutators::Flag decoration; // 1B: Mutation / access flags
        uint8_t              _pad;       // 1B: Explicit alignment padding to maintain 4-byte packing
        uint32_t             initVal;    // 4B: Immediate value, FuncID, or pool offset
    };
    static_assert(sizeof(TypeMember) == 12, "TypeMember must be exactly 12 bytes (3 uint32_t words)!");

    // --- Fast Hot Registry Entry (16 Bytes) ---
    struct TypeLean {
        TypeID   ID;
        uint32_t arenaOffset; // Word offset in TypeArena (0 if no payload)

        // Quick-look packed metadata
        uint64_t typeSize : 48; // Up to 256 TB
        uint64_t kind : 8;  // TypeKind enum
        uint64_t alignment : 8;  // Natural byte alignment (1, 2, 4, 8, 16...)

        [[nodiscard]] constexpr TypeKind getKind() const noexcept {
            return static_cast<TypeKind>(kind);
        }

        [[nodiscard]] constexpr bool hasArenaPayload() const noexcept {
            return arenaOffset != 0;
        }
    };
    static_assert(sizeof(TypeLean) == 16, "TypeLean must be exactly 16 bytes!");

    namespace detail {

        consteval TypeLean make_atomic_typelean(Atomic kind) {
            TypeLean lean{};
            lean.ID = static_cast<TypeID>(kind);
            lean.arenaOffset = 0; // Atomics never allocate in TypeArena
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
            case Atomic::first_type: // Meta 'type' (stores 32-bit TypeID)
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

    // --- Concrete Variable-Length Arena Header (12 Bytes / 3 Words) ---
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
    static_assert(sizeof(TypeContent) == 12, "TypeContent header must be exactly 12 bytes!");

    // --- Arena Bump Allocator ---
    class TypeArena {
    public:
        OsPagedVector<uint32_t> raw;

        TypeArena() {
            // Reserve word offset 0 so that arenaOffset == 0 unambiguously means "no payload"
            raw.push_back(0);
        }

        [[nodiscard]] const TypeContent* get(uint32_t wordOffset) const noexcept {
            if (wordOffset == 0 || wordOffset >= raw.size()) return nullptr;
            return reinterpret_cast<const TypeContent*>(raw.data() + wordOffset);
        }

        [[nodiscard]] TypeContent* get(uint32_t wordOffset) noexcept {
            if (wordOffset == 0 || wordOffset >= raw.size()) return nullptr;
            return reinterpret_cast<TypeContent*>(raw.data() + wordOffset);
        }

        // Pushes a new TypeContent record and returns its word offset
        uint32_t push(TypeID id, cc::CSpan<uint32_t> flags, cc::CSpan<TypeMember> members) {
            const uint32_t offset = static_cast<uint32_t>(raw.size());
            const uint32_t flagWords = static_cast<uint32_t>(flags.size());
            const uint32_t memberWords = static_cast<uint32_t>(members.size()) * TypeContent::MEMBER_STRIDE_WORDS;
            const uint32_t totalWords = TypeContent::HEADER_WORDS + flagWords + memberWords;

            raw.resize(offset + totalWords);
            uint32_t* dest = raw.data() + offset;

            // 1. Write Header
            auto* header = reinterpret_cast<TypeContent*>(dest);
            header->ID = id;
            header->totalWords = totalWords;
            header->membersWord = TypeContent::HEADER_WORDS + flagWords;

            // 2. Copy Flags
            if (flagWords > 0) {
                std::memcpy(dest + TypeContent::HEADER_WORDS, flags.data(), flagWords * sizeof(uint32_t));
            }

            // 3. Copy Members
            if (memberWords > 0) {
                std::memcpy(dest + header->membersWord, members.data(), memberWords * sizeof(uint32_t));
            }

            return offset;
        }

        // Rollback / pop support for scoped type definitions
        void rollback(uint32_t wordOffset) noexcept {
            if (wordOffset > 0 && wordOffset <= raw.size()) {
                raw.resize(wordOffset);
            }
        }
    };

    // --- Dense Type List (Indexed directly by TypeID) ---
    struct TypeList {
        OsPagedVector<TypeLean> raw;

        TypeList() {
            initPrimitives();
        }

        void initPrimitives() {
            constexpr auto count = static_cast<uint8_t>(Atomic::__count);
            raw.resize(count);
            for (uint8_t i = 0; i < count; ++i) {
                raw[i] = detail::ATOMIC_TYPELEANS[i];
            }
        }

        [[nodiscard]] const TypeLean& operator[](TypeID id) const noexcept {
            return raw[id];
        }

        [[nodiscard]] TypeLean& operator[](TypeID id) noexcept {
            return raw[id];
        }

        // Registers a user-defined type and returns its TypeID
        TypeID push(TypeLean lean) {
            const auto id = static_cast<TypeID>(raw.size());
            lean.ID = id;
            raw.push_back(lean);
            return id;
        }

        // Rollback support for scoped popping
        void rollback(uint32_t count) noexcept {
            if (count >= static_cast<uint32_t>(Atomic::__count) && count <= raw.size()) {
                raw.resize(count);
            }
        }
    };

} // namespace typer