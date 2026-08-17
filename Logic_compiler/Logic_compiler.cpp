#include "Logic_compiler.h"
#include "preprocessor.h"
#include <iostream>
#include <chrono>

#define EXAMPLE_PATH R"(C:/Users/stefa/DEV/C/Logic_compiler/Logic_compiler/example/)"

int main() {
    constexpr const char* targetFile = "example_preproc_02.lgc";

    try {
        Preprocessor preprocessor(EXAMPLE_PATH, targetFile, PreprocLogLevel::Trace);

        // Benchmark preprocessor.process() only
        const auto t0 = std::chrono::high_resolution_clock::now();

        preprocessor.process();

        const auto t1 = std::chrono::high_resolution_clock::now();

        const std::chrono::duration<double, std::milli> elapsed_ms = t1 - t0;
        const std::chrono::duration<double, std::micro> elapsed_us = t1 - t0;
        const std::string_view result = preprocessor.get_result();

        std::cout << "========================================\n"
            << " Pure Preprocessing Time (Log Off):\n"
            << "   " << elapsed_ms.count() << " ms (" << elapsed_us.count() << " us)\n"
            << " Output Size: " << result.size() << " bytes\n"
            << "========================================\n";
    }
    catch (const std::exception& e) {
        std::cerr << "[ERROR] Preprocessor failed: " << e.what() << '\n';
        return 1;
    }

    return 0;
}