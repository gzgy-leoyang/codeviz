// Common/DataTypes.h - 核心数据结构与接口数据结构定义（纯头文件）
// 对应设计文档 4.2.1 节（接口数据结构）和 4.2.2 节（核心数据结构）
// 所有模块共用，不允许自行发明新结构

#ifndef CODEVIZ_DATA_TYPES_H
#define CODEVIZ_DATA_TYPES_H

#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>

// ============================================================================
// 4.2.2 核心数据结构
// ============================================================================

// 符号类型枚举（从 RawSymbol::Kind 抽象并扩展）
enum class SymbolKind {
    FUNCTION,       // 函数（含成员函数）
    STRUCT,         // 结构体
    CLASS,          // 类
    ENUM,           // 枚举
    VARIABLE,       // 全局变量
    MACRO,          // 宏定义
    FILE_ENTITY     // 文件实体（用于包含图）
};

// 访问修饰符（C++ 专用，从 CST 的 access_specifier 节点提取）
enum class AccessSpecifier {
    PUBLIC,
    PROTECTED,
    PRIVATE,
    NONE           // C 语言或全局符号
};

// 全局符号表的基础存储单元
struct Symbol {
    uint32_t id = 0;                    // 唯一标识符（由 Indexer 自增分配）
    std::string name;                   // 符号短名称（如 "main"）
    std::string qualified_name;         // 完全限定名（如 "namespace::Class::method"）
    std::string file_path;              // 定义所在文件的绝对路径
    uint32_t line_start = 0;            // 定义起始行号
    uint32_t line_end = 0;              // 定义结束行号
    SymbolKind kind = SymbolKind::FUNCTION; // 符号类型
    AccessSpecifier access = AccessSpecifier::NONE; // 访问修饰符
    std::vector<uint32_t> references;   // 引用该符号的 SymbolRef 索引（由 Indexer 填充）
};

// 成员字段信息（对应需求 2.2.3.3 DR_3：成员名、类型、偏移、长度）
struct FieldInfo {
    std::string name;                   // 成员名称
    std::string type;                   // 成员类型字符串
    uint32_t offset = 0;               // 字节偏移（若可获取）
    size_t size = 0;                    // 成员大小（字节）
    AccessSpecifier access = AccessSpecifier::NONE; // 访问修饰符
};

// 从 RawSymbol 中提取函数特有信息，并由 Analyzer 补充统计字段
struct FunctionSymbol {
    uint32_t symbol_id = 0;             // 关联到 Symbol::id
    std::string return_type;            // 返回值类型字符串
    std::vector<std::string> parameters; // 参数类型列表
    std::string comment;                // 声明/定义前的注释文档
    bool is_virtual = false;            // 虚函数标记
    bool is_static = false;             // 静态函数标记
    bool is_inline = false;             // 内联函数标记
    int cyclomatic_complexity = 0;      // 圈复杂度（由 Analyzer 计算）
    int branch_count = 0;               // 分支节点数（ParserFrontend 统计，Analyzer 消费）
    int fan_in = 0;                     // 被调用次数（由 Analyzer 计算）
    int fan_out = 0;                    // 调用其他函数数量（由 Analyzer 计算）
    std::vector<uint32_t> callees;      // 调用的函数 Symbol ID 列表
};

// 结构体/类符号
struct CompositeSymbol {
    uint32_t symbol_id = 0;             // 关联到 Symbol::id
    std::vector<FieldInfo> fields;      // 成员字段列表
    std::vector<uint32_t> methods;      // 成员函数 Symbol ID 列表
    std::vector<uint32_t> base_classes; // 基类 Symbol ID 列表
    std::string comment;               // 声明前的注释文档
    bool is_pod = false;                // 是否为平凡类型
    size_t total_size = 0;              // 类型总大小（字节）
};

// 用于包含图构建和文件级统计
struct FileSymbol {
    uint32_t symbol_id = 0;             // 关联到 Symbol::id
    int total_lines = 0;                // 总行数
    int code_lines = 0;                 // 有效代码行数
    int comment_lines = 0;              // 注释行数
    std::vector<uint32_t> includes;     // 包含的头文件 Symbol ID 列表
    std::vector<uint32_t> symbols;      // 本文件内定义的符号 ID 列表
};

