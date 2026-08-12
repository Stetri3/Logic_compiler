#include "lexer.h"
#include "hashing.h"
#include <cctype>

namespace {
    constexpr bool isIdentStart(char c) noexcept {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
    }

    constexpr bool isIdentBody(char c) noexcept {
        return isIdentStart(c) || (c >= '0' && c <= '9');
    }

    constexpr bool isDigit(char c) noexcept {
        return c >= '0' && c <= '9';
    }
}

void Lexer::skipWhitespaceAndComments() noexcept {
    while (cursor < source.size()) {
        char c = peek();
        switch (c) {
        case ' ':
        case '\t':
        case '\r':
        case '\n':
            advance();
            break;

        case '/':
            if (peek(1) == '/') {
                // Commento a riga singola: salta fino a fine riga
                advance(); advance();
                while (peek() != '\0' && peek() != '\n') {
                    advance();
                }
            }
            else if (peek(1) == '*') {
                // Commento multilinea
                advance(); advance();
                while (peek() != '\0') {
                    if (peek() == '*' && peek(1) == '/') {
                        advance(); advance();
                        break;
                    }
                    advance();
                }
            }
            else {
                return; // È un operatore '/', gestione demandata al lexer
            }
            break;

        default:
            return;
        }
    }
}

Token Lexer::lexIdentifierOrKeyword() noexcept {
    const t::Offset start = cursor;

    while (isIdentBody(peek())) {
        advance();
    }

    const t::Length length = static_cast<t::Length>(cursor - start);
    const std::string_view text = source.substr(start, length);

    // Usa il tuo string_map hash-based generato in token.h
    t::TokenType type = t::string_map(text);

    if (type == t::TokenType::TBD) {
        type = t::TokenType::Ident;
    }

    return Token{ start, length, 0, 1, type };
}

Token Lexer::lexNumber() noexcept {
    const t::Offset start = cursor;
    bool isFloat = false;

    // Gestione esadecimale (es. 0x1F)
    if (peek() == '0' && (peek(1) == 'x' || peek(1) == 'X')) {
        advance(); advance();
        while (std::isxdigit(static_cast<unsigned char>(peek()))) {
            advance();
        }
    }
    else {
        while (isDigit(peek())) {
            advance();
        }

        // Parte frazionaria
        if (peek() == '.' && isDigit(peek(1))) {
            isFloat = true;
            advance(); // Consuma '.'
            while (isDigit(peek())) {
                advance();
            }
        }

        // Notazione scientifica (es. 1e-5 o 2.0E+10)
        if (peek() == 'e' || peek() == 'E') {
            isFloat = true;
            advance();
            if (peek() == '+' || peek() == '-') {
                advance();
            }
            while (isDigit(peek())) {
                advance();
            }
        }
    }

    // Siffissi opzionali per i tipi numerici (f, u, l, ecc.)
    while (peek() == 'f' || peek() == 'F' || peek() == 'u' || peek() == 'U' || peek() == 'l' || peek() == 'L') {
        advance();
    }

    const t::Length length = static_cast<t::Length>(cursor - start);
    return Token{ start, length, 0, 1, isFloat ? t::TokenType::LFloat : t::TokenType::LInt };
}

Token Lexer::lexString() noexcept {
    const t::Offset start = cursor;
    advance(); // Consuma il " di apertura

    while (peek() != '\0') {
        char c = advance();
        if (c == '\\') {
            if (peek() != '\0') advance(); // Salta il carattere gestito dall'escape
        }
        else if (c == '"') {
            break;
        }
    }

    const t::Length length = static_cast<t::Length>(cursor - start);
    return Token{ start, length, 0, 1, t::TokenType::LString };
}

Token Lexer::lexChar() noexcept {
    const t::Offset start = cursor;
    advance(); // Consuma il ' di apertura

    while (peek() != '\0') {
        char c = advance();
        if (c == '\\') {
            if (peek() != '\0') advance();
        }
        else if (c == '\'') {
            break;
        }
    }

    const t::Length length = static_cast<t::Length>(cursor - start);
    return Token{ start, length, 0, 1, t::TokenType::LChar };
}

