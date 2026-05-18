/**
 * @file Analyzer.h
 * @brief 分析引擎
 *
 * 统计计算：圈复杂度、扇入扇出、文件行数、热力图、循环包含检测。
 * 使用 Tarjan 算法检测头文件循环包含。
 * 对应设计文档 4.3.7 节。
 */

#ifndef CODEVIZ_ANALYZER_H
#define CODEVIZ_ANALYZER_H

#include <vector>
#include <unordered_map>
#include "Common/DataTypes.h"

/**
 * @class Analyzer
 * @brief 执行源码统计分析
 *
 * 在 Indexer 和 GraphBuilder 完成后执行，对符号表和图数据进行
 * 深度分析计算。主要功能包括：
 *   - 基于分支节点数计算每个函数的圈复杂度
 *   - 汇总文件级统计（行数、复杂度总和）
 *   - 使用 Tarjan SCC 算法检测头文件循环包含
 *   - 生成函数热力图排序
 */
class Analyzer {
public:
    /**
     * @brief 执行完整的统计分析
     *
     * 按顺序执行：文件统计 → 函数统计（含圈复杂度计算）→
     * 循环包含检测 → 热力图计算。
     *
     * @param ctx 已构建图数据的分析上下文（含文件池、函数池、包含边）
     * @param build_meta 构建元数据（来自 CMake 解析，用于报告中展示）
     * @return 完整统计结果
     */
    AnalysisStats analyze(const AnalysisContext& ctx, const BuildMetadata& build_meta);

private:
    /**
     * @brief 计算单个函数的圈复杂度
     *
     * M = 分支节点数 + 1
     * 分支节点由 ParserFrontend 遍历 CST 时统计：
     *   if / for / while / do / switch / case / 三元 ?:
     *
     * @param func 目标函数符号
     * @return 圈复杂度值
     */
    int compute_cyclomatic_complexity(const FunctionSymbol& func);

    /**
     * @brief 计算单个文件的统计数据
     *
     * 聚合文件的行数信息，并累加该文件内所有函数的圈复杂度总和。
     *
     * @param file 文件符号
     * @param ctx 分析上下文（用于查找函数复杂度信息）
     * @return 文件统计结果
     */
    FileStats compute_file_stats(const FileSymbol& file, const AnalysisContext& ctx);

    /**
     * @brief 使用 Tarjan 算法检测头文件循环包含
     *
     * 将包含关系建模为有向图，使用 Tarjan 强连通分量算法
     * 检测大小 > 1 的 SCC，即表示循环包含。
     *
     * @param edges 所有包含边
     * @param file_count 文件总数
     * @return 检测到的循环包含列表
     */
    std::vector<CircularInclude> detect_circular_includes(
        const std::vector<IncludeEdge>& edges, int file_count);

    /**
     * @brief 汇总所有函数的统计指标
     *
     * 遍历函数池，计算并回填每个函数的圈复杂度，
     * 同时收集扇入/扇出数据。
     *
     * @param ctx 分析上下文
     * @return 所有函数的统计结果列表
     */
    std::vector<FunctionStats> compute_function_stats(const AnalysisContext& ctx);

    /**
     * @brief 计算热力图排序
     *
     * 按被调用次数（fan_in）降序排列函数，用于前端热力着色。
     *
     * @param stats 待计算热力值的统计结果（会被修改）
     */
    void compute_hotspots(AnalysisStats& stats);

    /**
     * @brief Tarjan 算法的 DFS 递归实现
     *
     * 深度优先搜索计算每个节点的 order 和 low 值，
     * 通过栈追踪在当前搜索路径上的节点。
     *
     * @param node_id 当前节点 ID
     * @param adj 邻接表（节点 → 后继节点列表）
     * @param order 节点访问顺序号（发现时间）
     * @param low 节点可达的最小 order 值
     * @param on_stack 节点是否在当前 DFS 栈中
     * @param stk DFS 栈
     * @param timer 全局计时器（发现时间计数器）
     * @param sccs 已发现的强连通分量列表（输出参数）
     * @param id_to_path 节点 ID → 文件路径映射（仅用于日志）
     */
    void tarjan_dfs(uint32_t node_id,
                    const std::unordered_map<uint32_t, std::vector<uint32_t>>& adj,
                    std::unordered_map<uint32_t, int>& order,
                    std::unordered_map<uint32_t, int>& low,
                    std::unordered_map<uint32_t, bool>& on_stack,
                    std::vector<uint32_t>& stk,
                    int& timer,
                    std::vector<std::vector<uint32_t>>& sccs,
                    const std::unordered_map<uint32_t, std::string>& id_to_path);
};

#endif // CODEVIZ_ANALYZER_H
