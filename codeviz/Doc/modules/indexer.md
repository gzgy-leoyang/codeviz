# 符号索引模块

> 来源: 从设计规格说明书 4.3 节提取的 符号索引模块 详细设计。
> 原始文档: Doc/Code_Visualization_Tool_Design_Spec.md

#### 4.3.5 符号索引模块

##### 职责
汇总所有文件的 `FileParseResult`，进行符号去重、ID 分配、符号表构建，解析调用和包含关系为 ID 引用，填充 `AnalysisContext` 核心数据结构，并提供导出 `SymbolMetadata` 给 HTML 报告生成器。

##### 对外接口

```cpp
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
};
```

##### 内部函数

| 函数签名 | 功能 |
| :--- | :--- |
| void init_context(AnalysisContext& ctx) | 初始化上下文字段，重置 next_id_ |
| uint32_t get_or_create_symbol_id(const RawSymbol& raw, AnalysisContext& ctx) | 分配或获取全局唯一 Symbol ID（key: 文件路径::名称@行号）|
| Symbol convert_to_symbol(const RawSymbol& raw, uint32_t id) | 将 RawSymbol 转换为核心 Symbol 结构 |
| FunctionSymbol extract_function_detail(const RawSymbol& raw, uint32_t symbol_id) | 提取函数特有字段（返回类型、参数、virtual/static/inline、branch_count）并创建 FunctionSymbol |
| CompositeSymbol extract_composite_detail(const RawSymbol& raw, uint32_t symbol_id) | 提取复合类型特有字段并创建 CompositeSymbol；暂存基类名称为待解析列表 |
| FileSymbol create_file_symbol(const std::string& file_path, AnalysisContext& ctx) | 为输入文件创建 FileSymbol 并分配 ID（KEY = "FILE::" + 路径）|
| void process_calls(const std::vector<FileParseResult>& results, AnalysisContext& ctx) | 解析 callee_names 为 SymbolRef，生成 CallEdge；未解析的符号收集为 ExternalRef |
| void process_includes(const std::vector<FileParseResult>& results, AnalysisContext& ctx) | 解析 includes 关系为 IncludeEdge，跳过系统头文件 |
| void fill_reverse_references(AnalysisContext& ctx) | 根据 SymbolRef 填充被引用符号的 references 列表 |
| uint32_t resolve_include_file(const std::string& includee_path, const std::string& includer_file, const std::vector<std::string>& all_file_paths, const AnalysisContext& ctx) | 将 includee 文件名解析为项目中对应的 FileSymbol ID（精确匹配 → 后缀匹配）|
| void resolve_composite_base_classes(AnalysisContext& ctx) | 将暂存的基类名称列表解析为 Symbol ID，生成 TypeDependencyEdge(INHERITS) |

##### 成员变量
| 变量 | 类型 | 用途 |
| :--- | :--- | :--- |
| next_id_ | uint32_t | 自增 ID 分配器（初始为 1）|
| unresolved_base_classes_ | std::unordered_map<uint32_t, std::vector<std::string>> | 暂存复合类型的基类名称（待第二遍解析为 ID）|

##### 主流程步骤
1. 调用 init_context 初始化 AnalysisContext。
2. 第一遍遍历所有 FileParseResult：
   - 为每个文件调用 create_file_symbol 创建 FileSymbol。
   - 遍历该文件的 RawSymbol，对每个符号调用 get_or_create_symbol_id 分配 ID 并转换为 Symbol 存入 ctx.symbols。
   - 根据 RawSymbol 的 kind 调用 extract_function_detail 或 extract_composite_detail，创建对应的分类型符号并存入 ctx.functions / ctx.composites。
   - 回填文件行数（total_lines / code_lines / comment_lines）。
3. 第二遍遍历所有 FileParseResult：
   - process_calls：对每个函数符号的 callee_names，查找被调用者 ID，生成 CallEdge 和 SymbolRef；未解析的外部符号收集为 ExternalRef（按命名空间前缀推测库名），回填 FunctionSymbol::callees。
   - process_includes：解析 includes 关系，将文件名通过 resolve_include_file 转换为 FileSymbol ID，生成 IncludeEdge。
4. 调用 resolve_composite_base_classes，将基类名称解析为 Symbol ID，生成 INHERITS 类型依赖边。
5. 调用 fill_reverse_references，遍历所有 SymbolRef，填充被引用符号的 references 列表。
6. 返回 AnalysisContext。

##### 依赖的数据结构
- 输入接口数据：std::vector<FileParseResult>
- 输出核心数据：AnalysisContext
- 输出接口数据：std::vector<SymbolMetadata>（通过 export_metadata 从 AnalysisContext 投影）

##### 异常处理
- 被调用者符号无法定位（如外部库或解析缺失）：记录警告，使用特殊 ID（如 UINT32_MAX）表示外部符号，不中止处理。
- 符号名冲突（同文件同作用域出现重名）：记录警告，为后续符号生成唯一后缀，保留两者。
- 包含关系中的文件路径无法转换为 FileSymbol ID：记录警告并跳过该包含边。
