#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <iomanip>
#include "Logic_compiler.h"
#include "lexer.h"
#include "parser.h"
#include "AST_manager.h"
#include "compiler.h" // Added LLVM Compiler Header

#define EXAMPLE_PATH  R"(C:/Users/stefa/DEV/C/Logic_compiler/Logic_compiler/example/)"

int main() {
    // Definisci fino a quale ex_alpha_XX.lgc vuoi arrivare
    constexpr int MAX_N = 2;

    for (int i = 1; i <= MAX_N; ++i) {
        // Formatta il numero a due cifre (01, 02, etc.)
        std::ostringstream filenameStream;
        filenameStream << "ex_alpha_" << std::setw(2) << std::setfill('0') << i << ".lgc";
        std::string filename = filenameStream.str();

        std::ostringstream binFilenameStream;
        binFilenameStream << "ex_alpha_" << std::setw(2) << std::setfill('0') << i << ".bin";
        std::string binFilename = binFilenameStream.str();

        std::ostringstream irFilenameStream;
        irFilenameStream << "ex_alpha_" << std::setw(2) << std::setfill('0') << i << ".ll";
        std::string irFilename = irFilenameStream.str();

        std::string fullPath = std::string(EXAMPLE_PATH) + "testing/" + filename;
        std::string fullIrPath = std::string(EXAMPLE_PATH) + "testing/" + irFilename;

        std::cout << "==================================================\n";
        std::cout << "=== RUNNING PARSER & COMPILER ON " << filename << " ===\n";
        std::cout << "==================================================\n\n";

        std::ifstream file(fullPath);
        if (!file.is_open()) {
            std::cerr << "Impossibile aprire il file: " << fullPath << "\n\n";
            continue; // Salta al prossimo se il file non esiste
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string sourceCode = buffer.str();

        try {
            Lexer lexer(sourceCode);
            Parser parser(lexer, sourceCode);

            // 1. Parsing dell'intero file
            ast::ASTTree tree = parser.parseTranslationUnit();

            std::cout << " Parsing completato senza errori!\n";
            std::cout << " Nodi AST Generati: " << tree.nodes.size() << "\n\n";

            // 2. Dump dell'AST
            std::cout << "=== AST STRUCTURE DUMP FOR " << filename << " ===\n";
            ast::ASTManager::print(tree, sourceCode);

            // 3. Salvataggio su file binario per la cache
            if (ast::ASTManager::saveToFile(tree, binFilename)) {
                std::cout << "\n AST salvato con successo in '" << binFilename << "'\n\n";
            }

            // 4. Generazione LLVM IR
            std::cout << "=== GENERATING LLVM IR FOR " << filename << " ===\n\n";

            // Instanzia il compilatore usando il nome del file come ID modulo
            compiler::Compiler llvmCompiler(tree, sourceCode, filename);

            if (llvmCompiler.compile()) {
                std::cout << "--- LLVM IR OUTPUT ---\n";
                llvmCompiler.dumpIR();
                std::cout << "----------------------\n\n";

                // Salva il file .ll nella stessa cartella dei test
                if (llvmCompiler.emitIRToFile(fullIrPath)) {
                    std::cout << " LLVM IR salvato con successo in: " << fullIrPath << "\n\n";
                }
            }
            else {
                std::cerr << " Errore durante la generazione dell'LLVM IR!\n\n";
            }

        }
        catch (const std::exception& e) {
            std::cerr << "\n COMPILER ERROR in " << filename << ": " << e.what() << "\n\n";
        }
    }

    return 0;
}