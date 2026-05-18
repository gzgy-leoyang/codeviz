/**
 * @file GraphBuilder.h
 * @brief 图构建模块
 *
 * 构建函数调用图、头文件包含图、类型依赖图，计算扇入扇出。
 * 对应设计文档 4.3.6 节。
 */

#ifndef CODEVIZ_GRAPH_BUILDER_H
#define CODEVIZ_GRAPH_BUILDER_H

#include <string>
#include <vector>
#include <unordered_set>
#include "Common/DataTypes.h"

/**
 * @class GraphBuilder
 * @brief 图数据构建器
 *
 * 在 Indexer 完成后执行，基于 AnalysisContext 构建三种图：
 * - 调用图（从入口函数 BFS 展开，受深度限制）
 * - 包含图（文件间 #include 关系验证和统计）
 * - 类型依赖图（复合类型间的包含和继承关系）
 * 同时计算每个函数的扇入（被调用次数）和扇出（调用其他函数数）。
 */
class GraphBuilder {
public:
    /**
     * @brief 构建调用图、包含图和类型依赖图
     *
     * 执行步骤：
     * 1. 定位入口函数 Symbol ID
     * 2. 计算全量扇入/扇出（必须在 BFS 替换 call_edges 之前）
     * 3. 构建包含图
     * 4. 构建类型依赖图
     * 5. BFS 展开调用子图（替换 ctx.call_edges）
     *
     * @param ctx 分析上下文（输入输出，边数据和函数统计将被填充）
     * @param entry_function 调用图展开的入口函数名
     * @param depth 展开的最大深度（1~20）
     */
    void build(AnalysisContext& ctx, const std::string& entry_function, int depth);

    /**
     * @brief 导出为分析引擎消费的结构化图数据
     *
     * @param ctx 已构建边数据的上下文
     * @return 包含所有节点和边的 GraphData
     */
    GraphData export_graph_data(const AnalysisContext& ctx);

private:
    /**
     * @brief 定位入口函数的 Symbol ID
     *
     * 优先按完全限定名匹配，再按短名称匹配。
     *
     * @param ctx 分析上下文
     * @param entry_name 入口函数名
     * @return 入口函数的 Symbol ID，未找到返回 0
     */
    uint32_t find_entry_id(const AnalysisContext& ctx, const std::string& entry_name);

    /**
     * @brief 从入口函数 BFS 遍历构建调用子图
     *
     * 先保存完整调用边到 ctx.full_call_edges（供前端按需展开），
     * 然后 BFS 遍历生成子图，替换 ctx.call_edges。
     *
     * @param ctx 分析上下文（输入输出：call_edges 被替换为 BFS 子图）
     * @param entry_id 入口函数 Symbol ID
     * @param max_depth 最大展开深度
     */
    void build_call_graph(AnalysisContext& ctx, uint32_t entry_id, int max_depth);

    /**
     * @brief 验证和统计包含图
     *
     * 验证 IncludeEdge 两端 ID 的有效性，
     * 统计每个文件被包含的次数，找出热点头文件。
     *
     * @param ctx 分析上下文
     */
    void build_include_graph(AnalysisContext& ctx);

    /**
     * @brief 构建类型依赖图
     *
     * 遍历所有 CompositeSymbol，分析字段类型引用关系，
     * 创建 CONTAINS 类型的 TypeDependencyEdge（避免重复）。
     *
     * @param ctx 分析上下文（输出：type_edges）
     */
    void build_type_dependency_graph(AnalysisContext& ctx);

    /**
     * @brief 计算函数扇入（被调用次数）
     *
     * 统计每个 callee_id 在 call_edges 中出现的次数，
     * 回填到 FunctionSymbol::fan_in。
     *
     * @param ctx 分析上下文（输出：functions[].fan_in）
     */
    void compute_fan_in(AnalysisContext& ctx);

    /**
     * @brief 计算函数扇出（调用的不同函数数）
     *
     * 统计每个 caller_id 去重后的 callee 数量，
     * 回填到 FunctionSymbol::fan_out。
     *
     * @param ctx 分析上下文（输出：functions[].fan_out）
     */
    void compute_fan_out(AnalysisContext& ctx);

    /**
     * @brief 广度优先遍历调用关系
     *
     * 使用队列实现 BFS，受最大深度限制。
     * 构建 callee_map 快速索引避免重复遍历全量边。
     *
     * @param start_id 起始 Symbol ID
     * @param max_depth 最大深度
     * @param ctx 分析上下文
     * @param edges [out] BFS 子图的调用边集合
     */
    void bfs_traverse(uint32_t start_id, int max_depth, AnalysisContext& ctx,
                      std::vector<CallEdge>& edges);
};

#endif // CODEVIZ_GRAPH_BUILDER_H
