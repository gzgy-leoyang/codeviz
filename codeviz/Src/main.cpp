/**
 * @file main.cpp
 * @brief 程序入口，编排完整的源码分析流水线
 *
 * 调度各模块完成：参数解析 → 源文件扫描 → CMake/CompDB 解析 →
 * CST 解析 → 符号索引 → 图构建 → 统计分析 → HTML 报告生成。
 */

#include "CLI/CLI.h"
#include "CMakeParser/CMakeParser.h"
#include "CompDBParser/CompDBParser.h"
#include "Parser/ParserFrontend.h"
#include "Indexer/Indexer.h"
#include "GraphBuilder/GraphBuilder.h"
#include "Analyzer/Analyzer.h"
#include "Reporter/Reporter.h"

#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <iostream>

namespace fs = std::filesystem;

/**
 * @brief 程序入口函数
 *
 * 执行完整的源码分析流水线：
 * 1. 初始化日志系统
 * 2. 解析并校验命令行参数（-p/-e/-d/-o/-v）
 * 3. 扫描项目目录下的 C/C++ 源文件
 * 4. 解析 CMakeLists.txt（含递归子目录）获取构建元数据
 * 5. 解析 compile_commands.json 获取每个文件的编译参数
 * 6. 使用 tree-sitter 对每个源文件进行 CST 解析
 * 7. 构建全局符号索引
 * 8. BFS 剪枝构建调用图、包含图、类型依赖图
 * 9. 执行统计分析（圈复杂度、循环包含检测、热力图）
 * 10. 生成带交互式可视化（Cytoscape.js）的 HTML 报告
 *
 * @param argc 命令行参数个数
 * @param argv 命令行参数数组
 * @return int 0 表示成功，非 0 表示失败
 */
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

    // 若未指定输出路径，默认输出到项目目录同级的 .html 文件
    if (args.output_path.empty()) {
        args.output_path = args.project_path;
        if (args.output_path.back() == '/') args.output_path.pop_back();
        args.output_path += ".html";
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
    for (const auto& build_dir : {"build", "Build", "cmake-build-debug", "cmake-build-release"}) {
        std::string bd = args.project_path + "/" + build_dir;
        if (fs::exists(bd + "/compile_commands.json")) {
            spdlog::info("发现编译数据库: {}", bd);
            try {
                CompDBParser compdb_parser;
                compile_db = compdb_parser.parse(bd);

                if (build_meta.c_compiler.empty() || build_meta.cxx_compiler.empty()) {
                    std::ifstream ifs(bd + "/compile_commands.json");
                    nlohmann::json j;
                    ifs >> j;
                    if (j.is_array() && !j.empty()) {
                        std::string cmd = j[0].value("command", "");
                        if (!cmd.empty()) {
                            auto pos = cmd.find_first_not_of(" \t");
                            if (pos != std::string::npos) {
                                cmd = cmd.substr(pos);
                                std::string compiler = cmd.substr(0, cmd.find_first_of(" \t"));
                                std::string base = fs::path(compiler).filename().string();
                                bool is_cxx = (base.find("++") != std::string::npos ||
                                              base.find("clang") != std::string::npos ||
                                              base.find("gcc") == std::string::npos);
                                if (is_cxx && build_meta.cxx_compiler.empty())
                                    build_meta.cxx_compiler = compiler;
                                else if (!is_cxx && build_meta.c_compiler.empty())
                                    build_meta.c_compiler = compiler;
                                if (build_meta.c_compiler.empty() && !build_meta.cxx_compiler.empty())
                                    build_meta.c_compiler = build_meta.cxx_compiler;
                            }
                        }
                    }
                }
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
        ctx.command_line = "codeviz -p " + args.project_path
                         + " -e " + args.entry_function
                         + " -d " + std::to_string(args.expand_depth)
                         + " -o " + args.output_path;
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
