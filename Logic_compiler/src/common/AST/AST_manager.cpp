#include "AST_manager.h"

namespace ast {

    const char* ASTManager::nodeKindToString(NodeKind kind) noexcept {
        switch (kind) {
        case NodeKind::TranslationUnit: return "TranslationUnit";
        case NodeKind::VarDecl:         return "VarDecl";
        case NodeKind::TypeDecl:        return "TypeDecl";
        case NodeKind::StructDecl:      return "StructDecl";
        case NodeKind::NamespaceDecl:   return "NamespaceDecl";
        case NodeKind::FunctionDecl:    return "FunctionDecl";
        case NodeKind::BinaryExpr:      return "BinaryExpr";
        case NodeKind::UnaryExpr:       return "UnaryExpr";
        case NodeKind::LiteralExpr:     return "LiteralExpr";
        case NodeKind::IdentifierExpr:  return "IdentifierExpr";
        case NodeKind::CallExpr:        return "CallExpr";
        case NodeKind::MemberAccessExpr:return "MemberAccessExpr";
        case NodeKind::EvalExpr:        return "EvalExpr";
        case NodeKind::DeleteExpr:      return "DeleteExpr";
        case NodeKind::CastExpr:        return "CastExpr";
        case NodeKind::CompileTimeJump: return "CompileTimeJump";
        default:                        return "UnknownNode";
        }
    }

    bool ASTManager::saveToFile(const ASTTree& tree, const std::string& filepath) {
        std::ofstream file(filepath, std::ios::binary);
        if (!file.is_open()) return false;

        uint32_t nodeCount = static_cast<uint32_t>(tree.nodes.size());
        file.write(reinterpret_cast<const char*>(&nodeCount), sizeof(nodeCount));

        for (uint32_t i = 0; i < nodeCount; ++i) {
            file.write(reinterpret_cast<const char*>(&tree.get(i)), sizeof(Node));
        }

        return file.good();
    }

    bool ASTManager::loadFromFile(ASTTree& outTree, const std::string& filepath) {
        std::ifstream file(filepath, std::ios::binary);
        if (!file.is_open()) return false;

        uint32_t nodeCount = 0;
        file.read(reinterpret_cast<char*>(&nodeCount), sizeof(nodeCount));
        if (!file.good()) return false;

        outTree.nodes.clear();

        for (uint32_t i = 0; i < nodeCount; ++i) {
            Node n{};
            file.read(reinterpret_cast<char*>(&n), sizeof(Node));
            if (!file.good()) return false;

            outTree.nodes.push_back(n);
        }

        return true;
    }

    std::string ASTManager::dumpToString(const ASTTree& tree, std::string_view source) {
        if (tree.nodes.empty()) return "<Empty AST>\n";

        std::string out;
        out.reserve(tree.nodes.size() * 64);

        // Helper lambda per recuperare il nome dell'operatore dal token
        auto getOpStr = [](t::TokenType type) -> std::string_view {
            size_t idx = static_cast<size_t>(type);
            return (idx < static_cast<size_t>(t::TokenType::count)) ? t::TT_STRMAP[idx] : "?";
            };

        for (uint32_t i = 0; i < tree.nodes.size(); ++i) {
            const Node& node = tree.get(i);

            out += "[" + std::to_string(i) + "] " + nodeKindToString(node.kind);

            if (node.qualifiers.isConstexpr || node.qualifiers.isConst ||
                node.qualifiers.isStatic || node.qualifiers.isInline || node.qualifiers.isAuto) {
                out += " [";
                if (node.qualifiers.isConstexpr) out += "constexpr ";
                if (node.qualifiers.isConst)     out += "const ";
                if (node.qualifiers.isStatic)    out += "static ";
                if (node.qualifiers.isInline)    out += "inline ";
                if (node.qualifiers.isAuto)      out += "auto ";
                out.pop_back();
                out += "]";
            }

            if (!source.empty() && node.sourceLength > 0 &&
                (node.sourceOffset + node.sourceLength <= source.size())) {
                std::string_view snippet = source.substr(node.sourceOffset, node.sourceLength);
                out += " -> \"" + std::string(snippet) + "\"";
            }

            // Dettagli specifici per tipo di nodo
            switch (node.kind) {
            case NodeKind::BinaryExpr: {
                out += " (Op: '" + std::string(getOpStr(node.data.binary.op)) +
                    "', LHS: " + std::to_string(node.data.binary.lhs) +
                    ", RHS: " + std::to_string(node.data.binary.rhs) + ")";
                break;
            }
            case NodeKind::UnaryExpr: {
                out += " (Op: '" + std::string(getOpStr(node.data.unary.op)) +
                    "', Operand: " + std::to_string(node.data.unary.operand) + ")";
                break;
            }
            case NodeKind::VarDecl:
            case NodeKind::TypeDecl:
            case NodeKind::FunctionDecl: {
                out += " (TypeNode: " + std::to_string(node.data.varDecl.typeNode) +
                    ", NameNode: " + std::to_string(node.data.varDecl.nameNode) +
                    ", Init/Body: " + std::to_string(node.data.varDecl.initExpr) + ")";
                break;
            }
            case NodeKind::EvalExpr: {
                out += " (Cond: " + std::to_string(node.data.evalOp.cond) +
                    ", Then/Body: " + std::to_string(node.data.evalOp.thenBlock) +
                    ", Else/Init: " + std::to_string(node.data.evalOp.elseBlock) + ")";
                break;
            }
            case NodeKind::CompileTimeJump: {
                // Se si tratta di un break o continue (memorizzati via data.unary)
                if (node.data.unary.op == t::TokenType::kBreak || node.data.unary.op == t::TokenType::kContinue) {
                    out += " (Op: '" + std::string(getOpStr(node.data.unary.op)) + "')";
                }
                else { // Se si tratta di un ciclo (for / while, che usa data.evalOp)
                    out += " (Cond: " + std::to_string(node.data.evalOp.cond) +
                        ", Then/Body: " + std::to_string(node.data.evalOp.thenBlock) +
                        ", Else/Init: " + std::to_string(node.data.evalOp.elseBlock) + ")";
                }
                break;
            }
            case NodeKind::DeleteExpr:
            case NodeKind::CastExpr: {
                out += " (Target: " + std::to_string(node.data.deleteOp.target) + ")";
                break;
            }
            case NodeKind::CallExpr: {
                out += " (Target: " + std::to_string(node.data.call.target) +
                    ", ArgsList: " + std::to_string(node.data.call.argsList) + ")";
                break;
            }
            case NodeKind::MemberAccessExpr: {
                out += " (Op: '" + std::string(getOpStr(node.data.binary.op)) +
                    "', Target: " + std::to_string(node.data.binary.lhs) +
                    ", Member: " + std::to_string(node.data.binary.rhs) + ")";
                break;
            }
            case NodeKind::StructDecl:
            case NodeKind::NamespaceDecl: {
                out += " (NameOffset: " + std::to_string(node.data.call.target) +
                    ", BodyNode: " + std::to_string(node.data.call.argsList) + ")";
                break;
            }
            default:
                break;
            }

            out += "\n";
        }

        return out;
    }

    void ASTManager::print(const ASTTree& tree, std::string_view source) {
        std::cout << dumpToString(tree, source);
    }

} // namespace ast