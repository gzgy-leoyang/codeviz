// CMakeParser/CMakeParser.cpp - CMake 解析模块实现
// 使用 tree-sitter-cmake 解析 CMakeLists.txt 文件，提取构建元数据
// 对应设计文档 4.3.2 节

#include "CMakeParser/CMakeParser.h"
#include <tree_sitter/api.h>
#include <spdlog/spdlog.h>
#include <stdexcept>
#include <sstream>
#include <algorithm>

// 声明 tree-sitter-cmake 的入口函数
extern "C" const TSLanguage *tree_sitter_cmake();

int CMakeParser::parse(const CMakeFile& file, BuildMetadata& meta) {
    spdlog::info("解析 CMake 文件: {}", file.file_path);

    try {
        init_parser();

        // 创建解析器并设置语言
        TSParser* parser = ts_parser_new();
        const TSLanguage* lang = tree_sitter_cmake();
        if (lang) {
            ts_parser_set_language(parser, lang);
        }

        // 解析 CMakeLists.txt 内容
        const std::string& source = file.content;
        TSTree* tree = ts_parser_parse_string(parser, nullptr,
                                               source.c_str(),
                                               static_cast<uint32_t>(source.size()));

        if (tree == nullptr) {
            spdlog::warn("tree-sitter-cmake 解析失败: {}", file.file_path);
            ts_parser_delete(parser);
            // 降级：使用正则方式解析
            parse_with_regex(file, meta);
            return 0;
        }

        // 遍历 CST
        TSNode root = ts_tree_root_node(tree);
        if (!ts_node_is_null(root)) {
            traverse_cst(reinterpret_cast<void*>(root.context), meta, source);
        }

        ts_tree_delete(tree);
        ts_parser_delete(parser);

    } catch (const std::exception& e) {
        spdlog::error("CMake 解析异常: {}", e.what());
        // 降级：使用简单文本解析
        parse_with_regex(file, meta);
    }

    return 0;
}

void CMakeParser::init_parser() {
    // tree-sitter 解析器在每次调用时创建，无需全局初始化
    parser_initialized_ = true;
}

void CMakeParser::traverse_cst(void* root_ctx, BuildMetadata& meta, const std::string& source) {
    // tree-sitter CST 遍历逻辑
    // 由于桩实现中 TSNode 不可用，此函数留待真实库集成时实现
    // 实际遍历逻辑见 parse_with_regex
    spdlog::debug("traversing CMake CST (stub implementation)");
}

void CMakeParser::visit_project(void* node, BuildMetadata& meta, const std::string& source) {
    // 提取 project(name VERSION x.y) 中的项目名称
    spdlog::debug("visiting project node");
}

void CMakeParser::visit_add_executable(void* node, BuildMetadata& meta, const std::string& source) {
    spdlog::debug("visiting add_executable node");
}

void CMakeParser::visit_add_library(void* node, BuildMetadata& meta, const std::string& source) {
    spdlog::debug("visiting add_library node");
}

void CMakeParser::visit_target_link_libraries(void* node, BuildMetadata& meta, const std::string& source) {
    spdlog::debug("visiting target_link_libraries node");
}

void CMakeParser::visit_set_compiler(void* node, BuildMetadata& meta, const std::string& source) {
    spdlog::debug("visiting set_compiler node");
}

std::string CMakeParser::get_node_text(void* node, const std::string& source) {
    return "";
}

/**
 * 降级文本解析方法：使用简单字符串匹配解析 CMakeLists.txt
 * 用于 tree-sitter-cmake 不可用时的备用实现
 */
