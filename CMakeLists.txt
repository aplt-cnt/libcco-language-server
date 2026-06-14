cmake_minimum_required(VERSION 3.15)
project(cco-lsp VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Link against libcco (built separately or as subdirectory)
# Option 1: add_subdirectory if libcco is checked out as a sibling
if(EXISTS "${CMAKE_SOURCE_DIR}/../libcco/CMakeLists.txt")
    add_subdirectory(${CMAKE_SOURCE_DIR}/../libcco libcco)
    set(CCO_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/../libcco/include")
else()
    # Option 2: find pre-built library
    find_library(CCO_LIBRARY cco REQUIRED
        PATHS "${CMAKE_SOURCE_DIR}/../libcco/target"
        NO_DEFAULT_PATH
    )
    find_path(CCO_INCLUDE_DIR cnt/cco.h
        PATHS "${CMAKE_SOURCE_DIR}/../libcco/include"
        NO_DEFAULT_PATH
    )
    if(NOT CCO_LIBRARY OR NOT CCO_INCLUDE_DIR)
        message(FATAL_ERROR "libcco not found. Build libcco first or clone it as sibling directory.")
    endif()
    add_library(cco STATIC IMPORTED)
    set_target_properties(cco PROPERTIES IMPORTED_LOCATION "${CCO_LIBRARY}")
endif()

# Language server executable
add_executable(cco-lsp
    server/main.c
    server/lsp.c
    server/json.c
    server/cco_analyze.c
    server/text_document.c
)

target_link_libraries(cco-lsp PRIVATE cco)
target_include_directories(cco-lsp PRIVATE server)
target_include_directories(cco-lsp PRIVATE ${CCO_INCLUDE_DIR})

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(cco-lsp PRIVATE -Wall -Wextra -pedantic)
elseif(MSVC)
    target_compile_options(cco-lsp PRIVATE /utf-8)
endif()
