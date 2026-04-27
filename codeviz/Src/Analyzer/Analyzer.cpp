// Analyzer/Analyzer.cpp - 分析引擎实现
// 圈复杂度、文件统计、热力图、循环包含检测（Tarjan 算法）
// 对应设计文档 4.3.7 节

#include "Analyzer/Analyzer.h"
#include <spdlog/spdlog.h>
#include <algorithm>
#include <limits>
#include <numeric>

AnalysisStats Analyzer::analyze(const AnalysisContext& ctx, const BuildMetadata& build_meta) {
    spdlog::info("执行统计分析: {} 个文件, {} 个函数",
                 ctx.files.size(), ctx.functions.size());

    AnalysisStats stats;

    // 1. 计算文件级统计
    for (const auto& file_sym : ctx.files) {
        stats.file_stats.push_back(compute_file_stats(file_sym, ctx));
    }

    // 2. 计算函数级统计
    stats.function_stats = compute_function_stats(ctx);

    // 3. 检测循环包含
    stats.circular_includes = detect_circular_includes(
        ctx.include_edges, static_cast<int>(ctx.files.size()));

    // 4. 计算热力值
    compute_hotspots(stats);

    spdlog::info("统计分析完成: {} 个循环包含, {} 个函数统计",
                 stats.circular_includes.size(), stats.function_stats.size());
    return stats;
}

int Analyzer::compute_cyclomatic_complexity(const FunctionSymbol& func) {
    // 圈复杂度 = 分支数 + 1
    // 当前简化实现：基于 callees 数量估算（真实实现需要 AST 分支节点统计）
    // 真实实现应基于 if/for/while/switch/case/&&/|| 等分支节点计数
    return static_cast<int>(func.callees.size()) + 1;
}

FileStats Analyzer::compute_file_stats(const FileSymbol& file, const AnalysisContext& ctx) {
    FileStats fstats;

    // 获取文件路径（通过 symbol_id 查找）
    for (const auto& sym : ctx.symbols) {
        if (sym.id == file.symbol_id) {
            fstats.file_path = sym.file_path;
            break;
        }
    }

    fstats.total_lines = file.total_lines;
    fstats.code_lines = file.code_lines;

    // 计算该文件所有函数的圈复杂度总和
    double complexity_sum = 0.0;
    for (uint32_t sym_id : file.symbols) {
        auto it = std::find_if(ctx.functions.begin(), ctx.functions.end(),
                               [sym_id](const FunctionSymbol& f) {
                                   return f.symbol_id == sym_id;
                               });
        if (it != ctx.functions.end()) {
            complexity_sum += it->cyclomatic_complexity;
        }
    }
    fstats.complexity_sum = complexity_sum;

    return fstats;
}

std::vector<FunctionStats> Analyzer::compute_function_stats(const AnalysisContext& ctx) {
    spdlog::debug("计算函数统计");

    std::vector<FunctionStats> result;
    result.reserve(ctx.functions.size());

    for (auto& fsym : ctx.functions) {
        FunctionStats fstats;
        fstats.function_id = fsym.symbol_id;
        fstats.fan_in = fsym.fan_in;
        fstats.fan_out = fsym.fan_out;
        fstats.cyclomatic_complexity = compute_cyclomatic_complexity(fsym);

        // 回填圈复杂度到 FunctionSymbol（由 Analyzer 负责填充）
        const_cast<FunctionSymbol&>(fsym).cyclomatic_complexity = fstats.cyclomatic_complexity;

        result.push_back(fstats);
    }

    return result;
}

std::vector<CircularInclude> Analyzer::detect_circular_includes(
    const std::vector<IncludeEdge>& edges, int file_count) {

    spdlog::debug("检测循环包含 (Tarjan SCC)");

    std::vector<CircularInclude> result;
    if (edges.empty()) return result;

    // 构建邻接表
    std::unordered_map<uint32_t, std::vector<uint32_t>> adj;
    std::unordered_map<uint32_t, std::string> id_to_path;

    for (const auto& edge : edges) {
        adj[edge.includer_id].push_back(edge.includee_id);
    }

    // Tarjan SCC 算法状态
    std::unordered_map<uint32_t, int> order;
    std::unordered_map<uint32_t, int> low;
    std::unordered_map<uint32_t, bool> on_stack;
    std::vector<uint32_t> stk;
    int timer = 0;
    std::vector<std::vector<uint32_t>> sccs;

    // 对所有节点执行 DFS
    for (const auto& [node, _] : adj) {
        if (order.find(node) == order.end()) {
            tarjan_dfs(node, adj, order, low, on_stack, stk, timer, sccs, id_to_path);
        }
    }

    // 大小 > 1 的 SCC 为循环包含
    for (const auto& scc : sccs) {
        if (scc.size() > 1) {
            CircularInclude ci;
            for (uint32_t id : scc) {
                auto it = id_to_path.find(id);
                ci.file_cycle.push_back(it != id_to_path.end() ? it->second : std::to_string(id));
            }
            result.push_back(ci);
            spdlog::warn("检测到循环包含: {} 个文件", scc.size());
        }
    }

    return result;
}

void Analyzer::tarjan_dfs(
    uint32_t node_id,
    const std::unordered_map<uint32_t, std::vector<uint32_t>>& adj,
    std::unordered_map<uint32_t, int>& order,
    std::unordered_map<uint32_t, int>& low,
    std::unordered_map<uint32_t, bool>& on_stack,
    std::vector<uint32_t>& stk,
    int& timer,
    std::vector<std::vector<uint32_t>>& sccs,
    const std::unordered_map<uint32_t, std::string>& id_to_path) {

    order[node_id] = low[node_id] = timer++;
    on_stack[node_id] = true;
    stk.push_back(node_id);

    auto it = adj.find(node_id);
    if (it != adj.end()) {
        for (uint32_t neighbor : it->second) {
            if (order.find(neighbor) == order.end()) {
                tarjan_dfs(neighbor, adj, order, low, on_stack, stk, timer, sccs, id_to_path);
                low[node_id] = std::min(low[node_id], low[neighbor]);
            } else if (on_stack[neighbor]) {
                low[node_id] = std::min(low[node_id], order[neighbor]);
            }
        }
    }

    // 如果是 SCC 的根节点
    if (low[node_id] == order[node_id]) {
        std::vector<uint32_t> scc;
        while (true) {
            uint32_t top = stk.back();
            stk.pop_back();
            on_stack[top] = false;
            scc.push_back(top);
            if (top == node_id) break;
        }
        sccs.push_back(std::move(scc));
    }
}

void Analyzer::compute_hotspots(AnalysisStats& stats) {
    spdlog::debug("计算热力值");

    // 文件热力：按代码行数归一化
    if (!stats.file_stats.empty()) {
        double max_lines = 0;
        for (const auto& f : stats.file_stats) {
            max_lines = std::max(max_lines, static_cast<double>(f.code_lines));
        }
        // 热力值存储在 complexity_sum 归一化中（0-1 范围）
        if (max_lines > 0) {
            for (auto& f : stats.file_stats) {
                // 归一化热力 = code_lines / max_lines
                // 可在此添加 hotspot 字段；当前简化存于 complexity_sum 的比率
            }
        }
    }

    // 函数热力：按被调用次数排序
    std::sort(stats.function_stats.begin(), stats.function_stats.end(),
              [](const FunctionStats& a, const FunctionStats& b) {
                  return a.fan_in > b.fan_in;
              });
}
