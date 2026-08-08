#pragma once
#include <string_view>
#include <cstdint>
#include <vector>
#include <cstring>
#include <initializer_list>

#include "preproc_token.h" //common defs
#include "hashing.h" //common hashes
#include "file_def.h" // For FileManager
#include "file_manager.h" 
#include "Alloc_optimized.h" //For big buffers
#include "common_def.h" //small utilities (aligntwoof)
#include <unordered_map>

//sent value
static constexpr Snippet SNIPPET_SENT = Snippet{ .offset = POffsetMax, .size = PLengthMax };


class Preprocessor {
    //global data stuff
    FileInfo sourceFile; //Main file
    uint64_t out_cursor = 0; //Cursor out
    FileManager file_mgr;
    OsPagedVector<char> output_buffer; // Out

    //content context currently working on
    struct SourceFrame {
        Snippet content = SNIPPET_SENT;
        PCursor cursor = 0;
    };
    //always remember, cursor is relative to sourcecontent so if cursor is at &char1,
    // it's at file_mgr.getChar(sourceContent().offset + cursor()), NOT file_mgr.getChar(cursor)
    //while snippets are always absolute, never offseted ever

    //input stack (stack of content currently working on)
    std::vector<SourceFrame> inputStack;


    struct Defined {//Explicit name (or other info) of the Macro
        Snippet content = {};
        MacroType t = MacroType::None;
        uint16_t flags = 1u; //if 0, sentinel value
    };
    std::unordered_map<std::string_view, Defined> storedMacros; //macro names/contents map

    //If control flags
    std::vector<uint8_t> branchFlags;
    //& 11 = truth type 0 false, 1 true, 10/11 pass to parsing (unable to evaluate)

    //dynamic
    MacroType currActive = MacroType::None; //in some cases (debugging) it's good to save the current macro state


    [[nodiscard]] Snippet loadView(const char* pathrel); //Loads the code through a file manager


    // Internal helpers for parsing

    //shortcut helpers
    [[nodiscard]] inline SourceFrame& top() noexcept {
        return inputStack.back();
    }

    [[nodiscard]] inline const SourceFrame& top() const noexcept {
        return inputStack.back();
    }
    //context stack shortcuts
    [[nodiscard]] inline PCursor cursor() const noexcept { return top().cursor; }
    [[nodiscard]] inline PCursor& cursor() noexcept { return top().cursor; }
    [[nodiscard]] inline Snippet sourceContent() const noexcept { return top().content; }

    inline static constexpr bool end(PCursor cur, Snippet source) {
        return cur >= source.size;
    }
    inline bool end() const { return end(cursor(), sourceContent()); }

    //Offset relativity handling helpers
    
    //Length is from cursor() to the end
    inline Snippet forward(PLength length) const {
        return Snippet{ .offset = cursor() + sourceContent().offset, .size = length };
    }
    inline Snippet backward(PLength backwards_length) const {
        return Snippet(sourceContent().offset + cursor() - backwards_length, backwards_length);
    }
    //end_cursor is a cursor, so relative to the source (if end_cursor = cursor, size = 0)
    inline Snippet to(PCursor end_cursor) const {
        return Snippet(sourceContent().offset + cursor(), end_cursor - cursor());
    }
    inline Snippet from(PCursor start_cursor) const {
        return Snippet(sourceContent().offset + start_cursor, cursor() - start_cursor);
    }
    //end pos is absolute (if end_pos = sourceContent + cursor, size = 0)
    inline Snippet toAbs(POffset end_pos) const {
        return Snippet(sourceContent().offset + cursor(), end_pos - cursor() - sourceContent().offset);
    }
    inline Snippet fromAbs(POffset start_pos) const {
        return Snippet(start_pos, sourceContent().offset + cursor() - start_pos);
    }

    //str helpers
    char currChar() const { return file_mgr.getChar(sourceContent(), cursor()); }
    PCursor findNext(const char c) const;
    Snippet peekToNext(const char c) const; //keeps cursor static
    bool skipToNext(const char c); //moves cursor to next character c (includes current position in the search)
    Snippet readToNext(const char c); //Also moves cursor forwards, non const

    PCursor findNext(std::string_view sequence) const;
    PCursor findNext(std::initializer_list<char> cs) const;
    PCursor findNext(std::initializer_list<std::string_view> sequences) const;
    
    PCursor findNext(const CharBitLUT& charLUT) const;
    PCursor findNextInv(const CharBitLUT& falseLUT) const;
    
    inline PCursor findNextIdent() const { return findNext(IDENT_CHAR_BITMASK); };
    inline PCursor findNextIdentInv() const { return findNextInv(IDENT_CHAR_BITMASK); };

    PCursor findInLine(std::string_view sequence) const;
    PCursor findPrev(const char c) const;

    Snippet readToNext(std::initializer_list<char> cs);

    //macro helpers
    Defined findDefined(std::string_view macroName) const;
    bool isDefined(std::string_view macroName) const { return storedMacros.contains(macroName); }
    int addMacro(std::string_view macroName, Defined macroC);
    void forgetMacro(std::string_view macroName);

    void ol(); //Overlook, skipping context dependent spacing and comments
    void ol_back(); //Overlook back, for when you overshoot spacing and such
    //By default both ol() and ol_back() DONT LEAVE THE SPACE, need to insert manually in writing
    int process_directive(MacroType type);

    //preproc time resolution
    //1 for true, 0 for false, -1 (or other negatives) for unresolvable
    //n%2 = 0 for false, warning, n%2 = 1 for true, warning
    int evalCondition(Snippet cond);


    //Macro behavior helpers
    int skip_if_branch();

    //Directives
    //0: perfect, -1: error & abort, other: specific messages (negative for aborting, positive for saveable)
    int handle_none() { return 0; };
    int handle_unknown() { return -404; }; //not found
    int handle_define();
    int handle_define_m();
    int handle_include();
    int handle_if();
    int handle_ifndef();
    int handle_else();
    int handle_elif();
    int handle_endif();
    int handle_skip();//Skips next line (and only next line)
    int handle_import();//No implement yet
    int handle_param();//No implement yet


    void nextToken();

    void processLayer();

public:
    explicit Preprocessor(const char* base_path, const char* file_path)
        : file_mgr(base_path), sourceFile(file_mgr.loadFile(file_path)) {
        inputStack.reserve(16);
        inputStack.push_back(SourceFrame{ .content = sourceFile.content(), .cursor = 0 });
        output_buffer.reserve(align_two_of(sourceContent().size) * 2); // Estimate double size
        storedMacros.reserve(128);
    }

    explicit Preprocessor(const char* base_path, FileInfo srcFile)
        :file_mgr(base_path), sourceFile(srcFile) {
        inputStack.reserve(16);
        inputStack.push_back(SourceFrame{ .content = srcFile.content(), .cursor = 0 });
        output_buffer.reserve(align_two_of(sourceContent().size) * 2); // Estimate double size
        storedMacros.reserve(128);
    }

    void process(); //reads content and writes the processed in output

    [[nodiscard]] std::string_view get_result() const noexcept {
        return { output_buffer.data(), output_buffer.size() };
    }
};