#include "parser.h"

void Parser::initWindow() noexcept {
    for (uint8_t i = 0; i < kWindowSize; ++i) {
        window[i] = lexer.nextToken();
    }
}

const Token& Parser::peek(uint8_t offset) const noexcept {
    if (offset >= kWindowSize) {
        return window[kWindowSize - 1];
    }
    return window[offset];
}

Token Parser::advance() noexcept {
    Token current = window[0];

    // Shift della finestra di lookahead
    for (uint8_t i = 0; i < kWindowSize - 1; ++i) {
        window[i] = window[i + 1];
    }

    // Pull lazy del prossimo token dal Lexer se non siamo a EOF
    if (current.type != t::TokenType::Eof) {
        window[kWindowSize - 1] = lexer.nextToken();
    }
    else {
        // Mantiene l'EOF in coda alla finestra
        window[kWindowSize - 1] = current;
    }

    return current;
}

bool Parser::check(t::TokenType type) const noexcept {
    return peek(0).type == type;
}

bool Parser::match(t::TokenType type) noexcept {
    if (check(type)) {
        advance();
        return true;
    }
    return false;
}

[[noreturn]] void Parser::raiseError(const Token& tok, const std::string& msg) const {
    uint32_t line = 1;
    uint32_t col = 1;
    uint32_t offset = tok.globalOffset;

    if (offset > source.size()) {
        offset = static_cast<uint32_t>(source.size());
    }

    for (uint32_t i = 0; i < offset; ++i) {
        if (source[i] == '\n') {
            line++;
            col = 1;
        }
        else {
            col++;
        }
    }

    std::string tokenText;
    if (tok.type == t::TokenType::Eof) {
        tokenText = "<EOF>";
    }
    else {
        tokenText = std::string(source.substr(tok.globalOffset, tok.size));
    }

    std::string err = "[Line " + std::to_string(line) + ", Col " + std::to_string(col) +
        "] Syntax Error at '" + tokenText + "': " + msg;
    throw std::runtime_error(err);
}

Token Parser::expect(t::TokenType type, const char* errorMsg) {
    if (check(type)) return advance();
    raiseError(peek(0), errorMsg);
}

int Parser::getPrecedence(t::TokenType type) const noexcept {
    switch (type) {
    case t::TokenType::BBar:                                return 1;
    case t::TokenType::AAmp:                                return 2;
    case t::TokenType::EEquals: case t::TokenType::NEquals: return 3;
    case t::TokenType::Lesser:  case t::TokenType::Greater:
    case t::TokenType::LeEquals: case t::TokenType::GrEquals: return 4;
    case t::TokenType::LLesser: case t::TokenType::GGreater: return 5;
    case t::TokenType::Plus:   case t::TokenType::Minus:    return 6;
    case t::TokenType::Star:   case t::TokenType::Slash:
    case t::TokenType::Perc:                                return 7;
    default:                                                return 0;
    }
}

ast::Qualifiers Parser::parseQualifiers() noexcept {
    ast::Qualifiers q{};
    while (true) {
        if (match(t::TokenType::kConstexpr))      q.isConstexpr = 1;
        else if (match(t::TokenType::kConst))     q.isConst = 1;
        else if (match(t::TokenType::kStatic))    q.isStatic = 1;
        else if (match(t::TokenType::kInline))    q.isInline = 1;
        else if (match(t::TokenType::kAuto))      q.isAuto = 1;
        else break;
    }
    return q;
}

ast::NodeId Parser::parsePrimaryExpression() {
    const Token& tok = peek(0);

    // 1. Literals (Int, Float, String, Char)
    if (check(t::TokenType::LInt) || check(t::TokenType::LFloat) ||
        check(t::TokenType::LString) || check(t::TokenType::LChar)) {
        Token t = advance();
        ast::NodeId id = tree.createNode(ast::NodeKind::LiteralExpr, t.globalOffset, t.size);
        tree.get(id).data.literal = { t.type, 0 };
        return id;
    }

    // 2. Identifiers
    if (check(t::TokenType::Ident)) {
        Token t = advance();
        ast::NodeId id = tree.createNode(ast::NodeKind::IdentifierExpr, t.globalOffset, t.size);
        tree.get(id).data.literal = { t.type, 0 };
        return id;
    }

    // 3. Eval Construct
    if (check(t::TokenType::kEval)) {
        return parseEvalExpression();
    }

    // 4. Memory Re-casting / Sub-expression delete: (delete a)
    if (check(t::TokenType::Left) && peek(1).type == t::TokenType::kDelete) {
        advance(); // Consuma '('
        ast::NodeId delNode = parseDeleteStatement();
        expect(t::TokenType::Right, "Expected ')' after delete expression");

        ast::NodeId castId = tree.createNode(ast::NodeKind::CastExpr, tok.globalOffset);
        tree.get(castId).data.deleteOp = { delNode };
        return castId;
    }

    // 5. Block Statements usati come espressioni: { ... }
    if (match(t::TokenType::BrLeft)) {
        ast::NodeId blockId = tree.createNode(ast::NodeKind::TranslationUnit, tok.globalOffset);
        while (!check(t::TokenType::BrRight) && !check(t::TokenType::Eof)) {
            parseStatement();
        }
        expect(t::TokenType::BrRight, "Expected '}' at end of block expression");
        return blockId;
    }

    // 6. Parenthesized Expressions
    if (match(t::TokenType::Left)) {
        ast::NodeId expr = parseExpression(0);
        expect(t::TokenType::Right, "Expected ')'");
        return expr;
    }

    raiseError(tok, "Unexpected token in expression");
}