Token Lexer::nextToken() noexcept {
    skipWhitespaceAndComments();

    if (cursor >= source.size()) {
        Token eofTok{ static_cast<t::Offset>(source.size()), 0, 0, 1, t::TokenType::Eof };
        thisExpr.push_back(eofTok);
        return eofTok;
    }

    const t::Offset start = cursor;
    const char c = advance();

    // 1. Identificatori & Keywords
    if (isIdentStart(c)) {
        cursor--; // Backtrack per la scansione completa della parola
        Token tok = lexIdentifierOrKeyword();
        thisExpr.push_back(tok);
        return tok;
    }

    // 2. Literals Numerici
    if (isDigit(c)) {
        cursor--;
        Token tok = lexNumber();
        thisExpr.push_back(tok);
        return tok;
    }

    // 3. Literals Stringa e Carattere
    if (c == '"') {
        cursor--;
        Token tok = lexString();
        thisExpr.push_back(tok);
        return tok;
    }
    if (c == '\'') {
        cursor--;
        Token tok = lexChar();
        thisExpr.push_back(tok);
        return tok;
    }

    // 4. Operatori e Punteggiatura
    t::TokenType type = t::TokenType::Unknown;

    switch (c) {
    case '+':
        if (match('+'))      type = t::TokenType::PPlus;
        else if (match('=')) type = t::TokenType::PlusEquals;
        else                 type = t::TokenType::Plus;
        break;

    case '-':
        if (match('-'))      type = t::TokenType::MMinus;
        else if (match('=')) type = t::TokenType::MinusEquals;
        else if (match('>')) type = t::TokenType::Arrow;
        else                 type = t::TokenType::Minus;
        break;

    case '*':
        if (match('=')) type = t::TokenType::StarEquals;
        else            type = t::TokenType::Star;
        break;

    case '/':
        if (match('='))      type = t::TokenType::SlashEquals;
        else if (match('/')) type = t::TokenType::SSlash;
        else                 type = t::TokenType::Slash;
        break;

    case '%':
        if (match('=')) type = t::TokenType::PercEquals;
        else            type = t::TokenType::Perc;
        break;

    case '&':
        if (match('&'))      type = t::TokenType::AAmp;
        else if (match('=')) type = t::TokenType::AmpEquals;
        else                 type = t::TokenType::Amp;
        break;

    case '|':
        if (match('|'))      type = t::TokenType::BBar;
        else if (match('=')) type = t::TokenType::BarEquals;
        else                 type = t::TokenType::Bar;
        break;

    case '^':
        if (match('=')) type = t::TokenType::UpEquals;
        else            type = t::TokenType::Up;
        break;

    case '~':
        if (match('=')) type = t::TokenType::TildeEquals;
        else            type = t::TokenType::Tilde;
        break;

    case '!':
        if (match('=')) type = t::TokenType::NEquals;
        else            type = t::TokenType::Excl;
        break;

    case '=':
        if (match('=')) type = t::TokenType::EEquals;
        else            type = t::TokenType::Equals;
        break;

    case '<':
        if (match('<')) {
            if (match('=')) type = t::TokenType::LLeEquals;
            else            type = t::TokenType::LLesser;
        }
        else if (match('=')) {
            type = t::TokenType::LeEquals;
        }
        else {
            type = t::TokenType::Lesser;
        }
        break;

    case '>':
        if (match('>')) {
            if (match('=')) type = t::TokenType::GGrEquals;
            else            type = t::TokenType::GGreater;
        }
        else if (match('=')) {
            type = t::TokenType::GrEquals;
        }
        else {
            type = t::TokenType::Greater;
        }
        break;

    case ':':
        if (match(':')) type = t::TokenType::CColon;
        else            type = t::TokenType::Colon;
        break;

    case '.':
        if (peek() == '.' && peek(1) == '.') {
            advance(); advance();
            type = t::TokenType::Dots;
        }
        else {
            type = t::TokenType::Dot;
        }
        break;

    case '?': type = t::TokenType::Question; break;
    case '(': type = t::TokenType::Left; break;
    case ')': type = t::TokenType::Right; break;
    case '[': type = t::TokenType::SqLeft; break;
    case ']': type = t::TokenType::SqRight; break;
    case '{': type = t::TokenType::BrLeft; break;
    case '}': type = t::TokenType::BrRight; break;
    case ';': type = t::TokenType::Semi; break;
    case ',': type = t::TokenType::Comma; break;

    default:
        type = t::TokenType::Unknown;
        break;
    }

    const t::Length length = static_cast<t::Length>(cursor - start);
    Token tok{ start, length, 0, 1, type };
    thisExpr.push_back(tok);
    return tok;
}