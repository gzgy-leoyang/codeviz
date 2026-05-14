// test_graph_builder.cpp — GraphBuilder 模块单元测试
//
// GraphBuilder 接收 AnalysisContext（含完整调用边），执行 BFS 展开，
// 用"从入口函数可达"的子图替换 call_edges，同时计算扇入/扇出。
//
// 关键顺序约束: 扇入/扇出必须在 BFS 替换 call_edges 之前计算（基于全量边）。
// 测试策略: 构造调用链 → 验证 BFS 深度控制和扇入/扇出正确性

#include "doctest.h"
#include "GraphBuilder/GraphBuilder.h"

// 辅助: 创建符号表中一条记录
static Symbol make_sym(uint32_t id, const std::string& name) {
    Symbol s;
    s.id = id;
    s.name = name;
    s.kind = SymbolKind::FUNCTION;
    return s;
}

// 辅助: 创建调用边
static CallEdge make_edge(uint32_t caller, uint32_t callee) {
    CallEdge e;
    e.caller_id = caller;
    e.callee_id = callee;
    e.call_count = 1;
    return e;
}

TEST_CASE("GraphBuilder::build - 空上下文不崩溃") {
    GraphBuilder gb;
    AnalysisContext ctx;

    // 空上下文不应导致崩溃或无限循环
    CHECK_NOTHROW(gb.build(ctx, "main", 2));
}

TEST_CASE("GraphBuilder::build - 入口函数 ID 正确") {
    GraphBuilder gb;
    AnalysisContext ctx;

    // 构造: a -> b -> c
    ctx.symbols = {make_sym(1, "a"), make_sym(2, "b"), make_sym(3, "c")};
    ctx.functions.resize(3);
    for (int i = 0; i < 3; i++) ctx.functions[i].symbol_id = i + 1;
    ctx.functions[0].callees = {2};
    ctx.functions[1].callees = {3};
    ctx.call_edges = {make_edge(1, 2), make_edge(2, 3)};

    gb.build(ctx, "a", 2);

    CHECK(ctx.entry_function_id == 1);
}

TEST_CASE("GraphBuilder::build - BFS 深度控制展开范围") {
    GraphBuilder gb;
    AnalysisContext ctx;

    // 构造: 1 -> 2 -> 3 -> 4  (四层链)
    for (int i = 1; i <= 4; i++) {
        ctx.symbols.push_back(make_sym(i, "f" + std::to_string(i)));
    }
    ctx.functions.resize(4);
    for (int i = 0; i < 4; i++) ctx.functions[i].symbol_id = i + 1;
    ctx.functions[0].callees = {2};
    ctx.functions[1].callees = {3};
    ctx.functions[2].callees = {4};
    ctx.call_edges = {make_edge(1, 2), make_edge(2, 3), make_edge(3, 4)};
    ctx.full_call_edges = ctx.call_edges;

    gb.build(ctx, "f1", 1);  // 深度=1: 只展开1->2

    // 深度=1: 应该只有一条边 1->2
    REQUIRE(ctx.call_edges.size() == 1);
    CHECK(ctx.call_edges[0].caller_id == 1);
    CHECK(ctx.call_edges[0].callee_id == 2);
}

TEST_CASE("GraphBuilder::build - 扇入扇出基于全量边计算") {
    GraphBuilder gb;
    AnalysisContext ctx;

    // 构造: 1 -> 3, 2 -> 3, 1 -> 4  (3 被 1 和 2 调用；1 调用 3 和 4)
    for (int i = 1; i <= 4; i++) {
        ctx.symbols.push_back(make_sym(i, "f" + std::to_string(i)));
    }
    ctx.functions.resize(4);
    for (int i = 0; i < 4; i++) ctx.functions[i].symbol_id = i + 1;
    ctx.functions[0].callees = {3, 4};   // f1 调用了 f3, f4
    ctx.functions[1].callees = {3};       // f2 调用了 f3
    ctx.call_edges = {make_edge(1, 3), make_edge(2, 3), make_edge(1, 4)};

    gb.build(ctx, "f1", 2);

    // f3 的 fan_in 应该是 2（被 f1 和 f2 调用）
    // f1 的 fan_out 应该是 2（调用了 f3 和 f4）
    CHECK(ctx.functions[2].fan_in == 2);   // f3 (index 2)
    CHECK(ctx.functions[0].fan_out == 2);  // f1 (index 0)
}

TEST_CASE("GraphBuilder::build - full_call_edges 保留完整调用图") {
    GraphBuilder gb;
    AnalysisContext ctx;

    for (int i = 1; i <= 4; i++)
        ctx.symbols.push_back(make_sym(i, "f" + std::to_string(i)));
    ctx.functions.resize(4);
    for (int i = 0; i < 4; i++) ctx.functions[i].symbol_id = i + 1;
    ctx.functions[0].callees = {2};
    ctx.functions[1].callees = {3};
    ctx.functions[2].callees = {4};
    ctx.call_edges = {make_edge(1, 2), make_edge(2, 3), make_edge(3, 4)};

    gb.build(ctx, "f1", 1);

    // full_call_edges 应包含所有原始边，不被 BFS 裁剪
    CHECK(ctx.full_call_edges.size() == 3);
}

TEST_CASE("GraphBuilder::build - 入口函数不存在时 entry_id 为 0") {
    GraphBuilder gb;
    AnalysisContext ctx;

    ctx.symbols = {make_sym(1, "existing_func")};
    ctx.functions.resize(1);
    ctx.functions[0].symbol_id = 1;

    // "nonexistent" 不在符号表中，entry_function_id 应保持 0
    gb.build(ctx, "nonexistent", 2);

    CHECK(ctx.entry_function_id == 0);
}
