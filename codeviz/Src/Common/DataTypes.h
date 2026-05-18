/**
 * @file DataTypes.h
 * @brief 核心数据结构与接口数据结构定义（纯头文件）
 *
 * 对应设计文档 4.2.1 节（接口数据结构）和 4.2.2 节（核心数据结构）。
 * 所有模块共用，不允许自行发明新结构。
 *
 * @defgroup CoreData 核心数据结构
 * @defgroup InterfaceData 接口数据结构
 * @defgroup BuildMetadataGroup 构建元数据
 * @defgroup AnalysisContextGroup 统一数据容器
 */

#ifndef CODEVIZ_DATA_TYPES_H
#define CODEVIZ_DATA_TYPES_H

#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>

// ============================================================================
// 4.2.2 核心数据结构
// ============================================================================
/** @ingroup CoreData */
/** @{ */

/**
 * @brief 符号类型枚举
 *
 * 从 RawSymbol::Kind 抽象并扩展，覆盖 C/C++ 中的所有可见符号类型。
 */
enum class SymbolKind {
    FUNCTION,       ///< 函数（含成员函数）
    STRUCT,         ///< 结构体
    CLASS,          ///< 类
    ENUM,           ///< 枚举
    VARIABLE,       ///< 全局变量
    MACRO,          ///< 宏定义
    FILE_ENTITY     ///< 文件实体（用于包含图）
};

/**
 * @brief 访问修饰符枚举
 *
 * C++ 专用，从 CST 的 access_specifier 节点提取。
 */
enum class AccessSpecifier {
    PUBLIC,         ///< 公开 (public:)
    PROTECTED,      ///< 受保护 (protected:)
    PRIVATE,        ///< 私有 (private:)
    NONE            ///< C 语言或全局符号
};

/**
 * @brief 全局符号表的基础存储单元
 *
 * 所有符号的基类表示，存储通用字段。
 * 由 Indexer 模块创建，通过 AnalysisContext::symbols 向量共享。
 */
struct Symbol {
    uint32_t id = 0;                    ///< 全局唯一标识符（由 Indexer 自增分配）
    std::string name;                   ///< 符号短名称（如 "main"）
    std::string qualified_name;         ///< 完全限定名（如 "namespace::Class::method"）
    std::string file_path;              ///< 定义所在文件的绝对路径
    uint32_t line_start = 0;            ///< 定义起始行号
    uint32_t line_end = 0;              ///< 定义结束行号
    SymbolKind kind = SymbolKind::FUNCTION;    ///< 符号类型
    AccessSpecifier access = AccessSpecifier::NONE; ///< 访问修饰符
    std::vector<uint32_t> references;   ///< 引用该符号的 SymbolRef 索引列表（由 Indexer 填充）
};

/**
 * @brief 成员字段信息
 *
 * 对应需求 2.2.3.3 DR_3：成员名、类型、偏移、长度。
 */
struct FieldInfo {
    std::string name;                   ///< 成员名称
    std::string type;                   ///< 成员类型字符串
    uint32_t offset = 0;               ///< 字节偏移（若可获取）
    size_t size = 0;                    ///< 成员大小（字节）
    AccessSpecifier access = AccessSpecifier::NONE; ///< 访问修饰符
};

/**
 * @brief 函数符号扩展信息
 *
 * 从 RawSymbol 中提取函数特有信息，由 ParserFrontend 填充基本字段，
 * Indexer 关联到 Symbol 表，Analyzer 补充统计字段（圈复杂度、扇入/扇出）。
 */
struct FunctionSymbol {
    uint32_t symbol_id = 0;             ///< 关联到 Symbol::id
    std::string return_type;            ///< 返回值类型字符串
    std::vector<std::string> parameters; ///< 参数类型列表
    std::string comment;                ///< 声明/定义前的 Doxygen 注释文档
    bool is_virtual = false;            ///< 虚函数标记
    bool is_static = false;             ///< 静态函数标记
    bool is_inline = false;             ///< 内联函数标记
    int cyclomatic_complexity = 0;      ///< 圈复杂度（由 Analyzer 计算填充）
    int branch_count = 0;               ///< 分支节点数（ParserFrontend 统计，Analyzer 消费）
    int fan_in = 0;                     ///< 被调用次数（由 Analyzer 计算）
    int fan_out = 0;                    ///< 调用其他函数数量（由 Analyzer 计算）
    std::vector<uint32_t> callees;      ///< 直接调用的函数 Symbol ID 列表
};

