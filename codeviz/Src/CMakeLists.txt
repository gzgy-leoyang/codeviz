# Src/CMakeLists.txt - 源码子目录构建配置

# 收集所有源文件
set(CODEVIZ_SOURCES
    CLI/CLI.cpp
    CMakeParser/CMakeParser.cpp
    CompDBParser/CompDBParser.cpp
    Parser/ParserFrontend.cpp
    Indexer/Indexer.cpp
    GraphBuilder/GraphBuilder.cpp
    Analyzer/Analyzer.cpp
    Reporter/Reporter.cpp
)

add_executable(codeviz ${CODEVIZ_SOURCES})

target_include_directories(codeviz PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/3rdparty/tree-sitter/lib/include
    ${CMAKE_SOURCE_DIR}/3rdparty/CLI11/include
    ${CMAKE_SOURCE_DIR}/3rdparty/spdlog/include
    ${CMAKE_SOURCE_DIR}/3rdparty/inja/include
    ${CMAKE_SOURCE_DIR}/3rdparty/json/include
    ${CMAKE_SOURCE_DIR}/3rdparty
)

# 链接第三方库
target_link_libraries(codeviz PRIVATE
    tree-sitter
    tree-sitter-c
    tree-sitter-cpp
    tree-sitter-cmake
    cli11
    spdlog::spdlog
    nlohmann_json::nlohmann_json
    pantor::inja
    pthread
    stdc++fs  # std::filesystem on GCC < 9
)

# 禁用 nlohmann/json ABI 内联版本命名空间，避免前向声明歧义
target_compile_definitions(codeviz PRIVATE
    NLOHMANN_JSON_NAMESPACE_NO_VERSION=1
)

# 编译选项
target_compile_options(codeviz PRIVATE
    -Wall -Wextra -Wno-unused-parameter
)
