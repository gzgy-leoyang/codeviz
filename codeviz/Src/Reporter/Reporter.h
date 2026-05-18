/**
 * @file Reporter.h
 * @brief HTML 报告生成器
 *
 * 将符号元数据、统计结果和图结构数据序列化为 JSON，
 * 注入内嵌 HTML 模板 + Cytoscape.js 生成自包含的交互式 HTML 报告。
 * 对应设计文档 4.3.8 节。
 */

#ifndef CODEVIZ_REPORTER_H
#define CODEVIZ_REPORTER_H

#include <string>
#include <vector>
#include "Common/DataTypes.h"
#include <nlohmann/json.hpp>

/**
 * @class Reporter
 * @brief HTML 报告生成器
 *
 * 使用 Inja 模板引擎将分析数据渲染为自包含的 HTML 页面。
 * 报告包含：调用图（Cytoscape.js 交互式可视化）、
 * 包含图、类型图、统计分析（热力图）、符号查询面板。
 */
class Reporter {
public:
    /**
     * @brief 生成 HTML 报告
     *
     * 流程：加载模板 → 构建 JSON 数据 → 注入 Cytoscape.js 库
     * → Inja 模板渲染 → 返回 HTMLReport
     *
     * @param symbols 符号元数据列表
     * @param stats 统计分析结果
     * @param ctx 分析上下文（含图数据、外部引用等）
     * @return 完整的 HTML 报告内容
     */
    HTMLReport generate(const std::vector<SymbolMetadata>& symbols,
                        const AnalysisStats& stats,
                        const AnalysisContext& ctx);

private:
    /**
     * @brief 加载内嵌的 HTML 骨架模板
     *
     * @return HTML 模板字符串
     */
    std::string load_template();

    /**
     * @brief 构建完整的 JSON 数据对象
     *
     * 包含 metadata、symbols、composites、call_graph、
     * include_graph、type_graph、hotspots、anomalies、
     * external_refs、stats 各层数据。
     *
     * @param symbols 符号元数据
     * @param stats 统计分析结果
     * @param ctx 分析上下文
     * @return 序列化用的 JSON 对象
     */
    nlohmann::json build_json(const std::vector<SymbolMetadata>& symbols,
                              const AnalysisStats& stats,
                              const AnalysisContext& ctx);

    /**
     * @brief 将调用边转换为 Cytoscape.js 格式
     *
     * 边去重：合并相同 (caller, callee) 的权重，
     * 只包含出现在边中的节点。
     *
     * @param edges 调用边列表
     * @param symbols 符号表
     * @return Cytoscape.js 兼容的 JSON 子对象
     */
    nlohmann::json convert_call_graph(const std::vector<CallEdge>& edges,
                                      const std::vector<Symbol>& symbols);

    /**
     * @brief 将包含边转换为 Cytoscape.js 格式
     *
     * 节点标签只显示文件名（去除目录路径）。
     *
     * @param edges 包含边列表
     * @param files 文件符号池
     * @param symbols 符号表
     * @return Cytoscape.js 兼容的 JSON 子对象
     */
    nlohmann::json convert_include_graph(const std::vector<IncludeEdge>& edges,
                                         const std::vector<FileSymbol>& files,
                                         const std::vector<Symbol>& symbols);

    /**
     * @brief 将类型依赖边转换为 Cytoscape.js 格式
     *
     * 区分 INHERITS 和 CONTAINS 两种关系类型。
     *
     * @param edges 类型依赖边列表
     * @param composites 复合类型池
     * @param symbols 符号表
     * @return Cytoscape.js 兼容的 JSON 子对象
     */
    nlohmann::json convert_type_graph(const std::vector<TypeDependencyEdge>& edges,
                                      const std::vector<CompositeSymbol>& composites,
                                      const std::vector<Symbol>& symbols);

    /**
     * @brief 构建热力图数据
     *
     * 按文件代码行数和函数扇入计算归一化热力值，
     * 用于前端热力着色。
     *
     * @param stats 统计分析结果
     * @return JSON 格式的热力图数据
     */
    nlohmann::json build_hotspots(const AnalysisStats& stats);

    /**
     * @brief 构建异常检测结果数据
     *
     * @param stats 统计分析结果
     * @return JSON 格式的异常数据（当前包含循环包含检测结果）
     */
    nlohmann::json build_anomalies(const AnalysisStats& stats);

    /**
     * @brief 根据 Symbol ID 查找符号名称
     *
     * @param id 符号 ID
     * @param symbols 符号表
     * @return 符号名称，未找到返回 "sym_<id>"
     */
    std::string find_symbol_name(uint32_t id, const std::vector<Symbol>& symbols);
};

#endif // CODEVIZ_REPORTER_H
