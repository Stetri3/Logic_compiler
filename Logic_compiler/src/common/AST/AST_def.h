#pragma once
#include <cstdint>
#include <string_view>
#include "token.h"
#include "Alloc_optimized.h"

namespace ast {

    using NodeId = uint32_t;
    inline constexpr NodeId kNullNode = 0xFFFFFFFF;

    enum class NodeKind : uint8_t {
        // Declarations & Statements
        TranslationUnit,
        VarDecl,
        TypeDecl,       // type T = int; or type Clean { ... }
        StructDecl,
        NamespaceDecl,
        FunctionDecl,

        // Expressions
        BinaryExpr,
        UnaryExpr,
        LiteralExpr,
        IdentifierExpr,
        CallExpr,
        MemberAccessExpr,
        EvalExpr,       // eval(cond) { ... } else { ... }

        // Memory & Lifetime Operations
        DeleteExpr,     // delete a; or delete auto x;
        CastExpr,       // (delete a)

        // Directives / Modifiers
        CompileTimeJump // constexpr for, constexpr if
    };

    // Qualifiers mask bitfield for fast checks during semantic analysis
    struct Qualifiers {
        uint8_t isConstexpr : 1 = 0;
        uint8_t isConst : 1 = 0;
        uint8_t isStatic : 1 = 0;
        uint8_t isInline : 1 = 0;
        uint8_t isAuto : 1 = 0;
    };

    // Compact AST Node (Data-Oriented, 32 bytes)
    struct Node {
        NodeKind kind;
        Qualifiers qualifiers;
        uint16_t reserved{ 0 };

        // Source tracking for diagnostics
        t::Offset sourceOffset{ 0 };
        t::Length sourceLength{ 0 };

        // Node payload based on NodeKind
        union {
            struct { char op; NodeId lhs; NodeId rhs; } binary;
            struct { char op; NodeId operand; } unary;
            struct { t::TokenType type; uint32_t tokenIndex; } literal;
            struct { NodeId typeNode; NodeId nameNode; NodeId initExpr; } varDecl;
            struct { NodeId target; NodeId argsList; } call;
            struct { NodeId target; } deleteOp;
            struct { NodeId cond; NodeId thenBlock; NodeId elseBlock; } evalOp;
        } data;
    };

    // Global AST storage container
    struct ASTTree {
        OsPagedVector<Node> nodes{ 2048 };

        ASTTree() = default;

        // 1. Explicitly delete copy operations (Virtual memory is non-copyable)
        ASTTree(const ASTTree&) = delete;
        ASTTree& operator=(const ASTTree&) = delete;

        // 2. Explicitly define move operations so compiler allows std::move
        ASTTree(ASTTree&& other) noexcept : nodes(std::move(other.nodes)) {}
        ASTTree& operator=(ASTTree&& other) noexcept {
            if (this != &other) {
                nodes = std::move(other.nodes);
            }
            return *this;
        }

        NodeId createNode(NodeKind kind, t::Offset offset = 0, t::Length len = 0) {
            Node n{};
            n.kind = kind;
            n.sourceOffset = offset;
            n.sourceLength = len;
            nodes.push_back(n);
            return static_cast<NodeId>(nodes.size() - 1);
        }

        Node& get(NodeId id) { return nodes[id]; }
        const Node& get(NodeId id) const { return nodes[id]; }
    };

} // namespace ast