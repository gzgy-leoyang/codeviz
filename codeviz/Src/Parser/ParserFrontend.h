/**
 * @file ParserFrontend.h
 * @brief C/C++ 解析前端
 *
 * 基于 tree-sitter 对单个源文件进行 CST 语法分析，
 * 提取函数、类型、宏、调用、包含等原始符号信息。
 * 对应设计文档 4.3.4 节。
 */

#ifndef CODEVIZ_PARSER_FRONTEND_H
#define CODEVIZ_PARSER_FRONTEND_H

#include <string>
#include <vector>
#include <tree_sitter/api.h>
#include "Common/DataTypes.h"

/**
 * @class ParserFrontend
 * @brief C/C++ 源文件解析器
 *
 * 使用 tree-sitter 解析器对单个 C/C++ 源文件进行 CST 遍历，
 * 提取所有函数定义、函数调用、结构体/类定义、宏定义和 #include 指令。
 * 每个 visit_* 系列方法对应一种 CST 节点的处理逻辑。
 */
class ParserFrontend {
public:
    /**
     * @brief 解析单个源文件
     *
     * 根据文件扩展名选择 tree-sitter-c 或 tree-sitter-cpp 语言，
     * 创建解析器解析字符串，然后深度优先遍历 CST
     * 提取符号和包含关系。
     *
     * @param source 源文件内容及路径
     * @param args 编译参数（宏、头文件路径，当前版本保留以备后续扩展）
     * @return 该文件的解析结果（原始符号、包含关系、行数统计）
     */
    FileParseResult parse_file(const SourceFile& source, const CompileArgs& args);

private:
    /**
     * @brief 初始化解析器
     *
     * 根据文件扩展名选择语言。目前仅保留接口，实际解析器在 parse_file 中每次新建。
     *
     * @param file_ext 文件扩展名
     */
    void init_parser(const std::string& file_ext);

    /**
     * @brief 深度优先遍历 CST
     *
     * 根据节点类型分发到各 visit_* 处理函数。
     * 维护作用域栈和 current_func 指针以关联调用者。
     *
     * @param node 当前 CST 节点
     * @param result 解析结果（输出参数）
     * @param source 源码字符串
     * @param scope 当前作用域栈
     * @param current_func 当前正在遍历的函数指针（用于收集 callee 和分支计数）
     */
    void traverse_cst(TSNode node, FileParseResult& result,
                      const std::string& source, std::vector<std::string>& scope,
                      RawSymbol* current_func);

    /**
     * @brief 处理函数定义节点
     *
     * 提取函数名、返回类型、参数列表、修饰符，
     * 进入函数体遍历以收集调用关系和分支节点数。
     *
     * @param node function_definition 节点
     * @param result 解析结果
     * @param source 源码字符串
     * @param scope 作用域栈（入栈函数名，出栈恢复）
     */
    void visit_function_definition(TSNode node, FileParseResult& result,
                                   const std::string& source,
                                   std::vector<std::string>& scope);

    /**
     * @brief 提取函数签名信息
     *
     * 从 function_declarator 节点提取参数列表和修饰符。
     *
     * @param node function_declarator 节点
     * @param sym 目标原始符号
     * @param source 源码字符串
     */
    void visit_function_declarator(TSNode node, RawSymbol& sym,
                                    const std::string& source);

    /**
     * @brief 处理函数调用表达式
     *
     * 提取被调用函数名（去除空白字符），记录到 current_func 的 callee_names。
     *
     * @param node call_expression 节点
     * @param result 解析结果
     * @param source 源码字符串
     * @param current_func 当前函数指针
     */
    void visit_call_expression(TSNode node, FileParseResult& result,
                                const std::string& source,
                                RawSymbol* current_func);

    /**
     * @brief 处理结构体定义
     *
     * 提取结构体名称，关联前导注释，递归 scan 成员字段。
     * 支持匿名结构体识别。
     *
     * @param node struct_specifier 节点
     * @param result 解析结果
     * @param source 源码字符串
     * @param scope 作用域栈
     */
    void visit_struct_specifier(TSNode node, FileParseResult& result,
                                 const std::string& source,
                                 std::vector<std::string>& scope);

    /**
     * @brief 处理类定义
     *
     * 提取类名、基类列表，递归 scan 成员字段和嵌套类。
     *
     * @param node class_specifier 节点
     * @param result 解析结果
     * @param source 源码字符串
     * @param scope 作用域栈
     */
    void visit_class_specifier(TSNode node, FileParseResult& result,
                                const std::string& source,
                                std::vector<std::string>& scope);

    /**
     * @brief 提取成员字段信息
     *
     * 从 field_declaration 节点提取字段的类型和名称。
     * 支持多字段声明（如 int a, b, c;）。
     *
     * @param node field_declaration 节点
     * @param sym 目标原始符号（STRUCT 或 CLASS）
     * @param source 源码字符串
     */
    void visit_field_declaration(TSNode node, RawSymbol& sym,
                                  const std::string& source);

    /**
     * @brief 处理宏定义
     *
     * 提取宏名称并创建 MACRO 类型的原始符号。
     *
     * @param node preproc_def 或 preproc_function_def 节点
     * @param result 解析结果
     * @param source 源码字符串
     */
    void visit_preproc_def(TSNode node, FileParseResult& result,
                            const std::string& source);

    /**
     * @brief 处理 #include 指令
     *
     * 提取包含路径，去除引号或尖括号，记录包含关系对。
     *
     * @param node preproc_include 节点
     * @param result 解析结果
     * @param source 源码字符串
     */
    void visit_preproc_include(TSNode node, FileParseResult& result,
                                const std::string& source);

    /**
     * @brief 获取节点的源码文本
     *
     * @param node TSNode
     * @param source 源码字符串
     * @return 节点对应的源文本
     */
    std::string get_node_text(TSNode node, const std::string& source);

    /**
     * @brief 构造完全限定名
     *
     * 将当前名称与作用域栈拼接为 "scope1::scope2::name" 格式。
     *
     * @param base 基础名称
     * @param scope 作用域栈
     * @return 完全限定名
     */
    std::string get_qualified_name(const std::string& base,
                                    const std::vector<std::string>& scope);

    /**
     * @brief 进入新作用域
     *
     * @param scope 作用域栈
     * @param name 要压入的作用域名
     */
    void enter_scope(std::vector<std::string>& scope, const std::string& name);

    /**
     * @brief 退出当前作用域
     *
     * @param scope 作用域栈
     */
    void exit_scope(std::vector<std::string>& scope);

    bool is_cpp_ = false;                                       ///< 是否为 C++ 文件
    AccessSpecifier current_access_ = AccessSpecifier::NONE;    ///< 当前类体内的访问修饰符
    size_t current_composite_idx_ = SIZE_MAX;                   ///< 当前结构体/类在 symbols 中的索引
    std::string last_comment_;                                  ///< 上一个 Doxygen 注释节点内容
    uint32_t last_comment_line_ = 0;                            ///< comment 结束行号（用于判断关联性）
};

#endif // CODEVIZ_PARSER_FRONTEND_H
