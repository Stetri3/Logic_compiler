#pragma once
#include <string_view>
#include <cstdint>
#include <vector>
#include <cstring>
#include <initializer_list>
#include <unordered_map>

#include "preproc_token.h"
#include "hashing.h"
#include "file_def.h"
#include "file_manager.h" 
#include "OsVector.h"
#include "common_def.h"

enum class PreprocLogLevel : uint8_t {
    Off = 0, // Silent
    Summary = 1, // Start/End metrics & timers only
    Events = 2, // Directives, macro operations & branches
    Trace = 3  // Full stack trace & coordinates
};

static constexpr Snippet SNIPPET_SENT = Snippet{ .offset = POffsetMax, .size = PLengthMax };

class Preprocessor {
    FileManager file_mgr;
    FileInfo sourceFile;
    uint64_t out_cursor = 0;
    cc::OsPagedVector<char> output_buffer;
    PreprocLogLevel log_level = PreprocLogLevel::Summary; // Default to Summary (1)

    struct SourceFrame {
        Snippet content = SNIPPET_SENT;
        PCursor cursor = 0;
    };

    std::vector<SourceFrame> inputStack;

    struct Defined {
        Snippet content = {};
        MacroType t = MacroType::None;
        uint16_t flags = 1u;
    };
    std::unordered_map<std::string_view, Defined> storedMacros;

    std::vector<uint8_t> branchFlags;
    MacroType currActive = MacroType::None;

    [[nodiscard]] Snippet loadView(const char* pathrel);

    // Helpers
    [[nodiscard]] inline SourceFrame& top() noexcept { return inputStack.back(); }
    [[nodiscard]] inline const SourceFrame& top() const noexcept { return inputStack.back(); }
    [[nodiscard]] inline PCursor cursor() const noexcept { return top().cursor; }
    [[nodiscard]] inline PCursor& cursor() noexcept { return top().cursor; }
    [[nodiscard]] inline Snippet sourceContent() const noexcept { return top().content; }

    inline static constexpr bool end(PCursor cur, Snippet source) { return cur >= source.size; }
    inline bool end() const { return end(cursor(), sourceContent()); }

    inline Snippet forward(PLength length) const {
        return Snippet{ .offset = cursor() + sourceContent().offset, .size = length };
    }
    inline Snippet backward(PLength backwards_length) const {
        return Snippet(sourceContent().offset + cursor() - backwards_length, backwards_length);
    }
    inline Snippet to(PCursor end_cursor) const {
        return Snippet(sourceContent().offset + cursor(), end_cursor - cursor());
    }
    inline Snippet from(PCursor start_cursor) const {
        return Snippet(sourceContent().offset + start_cursor, cursor() - start_cursor);
    }
    inline Snippet toAbs(POffset end_pos) const {
        return Snippet(sourceContent().offset + cursor(), end_pos - cursor() - sourceContent().offset);
    }
    inline Snippet fromAbs(POffset start_pos) const {
        return Snippet(start_pos, sourceContent().offset + cursor() - start_pos);
    }

    char currChar() const { return file_mgr.getChar(sourceContent(), cursor()); }
    PCursor findNext(const char c) const;
    Snippet peekToNext(const char c) const;
    bool skipToNext(const char c);
    Snippet readToNext(const char c);

    PCursor findNext(std::string_view sequence) const;
    PCursor findNext(std::initializer_list<char> cs) const;
    PCursor findNext(std::initializer_list<std::string_view> sequences) const;

    PCursor findNext(const CharBitLUT& charLUT) const;
    PCursor findNextInv(const CharBitLUT& falseLUT) const;

    inline PCursor findNextIdent() const { return findNext(IDENT_CHAR_BITMASK); }
    inline PCursor findNextIdentInv() const { return findNextInv(IDENT_CHAR_BITMASK); }

    PCursor findInLine(std::string_view sequence) const;
    PCursor findPrev(const char c) const;

    Snippet readToNext(std::initializer_list<char> cs);

    Defined findDefined(std::string_view macroName) const;
    bool isDefined(std::string_view macroName) const { return storedMacros.contains(macroName); }
    int addMacro(std::string_view macroName, Defined macroC);
    void forgetMacro(std::string_view macroName);

    void ol();
    void ol_back();
    int process_directive(MacroType type);
    int evalCondition(Snippet cond);
    int skip_if_branch();

    int handle_none() { return 0; }
    int handle_unknown() { return -404; }
    int handle_define();
    int handle_define_m();
    int handle_include();
    int handle_if();
    int handle_ifndef();
    int handle_else();
    int handle_elif();
    int handle_endif();
    int handle_skip();
    int handle_import();
    int handle_param();

public:
    explicit Preprocessor(const char* base_path, const char* file_path, PreprocLogLevel lvl = PreprocLogLevel::Summary)
        : file_mgr(base_path), sourceFile(file_mgr.loadFile(file_path)), log_level(lvl) {
        inputStack.reserve(16);
        inputStack.push_back(SourceFrame{ .content = sourceFile.content(), .cursor = 0 });
        output_buffer.reserve(align_two_of(sourceContent().size) * 2);
        storedMacros.reserve(128);
    }

    explicit Preprocessor(const char* base_path, FileInfo srcFile, PreprocLogLevel lvl = PreprocLogLevel::Summary)
        : file_mgr(base_path), sourceFile(srcFile), log_level(lvl) {
        inputStack.reserve(16);
        inputStack.push_back(SourceFrame{ .content = srcFile.content(), .cursor = 0 });
        output_buffer.reserve(align_two_of(sourceContent().size) * 2);
        storedMacros.reserve(128);
    }

    void setLogLevel(PreprocLogLevel lvl) noexcept { log_level = lvl; }
    [[nodiscard]] PreprocLogLevel getLogLevel() const noexcept { return log_level; }

    void process();

    [[nodiscard]] std::string_view get_result() const noexcept {
        return { output_buffer.data(), output_buffer.size() };
    }
};