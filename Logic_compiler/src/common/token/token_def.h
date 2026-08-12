#pragma once
#include <cstdint>
#include "common_def.h"
#include "hashing.h"

// passive token relative definitions
namespace t {
    //standardized data types
    using Offset = uint32_t;
    using Cursor = uint32_t;
    using Length = uint16_t;
    using TypeSize = uint8_t;

    template <typename T>
    inline constexpr bool isSentinel(T value) {
        return unsigned_max_v<T>() == value;
    }


    enum class TokenType : TypeSize {
        // Keywords:
        // bool
        kFalse, kTrue,
        // Control
        kIf, kElse, kFor, kWhile, kEval, kBreak, kContinue, kReturn,
        // Env-relative
        kConstexpr, kConst, kStatic, kInline, kTemplate, // No virtual for now
        // Env-sectional
        kPublic, kPrivate, kRequires, kNamespace,
        // comp op
        kNew, kDelete, kAuto, kSizeof, kTypeof,
        // builtin
        kType, kByte, kVoid, kStruct, kEnum,
        // Identifiers
        Ident, LInt, LFloat, LString, LChar, // Ident = user defined object (variable)
        // Inline operators (and some access)
        Plus, Minus, Star, Slash, Amp, Bar, Up, Tilde, Excl, Perc, Question, Equals, Colon, Greater, Lesser,
        // Assignment operators
        PPlus, MMinus, PlusEquals, MinusEquals, StarEquals, SlashEquals, PercEquals, AmpEquals, BarEquals, TildeEquals, UpEquals,
        // Double/boolean operators
        EEquals, NEquals, AAmp, BBar, GrEquals, LeEquals,
        GGreater, LLesser, GGrEquals, LLeEquals,
        CColon, SSlash,
        // Access
        Dot, Dots, Arrow,
        // Contexting
        Left, Right, SqLeft, SqRight, BrLeft, BrRight, Semi, Comma,
        WideComment,
        // Meta
        Eof, Unknown, External,
        TBD, //To be determined

        count
    };


    //STRING REPRESENTATION, STANDARD (always refer to this for knames and const string representations)
    namespace impl {
        //String representation of const name tokens
        consteval auto make_token_strmap() {
            using TT = TokenType;
            std::array<std::string_view, (size_t)TT::count> res{ "" }; //Empty for non const
//quick enum to arr access
#define PLC(x) res[static_cast<uint8_t>(x)]
            // Keywords: bool
            PLC(TT::kFalse) = "false";
            PLC(TT::kTrue) = "true";

            // Control
            PLC(TT::kIf) = "if";
            PLC(TT::kElse) = "else";
            PLC(TT::kFor) = "for";
            PLC(TT::kWhile) = "while";
            PLC(TT::kEval) = "eval";
            PLC(TT::kBreak) = "break";
            PLC(TT::kContinue) = "continue";
            PLC(TT::kReturn) = "return";

            // Env-relative
            PLC(TT::kConstexpr) = "constexpr";
            PLC(TT::kConst) = "const";
            PLC(TT::kStatic) = "static";
            PLC(TT::kInline) = "inline";
            PLC(TT::kTemplate) = "template";

            // Env-sectional
            PLC(TT::kPublic) = "public";
            PLC(TT::kPrivate) = "private";
            PLC(TT::kRequires) = "requires";
            PLC(TT::kNamespace) = "namespace";

            // Comp op
            PLC(TT::kNew) = "new";
            PLC(TT::kDelete) = "delete";
            PLC(TT::kAuto) = "auto";
            PLC(TT::kSizeof) = "sizeof";
            PLC(TT::kTypeof) = "typeof";

            // Builtin
            PLC(TT::kType) = "type";
            PLC(TT::kByte) = "byte";
            PLC(TT::kVoid) = "void";
            PLC(TT::kStruct) = "struct";
            PLC(TT::kEnum) = "enum";

            // Single character operators & punctuation
            PLC(TT::Plus) = "+";
            PLC(TT::Minus) = "-";
            PLC(TT::Star) = "*";
            PLC(TT::Slash) = "/";
            PLC(TT::Amp) = "&";
            PLC(TT::Bar) = "|";
            PLC(TT::Up) = "^";
            PLC(TT::Tilde) = "~";
            PLC(TT::Excl) = "!";
            PLC(TT::Perc) = "%";
            PLC(TT::Question) = "?";
            PLC(TT::Equals) = "=";
            PLC(TT::Colon) = ":";
            PLC(TT::Greater) = ">";
            PLC(TT::Lesser) = "<";

            // Multi-char / Assignment operators
            PLC(TT::PPlus) = "++";
            PLC(TT::MMinus) = "--";
            PLC(TT::PlusEquals) = "+=";
            PLC(TT::MinusEquals) = "-=";
            PLC(TT::StarEquals) = "*=";
            PLC(TT::SlashEquals) = "/=";
            PLC(TT::PercEquals) = "%=";
            PLC(TT::AmpEquals) = "&=";
            PLC(TT::BarEquals) = "|=";
            PLC(TT::TildeEquals) = "~=";
            PLC(TT::UpEquals) = "^=";

            // Comparison, Bitwise shift & Double operators
            PLC(TT::EEquals) = "==";
            PLC(TT::NEquals) = "!=";
            PLC(TT::AAmp) = "&&";
            PLC(TT::BBar) = "||";
            PLC(TT::GrEquals) = ">=";
            PLC(TT::LeEquals) = "<=";
            PLC(TT::GGreater) = ">>";
            PLC(TT::LLesser) = "<<";
            PLC(TT::GGrEquals) = ">>=";
            PLC(TT::LLeEquals) = "<<=";
            PLC(TT::CColon) = "::";
            PLC(TT::SSlash) = "//";

            // Access operators
            PLC(TT::Dot) = ".";
            PLC(TT::Dots) = "...";
            PLC(TT::Arrow) = "->";

            // Contexting / Punctuation
            PLC(TT::Left) = "(";
            PLC(TT::Right) = ")";
            PLC(TT::SqLeft) = "[";
            PLC(TT::SqRight) = "]";
            PLC(TT::BrLeft) = "{";
            PLC(TT::BrRight) = "}";
            PLC(TT::Semi) = ";";
            PLC(TT::Comma) = ",";
            return res;
#undef PLC
        };
    }
    constexpr auto TT_STRMAP = impl::make_token_strmap();