// 调用边（从 SymbolRef 中 relation == CALLS 抽象）
struct CallEdge {
    uint32_t caller_id = 0;             // 调用者 Symbol ID
    uint32_t callee_id = 0;             // 被调用者 Symbol ID
    uint32_t call_count = 0;            // 调用次数（静态计数）
    uint32_t line = 0;                  // 调用发生的行号
    std::string file_path;              // 调用发生的文件
};

// 包含边（从 FileParseResult::includes 抽象）
struct IncludeEdge {
    uint32_t includer_id = 0;           // 包含者文件 Symbol ID
    uint32_t includee_id = 0;           // 被包含头文件 Symbol ID
    uint32_t line = 0;                  // #include 所在行号
    bool is_system = false;             // 是否为系统头文件（<> 形式）
};

// 类型依赖边（用于 CompositeSymbol 之间的关系）
struct TypeDependencyEdge {
    uint32_t source_id = 0;             // 源类型 Symbol ID
    uint32_t target_id = 0;             // 目标类型 Symbol ID
    enum Relation { CONTAINS, INHERITS, PARAMETER, RETURN } relation = CONTAINS;
};

// ============================================================================
// 4.2.1 接口数据结构
// ============================================================================

// 1. CLI 到 C/C++ 解析器
struct SourceFile {
    std::string file_path;
    std::string content;
};

// 2. CLI 到 CMake 解析器
struct CMakeFile {
    std::string file_path;     // 用于定位和错误报告
    std::string content;       // 解析器的输入
    std::string source_dir;    // 解析相对路径的基准目录
};

// 3. 编译数据库到 C/C++ 解析器
struct CompileArgs {
    std::string file_path;              // 关联到具体源文件
    std::vector<std::string> defines;   // -D 宏定义
    std::vector<std::string> includes;  // -I 头文件路径
    std::vector<std::string> flags;     // 其他编译选项
};

// 4. C/C++ 解析到符号索引模块
struct RawSymbol {
    std::string name;                   // 符号名称（可能有重名）
    std::string file_path;              // 定义位置
    uint32_t line_start = 0, line_end = 0; // 行号范围
    enum Kind { FUNC, STRUCT, CLASS, ENUM_KIND, VAR, MACRO } kind = FUNC;
    std::vector<std::string> callee_names; // 调用的函数名（未解析为 ID）
    // 函数扩展信息
    std::string return_type;
    std::vector<std::string> parameters;
    std::string comment;            // 声明/定义前的注释文档
    bool is_virtual = false;
    bool is_static = false;
    bool is_inline = false;
    int branch_count = 0;       // 分支节点数（由 ParserFrontend 统计）
    // 复合类型扩展信息
    std::vector<FieldInfo> fields;
    std::vector<uint32_t> method_symbol_ids; // 暂存，由 Indexer 解析
    std::vector<std::string> base_class_names; // 基类名称（未解析为 ID）
    AccessSpecifier access = AccessSpecifier::NONE;
};

struct FileParseResult {
    std::string file_path;
    std::vector<RawSymbol> symbols;
    std::vector<std::pair<std::string, std::string>> includes; // (includer, includee)
    int total_lines = 0;      // 文件总行数
    int code_lines = 0;       // 有效代码行数
    int comment_lines = 0;    // 注释行数
};

// 5. 符号索引模块到图构建模块
struct SymbolRef {
    uint32_t from_symbol_id = 0;   // 引用者
    uint32_t to_symbol_id = 0;     // 被引用者
    uint32_t line = 0;             // 引用位置（用于 UI 跳转）
    std::string file_path;         // 引用发生的文件
};

struct IndexedData {
    std::vector<Symbol> symbols;      // 全局符号表（复用核心数据结构）
    std::vector<SymbolRef> references; // 所有引用关系
};

// 6. 图构建模块到分析引擎
struct GraphNode {
    uint32_t id = 0;           // 对应 Symbol ID 或 File ID
    std::string label;
    enum Type { FUNCTION, FILE_ENTITY, STRUCT } type = FUNCTION;
};

struct GraphEdge {
    uint32_t source_id = 0;
    uint32_t target_id = 0;
    enum Relation { CALLS, INCLUDES, CONTAINS, INHERITS } relation = CALLS;
    uint32_t weight = 0;       // 调用次数或包含次数（用于热力计算）
};

