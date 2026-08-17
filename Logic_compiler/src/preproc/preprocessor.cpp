#include "preprocessor.h"
#include <string_view>
#include <cctype>
#include <iostream>
#include <chrono>

namespace BranchState {
    constexpr uint8_t ACTIVE = 1 << 0;
    constexpr uint8_t EVER_TAKEN = 1 << 1;
}

#define LOG_L1(...) if (static_cast<uint8_t>(log_level) >= 1) { std::cout << __VA_ARGS__; }
#define LOG_L2(...) if (static_cast<uint8_t>(log_level) >= 2) { std::cout << __VA_ARGS__; }
#define LOG_L3(...) if (static_cast<uint8_t>(log_level) >= 3) { std::cout << __VA_ARGS__; }

Snippet Preprocessor::loadView(const char* pathrel) {
    Snippet s = file_mgr.loadFile(pathrel).content();
    LOG_L2("[PREPROC] loadView(\"" << pathrel << "\") -> offset: " << s.offset << ", size: " << s.size << "\n");
    return s;
}

PCursor Preprocessor::findInLine(std::string_view sequence) const {
    const PCursor endLine = findNext('\n');
    if (endLine == PCursorMax) return PCursorMax;
    const Snippet forwards = to(endLine);
    const std::string_view forwards_str = file_mgr[forwards];
    const size_t nextC = forwards_str.find(sequence);
    if (nextC == std::string_view::npos || nextC + cursor() > PCursorMax)
        return PCursorMax;
    return static_cast<PCursor>(cursor() + nextC);
}

PCursor Preprocessor::findPrev(const char c) const {
    PCursor cur = cursor();
    while (cur > 0) {
        --cur;
        if (file_mgr.getChar(sourceContent(), cur) == c) {
            return cur;
        }
    }
    return PCursorMax;
}

Snippet Preprocessor::readToNext(std::initializer_list<char> cs) {
    const PCursor nextC = findNext(cs);
    if (nextC == PCursorMax)
        return SNIPPET_SENT;
    const PCursor oldCursor = cursor();
    cursor() = nextC;
    return from(oldCursor);
}

Preprocessor::Defined Preprocessor::findDefined(std::string_view macroName) const {
    if (auto it = storedMacros.find(macroName); it != storedMacros.end()) {
        return it->second;
    }
    return Defined{ .flags = 0 };
}

int Preprocessor::addMacro(std::string_view macroName, Defined macroC) {
    auto it = storedMacros.find(macroName);
    if (it != storedMacros.end()) {
        it->second = macroC;
        LOG_L2("[PREPROC] Macro Redefined: " << macroName << "\n");
        return 1;
    }
    storedMacros.insert({ macroName, macroC });
    LOG_L2("[PREPROC] Macro Added: " << macroName << "\n");
    return 0;
}

void Preprocessor::forgetMacro(std::string_view macroName) {
    LOG_L2("[PREPROC] Macro Forgotten/Absorbed: " << macroName << "\n");
    storedMacros.erase(macroName);
}

PCursor Preprocessor::findNext(const char c) const {
    PCursor cur = cursor();
    while (cur < sourceContent().size) {
        if (file_mgr.getChar(sourceContent(), cur) == c) {
            return cur;
        }
        ++cur;
    }
    return PCursorMax;
}

Snippet Preprocessor::peekToNext(const char c) const {
    const PCursor nextC = findNext(c);
    if (nextC == PCursorMax)
        return SNIPPET_SENT;
    return to(nextC);
}

bool Preprocessor::skipToNext(const char c) {
    const PCursor foundCur = findNext(c);
    if (foundCur != PCursorMax) {
        cursor() = foundCur;
        return true;
    }
    return false;
}

Snippet Preprocessor::readToNext(const char c) {
    const PCursor nextC = findNext(c);
    if (nextC == PCursorMax)
        return SNIPPET_SENT;
    const PCursor oldCursor = cursor();
    cursor() = nextC;
    return from(oldCursor);
}

PCursor Preprocessor::findNext(std::string_view sequence) const {
    const Snippet forwards = to(sourceContent().size);
    const std::string_view forwards_str = file_mgr[forwards];
    const size_t nextC = forwards_str.find(sequence);
    if (nextC == std::string_view::npos || static_cast<uint64_t>(nextC) + cursor() > PCursorMax)
        return PCursorMax;
    return static_cast<PCursor>(cursor() + nextC);
}

