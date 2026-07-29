# CMakeList.txt: progetto CMake per Logic_compiler. Includere l'origine e definire
# qui la logica specifica del progetto.
#

file(GLOB_RECURSE SRC_FILES 
    "src/*.cpp"
    "src/*.h"
)


# Aggiungere l'origine all'eseguibile di questo progetto.
add_executable (Logic_compiler "Logic_compiler.cpp" "Logic_compiler.h"
${SRC_FILES})

#per l'inclusione indipendentemente dal percorso locale
file(GLOB_RECURSE ALL_HEADERS "src/*.h" "src/*.hpp")

# 2. Extract unique directory paths from the collected headers
set(INCLUDE_DIRS "")
foreach(HEADER_FILE ${ALL_HEADERS})
    get_filename_component(DIR_PATH ${HEADER_FILE} DIRECTORY)
    list(APPEND INCLUDE_DIRS ${DIR_PATH})
endforeach()
list(REMOVE_DUPLICATES INCLUDE_DIRS)

# 3. Pass every subfolder to the preprocessor
target_include_directories(Logic_compiler PRIVATE ${INCLUDE_DIRS})

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET Logic_compiler PROPERTY CXX_STANDARD 20)
endif()

# TODO: Aggiungere i test e, se necessario, installare le destinazioni.
