// Parser/ParserFrontend.h - C/C++ 解析前端
// 基于 tree-sitter 对单个源文件进行语法分析，提取原始符号信息
// 对应设计文档 4.3.4 节

#ifndef CODEVIZ_PARSER_FRONTEND_H
#define CODEVIZ_PARSER_FRONTEND_H

#include <string>
#include <vector>
#include "Common/DataTypes.h"

// 使用不透明指针避免在头文件中暴露 tree-sitter 类型
struct TSNode_opaque;

class ParserFrontend {
public:
    /**
     * 解析单个源文件
     * @param source 源文件内容及路径
     * @param args 编译参数（宏、头文件路径，当前版本保留以备后续扩展）
     * @return 该文件的原始符号和引用关系
     */
    FileParseResult parse_file(const SourceFile& source, const CompileArgs& args);

private:
    /**
     * 根据扩展名选择 tree-sitter-c 或 tree-sitter-cpp 并初始化
     */
    void init_parser(const std::string& file_ext);

    /**
     * 深度优先遍历 CST，维护作用域栈，分发到各子处理函数
     */
    void traverse_cst(void* node, FileParseResult& result,
                      const std::string& source, std::vector<std::string>& scope);

    /**
     * 处理函数定义，产出 RawSymbol(kind=FUNC)
     */
    void visit_function_definition(void* node, FileParseResult& result,
                                   const std::string& source, std::vector<std::string>& scope);

    /**
     * 提取函数签名（返回类型、参数列表、虚/静/内联标记）
     */
    void visit_function_declarator(void* node, RawSymbol& sym, const std::string& source);

    /**
     * 处理函数调用，记录 callee_name
     */
    void visit_call_expression(void* node, FileParseResult& result, const std::string& source,
                               RawSymbol* current_func);

    /**
     * 处理结构体定义
     */
    void visit_struct_specifier(void* node, FileParseResult& result,
                                const std::string& source, std::vector<std::string>& scope);

    /**
     * 处理类定义，含基类列表
     */
    void visit_class_specifier(void* node, FileParseResult& result,
                               const std::string& source, std::vector<std::string>& scope);

    /**
     * 提取成员字段信息（名称、类型、访问修饰符）
     */
    void visit_field_declaration(void* node, RawSymbol& sym, const std::string& source);

    /**
     * 处理宏定义，产出 RawSymbol(kind=MACRO)
     */
    void visit_preproc_def(void* node, FileParseResult& result, const std::string& source);

    /**
     * 处理 #include 指令，记录包含关系
     */
    void visit_preproc_include(void* node, FileParseResult& result, const std::string& source);

    /**
     * 提取节点对应的源码文本
     */
    std::string get_node_text(void* node, const std::string& source);

    /**
     * 拼接完全限定名
     */
    std::string get_qualified_name(const std::string& base, const std::vector<std::string>& scope);

    /**
     * 进入作用域
     */
    void enter_scope(std::vector<std::string>& scope, const std::string& name);

    /**
     * 退出作用域
     */
    void exit_scope(std::vector<std::string>& scope);

    bool is_cpp_ = false; // 当前文件是否为 C++
};

#endif // CODEVIZ_PARSER_FRONTEND_H
