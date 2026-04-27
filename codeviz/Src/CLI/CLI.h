// CLI/CLI.h - CLI 入口模块
// 解析命令行参数，校验输入，调度各模块完成分析流程
// 对应设计文档 4.3.1 节

#ifndef CODEVIZ_CLI_H
#define CODEVIZ_CLI_H

#include <string>
#include <vector>
#include "Common/DataTypes.h"

/**
 * 解析命令行参数
 */
CommandLineArgs parse_arguments(int argc, char* argv[]);

/**
 * 校验参数合法性（路径存在、深度有效等）
 */
void validate_arguments(const CommandLineArgs& args);

/**
 * 初始化 spdlog 日志等级
 */
void init_logger(bool verbose);

/**
 * 递归扫描目录，返回所有 C/C++ 源文件路径（只读访问）
 */
std::vector<std::string> scan_source_files(const std::string& root);

/**
 * 确保输出目录存在（若不存在则创建）
 */
void ensure_output_dir(const std::string& path);

/**
 * 以只读方式读取文件内容
 */
std::string read_file_readonly(const std::string& path);

#endif // CODEVIZ_CLI_H
