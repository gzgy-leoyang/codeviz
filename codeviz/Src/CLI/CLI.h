/**
 * @file CLI.h
 * @brief CLI 入口模块
 *
 * 解析命令行参数，校验输入，扫描源文件，调度各模块完成分析流程。
 * 对应设计文档 4.3.1 节。
 */

#ifndef CODEVIZ_CLI_H
#define CODEVIZ_CLI_H

#include <string>
#include <vector>
#include "Common/DataTypes.h"

/**
 * @brief 解析命令行参数
 *
 * 使用 CLI11 库解析 -p/-e/-d/-o/-v 等参数。
 *
 * @param argc 命令行参数个数
 * @param argv 命令行参数数组
 * @return 解析后的命令行参数结构体
 */
CommandLineArgs parse_arguments(int argc, char* argv[]);

/**
 * @brief 校验参数合法性
 *
 * 检查项目路径是否存在且为目录，展开深度在 1~20 范围内。
 *
 * @param args 待校验的命令行参数
 * @throw std::invalid_argument 参数不合法时抛出
 */
void validate_arguments(const CommandLineArgs& args);

/**
 * @brief 初始化 spdlog 日志等级
 *
 * @param verbose 是否启用详细日志（debug 级别）
 */
void init_logger(bool verbose);

/**
 * @brief 递归扫描目录，收集所有 C/C++ 源文件路径
 *
 * 跳过构建目录（build/、CMakeFiles/ 等）和权限受限的目录。
 *
 * @param root 待扫描的根目录路径
 * @return C/C++ 源文件路径列表
 */
std::vector<std::string> scan_source_files(const std::string& root);

/**
 * @brief 确保输出目录存在
 *
 * 若输出路径的父目录不存在则自动创建。
 *
 * @param path 输出文件路径
 */
void ensure_output_dir(const std::string& path);

/**
 * @brief 以只读方式读取文件内容
 *
 * 使用 POSIX open(O_RDONLY) 保证不修改文件时间戳。
 * 满足需求 IR_3：只读访问，不产生副作用。
 *
 * @param path 文件路径
 * @return 文件内容字符串
 * @throw std::runtime_error 文件无法打开或读取失败时抛出
 */
std::string read_file_readonly(const std::string& path);

#endif // CODEVIZ_CLI_H
