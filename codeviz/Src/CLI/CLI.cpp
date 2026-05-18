/**
 * @file CLI.cpp
 * @brief CLI 入口模块实现
 *
 * 解析命令行参数、校验输入、扫描源文件、调度各模块完成分析流程。
 * 通过 POSIX low-level I/O 保证只读访问（不修改文件时间戳）。
 * 对应设计文档 4.3.1 节。
 */

#include "CLI/CLI.h"
#include "CMakeParser/CMakeParser.h"
#include "CompDBParser/CompDBParser.h"
#include "Parser/ParserFrontend.h"
#include "Indexer/Indexer.h"
#include "GraphBuilder/GraphBuilder.h"
#include "Analyzer/Analyzer.h"
#include "Reporter/Reporter.h"

#include <CLI/CLI.hpp>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

namespace fs = std::filesystem;

// ============================================================================
// 辅助函数实现
// ============================================================================

/**
 * @brief 初始化 spdlog 日志器
 *
 * 创建彩色终端日志器，根据 verbose 标志设置日志级别。
 * 如果指定了详细日志，启用 debug 级别；否则使用 info 级别。
 *
 * @param verbose 是否启用详细日志输出
 */
void init_logger(bool verbose) {
    auto console = spdlog::stdout_color_mt("codeviz");
    spdlog::set_default_logger(console);
    spdlog::set_level(verbose ? spdlog::level::debug : spdlog::level::info);
    spdlog::set_pattern("[%H:%M:%S %^%l%$] %v");
}

/**
 * @brief 解析命令行参数
 *
 * 使用 CLI11 库定义参数接口。
 * 支持参数：
 *   -p, --project  待分析项目的根目录路径（必需）
 *   -e, --entry    调用图展开的入口函数名（默认 main）
 *   -d, --depth    调用图递归展开深度（默认 2，范围 1~20）
 *   -o, --output   输出的 HTML 报告文件路径（默认 <project_path>.html）
 *   -v, --verbose  启用详细日志输出
 *
 * @param argc 命令行参数个数
 * @param argv 命令行参数数组
 * @return 解析后的 CommandLineArgs 结构体
 */
CommandLineArgs parse_arguments(int argc, char* argv[]) {
    CommandLineArgs args;

    CLI::App app{R"(
codeviz - C/C++ 源码可视化分析工具
对 C/C++ 源码项目进行静态分析，生成交互式 HTML 可视化报告。
包含函数调用图、头文件包含图、类型依赖图及统计热力图。

用法: codeviz -p <project_path> [options])"};

    app.add_option("-p,--project", args.project_path,
                   "待分析项目的根目录路径（必需）")
       ->required()
       ->check(CLI::ExistingDirectory);

    app.add_option("-e,--entry", args.entry_function,
                   "调用图展开的入口函数名")
       ->default_val("main");

    app.add_option("-d,--depth", args.expand_depth,
                   "调用图递归展开深度（范围: 1~20）")
       ->default_val(2);

    app.add_option("-o,--output", args.output_path,
                   "输出的 HTML 报告文件路径（默认: <project_path>.html）");

    app.add_flag("-v,--verbose", args.verbose,
                 "启用详细日志输出（用于调试）");

    app.footer(R"(
示例:
  codeviz -p ./my_project              # 输出到 ./my_project.html
  codeviz -p ./my_project -o /tmp/r.html
  codeviz -p ./my_project -e main -d 3
  codeviz -p ./my_project -v)");

    try { app.parse(argc, argv); }
    catch (const CLI::ParseError& e) { exit(app.exit(e)); }

    return args;
}

/**
 * @brief 校验命令行参数的合法性
 *
 * 验证内容：
 *   - 项目路径存在且为目录
 *   - 展开深度在 1~20 的合法范围内
 *
 * @param args 待校验的 CommandLineArgs
 * @throw std::invalid_argument 任一校验不通过时抛出
 */