/**
 * @brief 结构体/类符号扩展信息
 *
 * 存储复合类型特有信息：成员字段、成员函数、基类继承关系。
 */
struct CompositeSymbol {
    uint32_t symbol_id = 0;             ///< 关联到 Symbol::id
    std::vector<FieldInfo> fields;      ///< 成员字段列表
    std::vector<uint32_t> methods;      ///< 成员函数 Symbol ID 列表
    std::vector<uint32_t> base_classes; ///< 基类 Symbol ID 列表
    std::string comment;                ///< 声明前的 Doxygen 注释文档
    bool is_pod = false;                ///< 是否为平凡类型 (Plain Old Data)
    size_t total_size = 0;              ///< 类型总大小（字节）
};

/**
 * @brief 文件符号
 *
 * 用于包含图构建和文件级统计。
 * 每个被扫描的源文件和头文件对应一个 FileSymbol。
 */
struct FileSymbol {
    uint32_t symbol_id = 0;             ///< 关联到 Symbol::id
    int total_lines = 0;                ///< 总行数
    int code_lines = 0;                 ///< 有效代码行数
    int comment_lines = 0;              ///< 注释行数
    std::vector<uint32_t> includes;     ///< 包含的头文件 Symbol ID 列表
    std::vector<uint32_t> symbols;      ///< 本文件内定义的符号 ID 列表
};

/**
 * @brief 调用边
 *
 * 从 SymbolRef 中 relation == CALLS 抽象而来，
 * 表示两个函数之间的调用关系。
 */
struct CallEdge {
    uint32_t caller_id = 0;             ///< 调用者 Symbol ID
    uint32_t callee_id = 0;             ///< 被调用者 Symbol ID
    uint32_t call_count = 0;            ///< 调用次数（静态计数）
    uint32_t line = 0;                  ///< 调用发生的行号
    std::string file_path;              ///< 调用发生的文件路径
};

/**
 * @brief 包含边
 *
 * 从 FileParseResult::includes 抽象而来，
 * 表示源文件与头文件之间的 #include 关系。
 */
struct IncludeEdge {
    uint32_t includer_id = 0;           ///< 包含者文件 Symbol ID
    uint32_t includee_id = 0;           ///< 被包含头文件 Symbol ID
    uint32_t line = 0;                  ///< #include 所在行号
    bool is_system = false;             ///< 是否为系统头文件（<> 形式）
};

/**
 * @brief 类型依赖边
 *
 * 用于 CompositeSymbol 之间的关系，包括包含、继承、参数、返回四种关系。
 */
struct TypeDependencyEdge {
    uint32_t source_id = 0;             ///< 源类型 Symbol ID
    uint32_t target_id = 0;             ///< 目标类型 Symbol ID

    /** @brief 类型依赖关系类型 */
    enum Relation { CONTAINS, INHERITS, PARAMETER, RETURN } relation = CONTAINS;
};

/** @} */ // end of CoreData

// ============================================================================
// 4.2.1 接口数据结构
// ============================================================================
/** @ingroup InterfaceData */
/** @{ */

/**
 * @brief 源文件描述
 *
 * CLI 模块传递给 C/C++ 解析器的输入结构。
 * 包含文件路径和完整的文件内容。
 */
struct SourceFile {
    std::string file_path;   ///< 源文件绝对路径
    std::string content;     ///< 文件完整内容字符串
};

/**
 * @brief CMake 文件描述
 *
 * CLI 模块传递给 CMake 解析器的输入结构。
 */
struct CMakeFile {
    std::string file_path;     ///< 用于定位和错误报告
    std::string content;       ///< CMakeLists.txt 文件内容（解析器的输入）
    std::string source_dir;    ///< 解析相对路径的基准目录
};