PCursor Preprocessor::findNext(std::initializer_list<char> cs) const {
    if (cs.size() == 0) return PCursorMax;
    if (cs.size() == 1) return findNext(*cs.begin());

    bool lut[256] = { false };
    for (char c : cs) {
        lut[static_cast<unsigned char>(c)] = true;
    }

    PCursor cur = cursor();
    const PCursor endCur = sourceContent().size;

    while (cur < endCur) {
        unsigned char ch = static_cast<unsigned char>(file_mgr.getChar(sourceContent(), cur));
        if (lut[ch]) {
            return cur;
        }
        ++cur;
    }
    return PCursorMax;
}

PCursor Preprocessor::findNext(std::initializer_list<std::string_view> sequences) const {
    const Snippet forwards = to(sourceContent().size);
    const std::string_view forwards_str = file_mgr[forwards];

    size_t min_pos = std::string_view::npos;

    for (const std::string_view seq : sequences) {
        const size_t pos = forwards_str.find(seq);
        if (pos < min_pos) {
            min_pos = pos;
            if (min_pos == 0) break;
        }
    }

    if (min_pos == std::string_view::npos || static_cast<uint64_t>(min_pos) + cursor() >= PCursorMax)
        return PCursorMax;

    return cursor() + static_cast<PLength>(min_pos);
}

PCursor Preprocessor::findNext(const CharBitLUT& charLUT) const {
    PCursor cur = cursor();
    const PCursor end_cur = sourceContent().size;

    while (cur < end_cur) {
        char ch = file_mgr.getChar(sourceContent(), cur);
        if (is_in(charLUT, ch)) {
            return cur;
        }
        ++cur;
    }
    return PCursorMax;
}

PCursor Preprocessor::findNextInv(const CharBitLUT& falseLUT) const {
    PCursor cur = cursor();
    const PCursor end_cur = sourceContent().size;

    while (cur < end_cur) {
        char ch = file_mgr.getChar(sourceContent(), cur);
        if (!is_in(falseLUT, ch)) {
            return cur;
        }
        ++cur;
    }
    return PCursorMax;
}

void Preprocessor::ol() {
    while (cursor() < sourceContent().size) {
        const char c = currChar();
        if (c == ' ' || c == '\t' || c == '\r') {
            ++cursor();
            continue;
        }
        break;
    }
}

void Preprocessor::ol_back() {
    while (cursor() > 0) {
        const char c = file_mgr.getChar(sourceContent(), cursor() - 1);
        if (c == ' ' || c == '\t' || c == '\r') {
            --cursor();
        }
        else {
            break;
        }
    }
}

int Preprocessor::process_directive(MacroType type) {
    switch (type) {
    case MacroType::None:     return handle_none();
    case MacroType::Unknown:  return handle_unknown();
    case MacroType::Define:   return handle_define();
    case MacroType::Define_M: return handle_define_m();
    case MacroType::Include:  return handle_include();
    case MacroType::If:       return handle_if();
    case MacroType::Ifndef:   return handle_ifndef();
    case MacroType::Else:     return handle_else();
    case MacroType::Elif:     return handle_elif();
    case MacroType::Endif:    return handle_endif();
    case MacroType::Skip:     return handle_skip();
    case MacroType::Import:   return handle_import();
    case MacroType::Param:    return handle_param();
    default:                  break;
    }
    return -1;
}

