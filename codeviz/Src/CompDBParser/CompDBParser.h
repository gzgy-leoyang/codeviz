/**
 * @file CompDBParser.h
 * @brief 编译数据库解析模块
 *
 * 读取 CMake 生成的 compile_commands.json 文件，
 * 为每个源文件提取编译参数（宏定义、头文件路径、其他编译选项）。
 * 对应设计文档 4.3.3 节。
 */

#ifndef CODEVIZ_COMP_DB_PARSER_H
#define CODEVIZ_COMP_DB_PARSER_H

#include <string>
#include <unordered_map>
#include <vector>
#include "Common/DataTypes.h"
#include <nlohmann/json.hpp>

/**
 * @class CompDBParser
 * @brief compile_commands.json 解析器
 *
 * 解析 CMake 等构建工具生成的编译数据库 JSON 文件，
 * 提取每个源文件的编译参数：-D 宏定义、-I 头文件路径。
 * 支持 arguments 数组和 command 字符串两种格式。
 */
class CompDBParser {
public:
    /**
     * @brief 解析编译数据库
     *
     * 定位 compile_commands.json → 解析 JSON 数组 →
     * 逐条目提取编译参数 → 返回文件路径到参数的映射。
     *
     * @param build_dir 包含 compile_commands.json 的构建目录
     * @return 文件绝对路径 → CompileArgs 映射表
     */
    std::unordered_map<std::string, CompileArgs> parse(const std::string& build_dir);

private:
    /**
     * @brief 定位 compile_commands.json 文件路径
     *
     * 检查 build_dir + "/compile_commands.json" 是否存在。
     *
     * @param build_dir 构建目录路径
     * @return JSON 文件完整路径，不存在返回空字符串
     */
    std::string find_compile_commands(const std::string& build_dir);

    /**
     * @brief 解析单个编译条目
     *
     * 提取 file、directory 字段，优先使用 arguments 数组，
     * 否则解析 command 字符串。提取 -D/-I 参数后路径标准化。
     *
     * @param entry JSON 对象
     * @return CompileArgs 结构（file_path 为空表示无效条目）
     */
    CompileArgs parse_entry(const nlohmann::json& entry);

    /**
     * @brief 从命令字符串中提取 -D 宏定义
     *
     * 支持 -DFOO、-DFOO=BAR 和 -D FOO 两种格式。
     *
     * @param cmd 完整的编译命令字符串
     * @return 宏定义列表（不含 -D 前缀）
     */
    std::vector<std::string> extract_defines(const std::string& cmd);

    /**
     * @brief 从命令字符串中提取 -I 头文件搜索路径
     *
     * 支持 -I/path 和 -I /path 两种格式。
     *
     * @param cmd 完整的编译命令字符串
     * @return 头文件搜索路径列表（不含 -I 前缀）
     */
    std::vector<std::string> extract_includes(const std::string& cmd);

    /**
     * @brief 将路径转换为绝对路径
     *
     * 若已经是绝对路径（以 / 开头）则直接返回，
     * 否则与 base_dir 拼接。
     *
     * @param path 原始路径
     * @param base_dir 基准目录
     * @return 标准化的绝对路径
     */
    std::string normalize_path(const std::string& path, const std::string& base_dir);
};

#endif // CODEVIZ_COMP_DB_PARSER_H