void validate_arguments(const CommandLineArgs& args) {
    if (!fs::exists(args.project_path)) {
        throw std::invalid_argument("项目路径不存在: " + args.project_path);
    }
    if (!fs::is_directory(args.project_path)) {
        throw std::invalid_argument("项目路径不是目录: " + args.project_path);
    }
    if (args.expand_depth < 1 || args.expand_depth > 20) {
        throw std::invalid_argument("展开深度必须在 1~20 之间，当前: " +
                                    std::to_string(args.expand_depth));
    }
    spdlog::info("参数校验通过: project={}, entry={}, depth={}",
                 args.project_path, args.entry_function, args.expand_depth);
}

/**
 * @brief 递归扫描目录收集所有 C/C++ 源文件
 *
 * 扫描 root 目录下所有子目录，收集扩展名为 .c/.cpp/.cxx/.cc/.h/.hpp/.hxx/.hh 的文件。
 * 跳过构建目录（/build/、/Build/、/cmake-build-/、/_deps/、/CMakeFiles/）
 * 和权限受限的目录。
 *
 * @param root 扫描起始的根目录
 * @return 找到的源文件绝对路径列表
 */
std::vector<std::string> scan_source_files(const std::string& root) {
    spdlog::info("扫描源文件: {}", root);

    std::vector<std::string> files;
    static const std::vector<std::string> EXTENSIONS = {
        ".c", ".cpp", ".cxx", ".cc", ".h", ".hpp", ".hxx", ".hh"
    };

    try {
        for (const auto& entry : fs::recursive_directory_iterator(
                root, fs::directory_options::skip_permission_denied)) {
            if (!entry.is_regular_file()) continue;

            // 跳过构建目录中的文件（避免 CMakeCompilerId 等干扰分析结果）
            std::string path = entry.path().string();
            static const std::vector<std::string> SKIP_DIRS = {
                "/build/", "/Build/", "/cmake-build-", "/_deps/", "/CMakeFiles/"
            };
            bool skip = false;
            for (const auto& d : SKIP_DIRS) {
                if (path.find(d) != std::string::npos) { skip = true; break; }
            }
            if (skip) continue;

            std::string ext = entry.path().extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            for (const auto& valid_ext : EXTENSIONS) {
                if (ext == valid_ext) {
                    files.push_back(entry.path().string());
                    break;
                }
            }
        }
    } catch (const fs::filesystem_error& e) {
        spdlog::warn("扫描源文件时遇到错误: {}", e.what());
    }

    spdlog::info("扫描完成: 共找到 {} 个源文件", files.size());
    return files;
}

/**
 * @brief 确保输出目录存在
 *
 * 检查输出路径的父目录是否存在，若不存在则递归创建。
 *
 * @param path 输出文件路径
 */
void ensure_output_dir(const std::string& path) {
    fs::path p(path);
    if (p.has_parent_path() && !p.parent_path().empty()) {
        fs::create_directories(p.parent_path());
    }
}

/**
 * @brief 以只读方式读取文件的全部内容
 *
 * 严格遵守需求 IR_3：使用 POSIX open(O_RDONLY) 以只读方式打开文件，
 * 不修改文件时间戳（不使用 std::ifstream 以避免 atime 变更问题）。
 * 通过 fstat 获取文件大小后一次性读取全部内容。
 *
 * @param path 要读取的文件路径
 * @return 文件内容字符串
 * @throw std::runtime_error 无法打开文件（EACCES, ENOENT 等）或读取失败时抛出
 */
std::string read_file_readonly(const std::string& path) {
    int fd = open(path.c_str(), O_RDONLY);
    if (fd == -1) {
        throw std::runtime_error("无法以只读方式打开文件: " + path);
    }

    struct stat st;
    if (fstat(fd, &st) == -1) {
        close(fd);
        throw std::runtime_error("无法获取文件信息: " + path);
    }

    std::string content(static_cast<size_t>(st.st_size), '\0');
    ssize_t bytes_read = read(fd, content.data(), static_cast<size_t>(st.st_size));
    close(fd);

    if (bytes_read < 0) {
        throw std::runtime_error("读取文件失败: " + path);
    }
    content.resize(static_cast<size_t>(bytes_read));

    return content;
}