int Preprocessor::evalCondition(Snippet cond) {
    const std::string_view expr = file_mgr[cond];

    size_t start = expr.find_first_not_of(" \t\r\n");
    if (start == std::string_view::npos) return 0;
    size_t end = expr.find_last_not_of(" \t\r\n");
    std::string_view trimmed = expr.substr(start, end - start + 1);

    LOG_L3("[PREPROC] evalCondition on: \"" << trimmed << "\"\n");

    if (currActive == MacroType::Ifndef) {
        int res = !isDefined(trimmed) ? 1 : 0;
        LOG_L2("[PREPROC] #ifndef \"" << trimmed << "\" -> " << res << "\n");
        return res;
    }

    if (currActive == MacroType::If || currActive == MacroType::Elif) {
        if (trimmed == "0" || trimmed == "false") return 0;
        if (trimmed == "1" || trimmed == "true") return 1;

        // Check for binary equality / inequality: LHS == RHS or LHS != RHS
        size_t eq_pos = trimmed.find("==");
        size_t neq_pos = trimmed.find("!=");

        if (eq_pos != std::string_view::npos || neq_pos != std::string_view::npos) {
            bool is_eq = (eq_pos != std::string_view::npos);
            size_t op_pos = is_eq ? eq_pos : neq_pos;

            auto trim_sv = [](std::string_view s) {
                size_t b = s.find_first_not_of(" \t\r\n");
                if (b == std::string_view::npos) return std::string_view{};
                size_t e = s.find_last_not_of(" \t\r\n");
                return s.substr(b, e - b + 1);
                };

            std::string_view lhs = trim_sv(trimmed.substr(0, op_pos));
            std::string_view rhs = trim_sv(trimmed.substr(op_pos + 2));

            // Resolve LHS macro value if defined
            std::string_view lhs_val = lhs;
            if (isDefined(lhs)) {
                const Defined def = findDefined(lhs);
                if (def.content.size > 0 && def.content != SNIPPET_SENT) {
                    lhs_val = trim_sv(file_mgr[def.content]);
                }
            }

            // Resolve RHS macro value if defined
            std::string_view rhs_val = rhs;
            if (isDefined(rhs)) {
                const Defined def = findDefined(rhs);
                if (def.content.size > 0 && def.content != SNIPPET_SENT) {
                    rhs_val = trim_sv(file_mgr[def.content]);
                }
            }

            bool match = (lhs_val == rhs_val);
            return is_eq ? (match ? 1 : 0) : (!match ? 1 : 0);
        }

        // Single identifier check
        if (isDefined(trimmed)) {
            const Defined def = findDefined(trimmed);
            if (def.content.size > 0 && def.content != SNIPPET_SENT) {
                return evalCondition(def.content);
            }
            return 1;
        }

        return 0;
    }

    return -1;
}

