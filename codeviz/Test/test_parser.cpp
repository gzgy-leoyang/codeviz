// test_parser.cpp — ParserFrontend 模块单元测试
//
// ParserFrontend 使用 tree-sitter 解析 C/C++ 源码字符串，提取符号信息。
// parse_file() 不读磁盘——输入是 SourceFile（包含源码文本），
// 输出是 FileParseResult（包含 RawSymbol 列表和包含关系）。
// 测试策略: 构造 C/C++ 代码片段 → 验证解析产出的符号结构

#include "doctest.h"
#include "Parser/ParserFrontend.h"

// 辅助: 创建 SourceFile（.cpp 扩展名触发 tree-sitter-cpp）
static SourceFile make_source(const std::string& name, const std::string& content) {
    SourceFile s;
    s.file_path = "/tmp/" + name;
    s.content = content;
    return s;
}

static const CompileArgs EMPTY_ARGS;

TEST_CASE("Parser::parse_file - 空源码") {
    ParserFrontend parser;
    auto src = make_source("empty.cpp", "");
    auto result = parser.parse_file(src, EMPTY_ARGS);

    CHECK(result.symbols.empty());
    CHECK(result.includes.empty());
    CHECK(result.total_lines == 0);
}

TEST_CASE("Parser::parse_file - 函数定义") {
    ParserFrontend parser;
    auto src = make_source("test.cpp", R"(
int add(int a, int b) {
    return a + b;
}
)");
    auto result = parser.parse_file(src, EMPTY_ARGS);

    // 应提取出 add 函数
    bool found = false;
    for (const auto& sym : result.symbols) {
        if (sym.name == "add" && sym.kind == RawSymbol::FUNC) {
            found = true;
            CHECK(sym.line_start > 0);
            CHECK(sym.line_end >= sym.line_start);
            break;
        }
    }
    CHECK(found);
}

TEST_CASE("Parser::parse_file - 函数调用") {
    ParserFrontend parser;
    auto src = make_source("test.cpp", R"(
void callee() {}
void caller() {
    callee();
}
)");
    auto result = parser.parse_file(src, EMPTY_ARGS);

    // caller 的 callee_names 应包含 callee
    bool checked = false;
    for (const auto& sym : result.symbols) {
        if (sym.name == "caller") {
            CHECK(!sym.callee_names.empty());
            bool has_callee = false;
            for (const auto& c : sym.callee_names)
                if (c == "callee") has_callee = true;
            CHECK(has_callee);
            checked = true;
        }
    }
    CHECK(checked);
}

TEST_CASE("Parser::parse_file - #include 指令") {
    ParserFrontend parser;
    auto src = make_source("test.cpp", R"(
#include <stdio.h>
#include "myheader.h"

int main() {}
)");
    auto result = parser.parse_file(src, EMPTY_ARGS);

    // 应提取出两个 include 关系
    REQUIRE(result.includes.size() >= 2);
    bool has_stdio = false, has_myheader = false;
    for (const auto& inc : result.includes) {
        if (inc.second.find("stdio.h") != std::string::npos) has_stdio = true;
        if (inc.second.find("myheader.h") != std::string::npos) has_myheader = true;
    }
    CHECK(has_stdio);
    CHECK(has_myheader);
}

TEST_CASE("Parser::parse_file - 宏定义") {
    ParserFrontend parser;
    auto src = make_source("test.cpp", R"(
#define MAX_BUF 1024
#define GREETING "hello"
)");
    auto result = parser.parse_file(src, EMPTY_ARGS);

    // 应提取出宏定义符号
    bool has_max = false, has_greeting = false;
    for (const auto& sym : result.symbols) {
        if (sym.kind == RawSymbol::MACRO) {
            if (sym.name == "MAX_BUF") has_max = true;
            if (sym.name == "GREETING") has_greeting = true;
        }
    }
    CHECK(has_max);
    CHECK(has_greeting);
}

TEST_CASE("Parser::parse_file - 结构体定义") {
    ParserFrontend parser;
    auto src = make_source("test.h", R"(
struct Point {
    int x;
    int y;
};
)");
    auto result = parser.parse_file(src, EMPTY_ARGS);

    // 应提取出 Point 结构体
    bool found = false;
    for (const auto& sym : result.symbols) {
        if (sym.name == "Point" && sym.kind == RawSymbol::STRUCT) {
            found = true;
            break;
        }
    }
    CHECK(found);
}

TEST_CASE("Parser::parse_file - 类定义") {
    ParserFrontend parser;
    auto src = make_source("test.h", R"(
class MyClass {
public:
    int getValue();
private:
    int val;
};
)");
    auto result = parser.parse_file(src, EMPTY_ARGS);

    // 应提取出 MyClass 类符号
    bool has_class = false;
    for (const auto& sym : result.symbols) {
        if (sym.name == "MyClass" && sym.kind == RawSymbol::CLASS) has_class = true;
    }
    CHECK(has_class);
}
