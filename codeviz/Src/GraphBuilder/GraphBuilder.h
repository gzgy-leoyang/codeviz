// GraphBuilder/GraphBuilder.h - 图构建模块
// 构建函数调用图、头文件包含图、类型依赖图，计算扇入扇出
// 对应设计文档 4.3.6 节

#ifndef CODEVIZ_GRAPH_BUILDER_H
#define CODEVIZ_GRAPH_BUILDER_H

#include <string>
#include <vector>
#include <unordered_set>
#include "Common/DataTypes.h"

class GraphBuilder {
public:
    /**
     * 构建调用图、包含图和类型依赖图，并计算扇入扇出
     * @param ctx 分析上下文（输入输出，边数据和函数统计将被填充）
     * @param entry_function 调用图展开的入口函数名
     * @param depth 展开的最大深度
     */
    void build(AnalysisContext& ctx, const std::string& entry_function, int depth);

    /**
     * 导出为分析引擎消费的图数据
     * @param ctx 已构建边数据的上下文
     * @return 结构化的图数据
     */
    GraphData export_graph_data(const AnalysisContext& ctx);

private:
    /**
     * 根据名称定位入口函数的 Symbol ID
     */
    uint32_t find_entry_id(const AnalysisContext& ctx, const std::string& entry_name);

    /**
     * 从入口函数 BFS 遍历构建调用图，生成 CallEdge
     */
    void build_call_graph(AnalysisContext& ctx, uint32_t entry_id, int max_depth);

    /**
     * 将 include 关系转换为 IncludeEdge 填充到 ctx
     */
    void build_include_graph(AnalysisContext& ctx);

    /**
     * 分析 CompositeSymbol 之间的字段类型和继承关系，生成 TypeDependencyEdge
     */
    void build_type_dependency_graph(AnalysisContext& ctx);

    /**
     * 基于 CallEdge 统计每个函数的被调用次数
     */
    void compute_fan_in(AnalysisContext& ctx);

    /**
     * 基于 CallEdge 统计每个函数调用的不同函数数
     */
    void compute_fan_out(AnalysisContext& ctx);

    /**
     * 广度优先遍历调用关系，受深度限制
     */
    void bfs_traverse(uint32_t start_id, int max_depth, AnalysisContext& ctx,
                      std::vector<CallEdge>& edges);
};

#endif // CODEVIZ_GRAPH_BUILDER_H