int Preprocessor::skip_if_branch() {
    LOG_L3("[PREPROC] -> Entering skip_if_branch() at cursor: " << cursor() << "\n");
    bool stringed2 = false;
    bool stringed1 = false;
    uint16_t nestCount = 1;

    auto end_branching = [&]() {
        const PCursor endLine = findNext('\n');
        if (endLine == PCursorMax) {
            cursor() = sourceContent().size;
            return 422;
        }
        cursor() = endLine + 1;
        return 0;
        };

    while (nestCount > 0 && cursor() < sourceContent().size) {
        const char currInitial = currChar();

        if (currInitial == '\"') {
            stringed2 = !stringed2;
            ++cursor();
            continue;
        }
        if (currInitial == '\'') {
            stringed1 = !stringed1;
            ++cursor();
            continue;
        }
        if (stringed1 || stringed2) {
            ++cursor();
            continue;
        }

        if (currInitial == '/' && cursor() + 1 < sourceContent().size) {
            char nextC = file_mgr.getChar(sourceContent(), cursor() + 1);
            if (nextC == '/') {
                const PCursor endLine = findNext('\n');
                cursor() = (endLine == PCursorMax) ? sourceContent().size : endLine + 1;
                continue;
            }
            if (nextC == '*') {
                cursor() += 2;
                const PCursor endBlock = findNext("*/");
                cursor() = (endBlock == PCursorMax) ? sourceContent().size : endBlock + 2;
                continue;
            }
        }

        if (currInitial == '#') {
            const PCursor startHash = cursor();
            ++cursor();
            const PCursor startName = cursor();
            const PCursor endName = findNextIdentInv();
            const PCursor nameEnd = (endName == PCursorMax) ? sourceContent().size : endName;
            cursor() = nameEnd;

            const Snippet nameSnippet = from(startHash);
            const std::string_view macroName = file_mgr[nameSnippet];
            const MacroType matched = macro_hash_match(macroName);

            LOG_L3("[PREPROC] (skipping) encountered: " << macroName << ", nestCount: " << nestCount << "\n");

            switch (matched) {
            case MacroType::If:
            case MacroType::Ifndef:
                ++nestCount;
                break;
            case MacroType::Define: {
                const PCursor endLine = findNext('\n');
                cursor() = (endLine == PCursorMax) ? sourceContent().size : endLine + 1;
                break;
            }
            case MacroType::Define_M: {
                const PCursor endDef = findNext("#enddef");
                cursor() = (endDef == PCursorMax) ? sourceContent().size : endDef + 7;
                break;
            }
            case MacroType::Skip: {
                const PCursor endLine1 = findNext('\n');
                if (endLine1 != PCursorMax) {
                    cursor() = endLine1 + 1;
                    const PCursor endLine2 = findNext('\n');
                    cursor() = (endLine2 == PCursorMax) ? sourceContent().size : endLine2 + 1;
                }
                else {
                    cursor() = sourceContent().size;
                }
                break;
            }
            case MacroType::Elif:
                if (nestCount == 1) {
                    const uint8_t flags = branchFlags.empty() ? 0 : branchFlags.back();
                    if (flags & BranchState::EVER_TAKEN) {
                        const PCursor endLine = findNext('\n');
                        cursor() = (endLine == PCursorMax) ? sourceContent().size : endLine + 1;
                        break;
                    }
                    LOG_L2("[PREPROC] skip_if_branch reached eligible #elif\n");
                    return 105102;
                }
                break;
            case MacroType::Else:
                if (nestCount == 1) {
                    const uint8_t flags = branchFlags.empty() ? 0 : branchFlags.back();
                    if (flags & BranchState::EVER_TAKEN) {
                        const PCursor endLine = findNext('\n');
                        cursor() = (endLine == PCursorMax) ? sourceContent().size : endLine + 1;
                        break;
                    }
                    LOG_L2("[PREPROC] skip_if_branch reached eligible #else\n");
                    return end_branching();
                }
                break;
            case MacroType::Endif:
                if (nestCount == 1) {
                    LOG_L2("[PREPROC] skip_if_branch reached #endif\n");
                    return end_branching();
                }
                --nestCount;
                break;
            default:
                break;
            }
            continue;
        }

        ++cursor();
    }
    return (nestCount == 0) ? 0 : -422;
}

int Preprocessor::handle_define() {
    ol();
    const PCursor startName = cursor();
    const PCursor endName = findNextIdentInv();
    const PCursor nameEnd = (endName == PCursorMax) ? sourceContent().size : endName;
    cursor() = nameEnd;

    // Check if chained with '&' (e.g. #define PARAM&JUSTMACRO value)
    while (cursor() < sourceContent().size && currChar() == '&') {
        ++cursor(); // Consume '&'
        const PCursor nextEnd = findNextIdentInv();
        cursor() = (nextEnd == PCursorMax) ? sourceContent().size : nextEnd;
    }

    const Snippet defNameSnippet = from(startName);
    const std::string_view namesCombined = file_mgr[defNameSnippet];

    ol();
    const PCursor startVal = cursor();
    PCursor contentEnd = startVal;

    // Loop through any continuation lines (__/__)
    while (cursor() < sourceContent().size) {
        const PCursor endL = findNext('\n');
        const PCursor lineLimit = (endL == PCursorMax) ? sourceContent().size : endL;
        const std::string_view lineStr = file_mgr[to(lineLimit)];

        bool hasCont = (lineStr.find("__/__") != std::string_view::npos ||
            lineStr.find("__ / __") != std::string_view::npos);

        if (hasCont && endL != PCursorMax) {
            cursor() = endL + 1;
        }
        else {
            contentEnd = lineLimit;
            cursor() = lineLimit;
            if (endL != PCursorMax) ++cursor();
            break;
        }
    }

    // Trim trailing whitespace
    while (contentEnd > startVal) {
        char ch = file_mgr.getChar(sourceContent(), contentEnd - 1);
        if (ch == ' ' || ch == '\t' || ch == '\r') {
            --contentEnd;
        }
        else {
            break;
        }
    }

    const Snippet defContent = Snippet{
        .offset = sourceContent().offset + startVal,
        .size = contentEnd - startVal
    };

    const Defined def{
        .content = defContent,
        .t = MacroType::Define,
        .flags = 1
    };

    // Split names on '&' and register every alias to the same snippet content
    size_t prev_pos = 0;
    while (prev_pos < namesCombined.size()) {
        size_t next_amp = namesCombined.find('&', prev_pos);
        size_t count = (next_amp == std::string_view::npos) ? (namesCombined.size() - prev_pos) : (next_amp - prev_pos);

        std::string_view singleName = namesCombined.substr(prev_pos, count);

        // Trim any accidental space around '&'
        size_t b = singleName.find_first_not_of(" \t\r\n");
        if (b != std::string_view::npos) {
            size_t e = singleName.find_last_not_of(" \t\r\n");
            singleName = singleName.substr(b, e - b + 1);

            LOG_L2("[PREPROC] #define " << singleName << " = \"" << file_mgr[defContent] << "\"\n");
            addMacro(singleName, def);
        }

        if (next_amp == std::string_view::npos) break;
        prev_pos = next_amp + 1;
    }

    return 0;
}