/**
 * @brief 编译参数
 *
 * 编译数据库到 C/C++ 解析器的传递结构。
 * 每个源文件关联一组编译参数（宏定义、头文件路径、其他标志）。
 */
struct CompileArgs {
    std::string file_path;              ///< 关联到具体源文件
    std::vector<std::string> defines;   ///< -D 宏定义列表
    std::vector<std::string> includes;  ///< -I 头文件搜索路径列表
    std::vector<std::string> flags;     ///< 其他编译选项
};

/**
 * @brief 原始符号（解析器输出）
 *
 * C/C++ 解析器传递到符号索引模块的中间结构。
 * 包含从 CST 提取的原始信息，名称尚未解析为全局 ID。
 */
struct RawSymbol {
    std::string name;                   ///< 符号名称（可能有重名，尚未解析）
    std::string file_path;              ///< 定义所在文件路径
    uint32_t line_start = 0, line_end = 0; ///< 定义起止行号

    /** @brief 原始符号类型枚举 */
    enum Kind { FUNC, STRUCT, CLASS, ENUM_KIND, VAR, MACRO } kind = FUNC;

    std::vector<std::string> callee_names; ///< 函数内部调用的函数名列表（未解析为 ID）

    // 函数扩展信息
    std::string return_type;                ///< 返回值类型
    std::vector<std::string> parameters;    ///< 参数类型列表
    std::string comment;                    ///< 声明/定义前的 Doxygen 注释
    bool is_virtual = false;                ///< 虚函数标记
    bool is_static = false;                 ///< 静态函数标记
    bool is_inline = false;                 ///< 内联函数标记
    int branch_count = 0;                  ///< 分支节点数（由 ParserFrontend 统计）

    // 复合类型扩展信息
    std::vector<FieldInfo> fields;              ///< 成员字段列表
    std::vector<uint32_t> method_symbol_ids;    ///< 成员函数 ID（暂存，由 Indexer 解析）
    std::vector<std::string> base_class_names;  ///< 基类名称（未解析为 ID）
    AccessSpecifier access = AccessSpecifier::NONE; ///< 访问修饰符
};

/**
 * @brief 文件解析结果
 *
 * 每个源文件经过 ParserFrontend 解析后产生的输出。
 * 包含解析出的所有符号、包含关系及文件级统计信息。
 */
struct FileParseResult {
    std::string file_path;                              ///< 源文件路径
    std::vector<RawSymbol> symbols;                     ///< 文件内定义的符号列表
    std::vector<std::pair<std::string, std::string>> includes; ///< 包含关系对：(includer_path, includee_path)
    int total_lines = 0;   ///< 文件总行数
    int code_lines = 0;    ///< 有效代码行数
    int comment_lines = 0; ///< 注释行数
};

/**
 * @brief 符号引用
 *
 * 符号索引模块到图构建模块的传递结构。
 * 记录从一个符号到另一个符号的引用关系，用于构建调用图/包含图/类型图。
 */
struct SymbolRef {
    uint32_t from_symbol_id = 0;   ///< 引用者 Symbol ID
    uint32_t to_symbol_id = 0;     ///< 被引用者 Symbol ID
    uint32_t line = 0;             ///< 引用位置行号（用于 UI 跳转）
    std::string file_path;         ///< 引用发生的文件路径
};

/**
 * @brief 索引数据
 *
 * 符号索引模块的输出，包含全局符号表和所有引用关系。
 * 传递给图构建模块和分析引擎。
 */
struct IndexedData {
    std::vector<Symbol> symbols;      ///< 全局符号表（复用核心数据结构）
    std::vector<SymbolRef> references; ///< 所有引用关系
};

/**
 * @brief 图节点
 *
 * 图构建模块到分析引擎的传递结构。
 * 表示调用图/包含图/类型图中的一个节点。
 */
struct GraphNode {
    uint32_t id = 0;           ///< 对应 Symbol ID 或 File ID
    std::string label;         ///< 节点显示标签

    /** @brief 节点类型 */
    enum Type { FUNCTION, FILE_ENTITY, STRUCT } type = FUNCTION;
};

/**
 * @brief 图边
 *
 * 图构建模块到分析引擎的传递结构。
 * 表示图中的一条有向边，支持四种关系类型。
 */
