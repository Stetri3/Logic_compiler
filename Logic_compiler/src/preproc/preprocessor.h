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
#include "Alloc_optimized.h"
#include "common_def.h"



class Preprocessor {
    uint64_t cursor = 0;
    FileInfo source; //Main file
    uint64_t out_cursor = 0; //Cursor out
    FileManager file_mgr;
    OsPagedVector<char> output_buffer; // Out

    struct Defined {//Explicit name (or other info) of the Macro
        MacroDef def;
        uint16_t name; //defName = def.get()[0 - name]
    };

    [[nodiscard]] SourceView loadView(const char* pathrel); //Loads the code through a file manager

    // Helper interni per il parsing
    void ol() {}; //Overlook, skipping context dependent spacing and comments
    std::string_view next_token();
    std::string_view read_line();
    void process_directive(MacroType type);

public:
    explicit Preprocessor(const char* base_path = ".", const char* file_path)
        : file_mgr(base_path), source(file_mgr.loadFile(file_path)) {
        output_buffer.reserve(align_two_of(source.file_size)*2); // Estimate double size
    }

    void process();

    [[nodiscard]] std::string_view get_result() const noexcept {
        return { output_buffer.data(), output_buffer.size() };
    }
};