// test_cmake_parser.cpp — CMakeParser 模块单元测试
//
// CMakeParser 使用 tree-sitter-cmake 解析 CMakeLists.txt 字符串，
// 提取项目名、目标、链接库、编译器等信息到 BuildMetadata。
// 不读磁盘——输入是 CMakeFile 结构体（内容为字符串）。
//
// 测试策略: 构造 CMakeLists.txt 片段 → 验证 BuildMetadata 各字段

#include "doctest.h"
#include "CMakeParser/CMakeParser.h"

// 辅助: 创建 CMakeFile
static CMakeFile make_cmake(const std::string& content, const std::string& name = "CMakeLists.txt") {
    CMakeFile f;
    f.file_path = "/tmp/" + name;
    f.content = content;
    f.source_dir = "/tmp";
    return f;
}

TEST_CASE("CMakeParser::parse - 空内容") {
    CMakeParser parser;
    BuildMetadata meta;
    auto ret = parser.parse(make_cmake(""), meta);

    CHECK(ret == 0);  // 空内容不视为错误
    CHECK(meta.project_name.empty());
    CHECK(meta.targets.empty());
}

TEST_CASE("CMakeParser::parse - project 指令") {
    CMakeParser parser;
    BuildMetadata meta;
    parser.parse(make_cmake("project(MyApp VERSION 1.2.3 LANGUAGES C CXX)"), meta);

    CHECK(meta.project_name == "MyApp");
}

TEST_CASE("CMakeParser::parse - add_executable") {
    CMakeParser parser;
    BuildMetadata meta;
    parser.parse(make_cmake(R"(
project(TestProj)
add_executable(myapp main.cpp utils.cpp)
)"), meta);

    bool found = false;
    for (const auto& t : meta.targets)
        if (t == "myapp") found = true;
    CHECK(found);
}

TEST_CASE("CMakeParser::parse - add_library") {
    CMakeParser parser;
    BuildMetadata meta;
    parser.parse(make_cmake(R"(
project(TestProj)
add_library(mylib STATIC src/lib.cpp)
)"), meta);

    bool found = false;
    for (const auto& t : meta.targets)
        if (t == "mylib") found = true;
    CHECK(found);
}

TEST_CASE("CMakeParser::parse - target_link_libraries") {
    CMakeParser parser;
    BuildMetadata meta;
    parser.parse(make_cmake(R"(
project(TestProj)
add_executable(myapp main.cpp)
target_link_libraries(myapp PRIVATE pthread dl)
)"), meta);

    // myapp 应链接 pthread 和 dl
    auto it = meta.target_link_libs.find("myapp");
    CHECK(it != meta.target_link_libs.end());
    CHECK(it->second.size() >= 2);
}

TEST_CASE("CMakeParser::parse - cmake_minimum_required") {
    CMakeParser parser;
    BuildMetadata meta;
    parser.parse(make_cmake("cmake_minimum_required(VERSION 3.16)"), meta);

    CHECK(meta.cmake_version == "3.16");
}

TEST_CASE("CMakeParser::parse - 设置编译器") {
    CMakeParser parser;
    BuildMetadata meta;
    parser.parse(make_cmake(R"(
project(TestProj)
set(CMAKE_C_COMPILER /usr/bin/gcc)
set(CMAKE_CXX_COMPILER /usr/bin/g++)
)"), meta);

    CHECK(meta.c_compiler.find("gcc") != std::string::npos);
    CHECK(meta.cxx_compiler.find("g++") != std::string::npos);
}

TEST_CASE("CMakeParser::parse - add_subdirectory") {
    CMakeParser parser;
    BuildMetadata meta;
    parser.parse(make_cmake(R"(
project(TestProj)
add_subdirectory(sub)
add_subdirectory(lib/sub)
)"), meta);

    bool has_sub = false, has_lib_sub = false;
    for (const auto& d : meta.subdirectories) {
        if (d == "sub") has_sub = true;
        if (d == "lib/sub") has_lib_sub = true;
    }
    CHECK(has_sub);
    CHECK(has_lib_sub);
}

TEST_CASE("CMakeParser::parse - 错误语法容错") {
    CMakeParser parser;
    BuildMetadata meta;
    // 语法错误不应导致崩溃
    auto ret = parser.parse(make_cmake("project(Unclosed"), meta);
    CHECK(ret == 0);  // 容错处理
}
