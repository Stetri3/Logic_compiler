#pragma once
#include <cstdint>
#include <string_view>
#include "hashing.h"

enum class MacroType : uint16_t {
    None,
    Unknown,
    Define,
    DefineM,
    Enddef,
    Include,
    If,
    Ifndef,
    Skip,
    Import,
};

[[nodiscard]] constexpr MacroType macro_hash_match(std::string_view token) noexcept {
    switch (hash_directive(token)) {
    case "#define"_h:   return MacroType::Define;
    case "#define_m"_h: return MacroType::DefineM;
    case "#enddef"_h:   return MacroType::Enddef;
    case "#if"_h:       return MacroType::If;
    case "#ifndef"_h:   return MacroType::Ifndef;
    case "#include"_h:  return MacroType::Include;
    case "#skip"_h:     return MacroType::Skip;
    case "#import"_h:   return MacroType::Import;
    default:            return MacroType::Unknown;
    }
}

struct PPToken {
    uint64_t line;
    MacroType type;
};