/**
 * @file Analyzer.cpp
 * @brief 分析引擎实现
 *
 * 执行源码的深度统计分析，包括圈复杂度计算、文件统计、
 * Tarjan SCC 循环包含检测和函数热力图排序。
 * 对应设计文档 4.3.7 节。
 */

#include "Analyzer/Analyzer.h"
#include <spdlog/spdlog.h>
#include <algorithm>
#include <limits>
#include <numeric>

/**
 * @brief 执行完整的统计分析
 *
 * 编排 4 个主要步骤：
 * 1. 文件级统计：总行数、代码行数、复杂度总和
 * 2. 函数级统计：扇入、扇出、圈复杂度
 * 3. 循环包含检测：基于 Tarjan SCC 算法
 * 4. 热力图计算：按扇入排序
 *
 * @param ctx 分析上下文（含文件池、函数池、包含边）
 * @param build_meta 构建元数据（当前未使用，保留作扩展）
 * @return 完整统计结果
 */
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

/**
 * @brief 计算函数的圈复杂度
 *
 * 公式 M = branch_count + 1（McCabe 简化形式）。
 * branch_count 由 ParserFrontend 在遍历 CST 时统计，
 * 包括 if、for、while、do、switch、case 和三元 ?: 运算符。
 *
 * @param func 函数符号（需包含已统计的 branch_count）
 * @return 圈复杂度值
 */
int Analyzer::compute_cyclomatic_complexity(const FunctionSymbol& func) {
    return func.branch_count + 1;
}

/**
 * @brief 计算单个文件的统计
 *
 * 从 FileSymbol 获取行数信息，遍历文件内定义的符号
 * 累加所有函数的圈复杂度总和。
 *
 * @param file 文件符号
 * @param ctx 分析上下文
 * @return 文件统计结构体
 */
FileStats Analyzer::compute_file_stats(const FileSymbol& file, const AnalysisContext& ctx) {
    FileStats fstats;

    // 通过 symbol_id 查找文件路径
    for (const auto& sym : ctx.symbols) {
        if (sym.id == file.symbol_id) {
            fstats.file_path = sym.file_path;
            break;
        }
    }

    fstats.total_lines = file.total_lines;
    fstats.code_lines = file.code_lines;

    // 累加文件内所有函数的圈复杂度
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

/**
 * @brief 汇总所有函数的统计
 *
 * 遍历 ctx.functions，对每个函数计算圈复杂度并回填到 FunctionSymbol。
 * 注意：此处使用 const_cast 修改 FunctionSymbol 的圈复杂度字段，
 * 因为在分析流程中，Analyzer 负有填充统计数据的责任，
 * 而 Context 中存储的 functions 在逻辑上在此阶段可变。
 *
 * @param ctx 分析上下文
 * @return 函数统计列表
 */
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

/**
 * @brief 检测循环包含（Tarjan SCC 算法）
 *
 * 将文件间的包含关系建模为有向图，每个文件是一个节点，
 * #include 关系是从包含者到被包含者的有向边。
 * 使用 Tarjan 算法找出所有强连通分量，大小 > 1 的分量即为循环包含。
 *
 * @param edges 所有包含边
 * @param file_count 文件总数（当前未使用，保留作接口扩展）
 * @return 循环包含列表，每个包含一个形成循环的文件路径链
 */
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

    // 遍历所有节点进行 DFS（处理多连通分量情况）
    for (const auto& [node, _] : adj) {
        if (order.find(node) == order.end()) {
            tarjan_dfs(node, adj, order, low, on_stack, stk, timer, sccs, id_to_path);
        }
    }

    // 大小 > 1 的 SCC 即为循环包含
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

/**
 * @brief Tarjan 算法的 DFS 递归实现
 *
 * 深度优先搜索节点，分配访问顺序号（order），
 * 计算每个节点的 low 值（通过子节点回溯或返祖边更新）。
 * 当节点的 order == low 时，该节点是强连通分量的根，
 * 从栈中弹出该分量所有节点。
 *
 * @param node_id 当前访问的节点
 * @param adj 邻接表
 * @param order 节点发现时间映射
 * @param low 节点 low 值映射
 * @param on_stack 节点是否在栈中
 * @param stk DFS 节点栈
 * @param timer 发现时间计数器（引用传递）
 * @param sccs 已发现的强连通分量（输出参数）
 * @param id_to_path ID 到路径映射（用于日志）
 */
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
                // 树边：递归访问，然后尝试更新 low
                tarjan_dfs(neighbor, adj, order, low, on_stack, stk, timer, sccs, id_to_path);
                low[node_id] = std::min(low[node_id], low[neighbor]);
            } else if (on_stack[neighbor]) {
                // 返祖边：用发现时间更新 low
                low[node_id] = std::min(low[node_id], order[neighbor]);
            }
        }
    }

    // 到达 SCC 根节点，弹出一个强连通分量
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

/**
 * @brief 计算热力图排序
 *
 * 按 fan_in（被调用次数）降序排列函数统计。
 * 排序结果用于前端热力着色：调用频率越高的函数颜色越突出。
 *
 * @param stats 待排序的统计结果（会被修改）
 */
void Analyzer::compute_hotspots(AnalysisStats& stats) {
    spdlog::debug("计算热力值");

    // 函数热力：按被调用次数降序排列
    std::sort(stats.function_stats.begin(), stats.function_stats.end(),
              [](const FunctionStats& a, const FunctionStats& b) {
                  return a.fan_in > b.fan_in;
              });
}
