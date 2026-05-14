# Test/CMakeLists.txt - 单元测试构建配置

set(DOCTEST_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/3rdparty/doctest)

add_executable(codeviz_test
    test_main.cpp
    test_cli.cpp
    test_analyzer.cpp
    test_graph_builder.cpp
    test_indexer.cpp
    test_reporter.cpp
    test_parser.cpp
    test_cmake_parser.cpp
    test_compdb_parser.cpp
    $<TARGET_OBJECTS:codeviz_objlib>
)
target_include_directories(codeviz_test PRIVATE
    ${CMAKE_SOURCE_DIR}/Src
    ${DOCTEST_INCLUDE_DIR}
)
target_link_libraries(codeviz_test PRIVATE
    tree-sitter
    tree-sitter-c
    tree-sitter-cpp
    tree-sitter-cmake
    spdlog::spdlog
    nlohmann_json::nlohmann_json
    pantor::inja
    pthread
    stdc++fs
)

add_test(NAME codeviz_test COMMAND codeviz_test)
