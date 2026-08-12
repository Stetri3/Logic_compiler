#pragma once
#include <string_view>
#include <stdexcept>
#include "lexer.h"
#include "AST_def.h"

class Parser {
private:
    Lexer& lexer;
    std::string_view source;
    ast::ASTTree tree;

    uint32_t tokenCursor{ 0 };
    const OsPagedVector<Token>& tokens;

    // Helper utilities
    const Token& peek(uint32_t offset = 0) const noexcept;
    const Token& advance() noexcept;
    bool check(t::TokenType type) const noexcept;
    bool match(t::TokenType type) noexcept;
    Token expect(t::TokenType type, const char* errorMsg);

    // Parse Qualifiers (constexpr, const, static, etc.)
    ast::Qualifiers parseQualifiers() noexcept;

    // Recursive Descent Grammar Handlers
    ast::NodeId parseStatement();
    ast::NodeId parseVarOrTypeDecl(ast::Qualifiers qual);
    ast::NodeId parseDeleteStatement();
    ast::NodeId parseEvalExpression();

    // Expression Parsing (Pratt / Operator Precedence)
    ast::NodeId parseExpression(int minPrecedence = 0);
    ast::NodeId parsePrimaryExpression();

public:
    Parser(Lexer& lex, std::string_view src)
        : lexer(lex), source(src), tokens(lex.getBufferedTokens()) {}

    // Driver: parses full unit into the AST
    ast::ASTTree parseTranslationUnit();
};