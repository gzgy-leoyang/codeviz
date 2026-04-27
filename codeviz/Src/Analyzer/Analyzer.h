// Analyzer/Analyzer.h - 分析引擎
// 统计计算：圈复杂度、扇入扇出、文件行数、热力图、循环包含检测
// 对应设计文档 4.3.7 节

#ifndef CODEVIZ_ANALYZER_H
#define CODEVIZ_ANALYZER_H

#include <vector>
#include <unordered_map>
#include "Common/DataTypes.h"

class Analyzer {
public:
    /**
     * 执行统计分析
     * @param ctx 已构建图数据的分析上下文
     * @param build_meta 构建元数据（来自 CMake 解析，用于报告中展示）
     * @return 统计结果
     */
    AnalysisStats analyze(const AnalysisContext& ctx, const BuildMetadata& build_meta);

private:
    /**
     * 基于函数的分支节点数计算圈复杂度（分支数 + 1）
     */
    int compute_cyclomatic_complexity(const FunctionSymbol& func);

    /**
     * 聚合文件级统计（总行数、代码行数、注释行数、复杂度总和）
     */
    FileStats compute_file_stats(const FileSymbol& file, const AnalysisContext& ctx);

    /**
     * 使用 Tarjan 算法检测有向图中的强连通分量，报告循环包含
     */
    std::vector<CircularInclude> detect_circular_includes(
        const std::vector<IncludeEdge>& edges, int file_count);

    /**
     * 汇总所有函数的扇入、扇出、圈复杂度
     */
    std::vector<FunctionStats> compute_function_stats(const AnalysisContext& ctx);

    /**
     * 对文件和函数按指标排序，计算归一化热力值用于渲染着色
     */
    void compute_hotspots(AnalysisStats& stats);

    // Tarjan 算法内部状态
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
