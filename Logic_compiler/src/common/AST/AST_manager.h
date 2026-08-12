#pragma once
#include <string>
#include <string_view>
#include <fstream>
#include <iostream>
#include <vector>
#include "AST_def.h"
#include "token.h"

namespace ast {

    class ASTManager {
    public:
        // 1. Serializzazione binaria (Scrittura rapida su disco)
        static bool saveToFile(const ASTTree& tree, const std::string& filepath);

        // 2. Deserializzazione binaria (Caricamento diretto da disco)
        static bool loadFromFile(ASTTree& outTree, const std::string& filepath);

        // 3. Stringifier Human-Readable (Visualizzazione strutturata dell'AST)
        static std::string dumpToString(const ASTTree& tree, std::string_view source = {});

        // 4. Print diretto a stdout per debugging
        static void print(const ASTTree& tree, std::string_view source = {});

    private:
        static void dumpNode(const ASTTree& tree, NodeId id, std::string_view source,
            std::string& out, int depth, bool isLast);

        static const char* nodeKindToString(NodeKind kind) noexcept;
    };

} // namespace ast