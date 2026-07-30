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
    SourceView source; //Main file
    uint64_t out_cursor = 0; //Cursor per il file preprocessato
    FileManager file_mgr;
    std::vector<char> output_buffer; // Allocazione dinamica sicura e contigua

    struct Defined {//Explicit name (or other info) of the Macro
        MacroDef def;
        uint16_t name; //defName = def.get()[0 - name]
    };

    [[nodiscard]] SourceView loadView(const char* pathrel); //Loads the code through a file manager

    // Helper interni per il parsing
    void ol(); //Overlook, skipping context dependent spacing and comments
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