void CMakeParser::parse_with_regex(const CMakeFile& file, BuildMetadata& meta) {
    spdlog::info("使用文本解析模式解析 CMake 文件: {}", file.file_path);

    const std::string& content = file.content;
    std::istringstream iss(content);
    std::string line;

    while (std::getline(iss, line)) {
        // 去除注释和首尾空白
        auto comment_pos = line.find('#');
        if (comment_pos != std::string::npos) {
            line = line.substr(0, comment_pos);
        }
        // 去除首尾空白
        auto trim = [](std::string& s) {
            s.erase(0, s.find_first_not_of(" \t"));
            s.erase(s.find_last_not_of(" \t") + 1);
        };
        trim(line);
        if (line.empty()) continue;

        // 转换为小写用于关键字比较
        std::string lower = line;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

        // 提取括号内的参数列表
        auto extract_args = [&](const std::string& ln) -> std::vector<std::string> {
            std::vector<std::string> args;
            auto lp = ln.find('(');
            auto rp = ln.rfind(')');
            if (lp == std::string::npos || rp == std::string::npos || rp <= lp) return args;
            std::string inner = ln.substr(lp + 1, rp - lp - 1);
            std::istringstream aiss(inner);
            std::string token;
            while (aiss >> token) {
                if (!token.empty()) args.push_back(token);
            }
            return args;
        };

        // project(name ...)
        if (lower.find("project(") == 0 || lower.find("project (") == 0) {
            auto args = extract_args(line);
            if (!args.empty()) {
                meta.project_name = args[0];
                spdlog::debug("项目名: {}", meta.project_name);
            }
        }
        // cmake_minimum_required(VERSION x.y)
        else if (lower.find("cmake_minimum_required") == 0) {
            auto args = extract_args(line);
            for (size_t i = 0; i + 1 < args.size(); ++i) {
                std::string arglo = args[i];
                std::transform(arglo.begin(), arglo.end(), arglo.begin(), ::tolower);
                if (arglo == "version") {
                    meta.cmake_version = args[i + 1];
                    spdlog::debug("CMake 版本: {}", meta.cmake_version);
                    break;
                }
            }
        }
        // add_executable(target srcs...)
        else if (lower.find("add_executable(") == 0 || lower.find("add_executable (") == 0) {
            auto args = extract_args(line);
            if (!args.empty()) {
                meta.targets.push_back(args[0]);
                spdlog::debug("可执行目标: {}", args[0]);
            }
        }
        // add_library(target [STATIC|SHARED] srcs...)
        else if (lower.find("add_library(") == 0 || lower.find("add_library (") == 0) {
            auto args = extract_args(line);
            if (!args.empty()) {
                meta.targets.push_back(args[0]);
                spdlog::debug("库目标: {}", args[0]);
            }
        }
        // target_link_libraries(target [PRIVATE|PUBLIC|INTERFACE] libs...)
        else if (lower.find("target_link_libraries(") == 0 || lower.find("target_link_libraries (") == 0) {
            auto args = extract_args(line);
            if (!args.empty()) {
                std::string target = args[0];
                std::vector<std::string> libs;
                for (size_t i = 1; i < args.size(); ++i) {
                    std::string arglo = args[i];
                    std::transform(arglo.begin(), arglo.end(), arglo.begin(), ::tolower);
                    if (arglo != "private" && arglo != "public" && arglo != "interface") {
                        libs.push_back(args[i]);
                    }
                }
                meta.target_link_libs[target] = libs;
                spdlog::debug("目标 {} 链接库: {}", target, libs.size());
            }
        }
        // set(CMAKE_C_COMPILER ...) 或 set(CMAKE_CXX_COMPILER ...)
        else if (lower.find("set(cmake_c_compiler") != std::string::npos ||
                 lower.find("set(cmake_cxx_compiler") != std::string::npos) {
            auto args = extract_args(line);
            if (args.size() >= 2) {
                std::string keylo = args[0];
                std::transform(keylo.begin(), keylo.end(), keylo.begin(), ::tolower);
                if (keylo == "cmake_c_compiler") {
                    meta.c_compiler = args[1];
                } else if (keylo == "cmake_cxx_compiler") {
                    meta.cxx_compiler = args[1];
                }
            }
        }
        // add_subdirectory(path)
        else if (lower.find("add_subdirectory(") == 0 || lower.find("add_subdirectory (") == 0) {
            auto args = extract_args(line);
            if (!args.empty()) {
                meta.subdirectories.push_back(args[0]);
                spdlog::debug("子目录: {}", args[0]);
            }
        }
    }

    spdlog::info("CMake 文本解析完成: 目标数={}", meta.targets.size());
}
