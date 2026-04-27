# 3rdparty/CMakeLists.txt - 第三方库引入（使用真实源码）

# ============================================================
# tree-sitter 核心库 (v0.20.8)
# lib.c 是统一入口，include 目录指向 lib/src
# ============================================================
add_library(tree-sitter STATIC
    tree-sitter/lib/src/lib.c
)
target_include_directories(tree-sitter PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/tree-sitter/lib/include
)
target_include_directories(tree-sitter PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/tree-sitter/lib/src
)
target_compile_options(tree-sitter PRIVATE -fPIC -w)

# ============================================================
# tree-sitter-c 语法库 (v0.20.7)
# ============================================================
add_library(tree-sitter-c STATIC
    tree-sitter-c/src/parser.c
)
target_include_directories(tree-sitter-c PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/tree-sitter/lib/include
)
target_include_directories(tree-sitter-c PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/tree-sitter-c/src
)
target_compile_options(tree-sitter-c PRIVATE -fPIC -w)

# ============================================================
# tree-sitter-cpp 语法库 (v0.23.4)
# parser.c 是自动生成的大文件，scanner.c 实现外部扫描器
# ============================================================
add_library(tree-sitter-cpp STATIC
    tree-sitter-cpp/src/parser.c
    tree-sitter-cpp/src/scanner.c
)
target_include_directories(tree-sitter-cpp PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/tree-sitter/lib/include
)
target_include_directories(tree-sitter-cpp PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/tree-sitter-cpp/src
)
# 关闭所有警告，允许大文件编译
target_compile_options(tree-sitter-cpp PRIVATE -fPIC -w -O0)

# ============================================================
# tree-sitter-cmake 语法库 (v0.7.2)
# ============================================================
add_library(tree-sitter-cmake STATIC
    tree-sitter-cmake/src/parser.c
    tree-sitter-cmake/src/scanner.c
)
target_include_directories(tree-sitter-cmake PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/tree-sitter/lib/include
)
target_include_directories(tree-sitter-cmake PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/tree-sitter-cmake/src
)
target_compile_options(tree-sitter-cmake PRIVATE -fPIC -w)

# ============================================================
# CLI11 (header-only, v2.4.0)
# ============================================================
add_library(cli11 INTERFACE)
target_include_directories(cli11 INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/CLI11/include
)

# ============================================================
# spdlog (v1.17.0)
# ============================================================
add_subdirectory(spdlog)

# ============================================================
# nlohmann/json (header-only, v3.12.0)
# ============================================================
add_subdirectory(json)

# ============================================================
# Inja (header-only, v3.4.0，依赖 nlohmann/json)
# ============================================================
add_library(pantor-inja INTERFACE)
target_include_directories(pantor-inja INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/inja/include
)
target_link_libraries(pantor-inja INTERFACE nlohmann_json::nlohmann_json)
add_library(pantor::inja ALIAS pantor-inja)
