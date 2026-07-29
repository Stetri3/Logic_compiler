#pragma once
#include <string_view>
#include <cstdint>
#include <array>
#include <memory>
#include <vector>
#include <unordered_set>
#include <cstring>

#include "preproc_token.h"
#include "file_def.h"
#include "hashing.h"
#include "file_manager.h" // Per FileManager



class Preprocessor {
    uint64_t cursor = 0;
    MacroType current_macro = MacroType::None;
    SourceView source;

    FileManager file_mgr;
    std::vector<char> output_buffer; // Allocazione dinamica sicura e contigua
    std::array<SourceView, 64> loadedCache{};
    uint8_t cacheCounter = 0;

    std::unordered_set<std::string_view> defined_macros;

    [[nodiscard]] SourceView loadView(const char* pathrel);

    // Helper interni per il parsing
    void skip_whitespace_and_comments();
    std::string_view next_token();
    std::string_view read_line();
    void process_directive(MacroType type);

public:
    explicit Preprocessor(SourceView src, const char* base_path = ".")
        : source(src), file_mgr(base_path) {
        output_buffer.reserve(src.length() * 2); // Stima buffer di output
    }

    void process();

    [[nodiscard]] std::string_view get_result() const noexcept {
        return { output_buffer.data(), output_buffer.size() };
    }
};