// Reporter/Reporter.h - HTML 报告生成器
// 将符号元数据、统计结果和图结构数据序列化为 JSON，注入 HTML 模板
// 对应设计文档 4.3.8 节

#ifndef CODEVIZ_REPORTER_H
#define CODEVIZ_REPORTER_H

#include <string>
#include <vector>
#include "Common/DataTypes.h"
#include <nlohmann/json.hpp>

class Reporter {
public:
    /**
     * 生成 HTML 报告
     * @param symbols 符号元数据列表
     * @param stats 统计分析结果
     * @param ctx 分析上下文（含图数据）
     * @return 完整的 HTML 报告内容及建议输出路径
     */
    HTMLReport generate(const std::vector<SymbolMetadata>& symbols,
                        const AnalysisStats& stats,
                        const AnalysisContext& ctx);

private:
    /**
     * 加载内嵌的 HTML 骨架模板字符串
     */
    std::string load_template();

    /**
     * 构建完整的 JSON 数据对象
     */
    nlohmann::json build_json(const std::vector<SymbolMetadata>& symbols,
                              const AnalysisStats& stats,
                              const AnalysisContext& ctx);

    /**
     * 将调用边转换为 Cytoscape.js nodes/edges 格式
     */
    nlohmann::json convert_call_graph(const std::vector<CallEdge>& edges,
                                      const std::vector<Symbol>& symbols);

    /**
     * 将包含边转换为 Cytoscape.js nodes/edges 格式
     */
    nlohmann::json convert_include_graph(const std::vector<IncludeEdge>& edges,
                                         const std::vector<FileSymbol>& files,
                                         const std::vector<Symbol>& symbols);

    /**
     * 将类型依赖边转换为 Cytoscape.js nodes/edges 格式
     */
    nlohmann::json convert_type_graph(const std::vector<TypeDependencyEdge>& edges,
                                      const std::vector<CompositeSymbol>& composites,
                                      const std::vector<Symbol>& symbols);

    /**
     * 构建热力图数据（文件和函数的热力值及颜色映射）
     */
    nlohmann::json build_hotspots(const AnalysisStats& stats);

    /**
     * 构建异常检测结果数据（循环包含等）
     */
    nlohmann::json build_anomalies(const AnalysisStats& stats);

    /**
     * 辅助：根据 Symbol ID 查找 Symbol name
     */
    std::string find_symbol_name(uint32_t id, const std::vector<Symbol>& symbols);
};

#endif // CODEVIZ_REPORTER_H
