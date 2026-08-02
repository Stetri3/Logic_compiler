#pragma once
#include <cstdint>
#include <string_view>
#include "hashing.h"
#include <memory>


enum class MacroType : uint8_t {
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
    Param,
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
    case "#param"_h:    return MacroType::Param;
    default:            return MacroType::Unknown;
    }
}

struct MacroDef {
    uint32_t offset;
    uint32_t length;
    //2 pad bytes
    MacroType type;
};