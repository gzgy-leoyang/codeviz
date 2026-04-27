// Indexer/Indexer.h - 符号索引模块
// 汇总所有 FileParseResult，构建全局符号表，填充 AnalysisContext
// 对应设计文档 4.3.5 节

#ifndef CODEVIZ_INDEXER_H
#define CODEVIZ_INDEXER_H

#include <vector>
#include <unordered_map>
#include "Common/DataTypes.h"

class Indexer {
public:
    /**
     * 构建全局符号索引
     * @param parse_results 所有文件的解析结果
     * @return 填充好符号表和引用关系的分析上下文
     */
    AnalysisContext build_index(const std::vector<FileParseResult>& parse_results);

    /**
     * 导出符号元数据（供 Reporter 使用）
     * @param ctx 已构建的分析上下文
     * @return 符号元数据列表
     */
    std::vector<SymbolMetadata> export_metadata(const AnalysisContext& ctx);

private:
    /**
     * 初始化上下文字段
     */
    void init_context(AnalysisContext& ctx);

    /**
     * 分配或获取全局唯一 Symbol ID
     */
    uint32_t get_or_create_symbol_id(const RawSymbol& raw, AnalysisContext& ctx);

    /**
     * 将 RawSymbol 转换为核心 Symbol 结构
     */
    Symbol convert_to_symbol(const RawSymbol& raw, uint32_t id);

    /**
     * 提取函数特有字段并创建 FunctionSymbol
     */
    FunctionSymbol extract_function_detail(const RawSymbol& raw, uint32_t symbol_id);

    /**
     * 提取复合类型特有字段并创建 CompositeSymbol
     */
    CompositeSymbol extract_composite_detail(const RawSymbol& raw, uint32_t symbol_id);

    /**
     * 为输入文件创建 FileSymbol 并分配 ID
     */
    FileSymbol create_file_symbol(const std::string& file_path, AnalysisContext& ctx);

    /**
     * 解析 callee_names 为 SymbolRef，生成调用边
     */
    void process_calls(const std::vector<FileParseResult>& results, AnalysisContext& ctx);

    /**
     * 解析 includes 关系为 IncludeEdge
     */
    void process_includes(const std::vector<FileParseResult>& results, AnalysisContext& ctx);

    /**
     * 将 includee 文件名解析为项目中的完整文件路径对应的 FileSymbol ID
     */
    uint32_t resolve_include_file(const std::string& includee_path,
                                   const std::string& includer_file,
                                   const std::vector<std::string>& all_file_paths,
                                   const AnalysisContext& ctx);

    /**
     * 根据 SymbolRef 填充被引用符号的 references 列表
     */
    void fill_reverse_references(AnalysisContext& ctx);

    /**
     * 解析复合类型的基类名称为 Symbol ID，生成类型依赖边
     */
    void resolve_composite_base_classes(AnalysisContext& ctx);

    uint32_t next_id_ = 1; // 自增 ID 分配器

    // 暂存符号：symbol_id → 基类名称列表（待第二遍解析）
    std::unordered_map<uint32_t, std::vector<std::string>> unresolved_base_classes_;
};

#endif // CODEVIZ_INDEXER_H
