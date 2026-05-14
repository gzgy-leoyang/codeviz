// test_compdb_parser.cpp — CompDBParser 模块单元测试
//
// CompDBParser 读取 compile_commands.json 文件，为每个源文件提取编译参数。
// parse() 需要真实的文件系统——测试中创建临时目录和 JSON 文件。
// 私有辅助方法（extract_defines、extract_includes、normalize_path）为纯函数。
//
// 测试策略: 创建临时 compile_commands.json → 验证解析结果

#include "doctest.h"
#include "CompDBParser/CompDBParser.h"
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

// 辅助: 在指定目录下创建 compile_commands.json
static void create_compile_commands(const std::string& dir, const std::string& content) {
    std::ofstream ofs(dir + "/compile_commands.json");
    REQUIRE(ofs.is_open());
    ofs << content;
    ofs.close();
}

TEST_CASE("CompDBParser::parse - 文件不存在返回空映射") {
    CompDBParser parser;

    auto result = parser.parse("/tmp/_codeviz_nonexistent_dir_");

    CHECK(result.empty());
}

TEST_CASE("CompDBParser::parse - 单条编译条目") {
    auto tmp_dir_str = []() {
        char t[] = "/tmp/codeviz_compdb_XXXXXX";
        auto* d = mkdtemp(t);
        REQUIRE(d != nullptr);
        return std::string(d);
    }();
    auto tmp_dir = tmp_dir_str;

    create_compile_commands(tmp_dir, R"([
    {
        "directory": "/tmp/build",
        "command": "g++ -DNDEBUG -O2 -I/usr/include -c /tmp/src/main.cpp",
        "file": "/tmp/src/main.cpp"
    }
])");

    CompDBParser parser;
    auto result = parser.parse(tmp_dir);

    REQUIRE(result.size() == 1);
    auto it = result.find("/tmp/src/main.cpp");
    CHECK(it != result.end());

    // 验证 -D 宏定义
    bool has_ndebug = false;
    for (const auto& d : it->second.defines) {
        if (d == "NDEBUG") has_ndebug = true;
    }
    CHECK(has_ndebug);

    // 验证 -I 头文件路径
    bool has_include = false;
    for (const auto& inc : it->second.includes) {
        if (inc.find("/usr/include") != std::string::npos) has_include = true;
    }
    CHECK(has_include);

    fs::remove_all(tmp_dir);
}

TEST_CASE("CompDBParser::parse - 多条编译条目") {
    auto tmp_dir = []() {
        char t[] = "/tmp/codeviz_compdb_XXXXXX";
        auto* d = mkdtemp(t);
        REQUIRE(d != nullptr);
        return std::string(d);
    }();

    create_compile_commands(tmp_dir, R"([
    {
        "directory": "/tmp/build",
        "command": "g++ -c /tmp/src/a.cpp",
        "file": "/tmp/src/a.cpp"
    },
    {
        "directory": "/tmp/build",
        "command": "g++ -c /tmp/src/b.cpp",
        "file": "/tmp/src/b.cpp"
    }
])");

    CompDBParser parser;
    auto result = parser.parse(tmp_dir);

    CHECK(result.size() == 2);
    CHECK(result.find("/tmp/src/a.cpp") != result.end());
    CHECK(result.find("/tmp/src/b.cpp") != result.end());

    fs::remove_all(tmp_dir);
}

TEST_CASE("CompDBParser::parse - 多参数提取") {
    auto tmp_dir = []() {
        char t[] = "/tmp/codeviz_compdb_XXXXXX";
        auto* d = mkdtemp(t);
        REQUIRE(d != nullptr);
        return std::string(d);
    }();

    create_compile_commands(tmp_dir, R"([
    {
        "directory": "/tmp/build",
        "command": "g++ -DDEBUG -DVER=3 -Wall -I/opt/include -I/usr/local/include -c /tmp/src/test.cpp",
        "file": "/tmp/src/test.cpp"
    }
])");

    CompDBParser parser;
    auto result = parser.parse(tmp_dir);

    REQUIRE(result.size() == 1);
    auto& args = result.begin()->second;

    // 应提取出 2 个宏定义
    CHECK(args.defines.size() == 2);
    // 应提取出 2 个头文件路径
    CHECK(args.includes.size() == 2);

    fs::remove_all(tmp_dir);
}
