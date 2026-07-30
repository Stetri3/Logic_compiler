#include "preprocessor.h"

#include <vector>
#include <string_view>
#include <memory>
#include <cstring>
#include <stdexcept>
#include "hashing.h"

SourceView Preprocessor::loadView(const char* pathrel) {
    return file_mgr.loadFile(pathrel);
}

void Preprocessor::process() {
    while (cursor < source.length()) {
        ol();

        if (cursor >= source.length()) break;

        std::string_view token = next_token();

        if (!token.empty() && token.front() == '#') {
            MacroType type = macro_hash_match(token);
            process_directive(type);
        }
        else {
            // Append payload token directly to contiguous buffer
            if (!token.empty()) {
                output_buffer.insert(output_buffer.end(), token.begin(), token.end());
                out_cursor = output_buffer.size();
            }
        }
    }
}

std::string_view Preprocessor::next_token() {
    const char* data = source.get(); // Assumes source has a raw pointer accessor
    const uint64_t len = source.length();

    if (cursor >= len) {
        return {};
    }

    const uint64_t start = cursor;
    const char c = data[cursor];

    // 1. Directive symbol
    if (c == '#') {
        ++cursor;
        return std::string_view(data + start, 1);
    }

    // 2. Identifiers / Keywords / Directives names (e.g. "define", "my_var_1")
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_') {
        while (cursor < len) {
            const char ch = data[cursor];
            if ((ch >= 'a' && ch <= 'z') ||
                (ch >= 'A' && ch <= 'Z') ||
                (ch >= '0' && ch <= '9') ||
                ch == '_') {
                ++cursor;
            }
            else {
                break;
            }
        }
        return std::string_view(data + start, cursor - start);
    }

    // 3. String / Character literals (handles escaped quotes)
    if (c == '"' || c == '\'') {
        const char quote = c;
        ++cursor; // Consume opening quote

        while (cursor < len) {
            if (data[cursor] == '\\' && (cursor + 1) < len) {
                cursor += 2; // Skip escaped character
            }
            else if (data[cursor] == quote) {
                ++cursor; // Consume closing quote
                break;
            }
            else {
                ++cursor;
            }
        }
        return std::string_view(data + start, cursor - start);
    }

    // 4. Numeric literals
    if (c >= '0' && c <= '9') {
        while (cursor < len) {
            const char ch = data[cursor];
            if ((ch >= '0' && ch <= '9') || ch == '.' || ch == 'x' || ch == 'X' ||
                (ch >= 'a' && ch <= 'f') || (ch >= 'A' && ch <= 'F')) {
                ++cursor;
            }
            else {
                break;
            }
        }
        return std::string_view(data + start, cursor - start);
    }

    // 5. Punctuation / Single-character operators / Symbol fallback
    ++cursor;
    return std::string_view(data + start, 1);
}

void Preprocessor::process_directive(MacroType type) {
    switch (type) {
    case MacroType::Define: {
        break;
    }
    case MacroType::DefineM: {
        break;
    }
    case MacroType::Enddef: {
        break;
    }
    case MacroType::Include: {
        break;
    }
    case MacroType::If: {
        break;
    }
    case MacroType::Ifndef: {
        break;
    }
    case MacroType::Skip: {
        break;
    }
    case MacroType::Import: {
        break;
    }
    case MacroType::Param: {
        break;
    }
    case MacroType::Unknown:
    case MacroType::None:
    default:
        break;
    }
}