#pragma once
#include <string_view>
#include <stdexcept>
#include <string>
#include "lexer.h"
#include "AST_def.h"

class Parser {
private:
    Lexer& lexer;
    std::string_view source;
    ast::ASTTree tree;

    // Lookahead window a dimensione fissa per il parsing Lazy
    static constexpr uint8_t kWindowSize = 2;
    Token window[kWindowSize]{};

    // Inizializza la finestra di lookahead consumando dal Lexer
    void initWindow() noexcept;

    // Helper per l'ispezione e avanzamento Lazy
    const Token& peek(uint8_t offset = 0) const noexcept;
    Token advance() noexcept;
    bool check(t::TokenType type) const noexcept;
    bool match(t::TokenType type) noexcept;
    Token expect(t::TokenType type, const char* errorMsg);

    // Diagnostics
    [[noreturn]] void raiseError(const Token& tok, const std::string& msg) const;

    // Operator precedence mapping (Pratt)
    int getPrecedence(t::TokenType type) const noexcept;

    // Directives & Qualifiers
    ast::Qualifiers parseQualifiers() noexcept;

    // Recursive Descent Grammar Handlers
    ast::NodeId parseStatement();
    ast::NodeId parseVarOrTypeDecl(ast::Qualifiers qual);
    ast::NodeId parseDeleteStatement();
    ast::NodeId parseEvalExpression();

    // Expression Parsing
    ast::NodeId parseExpression(int minPrecedence = 0);
    ast::NodeId parsePrimaryExpression();

public:
    Parser(Lexer& lex, std::string_view src)
        : lexer(lex), source(src) {
        initWindow();
    }

    // Driver principale
    ast::ASTTree parseTranslationUnit();
};