struct GraphEdge {
    uint32_t source_id = 0;      ///< 起点节点 ID
    uint32_t target_id = 0;      ///< 终点节点 ID

    /** @brief 边的关系类型 */
    enum Relation { CALLS, INCLUDES, CONTAINS, INHERITS } relation = CALLS;

    uint32_t weight = 0;         ///< 权重（调用次数或包含次数，用于热力计算）
};

/**
 * @brief 图数据
 *
 * 图构建模块的输出，包含节点和边集合。
 */
struct GraphData {
    std::vector<GraphNode> nodes; ///< 所有图节点
    std::vector<GraphEdge> edges; ///< 所有图边
};

/**
 * @brief 符号元数据
 *
 * 符号索引模块到 HTML 报告生成器的传递结构。
 * 包含符号的基本信息和统计指标，用于前端展示。
 */
struct SymbolMetadata {
    uint32_t symbol_id = 0;         ///< 符号 ID
    std::string name;               ///< 符号短名称
    std::string qualified_name;     ///< 完全限定名
    std::string file_path;          ///< 定义文件路径
    uint32_t line = 0;              ///< 定义行号
    SymbolKind kind = SymbolKind::FUNCTION; ///< 符号类型
    int complexity = 0;   ///< 圈复杂度（待 Analyzer 填充后回填或二次传递）
    int fan_in = 0;       ///< 被调用次数
    int fan_out = 0;      ///< 调用其他函数数量
};

/**
 * @brief 文件统计
 *
 * 分析引擎到 HTML 报告生成器的传递结构。
 * 包含单个文件的行数和该文件内函数复杂度总和。
 */
struct FileStats {
    std::string file_path;             ///< 文件路径
    int total_lines = 0;               ///< 文件总行数
    int code_lines = 0;                ///< 有效代码行数
    double complexity_sum = 0.0;       ///< 该文件所有函数的复杂度总和
};

/**
 * @brief 函数统计
 *
 * 单个函数的统计指标，由 Analyzer 计算填充。
 */
struct FunctionStats {
    uint32_t function_id = 0;           ///< 函数 Symbol ID
    int fan_in = 0;                     ///< 被调用次数
    int fan_out = 0;                    ///< 调用其他函数数量
    int cyclomatic_complexity = 0;      ///< 圈复杂度
};

/**
 * @brief 循环包含检测结果
 *
 * 记录一组形成循环包含的文件路径链。
 */
struct CircularInclude {
    std::vector<std::string> file_cycle; ///< 形成循环的文件路径链
};

/**
 * @brief 分析结果统计
 *
 * Analyzer 模块的完整输出，包含文件统计、函数统计和循环包含检测结果。
 */
struct AnalysisStats {
    std::vector<FileStats> file_stats;      ///< 逐文件统计
    std::vector<FunctionStats> function_stats; ///< 逐函数统计
    std::vector<CircularInclude> circular_includes; ///< 循环包含列表
};

/**
 * @brief 外部符号引用
 *
 * 记录调用了项目外部符号的信息。
 * 被调用函数未在本项目中定义，推测其所属库名。
 */
struct ExternalRef {
    std::string caller_name;   ///< 调用者符号名
    std::string callee_name;   ///< 被调用者符号名（外部定义）
    std::string library;       ///< 推测的库名
};

/**
 * @brief HTML 报告
 *
 * HTML 报告生成器到文件系统的输出结构。
 */
struct HTMLReport {
    std::string content;     ///< 完整 HTML 字符串
    std::string output_path; ///< 建议的输出文件路径
};

/** @} */ // end of InterfaceData

// ============================================================================
// 附加数据结构（由 CMake 解析模块产出，CLI 模块传递）
// ============================================================================
/** @ingroup BuildMetadataGroup */
/** @{ */

/**
 * @brief 构建元数据
 *
 * 由 CMake 解析模块填充，分析引擎和报告生成器消费。
 * 包含从 CMakeLists.txt 提取的项目构建信息。
 */
