#include "parser.h"

const Token& Parser::peek(uint32_t offset) const noexcept {
    if (tokenCursor + offset >= tokens.size()) {
        return tokens.back(); // Returns EOF token
    }
    return tokens[tokenCursor + offset];
}

const Token& Parser::advance() noexcept {
    const Token& tok = peek();
    if (tok.type != t::TokenType::Eof) {
        tokenCursor++;
    }
    return tok;
}

bool Parser::check(t::TokenType type) const noexcept {
    return peek().type == type;
}

bool Parser::match(t::TokenType type) noexcept {
    if (check(type)) {
        advance();
        return true;
    }
    return false;
}

Token Parser::expect(t::TokenType type, const char* errorMsg) {
    if (check(type)) return advance();
    // In production, delegate to a diagnostic engine using tok.globalOffset
    throw std::runtime_error(errorMsg);
}

ast::Qualifiers Parser::parseQualifiers() noexcept {
    ast::Qualifiers q{};
    while (true) {
        if (match(t::TokenType::kConstexpr)) q.isConstexpr = 1;
        else if (match(t::TokenType::kConst)) q.isConst = 1;
        else if (match(t::TokenType::kStatic)) q.isStatic = 1;
        else if (match(t::TokenType::kInline)) q.isInline = 1;
        else if (match(t::TokenType::kAuto)) q.isAuto = 1;
        else break;
    }
    return q;
}

ast::NodeId Parser::parsePrimaryExpression() {
    const Token& tok = peek();

    // 1. Literals
    if (check(t::TokenType::LInt) || check(t::TokenType::LFloat) ||
        check(t::TokenType::LString) || check(t::TokenType::LChar)) {
        advance();
        ast::NodeId id = tree.createNode(ast::NodeKind::LiteralExpr, tok.globalOffset, tok.size);
        tree.get(id).data.literal = { tok.type, tokenCursor - 1 };
        return id;
    }

    // 2. Identifiers
    if (check(t::TokenType::Ident)) {
        advance();
        ast::NodeId id = tree.createNode(ast::NodeKind::IdentifierExpr, tok.globalOffset, tok.size);
        tree.get(id).data.literal = { tok.type, tokenCursor - 1 };
        return id;
    }

    // 3. Eval Construct: eval(cond) { ... } else { ... }
    if (check(t::TokenType::kEval)) {
        return parseEvalExpression();
    }

    // 4. Memory Re-casting / Delete Sub-expression: (delete a)
    if (check(t::TokenType::Left) && peek(1).type == t::TokenType::kDelete) {
        advance(); // consume '('
        ast::NodeId delNode = parseDeleteStatement();
        expect(t::TokenType::Right, "Expected ')' after delete expression");
        return delNode;
    }

    // 5. Parenthesized Expressions
    if (match(t::TokenType::Left)) {
        ast::NodeId expr = parseExpression();
        expect(t::TokenType::Right, "Expected ')'");
        return expr;
    }

    throw std::runtime_error("Unexpected token in expression");
}

ast::NodeId Parser::parseEvalExpression() {
    Token evalTok = expect(t::TokenType::kEval, "Expected 'eval'");
    expect(t::TokenType::Left, "Expected '(' after eval");

    ast::NodeId cond = parseExpression();
    expect(t::TokenType::Right, "Expected ')' after eval condition");

    ast::NodeId thenBlock = parseStatement();
    ast::NodeId elseBlock = ast::kNullNode;

    if (match(t::TokenType::kElse)) {
        elseBlock = parseStatement();
    }

    ast::NodeId id = tree.createNode(ast::NodeKind::EvalExpr, evalTok.globalOffset);
    tree.get(id).data.evalOp = { cond, thenBlock, elseBlock };
    return id;
}

ast::NodeId Parser::parseDeleteStatement() {
    Token delTok = expect(t::TokenType::kDelete, "Expected 'delete'");

    // Handle 'delete auto x;' vs 'delete x;'
    bool isAutoDelete = match(t::TokenType::kAuto);
    ast::NodeId target = parsePrimaryExpression();

    ast::NodeId id = tree.createNode(ast::NodeKind::DeleteExpr, delTok.globalOffset);
    tree.get(id).data.deleteOp = { target };
    return id;
}

ast::NodeId Parser::parseVarOrTypeDecl(ast::Qualifiers qual) {
    Token typeOrNameTok = advance(); // Type specifier or 'type' keyword
    Token nameTok = expect(t::TokenType::Ident, "Expected variable or type name");

    ast::NodeId initExpr = ast::kNullNode;
    if (match(t::TokenType::Equals)) {
        initExpr = parseExpression();
    }

    expect(t::TokenType::Semi, "Expected ';' after declaration");

    ast::NodeId id = tree.createNode(ast::NodeKind::VarDecl, typeOrNameTok.globalOffset);
    tree.get(id).qualifiers = qual;
    tree.get(id).data.varDecl = { ast::kNullNode, ast::kNullNode, initExpr };
    return id;
}

ast::NodeId Parser::parseStatement() {
    ast::Qualifiers qual = parseQualifiers();

    if (check(t::TokenType::kDelete)) {
        ast::NodeId delNode = parseDeleteStatement();
        expect(t::TokenType::Semi, "Expected ';' after delete");
        return delNode;
    }

    // Default: try variable declaration or expression statement
    if (check(t::TokenType::Ident) || check(t::TokenType::kType) || check(t::TokenType::kByte)) {
        return parseVarOrTypeDecl(qual);
    }

    ast::NodeId expr = parseExpression();
    expect(t::TokenType::Semi, "Expected ';'");
    return expr;
}

ast::NodeId Parser::parseExpression(int minPrecedence) {
    ast::NodeId lhs = parsePrimaryExpression();

    while (true) {
        const Token& tok = peek();
        // Expand precedence table matching your TokenType enum operators
        if (tok.type != t::TokenType::Plus && tok.type != t::TokenType::Minus &&
            tok.type != t::TokenType::Star && tok.type != t::TokenType::Slash) {
            break;
        }

        char op = (tok.type == t::TokenType::Plus) ? '+' : '-'; // Example mapping
        advance(); // consume operator

        ast::NodeId rhs = parseExpression(minPrecedence + 1);
        ast::NodeId binId = tree.createNode(ast::NodeKind::BinaryExpr, tok.globalOffset);
        tree.get(binId).data.binary = { op, lhs, rhs };
        lhs = binId;
    }

    return lhs;
}

ast::ASTTree Parser::parseTranslationUnit() {
    // Populate tokens via Lexer driver
    while (lexer.nextToken().type != t::TokenType::Eof) {}

    ast::NodeId root = tree.createNode(ast::NodeKind::TranslationUnit);

    while (tokenCursor < tokens.size() && tokens[tokenCursor].type != t::TokenType::Eof) {
        parseStatement();
    }
    //TODO: implement move for OsPagedVector
    return std::move(tree);
}