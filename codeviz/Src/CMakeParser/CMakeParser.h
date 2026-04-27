// CMakeParser/CMakeParser.h - CMake 解析模块
// 解析 CMakeLists.txt 文件，提取构建目标、链接库依赖和编译工具链信息
// 对应设计文档 4.3.2 节

#ifndef CODEVIZ_CMAKE_PARSER_H
#define CODEVIZ_CMAKE_PARSER_H

#include <string>
#include "Common/DataTypes.h"

class CMakeParser {
public:
    /**
     * 解析单个 CMakeLists.txt 文件，将提取的信息累积到 meta 中
     * @param file CMakeLists.txt 文件内容及路径
     * @param meta 构建元数据（输入输出参数，支持多文件累加）
     * @return 0 表示成功，-1 表示解析失败
     */
    int parse(const CMakeFile& file, BuildMetadata& meta);

private:
    /**
     * 初始化 tree-sitter-cmake 解析器（若未初始化）
     */
    void init_parser();

    /**
     * 遍历 CST 根节点，分发到各子处理函数
     */
    void traverse_cst(void* root, BuildMetadata& meta, const std::string& source);

    /**
     * 提取项目名称和版本
     */
    void visit_project(void* node, BuildMetadata& meta, const std::string& source);

    /**
     * 提取可执行目标名
     */
    void visit_add_executable(void* node, BuildMetadata& meta, const std::string& source);

    /**
     * 提取库目标名
     */
    void visit_add_library(void* node, BuildMetadata& meta, const std::string& source);

    /**
     * 提取目标及其链接库列表
     */
    void visit_target_link_libraries(void* node, BuildMetadata& meta, const std::string& source);

    /**
     * 提取 CMAKE_C_COMPILER / CMAKE_CXX_COMPILER
     */
    void visit_set_compiler(void* node, BuildMetadata& meta, const std::string& source);

    /**
     * 从源码中提取节点文本
     */
    std::string get_node_text(void* node, const std::string& source);

    /**
     * 降级：使用简单文本解析 CMakeLists.txt（tree-sitter 不可用时备用）
     */
    void parse_with_regex(const CMakeFile& file, BuildMetadata& meta);

    bool parser_initialized_ = false;
};

#endif // CODEVIZ_CMAKE_PARSER_H
