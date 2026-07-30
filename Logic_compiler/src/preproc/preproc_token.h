#pragma once
#include <cstdint>
#include <string_view>
#include "hashing.h"
#include <memory>


enum class MacroType : uint32_t {
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
    uint64_t line;
    std::unique_ptr<char[]> content;
    MacroType type;
    uint32_t cSize;

    inline static MacroDef make(std::string_view sv, uint64_t line, MacroType type) {
        auto buf = std::make_unique<char[]>(sv.size() + 1);
        std::memcpy(buf.get(), sv.data(), sv.size());
        buf[sv.size()] = '\0';

        return MacroDef{
            line,
            std::move(buf),
            type,
            static_cast<uint32_t>(sv.size()),
        };
    }

    [[nodiscard]] MacroDef dupe() const {
        if (!content)
            return MacroDef{ line, nullptr, type, 0 };

        auto buf = std::make_unique<char[]>(cSize + 1);
        std::memcpy(buf.get(), content.get(), cSize + 1); // Copies content + null terminator in one shot

        return MacroDef{
            line,
            std::move(buf),
            type,
            cSize
        };
    }

    [[nodiscard]] inline std::string_view view() const noexcept {
        return std::string_view(content.get(), cSize);
    }

    [[nodiscard]] inline const char* get() const noexcept {
        return content.get();
    }
};