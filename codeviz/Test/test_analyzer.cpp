// test_analyzer.cpp — Analyzer 模块单元测试
//
// Analyzer 是"纯计算"模块: 输入 AnalysisContext，输出 AnalysisStats。
// 不修改输入数据，不涉及文件系统或外部依赖，最适合用来理解"输入→输出"测试模式。
//
// 核心公式: 圈复杂度 = branch_count + 1
// 测试策略: 构造已知数据 → 调用 analyze → 验证每个统计字段

#include "doctest.h"
#include "Analyzer/Analyzer.h"

TEST_CASE("Analyzer::analyze - 空上下文返回空统计") {
    Analyzer analyzer;
    AnalysisContext ctx;
    BuildMetadata meta;

    auto stats = analyzer.analyze(ctx, meta);

    CHECK(stats.file_stats.empty());
    CHECK(stats.function_stats.empty());
    CHECK(stats.circular_includes.empty());
}

TEST_CASE("Analyzer::analyze - 圈复杂度 = branch_count + 1") {
    Analyzer analyzer;
    AnalysisContext ctx;
    BuildMetadata meta;

    FunctionSymbol func;
    func.symbol_id = 1;
    func.branch_count = 5;  // if/for/while 等分支节点数
    ctx.functions.push_back(func);

    auto stats = analyzer.analyze(ctx, meta);

    REQUIRE(stats.function_stats.size() == 1);
    // 圈复杂度 = 分支节点数 + 1（函数本身为一条路径）
    CHECK(stats.function_stats[0].cyclomatic_complexity == 6);
}

TEST_CASE("Analyzer::analyze - 扇入扇出统计") {
    Analyzer analyzer;
    AnalysisContext ctx;
    BuildMetadata meta;

    // Analyzer 读取 GraphBuilder 已计算好的 fan_in/fan_out，
    // 不自己从 call_edges 重新计算
    FunctionSymbol f1, f2, f3;
    f1.symbol_id = 1;  f1.fan_in = 0;  f1.fan_out = 2;  // f1 调用了两个
    f2.symbol_id = 2;  f2.fan_in = 1;  f2.fan_out = 1;  // 被 f1 调用，调用了 f3
    f3.symbol_id = 3;  f3.fan_in = 2;  f3.fan_out = 0;  // 被两个函数调用，不调用别人
    ctx.functions = {f1, f2, f3};

    auto stats = analyzer.analyze(ctx, meta);

    REQUIRE(stats.function_stats.size() == 3);

    // 不依赖排序，按 fan_in/fan_out 值匹配查找
    auto find_stat = [&](int fan_in, int fan_out) -> bool {
        for (const auto& s : stats.function_stats)
            if (s.fan_in == fan_in && s.fan_out == fan_out) return true;
        return false;
    };
    CHECK(find_stat(0, 2));  // f1
    CHECK(find_stat(1, 1));  // f2
    CHECK(find_stat(2, 0));  // f3
}

TEST_CASE("Analyzer::analyze - 文件统计累加") {
    Analyzer analyzer;
    AnalysisContext ctx;
    BuildMetadata meta;

    // 构造两个文件，各有不同的行数
    FileSymbol fs1, fs2;
    fs1.symbol_id = 1;  fs1.total_lines = 100;  fs1.code_lines = 80;  fs1.comment_lines = 10;
    fs2.symbol_id = 2;  fs2.total_lines = 50;   fs2.code_lines = 40;  fs2.comment_lines = 5;
    ctx.files = {fs1, fs2};

    auto stats = analyzer.analyze(ctx, meta);

    REQUIRE(stats.file_stats.size() == 2);
    CHECK(stats.file_stats[0].total_lines == 100);
    CHECK(stats.file_stats[1].total_lines == 50);
}

TEST_CASE("Analyzer::analyze - 循环包含检测") {
    Analyzer analyzer;
    AnalysisContext ctx;
    BuildMetadata meta;

    // 构造两个文件形成循环包含: A -> B -> A
    // 先创建文件符号（ID 对应文件名映射通过 include_edges 中的 ID 关联）
    FileSymbol fa, fb;
    fa.symbol_id = 1;
    fb.symbol_id = 2;
    ctx.files = {fa, fb};

    IncludeEdge ie1{1, 2, 1, false};  // file1 -> file2
    IncludeEdge ie2{2, 1, 1, false};  // file2 -> file1
    ctx.include_edges = {ie1, ie2};

    auto stats = analyzer.analyze(ctx, meta);

    // Tarjan SCC 应检测出 {1, 2} 是一个强连通分量（循环包含）
    CHECK(stats.circular_includes.size() >= 1);
}
