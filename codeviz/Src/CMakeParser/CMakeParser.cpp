// CMakeParser/CMakeParser.cpp - CMake 解析模块实现
// 使用 tree-sitter-cmake 解析 CMakeLists.txt 文件，提取构建元数据
// 对应设计文档 4.3.2 节

#include "CMakeParser/CMakeParser.h"
#include <tree_sitter/api.h>
#include <spdlog/spdlog.h>
#include <cstring>
#include <vector>

// 声明 tree-sitter-cmake 的入口函数
extern "C" const TSLanguage* tree_sitter_cmake();

// ============================================================================
// 内部辅助函数
// ============================================================================

/// 提取 TSNode 对应的源码文本
static std::string node_text(TSNode node, const std::string& source) {
    uint32_t start = ts_node_start_byte(node);
    uint32_t end = ts_node_end_byte(node);
    return source.substr(start, end - start);
}

/// 递归收集 argument_list 中的所有参数文本
static void flatten_arguments(TSNode node, std::vector<std::string>& args,
                              const std::string& source) {
    const char* type = ts_node_type(node);

    // 跳过语法符号和注释
    if (strcmp(type, "(") == 0 || strcmp(type, ")") == 0) return;
    if (strcmp(type, "line_comment") == 0) return;

    // 参数节点：提取文本（去除引号）
    if (strcmp(type, "argument") == 0 ||
        strcmp(type, "quoted_argument") == 0 ||
        strcmp(type, "unquoted_argument") == 0) {
        std::string text = node_text(node, source);
        // 去除外围引号
        if (text.size() >= 2 && text.front() == '"' && text.back() == '"') {
            text = text.substr(1, text.size() - 2);
        }
        if (!text.empty()) {
            args.push_back(text);
        }
        return;
    }

    // 递归遍历子节点
    uint32_t n = ts_node_child_count(node);
    for (uint32_t i = 0; i < n; i++) {
        flatten_arguments(ts_node_child(node, i), args, source);
    }
}

/// 从 argument_list 节点提取所有参数
static std::vector<std::string> extract_arguments(TSNode arg_list,
                                                   const std::string& source) {
    std::vector<std::string> args;
    flatten_arguments(arg_list, args, source);
    return args;
}

// ============================================================================
// CMakeParser 实现
// ============================================================================

int CMakeParser::parse(const CMakeFile& file, BuildMetadata& meta) {
    spdlog::info("解析 CMake 文件: {}", file.file_path);

    // 创建 tree-sitter 解析器
    TSParser* parser = ts_parser_new();
    if (!parser) {
        spdlog::error("无法创建 tree-sitter 解析器");
        return -1;
    }

    const TSLanguage* lang = tree_sitter_cmake();
    if (!lang) {
        spdlog::error("无法加载 tree-sitter-cmake 语言");
        ts_parser_delete(parser);
        return -1;
    }

    ts_parser_set_language(parser, lang);

    // 解析 CMakeLists.txt 内容
    const std::string& source = file.content;
    TSTree* tree = ts_parser_parse_string(
        parser, nullptr,
        source.c_str(),
        static_cast<uint32_t>(source.size()));

    if (!tree) {
        spdlog::error("tree-sitter-cmake 解析失败: {}", file.file_path);
        ts_parser_delete(parser);
        return -1;
    }

    // 检查解析错误
    TSNode root = ts_tree_root_node(tree);
    if (ts_node_has_error(root)) {
        spdlog::warn("tree-sitter-cmake 解析存在错误节点: {}", file.file_path);
        // 继续尝试，有错误时仍可能提取部分信息
    }

    // 遍历 CST
    traverse_cst(root, meta, source);

    ts_tree_delete(tree);
    ts_parser_delete(parser);

    return 0;
}

void CMakeParser::traverse_cst(TSNode node, BuildMetadata& meta,
                               const std::string& source) {
    const char* type = ts_node_type(node);

    // 处理 normal_command
    if (strcmp(type, "normal_command") == 0) {
        handle_normal_command(node, meta, source);
        return;  // 不继续向下，避免重复处理子节点
    }

    // 对结构化命令（if/foreach/function/macro/block），递归处理 body 内部
    // body 内的命令仍然是 normal_command，需要递进去处理
    uint32_t n = ts_node_child_count(node);
    for (uint32_t i = 0; i < n; i++) {
        traverse_cst(ts_node_child(node, i), meta, source);
    }
}

