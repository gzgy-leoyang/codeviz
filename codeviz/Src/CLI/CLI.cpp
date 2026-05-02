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
                   "输出的 HTML 报告文件路径")
       ->default_val("report.html");

    app.add_flag("-v,--verbose", args.verbose,
                 "启用详细日志输出（用于调试）");

    app.footer(R"(
示例:
  codeviz -p ./my_project
  codeviz -p ./my_project -e main -d 3 -o report.html
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

// ============================================================================
// 主函数
// ============================================================================

int main(int argc, char* argv[]) {
    // 1. 初始化日志（先用默认级别）
    init_logger(false);

    // 2. 解析命令行参数
    CommandLineArgs args;
    try {
        args = parse_arguments(argc, argv);
    } catch (const std::exception& e) {
        std::cerr << "[ERROR] 参数解析失败: " << e.what() << std::endl;
        return 1;
    }

    // 重新初始化日志（应用 verbose 设置）
    spdlog::drop_all();
    init_logger(args.verbose);

    spdlog::info("codeviz v1.0 启动");
    spdlog::info("分析目标: {}", args.project_path);

    // 3. 校验参数
    try {
        validate_arguments(args);
    } catch (const std::exception& e) {
        spdlog::error("参数校验失败: {}", e.what());
        return 1;
    }

    // 4. 扫描源文件（只读）
    auto source_file_paths = scan_source_files(args.project_path);
    if (source_file_paths.empty()) {
        spdlog::warn("未找到任何 C/C++ 源文件");
    }

    // ---- 构建元数据 ----
    BuildMetadata build_meta;

    // 5. 若存在 CMakeLists.txt，调用 CMakeParser 解析
    std::string cmake_path = args.project_path + "/CMakeLists.txt";
    if (fs::exists(cmake_path)) {
        spdlog::info("发现 CMakeLists.txt，开始解析");
        try {
            std::string cmake_content = read_file_readonly(cmake_path);
            CMakeFile cmake_file{cmake_path, cmake_content, args.project_path};
            CMakeParser cmake_parser;
            cmake_parser.parse(cmake_file, build_meta);

            // 递归处理 add_subdirectory
            for (const auto& subdir : build_meta.subdirectories) {
                std::string sub_cmake = args.project_path + "/" + subdir + "/CMakeLists.txt";
                if (fs::exists(sub_cmake)) {
                    try {
                        std::string sub_content = read_file_readonly(sub_cmake);
                        CMakeFile sub_file{sub_cmake, sub_content, args.project_path + "/" + subdir};
                        cmake_parser.parse(sub_file, build_meta);
                    } catch (const std::exception& e) {
                        spdlog::warn("子目录 CMake 解析失败: {}", e.what());
                    }
                }
            }
        } catch (const std::exception& e) {
            spdlog::warn("CMake 解析失败，跳过: {}", e.what());
        }
    }

    // 6. 若存在 compile_commands.json，调用 CompDBParser 解析
    std::unordered_map<std::string, CompileArgs> compile_db;
    // 搜索常见构建目录
    for (const auto& build_dir : {"build", "Build", "cmake-build-debug", "cmake-build-release"}) {
        std::string bd = args.project_path + "/" + build_dir;
        if (fs::exists(bd + "/compile_commands.json")) {
            spdlog::info("发现编译数据库: {}", bd);
            try {
                CompDBParser compdb_parser;
                compile_db = compdb_parser.parse(bd);
            } catch (const std::exception& e) {
                spdlog::warn("编译数据库解析失败: {}", e.what());
            }
            break;
        }
    }

    // 7. 对每个源文件调用 ParserFrontend::parse_file（只读）
    std::vector<FileParseResult> parse_results;
    ParserFrontend parser;
    static const CompileArgs empty_args;

    for (const auto& file_path : source_file_paths) {
        try {
            std::string content = read_file_readonly(file_path);
            SourceFile src{file_path, content};

            // 获取编译参数（若有）
            const CompileArgs& cargs = [&]() -> const CompileArgs& {
                auto it = compile_db.find(file_path);
                if (it != compile_db.end()) return it->second;
                return empty_args;
            }();

            auto result = parser.parse_file(src, cargs);
            parse_results.push_back(std::move(result));
        } catch (const std::exception& e) {
            spdlog::warn("跳过文件 {}: {}", file_path, e.what());
        }
    }

    // 8. 调用 Indexer::build_index 构建符号表
    spdlog::info("构建符号索引");
    Indexer indexer;
    AnalysisContext ctx;
    try {
        ctx = indexer.build_index(parse_results);
        ctx.project_root = build_meta.project_name.empty() ? args.project_path : build_meta.project_name;
        ctx.c_compiler = build_meta.c_compiler;
        ctx.cxx_compiler = build_meta.cxx_compiler;
        ctx.compile_params = compile_db;
    } catch (const std::exception& e) {
        spdlog::error("符号索引构建失败: {}", e.what());
        return 1;
    }

    // 9. 调用 GraphBuilder::build 构建图数据
    spdlog::info("构建图数据");
    GraphBuilder graph_builder;
    try {
        graph_builder.build(ctx, args.entry_function, args.expand_depth);
    } catch (const std::exception& e) {
        spdlog::warn("图构建失败（降级处理）: {}", e.what());
    }

    // 10. 调用 Analyzer::analyze 执行统计分析
    spdlog::info("执行统计分析");
    Analyzer analyzer;
    AnalysisStats stats;
    try {
        stats = analyzer.analyze(ctx, build_meta);
    } catch (const std::exception& e) {
        spdlog::warn("统计分析失败（降级处理）: {}", e.what());
    }

    // 11. 导出符号元数据
    auto symbol_metadata = indexer.export_metadata(ctx);

    // 12. 调用 Reporter::generate 生成 HTML 报告
    spdlog::info("生成 HTML 报告");
    Reporter reporter;
    HTMLReport html_report;
    try {
        html_report = reporter.generate(symbol_metadata, stats, ctx);
    } catch (const std::exception& e) {
        spdlog::error("HTML 报告生成失败: {}", e.what());
        return 1;
    }

    // 13. 写入输出文件
    ensure_output_dir(args.output_path);
    try {
        std::ofstream ofs(args.output_path, std::ios::binary);
        if (!ofs.is_open()) {
            spdlog::error("无法创建输出文件: {}", args.output_path);
            return 1;
        }
        ofs << html_report.content;
        ofs.close();
    } catch (const std::exception& e) {
        spdlog::error("写入报告失败: {}", e.what());
        return 1;
    }

    spdlog::info("分析完成! 报告已生成: {}", args.output_path);
    spdlog::info("使用浏览器打开报告: firefox {} 或 chromium {}", args.output_path, args.output_path);

    return 0;
}
