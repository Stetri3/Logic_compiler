#pragma once
#include <cstdint>
#include <string_view>
#include "hashing.h"
#include <memory>


enum class MacroType : uint8_t {
    None,
    Unknown,
    Define,
    Define_M,
    Include,
    If,
    Ifndef,
    Else,
    Elif,
    Endif,
    Enddef,
    Skip,
    Import,
    Param,
};

[[nodiscard]] constexpr MacroType macro_hash_match(std::string_view token) noexcept {
    switch (hash_directive(token)) {
    case "#define"_h:   return MacroType::Define;
    case "#define_m"_h: return MacroType::Define_M;
    case "#if"_h:       return MacroType::If;
    case "#ifndef"_h:   return MacroType::Ifndef;
    case "#else"_h:     return MacroType::Else;
    case "#elif"_h:     return MacroType::Elif;
    case "#endif"_h:    return MacroType::Endif;
    case "#enddef"_h:    return MacroType::Enddef;
    case "#include"_h:  return MacroType::Include;
    case "#skip"_h:     return MacroType::Skip;
    case "#import"_h:   return MacroType::Import;
    case "#param"_h:    return MacroType::Param;
    default:            return MacroType::Unknown;
    }
}

[[nodiscard]] inline constexpr uint64_t char_to_6bit(char c) noexcept {
    return IDENT_CHAR_LUT[static_cast<uint8_t>(c)];
}

[[nodiscard]] constexpr uint64_t macro_name_pack(std::string_view name) noexcept {
    //packs 10 chars in an uint64_t, plus extra length delimiter

    const bool l_of = name.length() >= (0xFu + 10); //check if the extra length would reach sentinel (0xF)
    const uint8_t toPack = name.length() < 10 ? static_cast<uint8_t>(name.length()) : 10u; //amount of chars to pack (up to 10)

    //extra length or sent value
    uint64_t packed_v = l_of ? 0xFULL << 60 : static_cast<uint64_t>(name.length() - toPack) << 60; 
    //notice length - toPack is fixed between 0 and 0xE

    for (uint8_t i = 0; i < toPack; ++i) {
        packed_v |= char_to_6bit(name[i]) << (6 * i); //LE
    }
    return packed_v;
}

constexpr uint8_t check_packing(uint64_t name_h, std::string_view test) noexcept {
    if (macro_name_pack(test) == name_h) return 3;
    if ((name_h >> 60) + 10u == test.length()) return 1;
    return 0;
}

[[nodiscard]] constexpr bool possible_match(uint64_t name_h, uint64_t test_h) {
    if (name_h == test_h) return true;
    //check if test is possible to be the starting part of name
    //ex. name = "MACRO" test = "MA"
}


//unsigned int aliases
//Just for readability
//P prefix stands for Preprocessor relative 
using POffset = uint32_t;
using PCursor = uint32_t;
using PLength = uint32_t;

static constexpr uint32_t POffsetMax = UINT32_MAX;
static constexpr uint32_t PCursorMax = UINT32_MAX;
static constexpr uint32_t PLengthMax = UINT32_MAX;

// Coordinate System Algebra:
// - POffset : Global Point  (Absolute position in source file)
// - PCursor : Local Index   (Position relative to frame origin)
// - PLength : Distance      (Origin-independent size or delta)
//
// Creating / Conversions:
//   POffset_target - POffset_origin = PCursor  (Creates local cursor from frame origin)
//   PCursor + POffset_origin        = POffset  (Promotes local cursor to global position)
//
// Operations:
//   POffset - POffset = PLength  (Distance between arbitrary global points)
//   PCursor - PCursor = PLength  (Distance between two local cursors)
//
//   PCursor ± PLength = PCursor  (Move within local frame)
//   POffset ± PLength = POffset  (Move within global space)
//   PLength ± PLength = PLength  (Combine distances)
//
// Undefined (UD):
//   POffset + POffset (Nonsense) | PCursor + PCursor (Nonsense)