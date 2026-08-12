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
    case t::TokenType::Equals:
    case t::TokenType::PlusEquals:  case t::TokenType::MinusEquals:
    case t::TokenType::StarEquals:  case t::TokenType::SlashEquals:
    case t::TokenType::PercEquals:  case t::TokenType::AmpEquals:
    case t::TokenType::BarEquals:   case t::TokenType::UpEquals:  return 1;
    case t::TokenType::BBar:                                      return 2;
    case t::TokenType::AAmp:                                      return 3;
    case t::TokenType::Bar:                                       return 4;
    case t::TokenType::Up:                                        return 5;
    case t::TokenType::Amp:                                       return 6;
    case t::TokenType::EEquals:     case t::TokenType::NEquals:   return 7;
    case t::TokenType::Lesser:      case t::TokenType::Greater:
    case t::TokenType::LeEquals:    case t::TokenType::GrEquals:  return 8;
    case t::TokenType::LLesser:     case t::TokenType::GGreater:  return 9;
    case t::TokenType::Plus:        case t::TokenType::Minus:     return 10;
    case t::TokenType::Star:        case t::TokenType::Slash:
    case t::TokenType::Perc:                                      return 11;
    default:                                                      return 0;
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

    // 1. Operatori Unari Prefissi: ++x, --x, !x, ~x, -x
    if (check(t::TokenType::PPlus) || check(t::TokenType::MMinus) ||
        check(t::TokenType::Excl) || check(t::TokenType::Tilde) ||
        check(t::TokenType::Minus) || check(t::TokenType::Amp) ||
        check(t::TokenType::Star)) {
        Token op = advance();
        ast::NodeId operand = parsePrimaryExpression();
        ast::NodeId id = tree.createNode(ast::NodeKind::UnaryExpr, op.globalOffset, op.size);
        tree.get(id).data.unary = { op.type, operand };
        return parsePostfixExpression(id);
    }

    // 2. Literals (Int, Float, String, Char, Bool)
    if (check(t::TokenType::LInt) || check(t::TokenType::LFloat) ||
        check(t::TokenType::LString) || check(t::TokenType::LChar) ||
        check(t::TokenType::kTrue) || check(t::TokenType::kFalse)) {
        Token t = advance();
        ast::NodeId id = tree.createNode(ast::NodeKind::LiteralExpr, t.globalOffset, t.size);
        tree.get(id).data.literal = { t.type, 0 };
        return parsePostfixExpression(id);
    }

    // 3. User Identifiers & Builtin Base Type Keywords (treated uniformly as IdentifierExpr)
    if (check(t::TokenType::Ident) || check(t::TokenType::kVoid) || check(t::TokenType::kByte)) {
        Token t = advance();
        ast::NodeId id = tree.createNode(ast::NodeKind::IdentifierExpr, t.globalOffset, t.size);
        tree.get(id).data.literal = { t.type, 0 };
        return parsePostfixExpression(id);
    }

    // 4. Eval Construct
    if (check(t::TokenType::kEval)) {
        return parsePostfixExpression(parseEvalExpression());
    }

    // 5. Memory Re-casting / Sub-expression delete: (delete a)
    if (check(t::TokenType::Left) && peek(1).type == t::TokenType::kDelete) {
        advance(); // Consuma '('
        ast::NodeId delNode = parseDeleteStatement();
        expect(t::TokenType::Right, "Expected ')' after delete expression");

        ast::NodeId castId = tree.createNode(ast::NodeKind::CastExpr, tok.globalOffset);
        tree.get(castId).data.deleteOp = { delNode };
        return parsePostfixExpression(castId);
    }

    // 6. Block Statements usati come espressioni: { ... }
    if (match(t::TokenType::BrLeft)) {
        ast::NodeId blockId = tree.createNode(ast::NodeKind::TranslationUnit, tok.globalOffset);
        while (!check(t::TokenType::BrRight) && !check(t::TokenType::Eof)) {
            parseStatement();
        }
        expect(t::TokenType::BrRight, "Expected '}' at end of block expression");
        return parsePostfixExpression(blockId);
    }

    // 7. Parenthesized Expressions
    if (match(t::TokenType::Left)) {
        ast::NodeId expr = parseExpression(0);
        expect(t::TokenType::Right, "Expected ')'");
        return parsePostfixExpression(expr);
    }

    raiseError(tok, "Unexpected token in expression");
}