struct BuildMetadata {
    std::string cmake_version;                                                    ///< CMake 最低版本要求
    std::string project_name;                                                     ///< 项目名称
    std::string c_compiler;                                                       ///< C 编译器路径
    std::string cxx_compiler;                                                     ///< C++ 编译器路径
    std::vector<std::string> targets;                                             ///< 构建目标名称列表
    std::unordered_map<std::string, std::vector<std::string>> target_link_libs;   ///< 目标 → 链接库列表
    std::vector<std::string> subdirectories;                                      ///< add_subdirectory 子目录路径列表
};

/** @} */ // end of BuildMetadataGroup

// ============================================================================
// AnalysisContext - 贯穿整个后端流程的统一数据容器
// ============================================================================
/** @ingroup AnalysisContextGroup */
/** @{ */

/**
 * @brief 统一分析上下文
 *
 * 贯穿整个后端流程的单一数据容器。
 * 包含符号表、分类型符号池、图边数据、命令行参数、项目元数据等所有运行时状态。
 * 所有模块通过引用此结构共享数据。
 */
struct AnalysisContext {
    /** @name 符号表（核心存储） */
    /** @{ */
    std::vector<Symbol> symbols;                               ///< 全局符号表
    std::unordered_map<std::string, uint32_t> symbol_name_to_id; ///< 完全限定名 → ID 映射
    /** @} */

    /** @name 分类型符号池
     *  存储索引而非完整对象，便于快速遍历。 */
    /** @{ */
    std::vector<FunctionSymbol> functions;   ///< 函数符号池
    std::vector<CompositeSymbol> composites; ///< 复合类型符号池
    std::vector<FileSymbol> files;           ///< 文件符号池
    /** @} */

    /** @name 图边数据 */
    /** @{ */
    std::vector<CallEdge> call_edges;          ///< 调用边（BFS 剪枝后）
    std::vector<IncludeEdge> include_edges;    ///< 包含边
    std::vector<TypeDependencyEdge> type_edges; ///< 类型依赖边
    std::vector<SymbolRef> references;         ///< 符号引用关系
    /** @} */

    /** @name 运行时参数 */
    /** @{ */
    std::string command_line;       ///< 原始命令行字符串（用于报告展示）
    uint32_t entry_function_id = 0; ///< 入口函数 Symbol ID（由 GraphBuilder 填充）
    /** @} */

    /**
     * @brief BFS 剪枝前的完整调用边
     *
     * 由 GraphBuilder 保存，在 BFS 展开前保留全量调用关系，
     * 供前端 JavaScript 按需展开/折叠节点时使用。
     */
    std::vector<CallEdge> full_call_edges;

    /** @name 外部与项目元数据 */
    /** @{ */
    std::vector<ExternalRef> external_refs;         ///< 外部符号引用列表
    std::string project_root;                       ///< 被分析项目的根目录
    std::vector<std::string> source_files;           ///< 所有被扫描的源文件路径
    std::unordered_map<std::string, CompileArgs> compile_params; ///< 文件 → 编译参数映射
    /** @} */

    /** @name 构建元数据（来自 CMake 解析模块） */
    /** @{ */
    std::string cmake_version;                                              ///< CMake 版本
    std::string c_compiler;                                                 ///< C 编译器
    std::string cxx_compiler;                                               ///< C++ 编译器
    std::vector<std::string> targets;                                       ///< 构建目标
    std::unordered_map<std::string, std::vector<std::string>> target_link_libs; ///< 目标链接库
    /** @} */
};

/**
 * @brief 命令行参数
 *
 * CLI 解析的结果，在分析流程的早期填充。
 * 注意：此结构与 CommandLineArgs 同名，在 CLI 模块内部定义使用。
 */
struct CommandLineArgs {
    std::string project_path;      ///< 被分析项目路径（-p, --project）
    std::string entry_function;    ///< 入口函数名（-e, --entry，默认 "main"）
    int expand_depth = 2;          ///< BFS 展开深度（-d, --depth，默认 2）
    std::string output_path;       ///< 输出 HTML 文件路径（-o, --output）
    bool verbose = false;          ///< 详细日志输出（-v, --verbose）
};

/** @} */ // end of AnalysisContextGroup

#endif // CODEVIZ_DATA_TYPES_H
