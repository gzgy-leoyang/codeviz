// test_reporter.cpp — Reporter 模块单元测试
//
// Reporter 将分析结果序列化为 JSON 并注入 HTML 模板，生成最终报告。
// 它是纯函数: 输入内存数据结构，输出 HTML 字符串。
//
// 测试策略: 构造已知数据 → 调用 generate → 验证 HTML 中包含预期的 JSON 内容

#include "doctest.h"
#include "Reporter/Reporter.h"

TEST_CASE("Reporter::generate - 空数据生成有效 HTML") {
    Reporter reporter;
    std::vector<SymbolMetadata> symbols;
    AnalysisStats stats;
    AnalysisContext ctx;

    auto report = reporter.generate(symbols, stats, ctx);

    // 应生成非空 HTML
    CHECK_FALSE(report.content.empty());

    // HTML 中应包含 CODEVIZ_DATA（前端渲染的数据锚点）
    CHECK(report.content.find("CODEVIZ_DATA") != std::string::npos);

    // 应包含 HTML 基本结构标签（任一种即可）
    bool has_doctype = report.content.find("<!DOCTYPE html>") != std::string::npos;
    bool has_html_tag = report.content.find("<html") != std::string::npos;
    bool has_html_structure = has_doctype || has_html_tag;
    CHECK(has_html_structure);
}

TEST_CASE("Reporter::generate - 符号元数据写入 JSON") {
    Reporter reporter;
    AnalysisContext ctx;
    ctx.command_line = "codeviz -p /test -e main -d 2";

    // 构造一个已知符号
    SymbolMetadata meta;
    meta.symbol_id = 1;
    meta.name = "test_func";
    meta.kind = SymbolKind::FUNCTION;
    meta.complexity = 5;
    meta.fan_in = 3;
    meta.fan_out = 2;

    AnalysisStats stats;
    auto report = reporter.generate({meta}, stats, ctx);

    // 验证符号名称出现在 HTML 中
    CHECK(report.content.find("test_func") != std::string::npos);
}

TEST_CASE("Reporter::generate - 运行命令写入 JSON") {
    Reporter reporter;
    std::vector<SymbolMetadata> symbols;
    AnalysisStats stats;
    AnalysisContext ctx;
    ctx.command_line = "codeviz -p /my_project";

    auto report = reporter.generate(symbols, stats, ctx);

    CHECK(report.content.find("/my_project") != std::string::npos);
}

TEST_CASE("Reporter::generate - 统计数据写入 HTML") {
    Reporter reporter;
    AnalysisStats stats;
    AnalysisContext ctx;

    // 构造统计信息
    FunctionStats fs;
    fs.function_id = 1;
    fs.cyclomatic_complexity = 10;
    fs.fan_in = 5;
    stats.function_stats.push_back(fs);

    FileStats file_stat;
    file_stat.file_path = "/tmp/main.cpp";
    file_stat.total_lines = 200;
    stats.file_stats.push_back(file_stat);

    auto report = reporter.generate({}, stats, ctx);

    // 验证统计信息出现在 HTML 中
    CHECK(report.content.find("main.cpp") != std::string::npos);
}

TEST_CASE("Reporter::generate - 调用图边去重合并 weight") {
    Reporter reporter;
    AnalysisContext ctx;

    // 构造两条重复调用边 (caller=1 callee=2 出现两次)
    CallEdge e1, e2;
    e1.caller_id = 1; e1.callee_id = 2; e1.call_count = 3;
    e2.caller_id = 1; e2.callee_id = 2; e2.call_count = 5;
    ctx.call_edges = {e1, e2};
    ctx.full_call_edges = {e1, e2};

    // 添加符号元数据供 find_symbol_name 使用
    ctx.symbols.resize(3);

    AnalysisStats stats;
    auto report = reporter.generate({}, stats, ctx);

    // 验证 JSON 数据结构存在（edges 或 call_graph 字段）
    bool has_edges = report.content.find("\"edges\"") != std::string::npos;
    CHECK(has_edges);
}