ast::NodeId Parser::parsePostfixExpression(ast::NodeId expr) {
    while (true) {
        // Chiamata di funzione: fn(arg1, arg2)
        if (match(t::TokenType::Left)) {
            ast::NodeId argsList = ast::kNullNode;
            if (!check(t::TokenType::Right)) {
                argsList = parseExpression(0);
                while (match(t::TokenType::Comma)) {
                    parseExpression(0);
                }
            }
            expect(t::TokenType::Right, "Expected ')' after call arguments");
            ast::NodeId callId = tree.createNode(ast::NodeKind::CallExpr, peek(0).globalOffset);
            tree.get(callId).data.call = { expr, argsList };
            expr = callId;
        }
        // Accesso membri: obj.member o ptr->member
        else if (check(t::TokenType::Dot) || check(t::TokenType::Arrow)) {
            Token op = advance();
            Token member = expect(t::TokenType::Ident, "Expected member name after access operator");
            ast::NodeId memberNode = tree.createNode(ast::NodeKind::IdentifierExpr, member.globalOffset, member.size);
            tree.get(memberNode).data.literal = { member.type, 0 };

            ast::NodeId accessId = tree.createNode(ast::NodeKind::MemberAccessExpr, op.globalOffset);
            tree.get(accessId).data.binary = { op.type, expr, memberNode };
            expr = accessId;
        }
        // Indicizzazione Array: arr[index]
        else if (match(t::TokenType::SqLeft)) {
            ast::NodeId indexExpr = parseExpression(0);
            expect(t::TokenType::SqRight, "Expected ']' after array index");
            ast::NodeId idxId = tree.createNode(ast::NodeKind::BinaryExpr, peek(0).globalOffset);
            tree.get(idxId).data.binary = { t::TokenType::SqLeft, expr, indexExpr };
            expr = idxId;
        }
        // Postfix ++ e --
        else if (check(t::TokenType::PPlus) || check(t::TokenType::MMinus)) {
            Token op = advance();
            ast::NodeId postFixId = tree.createNode(ast::NodeKind::UnaryExpr, op.globalOffset);
            tree.get(postFixId).data.unary = { op.type, expr };
            expr = postFixId;
        }
        else {
            break;
        }
    }
    return expr;
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

ast::NodeId Parser::parseIfStatement() {
    Token ifTok = expect(t::TokenType::kIf, "Expected 'if'");
    expect(t::TokenType::Left, "Expected '(' after if");
    ast::NodeId cond = parseExpression(0);
    expect(t::TokenType::Right, "Expected ')' after condition");

    ast::NodeId thenBlock = parseStatement();
    ast::NodeId elseBlock = ast::kNullNode;

    if (match(t::TokenType::kElse)) {
        elseBlock = parseStatement();
    }

    ast::NodeId id = tree.createNode(ast::NodeKind::EvalExpr, ifTok.globalOffset);
    tree.get(id).data.evalOp = { cond, thenBlock, elseBlock };
    return id;
}

ast::NodeId Parser::parseWhileStatement() {
    Token whileTok = expect(t::TokenType::kWhile, "Expected 'while'");
    expect(t::TokenType::Left, "Expected '(' after while");
    ast::NodeId cond = parseExpression(0);
    expect(t::TokenType::Right, "Expected ')' after while condition");

    ast::NodeId body = parseStatement();

    ast::NodeId id = tree.createNode(ast::NodeKind::CompileTimeJump, whileTok.globalOffset);
    tree.get(id).data.evalOp = { cond, body, ast::kNullNode };
    return id;
}

ast::NodeId Parser::parseForStatement() {
    Token forTok = expect(t::TokenType::kFor, "Expected 'for'");
    expect(t::TokenType::Left, "Expected '(' after for");

    ast::NodeId init = ast::kNullNode;
    if (!check(t::TokenType::Semi)) {
        init = parseStatement();
    }
    else {
        advance();
    }

    ast::NodeId cond = ast::kNullNode;
    if (!check(t::TokenType::Semi)) {
        cond = parseExpression(0);
    }
    expect(t::TokenType::Semi, "Expected ';' after for condition");

    ast::NodeId incr = ast::kNullNode;
    if (!check(t::TokenType::Right)) {
        incr = parseExpression(0);
    }
    expect(t::TokenType::Right, "Expected ')' after for clause");

    ast::NodeId body = parseStatement();

    ast::NodeId id = tree.createNode(ast::NodeKind::CompileTimeJump, forTok.globalOffset);
    tree.get(id).data.evalOp = { cond, body, init };
    return id;
}

ast::NodeId Parser::parseReturnStatement() {
    Token retTok = expect(t::TokenType::kReturn, "Expected 'return'");
    ast::NodeId expr = ast::kNullNode;
    if (!check(t::TokenType::Semi)) {
        expr = parseExpression(0);
    }
    expect(t::TokenType::Semi, "Expected ';' after return value");

    ast::NodeId id = tree.createNode(ast::NodeKind::UnaryExpr, retTok.globalOffset);
    tree.get(id).data.unary = { t::TokenType::kReturn, expr };
    return id;
}

ast::NodeId Parser::parseStructDecl() {
    Token structTok = expect(t::TokenType::kStruct, "Expected 'struct'");
    Token nameTok = expect(t::TokenType::Ident, "Expected struct name");

    ast::NodeId body = ast::kNullNode;
    if (check(t::TokenType::BrLeft)) {
        body = parseStatement();
    }
    if (check(t::TokenType::Semi)) advance();

    ast::NodeId id = tree.createNode(ast::NodeKind::StructDecl, structTok.globalOffset);
    tree.get(id).data.call = { nameTok.globalOffset, body };
    return id;
}

ast::NodeId Parser::parseNamespaceDecl() {
    Token nsTok = expect(t::TokenType::kNamespace, "Expected 'namespace'");
    Token nameTok = expect(t::TokenType::Ident, "Expected namespace name");
    ast::NodeId body = parseStatement();

    ast::NodeId id = tree.createNode(ast::NodeKind::NamespaceDecl, nsTok.globalOffset);
    tree.get(id).data.call = { nameTok.globalOffset, body };
    return id;
}

ast::NodeId Parser::parseParamDecl() {
    ast::Qualifiers qual = parseQualifiers();
    Token typeTok{};
    Token nameTok{};

    if (qual.isAuto) {
        typeTok = Token{ 0, 0, 0, 1, t::TokenType::kAuto };
        nameTok = expect(t::TokenType::Ident, "Expected parameter name after 'auto'");
    }
    else {
        typeTok = advance(); // Type specifier (e.g. 'int')
        if (check(t::TokenType::Ident)) {
            nameTok = advance(); // Name (e.g. 'n')
        }
        else {
            // Support unnamed parameters e.g. void foo(int)
            nameTok = Token{ 0, 0, 0, 1, t::TokenType::Unknown };
        }
    }

    ast::NodeId initExpr = ast::kNullNode;
    if (match(t::TokenType::Equals)) {
        initExpr = parseExpression(0);
    }

    ast::NodeId id = tree.createNode(ast::NodeKind::VarDecl, typeTok.globalOffset);
    ast::NodeId typeNode = tree.createNode(ast::NodeKind::IdentifierExpr, typeTok.globalOffset, typeTok.size);
    tree.get(typeNode).data.literal = { typeTok.type, 0 };

    ast::NodeId nameNode = tree.createNode(ast::NodeKind::IdentifierExpr, nameTok.globalOffset, nameTok.size);
    tree.get(nameNode).data.literal = { nameTok.type, 0 };

    tree.get(id).qualifiers = qual;
    tree.get(id).data.varDecl = { typeNode, nameNode, initExpr };
    return id;
}

ast::NodeId Parser::parseFunctionDecl(ast::Qualifiers qual, Token typeTok, Token nameTok) {
    expect(t::TokenType::Left, "Expected '(' after function name");

    // Parse parameter list using parseParamDecl instead of parseVarOrTypeDecl
    ast::NodeId paramsList = ast::kNullNode;
    if (!check(t::TokenType::Right)) {
        paramsList = parseParamDecl();
        while (match(t::TokenType::Comma)) {
            parseParamDecl();
        }
    }
    expect(t::TokenType::Right, "Expected ')' after parameters");

    // Function Body { ... }
    ast::NodeId body = ast::kNullNode;
    if (check(t::TokenType::BrLeft)) {
        body = parseStatement();
    }
    else {
        expect(t::TokenType::Semi, "Expected ';' or '{' after function signature");
    }

    ast::NodeId typeNode = tree.createNode(ast::NodeKind::IdentifierExpr, typeTok.globalOffset, typeTok.size);
    tree.get(typeNode).data.literal = { typeTok.type, 0 };

    ast::NodeId nameNode = tree.createNode(ast::NodeKind::IdentifierExpr, nameTok.globalOffset, nameTok.size);
    tree.get(nameNode).data.literal = { nameTok.type, 0 };

    ast::NodeId fnId = tree.createNode(ast::NodeKind::FunctionDecl, nameTok.globalOffset);
    tree.get(fnId).qualifiers = qual;
    tree.get(fnId).data.varDecl = { typeNode, nameNode, body };
    return fnId;
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

    // Caso 1: 'type T ...'
    if (check(t::TokenType::kType)) {
        typeOrNameTok = advance(); // Consuma 'type'
        isTypeKeyword = true;
        nameTok = expect(t::TokenType::Ident, "Expected type name after 'type'");

        ast::NodeId baseTypeNode = ast::kNullNode;

        // Check if there is an inheritance/alias specifier: '=', ':', or contextual 'using'
        if (match(t::TokenType::Equals)) {
            baseTypeNode = parseExpression(0);
        }
        else if (match(t::TokenType::Colon)) {
            baseTypeNode = parsePrimaryExpression(); // Base type
        }
        else if (check(t::TokenType::Ident) && source.substr(peek(0).globalOffset, peek(0).size) == "using") {
            advance(); // Consume contextual 'using'
            baseTypeNode = parsePrimaryExpression(); // Base type
        }

        // Check for optional struct/type body block { ... }
        ast::NodeId bodyNode = ast::kNullNode;
        if (check(t::TokenType::BrLeft)) {
            bodyNode = parseStatement(); // Parses { ... }
        }

        // Semicolon is optional if followed by a block, required if a single-line alias
        if (check(t::TokenType::Semi)) {
            advance();
        }

        ast::NodeId typeDeclId = tree.createNode(ast::NodeKind::TypeDecl, typeOrNameTok.globalOffset);

        ast::NodeId typeNode = tree.createNode(ast::NodeKind::IdentifierExpr, typeOrNameTok.globalOffset, typeOrNameTok.size);
        tree.get(typeNode).data.literal = { typeOrNameTok.type, 0 };

        ast::NodeId nameNode = tree.createNode(ast::NodeKind::IdentifierExpr, nameTok.globalOffset, nameTok.size);
        tree.get(nameNode).data.literal = { nameTok.type, 0 };

        tree.get(typeDeclId).qualifiers = qual;

        // Link base type and optional body node
        ast::NodeId targetInit = (bodyNode != ast::kNullNode) ? bodyNode : baseTypeNode;
        tree.get(typeDeclId).data.varDecl = { typeNode, nameNode, targetInit };
        return typeDeclId;
    }

    // Caso 2: 'auto x = ...' oppure 'constexpr auto c = ...'
    else if (qual.isAuto) {
        typeOrNameTok = Token{ 0, 0, 0, 1, t::TokenType::kAuto };
        nameTok = expect(t::TokenType::Ident, "Expected variable name after 'auto'");
    }
    // Caso 3: Dichiarazione standard 'Type name ...'
    else {
        typeOrNameTok = advance();
        if (check(t::TokenType::Ident)) {
            nameTok = advance();
        }
        else {
            raiseError(peek(0), "Expected variable name after type specifier");
        }
    }

    // INTERCETTAZIONE FUNZIONI: Se trova '(', si tratta di una dichiarazione/definizione di funzione!
    if (check(t::TokenType::Left)) {
        return parseFunctionDecl(qual, typeOrNameTok, nameTok);
    }

    // Altrimenti è una normale variabile
    ast::NodeId initExpr = ast::kNullNode;
    if (match(t::TokenType::Equals)) {
        initExpr = parseExpression(0);
    }

    expect(t::TokenType::Semi, "Expected ';' after declaration");

    ast::NodeId id = tree.createNode(ast::NodeKind::VarDecl, typeOrNameTok.globalOffset);

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

    // 2. Control Flow Constructs
    if (check(t::TokenType::kIf))       return parseIfStatement();
    if (check(t::TokenType::kWhile))    return parseWhileStatement();
    if (check(t::TokenType::kFor))      return parseForStatement();
    if (check(t::TokenType::kReturn))   return parseReturnStatement();
    if (check(t::TokenType::kBreak) || check(t::TokenType::kContinue)) {
        Token ctrlTok = advance();
        expect(t::TokenType::Semi, "Expected ';' after loop control statement");

        ast::NodeId id = tree.createNode(ast::NodeKind::CompileTimeJump, ctrlTok.globalOffset);
        tree.get(id).data.unary = { ctrlTok.type, ast::kNullNode };
        return id;
    }

    // 3. Structural Declarations
    if (check(t::TokenType::kStruct))    return parseStructDecl();
    if (check(t::TokenType::kNamespace)) return parseNamespaceDecl();

    // 4. Memory Delete Statement
    if (check(t::TokenType::kDelete)) {
        ast::NodeId delNode = parseDeleteStatement();
        if (check(t::TokenType::Semi)) advance();
        return delNode;
    }

    // 5. Contextual "requires:" clause inside type constraint blocks
    if (match(t::TokenType::kRequires)) {
        expect(t::TokenType::Colon, "Expected ':' after 'requires'");
        ast::NodeId reqExpr = parseExpression(0);
        if (check(t::TokenType::Semi)) advance();
        return reqExpr;
    }

    // 6. Variable / Type / Function Declarations
    // Check if this is explicitly a declaration:
    // - Has qualifiers or explicit keywords (auto, type, byte, struct)
    // - OR (Ident Ident) -> e.g. "int x", "float y", "Point p", "Counter count"
    bool isDecl = qual.isAuto || check(t::TokenType::kType) || check(t::TokenType::kByte) || check(t::TokenType::kStruct);

    if (!isDecl && check(t::TokenType::Ident)) {
        t::TokenType nextType = peek(1).type;
        if (nextType == t::TokenType::Ident) {
            isDecl = true; // "Ident Ident" pattern -> "int x", "Scalar weight", etc.
        }
    }

    if (isDecl) {
        return parseVarOrTypeDecl(qual);
    }

    // 7. Otherwise: Expression Statement (e.g. "sum += i;", "p.x = 100;", "globalCount++;")
    ast::NodeId expr = parseExpression(0);
    if (check(t::TokenType::Semi)) advance();
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

        // Guardrail per evitare loop infiniti
        if (window[0].globalOffset == prevCursor && !check(t::TokenType::Eof)) {
            advance();
        }
    }

    return std::move(tree);
}