int Preprocessor::handle_define_m() {
    ol();
    const PCursor startName = cursor();

    // Consume all identifiers connected with '&' (e.g. A&B&C)
    while (cursor() < sourceContent().size) {
        const PCursor nextInv = findNextIdentInv();
        cursor() = (nextInv == PCursorMax) ? sourceContent().size : nextInv;

        ol(); // Skip any whitespace around '&'
        if (cursor() < sourceContent().size && currChar() == '&') {
            ++cursor(); // Consume '&'
            ol();       // Skip whitespace after '&'
            continue;
        }
        break;
    }

    const PCursor namesEnd = cursor();
    const Snippet defNameSnippet = Snippet{
        .offset = sourceContent().offset + startName,
        .size = namesEnd - startName
    };
    const std::string_view namesCombined = file_mgr[defNameSnippet];

    const PCursor endDef = findNext("#enddef");
    if (endDef == PCursorMax) {
        LOG_L1("[PREPROC ERR] #define_m missing matching #enddef for " << namesCombined << "\n");
        return -3;
    }

    const Snippet defContent = to(endDef);
    cursor() = endDef + 7;

    const PCursor endLine = findNext('\n');
    if (endLine != PCursorMax) cursor() = endLine + 1;

    const Defined def{
        .content = defContent,
        .t = MacroType::Define_M,
        .flags = 1
    };

    // Register all '&'-separated aliases
    size_t prev_pos = 0;
    while (prev_pos < namesCombined.size()) {
        size_t next_amp = namesCombined.find('&', prev_pos);
        size_t count = (next_amp == std::string_view::npos) ? (namesCombined.size() - prev_pos) : (next_amp - prev_pos);

        std::string_view singleName = namesCombined.substr(prev_pos, count);

        size_t b = singleName.find_first_not_of(" \t\r\n");
        if (b != std::string_view::npos) {
            size_t e = singleName.find_last_not_of(" \t\r\n");
            singleName = singleName.substr(b, e - b + 1);

            LOG_L2("[PREPROC] #define_m " << singleName << "\n");
            addMacro(singleName, def);
        }

        if (next_amp == std::string_view::npos) break;
        prev_pos = next_amp + 1;
    }

    return 0;
}

int Preprocessor::handle_include() {
    ol();
    if (end()) return -1;

    const char openChar = currChar();
    char closeChar = '\0';
    if (openChar == '"')      closeChar = '"';
    else if (openChar == '<') closeChar = '>';
    else return -1;

    ++cursor();
    const Snippet pathSnippet = readToNext(closeChar);
    if (pathSnippet.offset == SNIPPET_SENT.offset) return -1;
    ++cursor(); // Consume closing quote/angle bracket

    const PCursor endL = findNext('\n');
    if (endL != PCursorMax) cursor() = endL + 1;

    const std::string_view includePathView = file_mgr[pathSnippet];
    const std::string includePath(includePathView); // <--- Null-terminated std::string!

    LOG_L2("[PREPROC] #include path: " << includePath << "\n");

    Snippet includedContent = loadView(includePath.c_str());
    if (includedContent.offset == SNIPPET_SENT.offset || includedContent.size == PLengthMax || includedContent.size == 0) {
        LOG_L1("[PREPROC ERR] Could not load include file: " << includePath << "\n");
        return -2;
    }

    inputStack.push_back(SourceFrame{
        .content = includedContent,
        .cursor = 0
        });
    return 0;
}

