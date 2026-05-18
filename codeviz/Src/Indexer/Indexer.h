/**
 * @file Indexer.h
 * @brief 符号索引模块
 *
 * 汇总所有 FileParseResult，两遍遍历构建全局符号表，
 * 解析调用关系、包含关系和类型继承关系。
 * 对应设计文档 4.3.5 节。
 */

#ifndef CODEVIZ_INDEXER_H
#define CODEVIZ_INDEXER_H

#include <vector>
#include <unordered_map>
#include "Common/DataTypes.h"

/**
 * @class Indexer
 * @brief 符号索引构建器
 *
 * 执行两遍遍历：
 * 第一遍为每个文件和符号分配全局唯一 ID，填充符号表；
 * 第二遍解析调用关系、包含关系和复合类型基类继承关系。
 */
class Indexer {
public:
    /**
     * @brief 构建全局符号索引
     *
     * 两遍遍历 FileParseResult 列表：
     * 1) 分配 ID、创建 Symbol/FunctionSymbol/CompositeSymbol/FileSymbol
     * 2) 解析调用边、包含边、类型继承边，填充反向引用
     *
     * @param parse_results 所有文件的解析结果
     * @return 填充好符号表和引用关系的分析上下文
     */
    AnalysisContext build_index(const std::vector<FileParseResult>& parse_results);

    /**
     * @brief 导出符号元数据
     *
     * 从 AnalysisContext 中提取符号的基本信息和统计指标，
     * 供 Reporter 生成 HTML 报告使用。
     *
     * @param ctx 已构建的分析上下文
     * @return 符号元数据列表
     */
    std::vector<SymbolMetadata> export_metadata(const AnalysisContext& ctx);

private:
    /**
     * @brief 初始化分析上下文
     *
     * 清空所有容器，重置自增 ID 计数器和待解析基类映射。
     *
     * @param ctx 待初始化的分析上下文
     */
    void init_context(AnalysisContext& ctx);

    /**
     * @brief 获取或创建符号的全局唯一 ID
     *
     * 使用 "文件路径::名称@行号" 作为唯一键，
     * 避免同名符号的 ID 冲突。
     *
     * @param raw 原始符号
     * @param ctx 分析上下文（含 symbol_name_to_id 映射）
     * @return 符号的全局唯一 ID
     */
    uint32_t get_or_create_symbol_id(const RawSymbol& raw, AnalysisContext& ctx);

    /**
     * @brief 将 RawSymbol 转换为核心 Symbol
     *
     * @param raw 原始符号
     * @param id 分配的全局 ID
     * @return 转换后的 Symbol
     */
    Symbol convert_to_symbol(const RawSymbol& raw, uint32_t id);

    /**
     * @brief 从 RawSymbol 提取函数特有信息
     *
     * @param raw 原始符号
     * @param symbol_id 符号 ID
     * @return 函数符号
     */
    FunctionSymbol extract_function_detail(const RawSymbol& raw, uint32_t symbol_id);

    /**
     * @brief 从 RawSymbol 提取复合类型特有信息
     *
     * 暂存基类名称字符串，留待第二遍解析。
     *
     * @param raw 原始符号
     * @param symbol_id 符号 ID
     * @return 复合类型符号
     */
    CompositeSymbol extract_composite_detail(const RawSymbol& raw, uint32_t symbol_id);

    /**
     * @brief 为源文件创建文件符号
     *
     * 创建 FILE_ENTITY 类型的 Symbol 和 FileSymbol，
     * 分配文件级别 ID。
     *
     * @param file_path 文件路径
     * @param ctx 分析上下文
     * @return 创建的文件符号
     */
    FileSymbol create_file_symbol(const std::string& file_path, AnalysisContext& ctx);

    /**
     * @brief 解析函数调用关系
     *
     * 遍历所有函数符号的 callee_names，尝试解析为 Symbol 引用。
     * 未解析的外部符号标记为 uint32_t::max()，
     * 并按命名空间前缀推测所属库名。
     *
     * @param results 所有文件的解析结果
     * @param ctx 分析上下文（输出：call_edges, references, external_refs）
     */
    void process_calls(const std::vector<FileParseResult>& results, AnalysisContext& ctx);

    /**
     * @brief 解析头文件包含关系
     *
     * 将每对 (includer, includee) 解析为 IncludeEdge，
     * 跳过系统头文件。
     *
     * @param results 所有文件的解析结果
     * @param ctx 分析上下文（输出：include_edges）
     */
    void process_includes(const std::vector<FileParseResult>& results, AnalysisContext& ctx);

    /**
     * @brief 解析包含文件路径为 FileSymbol ID
     *
     * 先精确匹配，再后缀模糊匹配项目内的已知文件路径。
     *
     * @param includee_path 被包含的文件路径
     * @param includer_file 包含者的文件路径
     * @param all_file_paths 项目内所有已知文件路径列表
     * @param ctx 分析上下文
     * @return 匹配到的 FileSymbol ID，未找到返回 0
     */
    uint32_t resolve_include_file(const std::string& includee_path,
                                   const std::string& includer_file,
                                   const std::vector<std::string>& all_file_paths,
                                   const AnalysisContext& ctx);

    /**
     * @brief 填充被引用符号的反向引用列表
     *
     * 遍历所有 SymbolRef，将引用者的 ID 填充到被引用 Symbol 的 references 列表。
     *
     * @param ctx 分析上下文（输出：symbols[].references）
     */
    void fill_reverse_references(AnalysisContext& ctx);

    /**
     * @brief 解析复合类型的基类名称
     *
     * 将第二遍遍历时暂存的基类名称字符串解析为 Symbol ID，
     * 创建 INHERITS 类型的 TypeDependencyEdge。
     *
     * @param ctx 分析上下文（输出：type_edges, composites[].base_classes）
     */
    void resolve_composite_base_classes(AnalysisContext& ctx);

    uint32_t next_id_ = 1; ///< 自增 ID 分配器

    /// 暂存符号 ID → 基类名称列表（待第二遍解析时使用）
    std::unordered_map<uint32_t, std::vector<std::string>> unresolved_base_classes_;
};

#endif // CODEVIZ_INDEXER_H
