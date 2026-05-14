# Src/CMakeLists.txt - 源码子目录构建配置

# 设置可执行文件输出目录为 build/output/
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output)

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

# 第三方库列表（各目标共用）
set(CODEVIZ_LIBS
    tree-sitter
    tree-sitter-c
    tree-sitter-cpp
    tree-sitter-cmake
    cli11
    spdlog::spdlog
    nlohmann_json::nlohmann_json
    pantor::inja
    pthread
    stdc++fs
)

# 源码编译为 OBJECT 库，供主程序和测试目标共用
add_library(codeviz_objlib OBJECT ${CODEVIZ_SOURCES})
target_include_directories(codeviz_objlib PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/3rdparty/tree-sitter/lib/include
    ${CMAKE_SOURCE_DIR}/3rdparty/CLI11/include
    ${CMAKE_SOURCE_DIR}/3rdparty/spdlog/include
    ${CMAKE_SOURCE_DIR}/3rdparty/inja/include
    ${CMAKE_SOURCE_DIR}/3rdparty/json/include
    ${CMAKE_SOURCE_DIR}/3rdparty
)
target_link_libraries(codeviz_objlib PRIVATE
    ${CODEVIZ_LIBS}
)
target_compile_definitions(codeviz_objlib PRIVATE
    NLOHMANN_JSON_NAMESPACE_NO_VERSION=1
)
target_compile_options(codeviz_objlib PRIVATE
    -Wall -Wextra -Wno-unused-parameter
)

# 主程序（含 main.cpp）
add_executable(codeviz main.cpp $<TARGET_OBJECTS:codeviz_objlib>)
target_include_directories(codeviz PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/3rdparty/spdlog/include
    ${CMAKE_SOURCE_DIR}/3rdparty/json/include
)
target_compile_definitions(codeviz PRIVATE
    NLOHMANN_JSON_NAMESPACE_NO_VERSION=1
)
target_link_libraries(codeviz PRIVATE ${CODEVIZ_LIBS})