int Preprocessor::handle_if() {
    currActive = MacroType::If;
    ol();
    const PCursor beginning = cursor();
    const PCursor endLine = findNext('\n');
    if (endLine == PCursorMax) return -422;

    cursor() = endLine;
    ol_back();
    const Snippet rawCond = from(beginning);
    cursor() = endLine + 1;

    const int res = evalCondition(rawCond);
    if (res < 0) return res;

    const bool condPassed = (res & 1u);
    uint8_t flags = 0;
    if (condPassed) {
        flags = BranchState::ACTIVE | BranchState::EVER_TAKEN;
    }
    branchFlags.push_back(flags);
    LOG_L2("[PREPROC] #if result: " << (condPassed ? 1 : 0) << "\n");

    if (!condPassed) {
        return skip_if_branch();
    }
    return 0;
}

int Preprocessor::handle_ifndef() {
    currActive = MacroType::Ifndef;
    ol();
    const PCursor startName = cursor();
    const PCursor endName = findNextIdentInv();
    const PCursor nameEnd = (endName == PCursorMax) ? sourceContent().size : endName;
    cursor() = nameEnd;

    const Snippet nameSnippet = from(startName);
    const PCursor endLine = findNext('\n');
    if (endLine != PCursorMax) cursor() = endLine + 1;

    const int res = evalCondition(nameSnippet);
    const bool condPassed = (res & 1u);
    uint8_t flags = 0;
    if (condPassed) {
        flags = BranchState::ACTIVE | BranchState::EVER_TAKEN;
    }
    branchFlags.push_back(flags);
    LOG_L2("[PREPROC] #ifndef result: " << (condPassed ? 1 : 0) << "\n");

    if (!condPassed) {
        return skip_if_branch();
    }
    return 0;
}

int Preprocessor::handle_else() {
    if (branchFlags.empty()) return -400;

    const PCursor endLine = findNext('\n');
    if (endLine != PCursorMax) cursor() = endLine + 1;

    uint8_t& flags = branchFlags.back();
    LOG_L2("[PREPROC] #else encountered, prev flags: " << (int)flags << "\n");

    if (flags & BranchState::EVER_TAKEN) {
        flags &= ~BranchState::ACTIVE;
        return skip_if_branch();
    }

    flags = BranchState::ACTIVE | BranchState::EVER_TAKEN;
    return 0;
}

int Preprocessor::handle_elif() {
    if (branchFlags.empty()) return -400;
    currActive = MacroType::Elif;

    uint8_t& flags = branchFlags.back();
    LOG_L2("[PREPROC] #elif encountered, prev flags: " << (int)flags << "\n");

    if (flags & BranchState::EVER_TAKEN) {
        flags &= ~BranchState::ACTIVE;
        return skip_if_branch();
    }

    ol();
    const PCursor beginning = cursor();
    const PCursor endLine = findNext('\n');
    if (endLine == PCursorMax) return -422;

    cursor() = endLine;
    ol_back();
    const Snippet rawCond = from(beginning);
    cursor() = endLine + 1;

    const int res = evalCondition(rawCond);
    if (res < 0) return res;

    const bool condPassed = (res & 1u);
    if (condPassed) {
        flags = BranchState::ACTIVE | BranchState::EVER_TAKEN;
    }
    else {
        flags = 0;
    }
    LOG_L2("[PREPROC] #elif result: " << (condPassed ? 1 : 0) << "\n");

    if (!condPassed) {
        return skip_if_branch();
    }
    return 0;
}

int Preprocessor::handle_endif() {
    if (branchFlags.empty()) {
        LOG_L1("[PREPROC ERR] Unmatched #endif\n");
        return -400;
    }
    branchFlags.pop_back();
    LOG_L2("[PREPROC] #endif, remaining branch depth: " << branchFlags.size() << "\n");

    const PCursor endLine = findNext('\n');
    if (endLine != PCursorMax) cursor() = endLine + 1;
    return 0;
}