void CMakeParser::handle_normal_command(TSNode node, BuildMetadata& meta,
                                        const std::string& source) {
    // normal_command 结构: [identifier, "(", argument_list, ")"]
    // 按类型查找子节点以正确跳过匿名节点
    uint32_t child_count = ts_node_child_count(node);
    std::string cmd;
    TSNode arg_list = {};  // 初始化为空 TSNode

    for (uint32_t i = 0; i < child_count; i++) {
        TSNode child = ts_node_child(node, i);
        const char* type = ts_node_type(child);
        if (strcmp(type, "identifier") == 0) {
            cmd = get_node_text(child, source);
        } else if (strcmp(type, "argument_list") == 0) {
            arg_list = child;
        }
    }

    if (cmd.empty() || ts_node_is_null(arg_list)) return;

    // 根据命令名分发
    if (cmd == "project") {
        visit_project(arg_list, meta, source);
    } else if (cmd == "cmake_minimum_required") {
        visit_cmake_minimum_required(arg_list, meta, source);
    } else if (cmd == "add_executable") {
        visit_add_executable(arg_list, meta, source);
    } else if (cmd == "add_library") {
        visit_add_library(arg_list, meta, source);
    } else if (cmd == "target_link_libraries") {
        visit_target_link_libraries(arg_list, meta, source);
    } else if (cmd == "set") {
        visit_set_compiler(arg_list, meta, source);
    } else if (cmd == "add_subdirectory") {
        visit_add_subdirectory(arg_list, meta, source);
    }
}

void CMakeParser::visit_project(TSNode arg_list, BuildMetadata& meta,
                                const std::string& source) {
    auto args = extract_arguments(arg_list, source);
    if (!args.empty()) {
        meta.project_name = args[0];
        spdlog::debug("项目名: {}", meta.project_name);
    }
}

void CMakeParser::visit_cmake_minimum_required(TSNode arg_list,
                                                BuildMetadata& meta,
                                                const std::string& source) {
    auto args = extract_arguments(arg_list, source);
    for (size_t i = 0; i + 1 < args.size(); ++i) {
        std::string lower = args[i];
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        if (lower == "version") {
            meta.cmake_version = args[i + 1];
            spdlog::debug("CMake 版本: {}", meta.cmake_version);
            break;
        }
    }
}

void CMakeParser::visit_add_executable(TSNode arg_list, BuildMetadata& meta,
                                       const std::string& source) {
    auto args = extract_arguments(arg_list, source);
    if (!args.empty()) {
        meta.targets.push_back(args[0]);
        spdlog::debug("可执行目标: {}", args[0]);
    }
}

void CMakeParser::visit_add_library(TSNode arg_list, BuildMetadata& meta,
                                    const std::string& source) {
    auto args = extract_arguments(arg_list, source);
    if (!args.empty()) {
        meta.targets.push_back(args[0]);
        spdlog::debug("库目标: {}", args[0]);
    }
}

void CMakeParser::visit_target_link_libraries(TSNode arg_list,
                                               BuildMetadata& meta,
                                               const std::string& source) {
    auto args = extract_arguments(arg_list, source);
    if (args.empty()) return;

    std::string target = args[0];
    std::vector<std::string> libs;
    for (size_t i = 1; i < args.size(); ++i) {
        std::string lower = args[i];
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        if (lower != "private" && lower != "public" && lower != "interface") {
            libs.push_back(args[i]);
        }
    }
    meta.target_link_libs[target] = libs;
    spdlog::debug("目标 {} 链接库: {}", target, libs.size());
}

void CMakeParser::visit_set_compiler(TSNode arg_list, BuildMetadata& meta,
                                     const std::string& source) {
    auto args = extract_arguments(arg_list, source);
    if (args.size() >= 2) {
        if (args[0] == "CMAKE_C_COMPILER") {
            meta.c_compiler = args[1];
            spdlog::debug("C 编译器: {}", meta.c_compiler);
        } else if (args[0] == "CMAKE_CXX_COMPILER") {
            meta.cxx_compiler = args[1];
            spdlog::debug("C++ 编译器: {}", meta.cxx_compiler);
        }
    }
}

void CMakeParser::visit_add_subdirectory(TSNode arg_list, BuildMetadata& meta,
                                         const std::string& source) {
    auto args = extract_arguments(arg_list, source);
    if (!args.empty()) {
        meta.subdirectories.push_back(args[0]);
        spdlog::debug("子目录: {}", args[0]);
    }
}

std::string CMakeParser::get_node_text(TSNode node, const std::string& source) {
    return node_text(node, source);
}
