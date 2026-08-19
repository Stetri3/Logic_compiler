# CMakeList.txt: progetto CMake per Logic_compiler.

# -----------------------------------------------------------------------------
# Configurazione MSVC per combaciare con LLVM /MT (Static Release)
# (Deve stare PRIMA di add_executable per modificare i default di CMake)
# -----------------------------------------------------------------------------
if(MSVC)
    # Sostituisci /MD e /MDd con /MT nei flag globali
    string(REPLACE "/MDd" "/MT" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
    string(REPLACE "/MD" "/MT" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
    string(REPLACE "/MDd" "/MT" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    string(REPLACE "/MD" "/MT" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")

    # Sostituisci /GR con /GR- nei flag globali (rimuove il warning D9025)
    string(REPLACE "/GR" "/GR-" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
endif()

# 1. Trova ricorsivamente sorgenti ed header
file(GLOB_RECURSE SRC_FILES 
    "src/*.cpp"
    "src/*.h"
)

# 2. Aggiungi l'eseguibile del progetto
add_executable (Logic_compiler "Logic_compiler.cpp" "Logic_compiler.h"
    ${SRC_FILES}
)

if(MSVC)
    # Forza la disabilitazione dell'Iterator Debugging
    target_compile_definitions(Logic_compiler PRIVATE _ITERATOR_DEBUG_LEVEL=0)
endif()

# 3. Estrai e includi le cartelle dei tuoi header locali
file(GLOB_RECURSE ALL_HEADERS "src/*.h" "src/*.hpp")

set(INCLUDE_DIRS "")
foreach(HEADER_FILE ${ALL_HEADERS})
    get_filename_component(DIR_PATH ${HEADER_FILE} DIRECTORY)
    list(APPEND INCLUDE_DIRS ${DIR_PATH})
endforeach()
list(REMOVE_DUPLICATES INCLUDE_DIRS)

target_include_directories(Logic_compiler PRIVATE ${INCLUDE_DIRS})

target_include_directories(Logic_compiler PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/meta)
target_precompile_headers(Logic_compiler PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/meta/predef.h")

set(LOCAL_LLVM_DIR "${CMAKE_CURRENT_SOURCE_DIR}/extra/llvm")

# Includi gli header di LLVM
target_include_directories(Logic_compiler SYSTEM PRIVATE "${LOCAL_LLVM_DIR}/include")

# Colleziona e collega le librerie statiche LLVM
file(GLOB LOCAL_LLVM_LIBS "${LOCAL_LLVM_DIR}/lib/LLVM*.lib")

target_link_libraries(Logic_compiler PRIVATE
    ${LOCAL_LLVM_LIBS}
    version.lib
    dbghelp.lib
    shell32.lib
    ole32.lib
    uuid.lib
    ntdll.lib
)

set_property(TARGET Logic_compiler PROPERTY CXX_STANDARD 23)