struct GraphData {
    std::vector<GraphNode> nodes;
    std::vector<GraphEdge> edges;
};

// 7. 符号索引模块到 HTML 报告生成器
struct SymbolMetadata {
    uint32_t symbol_id = 0;
    std::string name;
    std::string qualified_name;
    std::string file_path;
    uint32_t line = 0;
    SymbolKind kind = SymbolKind::FUNCTION;
    int complexity = 0;   // 待 Analyzer 填充后回填或二次传递
    int fan_in = 0;
    int fan_out = 0;
};

// 8. 分析引擎到 HTML 报告生成器
struct FileStats {
    std::string file_path;
    int total_lines = 0;
    int code_lines = 0;
    double complexity_sum = 0.0;   // 该文件所有函数的复杂度总和
};

struct FunctionStats {
    uint32_t function_id = 0;
    int fan_in = 0;
    int fan_out = 0;
    int cyclomatic_complexity = 0;
};

struct CircularInclude {
    std::vector<std::string> file_cycle;
};

struct AnalysisStats {
    std::vector<FileStats> file_stats;
    std::vector<FunctionStats> function_stats;
    std::vector<CircularInclude> circular_includes;
};

/// 外部符号引用（来自未在本项目中定义的函数/变量）
struct ExternalRef {
    std::string caller_name;   // 调用者符号名
    std::string callee_name;   // 被调用者符号名（外部）
    std::string library;       // 推测的库名
};

// 9. HTML 报告生成器到文件系统
struct HTMLReport {
    std::string content;     // 完整 HTML 字符串
    std::string output_path; // 建议的输出路径
};

// ============================================================================
// 附加数据结构（由 CMake 解析模块产出，CLI 模块传递）
// ============================================================================

// 构建元数据（由 CMake 解析模块填充，分析引擎和报告生成器消费）
struct BuildMetadata {
    std::string cmake_version;
    std::string project_name;
    std::string c_compiler;
    std::string cxx_compiler;
    std::vector<std::string> targets;                              // 目标名称列表
    std::unordered_map<std::string, std::vector<std::string>> target_link_libs; // 目标→链接库列表
    std::vector<std::string> subdirectories;                       // add_subdirectory 路径
};

// ============================================================================
// AnalysisContext - 贯穿整个后端流程的统一数据容器
// ============================================================================

struct AnalysisContext {
    // 符号表（核心存储）
    std::vector<Symbol> symbols;
    std::unordered_map<std::string, uint32_t> symbol_name_to_id; // 完全限定名 → ID

    // 分类型符号池（便于快速遍历，存储索引而非完整对象）
    std::vector<FunctionSymbol> functions;
    std::vector<CompositeSymbol> composites;
    std::vector<FileSymbol> files;

    // 图边数据
    std::vector<CallEdge> call_edges;
    std::vector<IncludeEdge> include_edges;
    std::vector<TypeDependencyEdge> type_edges;
    std::vector<SymbolRef> references;

    // 命令行参数（用于报告展示）
    std::string command_line;

    // 入口函数 Symbol ID（由 GraphBuilder 填充，Reporter 序列化到前端）
    uint32_t entry_function_id = 0;

    // BFS 剪枝前的完整调用边（由 GraphBuilder 保存，供前端按需展开）
    std::vector<CallEdge> full_call_edges;

    // 外部符号引用
    std::vector<ExternalRef> external_refs;

    // 项目元数据
    std::string project_root;
    std::vector<std::string> source_files;
    std::unordered_map<std::string, CompileArgs> compile_params; // 文件 → 编译参数

    // 构建元数据（来自 CMake 解析模块）
    std::string cmake_version;
    std::string c_compiler;
    std::string cxx_compiler;
    std::vector<std::string> targets;
    std::unordered_map<std::string, std::vector<std::string>> target_link_libs;
};

// CommandLineArgs - CLI 模块内部定义
struct CommandLineArgs {
    std::string project_path;      // -p, --project  被分析项目路径
    std::string entry_function;    // -e, --entry    入口函数名（默认 "main"）
    int expand_depth = 2;          // -d, --depth    展开深度（默认 2）
    std::string output_path;       // -o, --output   输出 HTML 路径
    bool verbose = false;          // -v, --verbose  详细日志
};

#endif // CODEVIZ_DATA_TYPES_H
