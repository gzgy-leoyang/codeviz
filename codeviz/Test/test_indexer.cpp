// test_indexer.cpp — Indexer 模块单元测试
//
// Indexer 将 ParserFrontend 的输出（FileParseResult）转换为统一的符号表
// （AnalysisContext），执行两遍: 第一遍分配 ID 建表，第二遍解析引用。
//
// 注意: Indexer 会为每个文件创建一个 FILE_ENTITY 类型的 Symbol，
// 所以符号总数 = 文件数 + 函数数 + 其他符号数。
//
// 测试策略: 构造 FileParseResult（模拟解析器的输出）→ 调用 build_index
// → 验证符号表、调用边、包含边的正确性

#include "doctest.h"
#include "Indexer/Indexer.h"

// 辅助: 创建 FileParseResult
static FileParseResult make_result(const std::string& file, const std::vector<RawSymbol>& syms) {
    FileParseResult r;
    r.file_path = file;
    r.symbols = syms;
    r.total_lines = 100;
    r.code_lines = 80;
    r.comment_lines = 10;
    return r;
}

// 辅助: 创建 RawSymbol
static RawSymbol make_raw(const std::string& name, RawSymbol::Kind kind) {
    RawSymbol r;
    r.name = name;
    r.kind = kind;
    return r;
}

TEST_CASE("Indexer::build_index - 空解析结果") {
    Indexer indexer;
    std::vector<FileParseResult> results;

    auto ctx = indexer.build_index(results);

    CHECK(ctx.symbols.empty());
    CHECK(ctx.functions.empty());
    CHECK(ctx.files.empty());
    CHECK(ctx.call_edges.empty());
}

TEST_CASE("Indexer::build_index - 单个函数创建对应符号") {
    Indexer indexer;

    RawSymbol raw = make_raw("foo", RawSymbol::FUNC);
    raw.return_type = "int";
    raw.parameters = {"int", "char*"};
    raw.file_path = "/tmp/test.cpp";
    raw.line_start = 10;
    raw.line_end = 30;

    auto results = {make_result("/tmp/test.cpp", {raw})};
    auto ctx = indexer.build_index(results);

    // 符号表有 2 条: 1 个文件实体 + 1 个函数
    REQUIRE(ctx.symbols.size() == 2);
    // 验证有一个叫 "foo" 的函数符号
    auto it = ctx.symbol_name_to_id.find("foo");
    REQUIRE(it != ctx.symbol_name_to_id.end());
    uint32_t foo_id = it->second;
    CHECK(ctx.symbols[foo_id - 1].kind == SymbolKind::FUNCTION);
    CHECK(ctx.symbols[foo_id - 1].line_start == 10);
    CHECK(ctx.symbols[foo_id - 1].line_end == 30);

    // FunctionSymbol 应有 1 条
    REQUIRE(ctx.functions.size() == 1);
    CHECK(ctx.functions[0].symbol_id == foo_id);
    CHECK(ctx.functions[0].return_type == "int");
    REQUIRE(ctx.functions[0].parameters.size() == 2);
    CHECK(ctx.functions[0].parameters[0] == "int");
}

TEST_CASE("Indexer::build_index - 调用边生成") {
    Indexer indexer;

    // foo 调用了 bar——通过 callee_names 表达
    RawSymbol foo = make_raw("foo", RawSymbol::FUNC);
    foo.callee_names = {"bar"};
    foo.file_path = "/tmp/a.cpp";

    RawSymbol bar = make_raw("bar", RawSymbol::FUNC);
    bar.file_path = "/tmp/a.cpp";

    auto results = {make_result("/tmp/a.cpp", {foo, bar})};
    auto ctx = indexer.build_index(results);

    // 符号: 1 文件实体 + 2 函数 = 3
    REQUIRE(ctx.symbols.size() == 3);
    // 调用边: foo -> bar
    REQUIRE(ctx.call_edges.size() == 1);
    CHECK(ctx.call_edges[0].caller_id == ctx.symbol_name_to_id["foo"]);
    CHECK(ctx.call_edges[0].callee_id == ctx.symbol_name_to_id["bar"]);
}

TEST_CASE("Indexer::build_index - 包含边生成") {
    Indexer indexer;

    auto r1 = make_result("/tmp/a.cpp", {});
    r1.includes = {{"/tmp/a.cpp", "/tmp/b.h"}};

    auto r2 = make_result("/tmp/b.h", {});

    std::vector<FileParseResult> results = {r1, r2};
    auto ctx = indexer.build_index(results);

    // 应产生一条包含边
    REQUIRE(ctx.include_edges.size() == 1);
    CHECK(ctx.include_edges[0].is_system == false);
}

TEST_CASE("Indexer::build_index - 同一文件内同名函数 Symbol ID 相同") {
    Indexer indexer;

    // 同一文件中出现两次同名声明，Symbol ID 应相同（合并），
    // 但 FunctionSymbol 各自保留（可能包含声明+定义的不同信息）
    RawSymbol r1 = make_raw("common_func", RawSymbol::FUNC);
    r1.file_path = "/tmp/test.h";

    RawSymbol r2 = make_raw("common_func", RawSymbol::FUNC);
    r2.file_path = "/tmp/test.h";

    auto results = {make_result("/tmp/test.h", {r1, r2})};
    auto ctx = indexer.build_index(results);

    // 同一名称应映射到同一个 ID
    uint32_t id = ctx.symbol_name_to_id["common_func"];
    CHECK(id > 0);

    // 所有同名符号共享同一个 ID
    for (const auto& sym : ctx.symbols) {
        if (sym.name == "common_func" && sym.kind == SymbolKind::FUNCTION)
            CHECK(sym.id == id);
    }
}