int Preprocessor::handle_skip() {
    LOG_L2("[PREPROC] #skip removing next line\n");
    const PCursor endLine1 = findNext('\n');
    if (endLine1 == PCursorMax || endLine1 + 1 >= sourceContent().size) {
        cursor() = sourceContent().size;
        return 0;
    }
    cursor() = endLine1 + 1;

    const PCursor endLine2 = findNext('\n');
    if (endLine2 == PCursorMax) {
        cursor() = sourceContent().size;
        return 0;
    }
    cursor() = endLine2 + 1;
    return 0;
}

int Preprocessor::handle_import() {
    return handle_include();
}

int Preprocessor::handle_param() {
    auto trim = [](std::string_view sv) -> std::string_view {
        size_t start = sv.find_first_not_of(" \t\r\n");
        if (start == std::string_view::npos) return "";
        size_t end = sv.find_last_not_of(" \t\r\n");
        return sv.substr(start, end - start + 1);
        };

    auto emit = [this](std::string_view sv) {
        for (char ch : sv) {
            output_buffer.push_back(ch);
        }
        };

    ol();
    if (end() || currChar() != '(') return -400;
    ++cursor();

    const PCursor startType = cursor();
    uint16_t parenDepth = 1;

    while (!end() && parenDepth > 0) {
        const char c = currChar();
        if (c == '(') {
            ++parenDepth;
        }
        else if (c == ')') {
            --parenDepth;
            if (parenDepth == 0) break;
        }
        else if (c == '\n') {
            return -400;
        }
        ++cursor();
    }

    if (parenDepth != 0) return -400;

    const Snippet typeSnippet = from(startType);
    const std::string_view typeStr = trim(file_mgr[typeSnippet]);
    if (typeStr.empty()) return -400;
    ++cursor();

    ol();
    const PCursor startName = cursor();
    const PCursor endName = findNextIdentInv();
    const PCursor nameEnd = (endName == PCursorMax) ? sourceContent().size : endName;
    cursor() = nameEnd;

    const Snippet nameSnippet = from(startName);
    const std::string_view nameStr = trim(file_mgr[nameSnippet]);
    if (nameStr.empty()) return -400;

    ol();
    if (end() || currChar() != '=') return -400;
    ++cursor();

    ol();
    const PCursor startDef = cursor();
    const PCursor endLine = findNext('\n');
    const PCursor lineEnd = (endLine == PCursorMax) ? sourceContent().size : endLine;
    cursor() = lineEnd;

    const Snippet defaultSnippet = from(startDef);
    const std::string_view defaultStr = trim(file_mgr[defaultSnippet]);

    if (endLine != PCursorMax) {
        ++cursor();
    }

    std::string_view finalVal;
    if (isDefined(nameStr)) {
        const Defined def = findDefined(nameStr);
        if (def.content.size > 0 && def.content != SNIPPET_SENT) {
            finalVal = trim(file_mgr[def.content]);
        }
        else {
            finalVal = "1";
        }
        forgetMacro(nameStr);
    }
    else {
        finalVal = defaultStr;
    }

    LOG_L2("[PREPROC] #param synthesized -> constexpr (" << typeStr << ") "
        << nameStr << " = static_cast<" << typeStr << ">(" << finalVal << ");\n");

    emit("constexpr (");
    emit(typeStr);
    emit(") ");
    emit(nameStr);
    emit(" = static_cast<");
    emit(typeStr);
    emit(">(");
    emit(finalVal);
    emit(");\n");

    return 0;
}

