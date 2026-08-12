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

        // Iterazione sequenziale sui nodi memorizzati nel paged vector
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
                // Recupera la rappresentazione stringa dal token index della mappa statica
                size_t opIdx = static_cast<size_t>(node.data.binary.op);
                std::string_view opStr = (opIdx < static_cast<size_t>(t::TokenType::count))
                    ? t::TT_STRMAP[opIdx]
                    : "?";

                out += " (Op: '" + std::string(opStr) +
                    "', LHS: " + std::to_string(node.data.binary.lhs) +
                    ", RHS: " + std::to_string(node.data.binary.rhs) + ")";
                break;
            }
            case NodeKind::VarDecl:
            case NodeKind::TypeDecl:
                out += " (TypeNode: " + std::to_string(node.data.varDecl.typeNode) +
                    ", NameNode: " + std::to_string(node.data.varDecl.nameNode) +
                    ", InitExpr: " + std::to_string(node.data.varDecl.initExpr) + ")";
                break;
            case NodeKind::EvalExpr:
                out += " (Cond: " + std::to_string(node.data.evalOp.cond) +
                    ", Then: " + std::to_string(node.data.evalOp.thenBlock) +
                    ", Else: " + std::to_string(node.data.evalOp.elseBlock) + ")";
                break;
            case NodeKind::DeleteExpr:
            case NodeKind::CastExpr:
                out += " (Target: " + std::to_string(node.data.deleteOp.target) + ")";
                break;
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