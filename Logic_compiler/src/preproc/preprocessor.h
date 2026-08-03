#pragma once
#include <string_view>
#include <cstdint>
#include <vector>
#include <cstring>

#include "preproc_token.h"
#include "file_def.h"
#include "hashing.h"
#include "file_manager.h" // Per FileManager
#include "Alloc_optimized.h"
#include "common_def.h"
#include <unordered_map>



class Preprocessor {
    uint64_t cursor = 0;
    FileInfo sourceFile; //Main file
    Snippet sourceContent; //content of main file
    //Technically sourceContent is detectable from sourceFile, but this way is better for offloading and performance
    uint64_t out_cursor = 0; //Cursor out
    FileManager file_mgr;
    OsPagedVector<char> output_buffer; // Out

    //dynamic
    MacroType currActive = MacroType::None; //in some cases it's good to save the current macro state

    struct Defined {//Explicit name (or other info) of the Macro
        Snippet content;
        MacroType t;
        uint16_t flags = 1u; //if 0, sentinel value
    };

    std::unordered_map<std::string_view, Defined> storedMacros;

    [[nodiscard]] Snippet loadView(const char* pathrel); //Loads the code through a file manager

    //SourceView: 

    // Helper interni per il parsing
    inline static constexpr bool end(uint32_t cur, Snippet source) {
        return cur >= source.size;
    }
    inline bool end() const { return end(cursor, sourceContent); }

    //str helpers
    char currChar() const { return file_mgr.getChar(sourceContent, cursor); }
    uint32_t findNext(const char c);
    bool skipToNext(const char c); //moves cursor to next character c (includes current position in the search)

    //macro helpers
    Defined findDefined(std::string_view macroName) const;
    bool isDefined(std::string_view macroName) const { return storedMacros.contains(macroName); }

    void ol(); //Overlook, skipping context dependent spacing and comments
    void process_directive(MacroType type);

    //preproc time resolution
    bool evalCondition();

public:
    explicit Preprocessor(const char* base_path, const char* file_path)
        : file_mgr(base_path), sourceFile(file_mgr.loadFile(file_path)),
        sourceContent(sourceFile.content()) {
        output_buffer.reserve(align_two_of(sourceContent.size)*2); // Estimate double size
        storedMacros.reserve(128);
    }

    explicit Preprocessor(const char* base_path, FileInfo srcFile)
        :file_mgr(base_path), sourceFile(srcFile), sourceContent(srcFile.content()){
        output_buffer.reserve(align_two_of(sourceContent.size) * 2); // Estimate double size
        storedMacros.reserve(128);
    }

    void process(); //reads content and writes the processed in output

    [[nodiscard]] std::string_view get_result() const noexcept {
        return { output_buffer.data(), output_buffer.size() };
    }
};