ast::NodeId Parser::parseEvalExpression() {
    Token evalTok = expect(t::TokenType::kEval, "Expected 'eval'");
    expect(t::TokenType::Left, "Expected '(' after eval");

    ast::NodeId cond = parseExpression(0);
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

    bool isAutoDelete = match(t::TokenType::kAuto);
    ast::NodeId target = parsePrimaryExpression();

    ast::NodeId id = tree.createNode(ast::NodeKind::DeleteExpr, delTok.globalOffset);
    tree.get(id).qualifiers.isAuto = isAutoDelete ? 1 : 0;
    tree.get(id).data.deleteOp = { target };
    return id;
}

ast::NodeId Parser::parseVarOrTypeDecl(ast::Qualifiers qual) {
    Token typeOrNameTok{};
    Token nameTok{};
    bool isTypeKeyword = false;

    // Caso 1: 'type T = int;'
    if (check(t::TokenType::kType)) {
        typeOrNameTok = advance(); // Consuma 'type'
        isTypeKeyword = true;
        nameTok = expect(t::TokenType::Ident, "Expected type name after 'type'");
    }
    // Caso 2: 'auto x = ...' oppure 'constexpr auto c = ...' (il tipo è implicitamente auto)
    else if (qual.isAuto) {
        typeOrNameTok = Token{ 0, 0, 0, 1, t::TokenType::kAuto }; // Sintetico
        nameTok = expect(t::TokenType::Ident, "Expected variable name after 'auto'");
    }
    // Caso 3: Dichiarazione standard 'Type name [= expr];' (es. int a = 42; o Counter count = 0;)
    else {
        typeOrNameTok = advance(); // Consuma il Tipo (es. 'int' o 'Counter')
        if (check(t::TokenType::Ident)) {
            nameTok = advance();   // Consuma il Nome (es. 'a' o 'count')
        }
        else {
            // Se non c'è un secondo identificatore ma un '=', allora typeOrNameTok era il nome e mancava il tipo
            raiseError(peek(0), "Expected variable name after type specifier");
        }
    }

    ast::NodeId initExpr = ast::kNullNode;
    if (match(t::TokenType::Equals)) {
        initExpr = parseExpression(0);
    }

    expect(t::TokenType::Semi, "Expected ';' after declaration");

    ast::NodeKind kind = isTypeKeyword ? ast::NodeKind::TypeDecl : ast::NodeKind::VarDecl;
    ast::NodeId id = tree.createNode(kind, typeOrNameTok.globalOffset);

    ast::NodeId typeNode = tree.createNode(ast::NodeKind::IdentifierExpr, typeOrNameTok.globalOffset, typeOrNameTok.size);
    tree.get(typeNode).data.literal = { typeOrNameTok.type, 0 };

    ast::NodeId nameNode = tree.createNode(ast::NodeKind::IdentifierExpr, nameTok.globalOffset, nameTok.size);
    tree.get(nameNode).data.literal = { nameTok.type, 0 };

    tree.get(id).qualifiers = qual;
    tree.get(id).data.varDecl = { typeNode, nameNode, initExpr };
    return id;
}

ast::NodeId Parser::parseStatement() {
    ast::Qualifiers qual = parseQualifiers();

    // 1. Compound Block Statement: { ... }
    if (match(t::TokenType::BrLeft)) {
        const Token& startTok = peek(0);
        ast::NodeId blockId = tree.createNode(ast::NodeKind::TranslationUnit, startTok.globalOffset);
        while (!check(t::TokenType::BrRight) && !check(t::TokenType::Eof)) {
            parseStatement();
        }
        expect(t::TokenType::BrRight, "Expected '}' at end of block");
        return blockId;
    }

    // 2. Delete Statement
    if (check(t::TokenType::kDelete)) {
        ast::NodeId delNode = parseDeleteStatement();
        if (check(t::TokenType::Semi)) {
            advance();
        }
        return delNode;
    }

    // 3. Variable or Type Declaration (Gestisce anche 'auto' già flaggato nei qualificatori)
    if (qual.isAuto || check(t::TokenType::Ident) || check(t::TokenType::kType) || check(t::TokenType::kByte)) {
        return parseVarOrTypeDecl(qual);
    }

    // 4. Expression Statement
    ast::NodeId expr = parseExpression(0);

    if (check(t::TokenType::Semi)) {
        advance();
    }
    return expr;
}

ast::NodeId Parser::parseExpression(int minPrecedence) {
    ast::NodeId lhs = parsePrimaryExpression();

    while (true) {
        const Token& tok = peek(0);
        int prec = getPrecedence(tok.type);

        if (prec < minPrecedence || prec == 0) {
            break;
        }

        Token opTok = advance();
        ast::NodeId rhs = parseExpression(prec + 1);

        ast::NodeId binId = tree.createNode(ast::NodeKind::BinaryExpr, opTok.globalOffset);

        tree.get(binId).data.binary = { opTok.type, lhs, rhs };

        lhs = binId;
    }

    return lhs;
}

ast::ASTTree Parser::parseTranslationUnit() {
    ast::NodeId root = tree.createNode(ast::NodeKind::TranslationUnit);

    while (!check(t::TokenType::Eof)) {
        uint32_t prevCursor = window[0].globalOffset;
        parseStatement();

        // Guardrail: se parseStatement non consuma token, interrompi il loop
        if (window[0].globalOffset == prevCursor && !check(t::TokenType::Eof)) {
            advance(); // forza l'avanzamento per evitare loop infiniti
        }
    }

    return std::move(tree);
}