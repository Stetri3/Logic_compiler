#include "Logic_compiler.h"
#include "preprocessor.h"
#include "CAllocators.h"
#include "CMemory.h"
#include "CString.h"
#include "CStringView.h"
#include "CPrint.h"
#include "CTime.h"
#include "CVectors.h"
#include "CFilestream.h"

#define EXAMPLE_PATH R"(C:/Users/stefa/DEV/C/Logic_compiler/Logic_compiler/example/)"

int main() {
    constexpr const char* targetFile = "example_preproc_02.lgc";
    try {
        Preprocessor preprocessor(EXAMPLE_PATH, targetFile, PreprocLogLevel::Trace);
        
        // Benchmark preprocessor.process() only
        cc::CTimer timer;

        preprocessor.process();

        const cc::CDuration elapsed = timer.elapsed();
        const auto result = preprocessor.get_result();

        cc::CCout << "========================================\n"
                  << " Pure Preprocessing Time (Log Off):\n"
                  << "   " << elapsed.as_milliseconds() << " ms (" << elapsed.as_microseconds() << " us)\n"
                  << " Output Size: " << result.size() << " bytes\n"
                  << "========================================" << cc::endl;
    }
    catch (const std::exception& e) {
        // Output di errore via CCout (o gestione custom)
        cc::CCout << "[ERROR] Preprocessor failed: " << e.what() << cc::endl;
        return 1;
    }

    return 0;
}