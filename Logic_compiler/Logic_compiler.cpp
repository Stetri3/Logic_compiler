#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include "Logic_compiler.h"
#include "lexer.h"
#include "parser.h"
#include "AST_manager.h"

#define EXAMPLE_PATH  R"(C:/Users/stefa/DEV/C/Logic_compiler/Logic_compiler/example/)"

int main() {
    std::ifstream file(EXAMPLE_PATH R"(testing/ex_alpha_01.lgc)");
    if (!file.is_open()) {
        std::cerr << "Impossibile aprire il file ex_alpha_01.lgc!\n";
        return 1;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string sourceCode = buffer.str();

    std::cout << "=== RUNNING PARSER ON ex_alpha_01.lgc ===\n\n";

    try {
        Lexer lexer(sourceCode);
        Parser parser(lexer, sourceCode);

        // 1. Parsing dell'intero file
        ast::ASTTree tree = parser.parseTranslationUnit();

        std::cout << " Parsing completato senza errori!\n";
        std::cout << " Nodi AST Generati: " << tree.nodes.size() << "\n\n";

        // 2. Dump dell'AST
        std::cout << "=== AST STRUCTURE DUMP ===\n";
        ast::ASTManager::print(tree, sourceCode);

        // 3. Salvataggio su file binario per la cache
        if (ast::ASTManager::saveToFile(tree, "ex_alpha_01.bin")) {
            std::cout << "\n AST salvato con successo in 'ex_alpha_01.bin'\n";
        }

    }
    catch (const std::exception& e) {
        std::cerr << "\n PARSER ERROR: " << e.what() << "\n";
        return 1;
    }

    return 0;
}