/**
 * @file CompDBParser.cpp
 * @brief 编译数据库解析模块实现
 *
 * 解析 compile_commands.json 文件中每个编译条目的
 * file、directory、arguments/command 字段，提取宏定义和
 * 头文件搜索路径。
 * 对应设计文档 4.3.3 节。
 */

#include "CompDBParser/CompDBParser.h"
#include <fstream>
#include <sstream>
#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

/**
 * @brief 解析整个编译数据库
 *
 * 定位 compile_commands.json → JSON 解析 → 逐条目处理。
 * 使用 try-catch 捕获 JSON 解析错误，降级返回空映射。
 *
 * @param build_dir 构建目录路径
 * @return 文件路径到编译参数的映射
 */
std::unordered_map<std::string, CompileArgs> CompDBParser::parse(const std::string& build_dir) {
    spdlog::info("解析编译数据库: {}", build_dir);

    std::unordered_map<std::string, CompileArgs> result;

    std::string db_path = find_compile_commands(build_dir);
    if (db_path.empty()) {
        spdlog::warn("未找到 compile_commands.json，跳过编译数据库解析");
        return result;
    }

    std::ifstream ifs(db_path);
    if (!ifs.is_open()) {
        spdlog::error("无法打开编译数据库: {}", db_path);
        return result;
    }

    json db;
    try {
        ifs >> db;
    } catch (const json::parse_error& e) {
        spdlog::error("JSON 解析失败: {}", e.what());
        return result;
    }

    if (!db.is_array()) {
        spdlog::error("compile_commands.json 顶层应为数组");
        return result;
    }

    for (const auto& entry : db) {
        try {
            CompileArgs args = parse_entry(entry);
            if (!args.file_path.empty()) {
                result[args.file_path] = args;
            }
        } catch (const std::exception& e) {
            spdlog::warn("跳过无效编译条目: {}", e.what());
        }
    }

    spdlog::info("编译数据库解析完成，共 {} 个文件条目", result.size());
    return result;
}

/**
 * @brief 定位 compile_commands.json 文件
 *
 * 在 build_dir 路径后拼接 "/compile_commands.json"，
 * 尝试打开确认文件存在。
 *
 * @param build_dir 构建目录
 * @return 完整文件路径，不存在返回空字符串
 */
std::string CompDBParser::find_compile_commands(const std::string& build_dir) {
    std::string path = build_dir;
    while (!path.empty() && path.back() == '/') {
        path.pop_back();
    }
    path += "/compile_commands.json";

    std::ifstream ifs(path);
    if (ifs.is_open()) {
        return path;
    }
    return "";
}

/**
 * @brief 解析单个编译条目
 *
 * 提取 file、directory 字段。优先使用 arguments 数组
 * （拼接为命令字符串），否则使用 command 字符串。
 * 对 -D 和 -I 参数提取后，将相对路径转换为绝对路径。
 *
 * @param entry JSON 编译条目
 * @return CompileArgs 结构
 */
CompileArgs CompDBParser::parse_entry(const json& entry) {
    CompileArgs args;

    if (!entry.contains("file")) {
        spdlog::warn("编译条目缺少 file 字段，跳过");
        return args;
    }
    args.file_path = entry["file"].get<std::string>();

    std::string directory = ".";
    if (entry.contains("directory")) {
        directory = entry["directory"].get<std::string>();
    }

    // 提取编译参数：优先使用 arguments 数组，否则解析 command 字符串
    std::string full_cmd;
    if (entry.contains("arguments") && entry["arguments"].is_array()) {
        for (const auto& arg : entry["arguments"]) {
            if (arg.is_string()) {
                full_cmd += arg.get<std::string>() + " ";
            }
        }
    } else if (entry.contains("command")) {
        full_cmd = entry["command"].get<std::string>();
    }

    args.defines = extract_defines(full_cmd);
    args.includes = extract_includes(full_cmd);

    for (auto& inc_path : args.includes) {
        inc_path = normalize_path(inc_path, directory);
    }

    args.file_path = normalize_path(args.file_path, directory);

    return args;
}

/**
 * @brief 从编译命令中提取 -D 宏定义
 *
 * 支持两种格式：
 * - 合并式：-DFOO、-DFOO=BAR
 * - 分离式：-D FOO（需要读取下一个 token）
 *
 * @param cmd 完整编译命令字符串
 * @return 宏定义列表（不含 -D 前缀）
 */
std::vector<std::string> CompDBParser::extract_defines(const std::string& cmd) {
    std::vector<std::string> defines;
    std::istringstream iss(cmd);
    std::string token;
    while (iss >> token) {
        if (token.substr(0, 2) == "-D") {
            if (token.size() > 2) {
                defines.push_back(token.substr(2));
            } else {
                std::string next;
                if (iss >> next) {
                    defines.push_back(next);
                }
            }
        }
    }
    return defines;
}

/**
 * @brief 从编译命令中提取 -I 头文件路径
 *
 * 支持两种格式：
 * - 合并式：-I/path/to/headers
 * - 分离式：-I /path/to/headers
 *
 * @param cmd 完整编译命令字符串
 * @return 头文件搜索路径列表（不含 -I 前缀）
 */
std::vector<std::string> CompDBParser::extract_includes(const std::string& cmd) {
    std::vector<std::string> includes;
    std::istringstream iss(cmd);
    std::string token;
    while (iss >> token) {
        if (token.substr(0, 2) == "-I") {
            if (token.size() > 2) {
                includes.push_back(token.substr(2));
            } else {
                std::string next;
                if (iss >> next) {
                    includes.push_back(next);
                }
            }
        }
    }
    return includes;
}

/**
 * @brief 标准化文件路径为绝对路径
 *
 * 若 path 以 '/' 开头则直接返回（已是绝对路径），
 * 否则与 base_dir 拼接为绝对路径。
 *
 * @param path 原始路径（相对或绝对）
 * @param base_dir 基准目录（用于相对路径拼接）
 * @return 标准化后的绝对路径
 */
std::string CompDBParser::normalize_path(const std::string& path, const std::string& base_dir) {
    if (path.empty()) return path;
    if (path[0] == '/') return path;

    std::string result = base_dir;
    if (!result.empty() && result.back() != '/') {
        result += '/';
    }
    result += path;
    return result;
}