    // Helper to hash the string stored at a given TokenType index in TT_STRMAP
    constexpr uint64_t hash_token(TokenType type) noexcept {
        return hash_directive(TT_STRMAP[static_cast<size_t>(type)]);
    }

    //checked string to token hash map
    inline constexpr TokenType string_map(std::string_view sv) noexcept {
        const uint64_t hash = hash_directive(sv);

        // Exact string match validation against single truth
        auto check = [sv](TokenType type) noexcept -> TokenType {
            return (sv == TT_STRMAP[static_cast<size_t>(type)]) ? type : TokenType::TBD;
            };

        switch (hash) {
            // Keywords: bool
        case hash_token(TokenType::kFalse):     return check(TokenType::kFalse);
        case hash_token(TokenType::kTrue):      return check(TokenType::kTrue);

            // Control
        case hash_token(TokenType::kIf):        return check(TokenType::kIf);
        case hash_token(TokenType::kElse):      return check(TokenType::kElse);
        case hash_token(TokenType::kFor):       return check(TokenType::kFor);
        case hash_token(TokenType::kWhile):     return check(TokenType::kWhile);
        case hash_token(TokenType::kEval):      return check(TokenType::kEval);
        case hash_token(TokenType::kBreak):     return check(TokenType::kBreak);
        case hash_token(TokenType::kContinue):  return check(TokenType::kContinue);
        case hash_token(TokenType::kReturn):    return check(TokenType::kReturn);

            // Env-relative
        case hash_token(TokenType::kConstexpr): return check(TokenType::kConstexpr);
        case hash_token(TokenType::kConst):     return check(TokenType::kConst);
        case hash_token(TokenType::kStatic):    return check(TokenType::kStatic);
        case hash_token(TokenType::kInline):    return check(TokenType::kInline);
        case hash_token(TokenType::kTemplate):  return check(TokenType::kTemplate);

            // Env-sectional
        case hash_token(TokenType::kPublic):    return check(TokenType::kPublic);
        case hash_token(TokenType::kPrivate):   return check(TokenType::kPrivate);
        case hash_token(TokenType::kRequires):  return check(TokenType::kRequires);
        case hash_token(TokenType::kNamespace): return check(TokenType::kNamespace);

            // Comp op
        case hash_token(TokenType::kNew):       return check(TokenType::kNew);
        case hash_token(TokenType::kDelete):    return check(TokenType::kDelete);
        case hash_token(TokenType::kAuto):      return check(TokenType::kAuto);
        case hash_token(TokenType::kSizeof):    return check(TokenType::kSizeof);
        case hash_token(TokenType::kTypeof):    return check(TokenType::kTypeof);

            // Builtin
        case hash_token(TokenType::kType):      return check(TokenType::kType);
        case hash_token(TokenType::kByte):      return check(TokenType::kByte);
        case hash_token(TokenType::kVoid):      return check(TokenType::kVoid);
        case hash_token(TokenType::kStruct):    return check(TokenType::kStruct);
        case hash_token(TokenType::kEnum):      return check(TokenType::kEnum);

            // Single character operators & punctuation
        case hash_token(TokenType::Plus):       return check(TokenType::Plus);
        case hash_token(TokenType::Minus):      return check(TokenType::Minus);
        case hash_token(TokenType::Star):       return check(TokenType::Star);
        case hash_token(TokenType::Slash):      return check(TokenType::Slash);
        case hash_token(TokenType::Amp):        return check(TokenType::Amp);
        case hash_token(TokenType::Bar):        return check(TokenType::Bar);
        case hash_token(TokenType::Up):         return check(TokenType::Up);
        case hash_token(TokenType::Tilde):      return check(TokenType::Tilde);
        case hash_token(TokenType::Excl):       return check(TokenType::Excl);
        case hash_token(TokenType::Perc):       return check(TokenType::Perc);
        case hash_token(TokenType::Question):   return check(TokenType::Question);
        case hash_token(TokenType::Equals):     return check(TokenType::Equals);
        case hash_token(TokenType::Colon):      return check(TokenType::Colon);
        case hash_token(TokenType::Greater):    return check(TokenType::Greater);
        case hash_token(TokenType::Lesser):     return check(TokenType::Lesser);

            // Multi-char / Assignment operators
        case hash_token(TokenType::PPlus):       return check(TokenType::PPlus);
        case hash_token(TokenType::MMinus):      return check(TokenType::MMinus);
        case hash_token(TokenType::PlusEquals):  return check(TokenType::PlusEquals);
        case hash_token(TokenType::MinusEquals): return check(TokenType::MinusEquals);
        case hash_token(TokenType::StarEquals):  return check(TokenType::StarEquals);
        case hash_token(TokenType::SlashEquals): return check(TokenType::SlashEquals);
        case hash_token(TokenType::PercEquals):  return check(TokenType::PercEquals);
        case hash_token(TokenType::AmpEquals):   return check(TokenType::AmpEquals);
        case hash_token(TokenType::BarEquals):   return check(TokenType::BarEquals);
        case hash_token(TokenType::TildeEquals): return check(TokenType::TildeEquals);
        case hash_token(TokenType::UpEquals):    return check(TokenType::UpEquals);

            // Comparison, Bitwise shift & Double operators
        case hash_token(TokenType::EEquals):    return check(TokenType::EEquals);
        case hash_token(TokenType::NEquals):    return check(TokenType::NEquals);
        case hash_token(TokenType::AAmp):       return check(TokenType::AAmp);
        case hash_token(TokenType::BBar):       return check(TokenType::BBar);
        case hash_token(TokenType::GrEquals):   return check(TokenType::GrEquals);
        case hash_token(TokenType::LeEquals):   return check(TokenType::LeEquals);
        case hash_token(TokenType::GGreater):   return check(TokenType::GGreater);
        case hash_token(TokenType::LLesser):    return check(TokenType::LLesser);
        case hash_token(TokenType::GGrEquals):  return check(TokenType::GGrEquals);
        case hash_token(TokenType::LLeEquals):  return check(TokenType::LLeEquals);
        case hash_token(TokenType::CColon):     return check(TokenType::CColon);
        case hash_token(TokenType::SSlash):     return check(TokenType::SSlash);

            // Access operators
        case hash_token(TokenType::Dot):        return check(TokenType::Dot);
        case hash_token(TokenType::Dots):       return check(TokenType::Dots);
        case hash_token(TokenType::Arrow):      return check(TokenType::Arrow);

            // Contexting / Punctuation
        case hash_token(TokenType::Left):       return check(TokenType::Left);
        case hash_token(TokenType::Right):      return check(TokenType::Right);
        case hash_token(TokenType::SqLeft):     return check(TokenType::SqLeft);
        case hash_token(TokenType::SqRight):    return check(TokenType::SqRight);
        case hash_token(TokenType::BrLeft):     return check(TokenType::BrLeft);
        case hash_token(TokenType::BrRight):    return check(TokenType::BrRight);
        case hash_token(TokenType::Semi):       return check(TokenType::Semi);
        case hash_token(TokenType::Comma):      return check(TokenType::Comma);

        default:                                return TokenType::TBD;
        }
    }

}
struct Token {
    t::Offset globalOffset = 0; // offset in the manager data
    t::Length size = 0;
    uint32_t extra = 0; //content based on flags (ex. extra length for literals)
    uint8_t flags = 1;
    t::TokenType type = t::TokenType::Unknown;
};
