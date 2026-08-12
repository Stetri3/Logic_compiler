#pragma once
#include <cstdint>
#include <string_view>
#include "Alloc_optimized.h"
#include "token.h"

class Lexer {
private:
    std::string_view source;
    t::Cursor cursor{ 0 };

    // Vector temporaneo per memorizzare i token della riga/espressione corrente
    OsPagedVector<Token> thisExpr{ 1024 };

    // Helper per l'ispezione dei caratteri
    [[nodiscard]] constexpr char peek(uint32_t offset = 0) const noexcept {
        if (cursor + offset >= source.size()) return '\0';
        return source[cursor + offset];
    }

    constexpr char advance() noexcept {
        if (cursor >= source.size()) return '\0';
        return source[cursor++];
    }

    constexpr bool match(char expected) noexcept {
        if (cursor >= source.size() || source[cursor] != expected) return false;
        cursor++;
        return true;
    }

    void skipWhitespaceAndComments() noexcept;

    // Handlers di parsing dedicati per categoria
    Token lexIdentifierOrKeyword() noexcept;
    Token lexNumber() noexcept;
    Token lexString() noexcept;
    Token lexChar() noexcept;

public:
    explicit Lexer(std::string_view src) noexcept : source(src), cursor(0) {}

    // Imposta o resetta il sorgente per il lexer
    void setSource(std::string_view src) noexcept {
        source = src;
        cursor = 0;
        thisExpr.clear();
    }

    // Estrae il prossimo token
    Token nextToken() noexcept;

    // Utility per recuperare l'estratto di testo associato a un Token
    [[nodiscard]] constexpr std::string_view getText(const Token& tok) const noexcept {
        if (tok.globalOffset + tok.size > source.size()) return {};
        return source.substr(tok.globalOffset, tok.size);
    }

    // Accessore al buffer interno dell'espressione
    const OsPagedVector<Token>& getBufferedTokens() const noexcept { return thisExpr; }
};