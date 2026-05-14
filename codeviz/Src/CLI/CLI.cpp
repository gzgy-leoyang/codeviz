// CLI/CLI.cpp - CLI 入口模块实现
// 解析参数、扫描源文件、调度各模块完成完整分析流程，写入 HTML 报告
// 对应设计文档 4.3.1 节

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

void init_logger(bool verbose) {
    auto console = spdlog::stdout_color_mt("codeviz");
    spdlog::set_default_logger(console);
    spdlog::set_level(verbose ? spdlog::level::debug : spdlog::level::info);
    spdlog::set_pattern("[%H:%M:%S %^%l%$] %v");
}

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
            // 转小写
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

void ensure_output_dir(const std::string& path) {
    fs::path p(path);
    if (p.has_parent_path() && !p.parent_path().empty()) {
        fs::create_directories(p.parent_path());
    }
}

std::string read_file_readonly(const std::string& path) {
    // 严格遵守需求 IR_3：以只读方式打开文件，不修改时间戳
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
