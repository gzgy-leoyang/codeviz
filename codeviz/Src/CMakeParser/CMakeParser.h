/**
 * @file CMakeParser.h
 * @brief CMake 解析模块
 *
 * 使用 tree-sitter-cmake 解析 CMakeLists.txt 文件，
 * 提取项目名、编译工具链、构建目标、链接库依赖等信息。
 * 对应设计文档 4.3.2 节。
 */

#ifndef CODEVIZ_CMAKE_PARSER_H
#define CODEVIZ_CMAKE_PARSER_H

#include <string>
#include <tree_sitter/api.h>
#include "Common/DataTypes.h"

/**
 * @class CMakeParser
 * @brief CMakeLists.txt 解析器
 *
 * 使用 tree-sitter-cmake 解析 CMake 构建脚本，
 * 支持的命令：project、cmake_minimum_required、add_executable、
 * add_library、target_link_libraries、set（CMAKE_C/CXX_COMPILER）、
 * add_subdirectory。
 * 支持多次 parse() 调用累加同一 BuildMetadata 对象。
 */
class CMakeParser {
public:
    /**
     * @brief 解析单个 CMakeLists.txt 文件
     *
     * 创建 tree-sitter 解析器，解析文件内容，遍历 CST
     * 提取构建元数据。支持多次调用以累加子目录信息。
     *
     * @param file CMakeLists.txt 文件内容及路径
     * @param meta 构建元数据（输入输出参数，支持多文件累加）
     * @return 0 表示成功，-1 表示解析器创建失败或语言加载失败
     */
    int parse(const CMakeFile& file, BuildMetadata& meta);

private:
    /**
     * @brief 遍历 CST 根节点
     *
     * 递归遍历，遇到 normal_command 节点分发给 handle_normal_command。
     *
     * @param root CST 根节点
     * @param meta 构建元数据
     * @param source 源码字符串
     */
    void traverse_cst(TSNode root, BuildMetadata& meta, const std::string& source);

    /**
     * @brief 处理 normal_command 节点
     *
     * 提取命令名和参数列表，按命令名分发到各 visit_* 方法。
     *
     * @param node normal_command 节点
     * @param meta 构建元数据
     * @param source 源码字符串
     */
    void handle_normal_command(TSNode node, BuildMetadata& meta, const std::string& source);

    /**
     * @brief 提取项目名称
     *
     * @param arg_list argument_list 节点
     * @param meta 构建元数据
     * @param source 源码字符串
     */
    void visit_project(TSNode arg_list, BuildMetadata& meta, const std::string& source);

    /**
     * @brief 提取 cmake_minimum_required 版本
     *
     * 在参数列表中查找 "VERSION" 关键字，提取下一个参数作为版本号。
     *
     * @param arg_list argument_list 节点
     * @param meta 构建元数据
     * @param source 源码字符串
     */
    void visit_cmake_minimum_required(TSNode arg_list, BuildMetadata& meta, const std::string& source);

    /**
     * @brief 提取 add_executable 目标名
     *
     * @param arg_list argument_list 节点
     * @param meta 构建元数据
     * @param source 源码字符串
     */
    void visit_add_executable(TSNode arg_list, BuildMetadata& meta, const std::string& source);

    /**
     * @brief 提取 add_library 目标名
     *
     * @param arg_list argument_list 节点
     * @param meta 构建元数据
     * @param source 源码字符串
     */
    void visit_add_library(TSNode arg_list, BuildMetadata& meta, const std::string& source);

    /**
     * @brief 提取 target_link_libraries 的链接库列表
     *
     * 跳过 PRIVATE/PUBLIC/INTERFACE 关键字。
     *
     * @param arg_list argument_list 节点
     * @param meta 构建元数据
     * @param source 源码字符串
     */
    void visit_target_link_libraries(TSNode arg_list, BuildMetadata& meta, const std::string& source);

    /**
     * @brief 提取 CMAKE_C_COMPILER 和 CMAKE_CXX_COMPILER
     *
     * 从 set() 命令中识别编译器变量并提取值。
     *
     * @param arg_list argument_list 节点
     * @param meta 构建元数据
     * @param source 源码字符串
     */
    void visit_set_compiler(TSNode arg_list, BuildMetadata& meta, const std::string& source);

    /**
     * @brief 提取 add_subdirectory 的子目录路径
     *
     * @param arg_list argument_list 节点
     * @param meta 构建元数据
     * @param source 源码字符串
     */
    void visit_add_subdirectory(TSNode arg_list, BuildMetadata& meta, const std::string& source);

    /**
     * @brief 从源码中提取节点的文本内容
     *
     * @param node TSNode
     * @param source 源码字符串
     * @return 节点的源文本
     */
    std::string get_node_text(TSNode node, const std::string& source);

    bool parser_initialized_ = false; ///< 解析器初始化标志（当前保留未使用）
};

#endif // CODEVIZ_CMAKE_PARSER_H
