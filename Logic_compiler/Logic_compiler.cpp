#include <iostream>
#include "Logic_compiler.h"
#include "lexer.h"
#include "parser.h"
#include "AST_manager.h"

int main() {
    constexpr std::string_view source = R"(
        int a = 42;
        constexpr const float b = 3.14f;
        constexpr auto c = a + 10 * 2;

        type Counter = int;
        Counter count = 0;

        auto status = eval(a == 42) {
            byte flag = 1;
        } else {
            byte flag = 0;
        };

        delete a;
        int reclaimed = (delete count);
        delete auto status;
    )";

    try {
        Lexer lexer(source);
        Parser parser(lexer, source);

        // 1. Generazione AST dal Parser
        ast::ASTTree tree = parser.parseTranslationUnit();

        // 2. Dump leggibile in console con rami e codice sorgente
        std::cout << "=== AST STRUCTURE DUMP ===\n";
        ast::ASTManager::print(tree, source);

        // 3. Salva l'AST su file binario
        if (ast::ASTManager::saveToFile(tree, "ast_cache.bin")) {
            std::cout << "\nAST salvato con successo in 'ast_cache.bin'\n";
        }

        // 4. Ricarica l'AST da file binario in una nuova istanza
        ast::ASTTree loadedTree;
        if (ast::ASTManager::loadFromFile(loadedTree, "ast_cache.bin")) {
            std::cout << "AST caricato con successo! Nodi caricati: "
                << loadedTree.nodes.size() << "\n\n";

            std::cout << "=== LOADED AST DUMP ===\n";
            ast::ASTManager::print(loadedTree, source);
        }

    }
    catch (const std::exception& e) {
        std::cerr << "ERRORE: " << e.what() << "\n";
    }

    return 0;
}