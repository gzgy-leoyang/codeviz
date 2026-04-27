// CompDBParser/CompDBParser.h - 编译数据库解析模块
// 读取 CMake 生成的 compile_commands.json，为每个源文件提取编译参数
// 对应设计文档 4.3.3 节

#ifndef CODEVIZ_COMP_DB_PARSER_H
#define CODEVIZ_COMP_DB_PARSER_H

#include <string>
#include <unordered_map>
#include <vector>
#include "Common/DataTypes.h"
#include <nlohmann/json.hpp>

class CompDBParser {
public:
    /**
     * 解析编译数据库
     * @param build_dir 包含 compile_commands.json 的构建目录路径
     * @return 文件绝对路径到编译参数的映射表，若文件不存在则返回空映射
     */
    std::unordered_map<std::string, CompileArgs> parse(const std::string& build_dir);

private:
    /**
     * 定位 compile_commands.json 文件路径
     */
    std::string find_compile_commands(const std::string& build_dir);

    /**
     * 解析单个编译条目，提取宏、头文件路径、其他选项
     */
    CompileArgs parse_entry(const nlohmann::json& entry);

    /**
     * 从命令字符串中提取 -D 宏定义
     */
    std::vector<std::string> extract_defines(const std::string& cmd);

    /**
     * 从命令字符串中提取 -I 头文件路径
     */
    std::vector<std::string> extract_includes(const std::string& cmd);

    /**
     * 将相对路径转换为基于 base_dir 的绝对路径
     */
    std::string normalize_path(const std::string& path, const std::string& base_dir);
};

#endif // CODEVIZ_COMP_DB_PARSER_H
