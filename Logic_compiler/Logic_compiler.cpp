#include <iostream>
#include <string_view>
#include "Logic_compiler.h"
#include "lexer.h"
#include "parser.h"

// Driver di test
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
        Parser parser(lexer, source); // La window consuma lazy i primi 2 token

        ast::ASTTree tree = parser.parseTranslationUnit();

        std::cout << "AST generato con successo! Nodi totali: " << tree.nodes.size() << "\n";
    }
    catch (const std::exception& e) {
        std::cerr << "ERRORE PARSER: " << e.what() << "\n";
    }

    return 0;
}