void Preprocessor::process() {
    const auto startTime = std::chrono::high_resolution_clock::now();
    const uint32_t totalInputBytes = sourceFile.file_size;

    if (log_level >= PreprocLogLevel::Events) {
        std::cout << "========================================\n"
            << "[PREPROC START] Root size: " << totalInputBytes << " bytes\n"
            << "========================================\n";
    }

    while (!inputStack.empty()) {
        if (top().cursor >= top().content.size) {
            LOG_L3("[PREPROC] Frame finished, pop stack. Remaining: " << inputStack.size() - 1 << "\n");
            inputStack.pop_back();
            continue;
        }

        const char c = currChar();

        // 1. Comments
        if (c == '/' && cursor() + 1 < sourceContent().size) {
            const char nextC = file_mgr.getChar(sourceContent(), cursor() + 1);
            if (nextC == '/') {
                const PCursor startComment = cursor();
                const PCursor endLine = findNext('\n');
                const PCursor commentEnd = (endLine == PCursorMax) ? sourceContent().size : endLine + 1;
                cursor() = commentEnd;
                const std::string_view commentStr = file_mgr[from(startComment)];
                for (char ch : commentStr) output_buffer.push_back(ch);
                continue;
            }
            if (nextC == '*') {
                const PCursor startComment = cursor();
                cursor() += 2;
                const PCursor endBlock = findNext("*/");
                const PCursor commentEnd = (endBlock == PCursorMax) ? sourceContent().size : endBlock + 2;
                cursor() = commentEnd;
                const std::string_view commentStr = file_mgr[from(startComment)];
                for (char ch : commentStr) output_buffer.push_back(ch);
                continue;
            }
        }

        // 2. String & Character Literals
        if (c == '\"' || c == '\'') {
            const char quote = c;
            output_buffer.push_back(quote);
            ++cursor();
            while (cursor() < sourceContent().size) {
                const char sc = currChar();
                output_buffer.push_back(sc);
                ++cursor();
                if (sc == '\\' && cursor() < sourceContent().size) {
                    output_buffer.push_back(currChar());
                    ++cursor();
                    continue;
                }
                if (sc == quote) break;
            }
            continue;
        }

        // 3. Directives
        if (c == '#') {
            const PCursor startHash = cursor();
            ++cursor();
            const PCursor startName = cursor();
            const PCursor endName = findNextIdentInv();
            const PCursor nameEnd = (endName == PCursorMax) ? sourceContent().size : endName;
            cursor() = nameEnd;

            const Snippet dirSnippet = from(startHash);
            const std::string_view dirName = file_mgr[dirSnippet];
            const MacroType type = macro_hash_match(dirName);

            LOG_L2("[PREPROC] Directive detected: " << dirName
                << " (enum: " << (int)type << ") at cursor: " << startHash << "\n");

            currActive = type;
            int ret = process_directive(type);
            if (ret < 0) {
                LOG_L1("[PREPROC ERR] Directive failed with code: " << ret << "\n");
                return;
            }
            continue;
        }

        // 4. Identifiers & Macro Expansion
        if (is_in(IDENT_CHAR_BITMASK, c)) {
            const PCursor startIdent = cursor();
            const PCursor endIdent = findNextIdentInv();
            const PCursor identEnd = (endIdent == PCursorMax) ? sourceContent().size : endIdent;

            cursor() = identEnd;
            const Snippet identSnippet = from(startIdent);
            const std::string_view identStr = file_mgr[identSnippet];

            if (isDefined(identStr)) {
                const Defined macro = findDefined(identStr);
                if (macro.content.size > 0 && macro.content != SNIPPET_SENT) {
                    LOG_L2("[PREPROC] Expanding macro: " << identStr << "\n");
                    const std::string_view macroVal = file_mgr[macro.content];
                    for (char mc : macroVal) {
                        output_buffer.push_back(mc);
                    }
                }
            }
            else {
                for (size_t i = 0; i < identStr.size(); ++i) {
                    output_buffer.push_back(identStr[i]);
                }
            }
            continue;
        }

        // 5. Normal Character Passthrough
        output_buffer.push_back(c);
        ++cursor();
    }

    const auto endTime = std::chrono::high_resolution_clock::now();
    const std::chrono::duration<double, std::milli> duration = endTime - startTime;

    if (log_level == PreprocLogLevel::Summary) {
        std::cout << "========================================\n"
            << "[PREPROCESSOR SUMMARY]\n"
            << " Input Size  : " << totalInputBytes << " bytes\n"
            << " Output Size : " << output_buffer.size() << " bytes\n"
            << " Time Taken  : " << duration.count() << " ms\n"
            << "========================================\n";
    }
    else if (log_level >= PreprocLogLevel::Events) {
        std::cout << "========================================\n"
            << "[PREPROC END] Written: " << output_buffer.size() << " bytes\n"
            << "[PREPROC TIME] " << duration.count() << " ms\n"
            << "========================================\n";
    }
}