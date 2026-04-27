// CompDBParser/CompDBParser.cpp - 编译数据库解析模块实现
// 对应设计文档 4.3.3 节

#include "CompDBParser/CompDBParser.h"
#include <fstream>
#include <sstream>
#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

std::unordered_map<std::string, CompileArgs> CompDBParser::parse(const std::string& build_dir) {
    spdlog::info("解析编译数据库: {}", build_dir);

    std::unordered_map<std::string, CompileArgs> result;

    // 查找 compile_commands.json 文件
    std::string db_path = find_compile_commands(build_dir);
    if (db_path.empty()) {
        spdlog::warn("未找到 compile_commands.json，跳过编译数据库解析");
        return result;
    }

    // 读取文件内容
    std::ifstream ifs(db_path);
    if (!ifs.is_open()) {
        spdlog::error("无法打开编译数据库: {}", db_path);
        return result;
    }

    // 解析 JSON
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

    // 遍历每个编译条目
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

std::string CompDBParser::find_compile_commands(const std::string& build_dir) {
    std::string path = build_dir;
    // 移除末尾的斜杠
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

CompileArgs CompDBParser::parse_entry(const json& entry) {
    CompileArgs args;

    // 获取源文件路径
    if (!entry.contains("file")) {
        spdlog::warn("编译条目缺少 file 字段，跳过");
        return args;
    }
    args.file_path = entry["file"].get<std::string>();

    // 获取工作目录
    std::string directory = ".";
    if (entry.contains("directory")) {
        directory = entry["directory"].get<std::string>();
    }

    // 提取编译参数：优先使用 arguments 数组，否则解析 command 字符串
    std::string full_cmd;
    if (entry.contains("arguments") && entry["arguments"].is_array()) {
        // 将 arguments 数组拼接为命令字符串
        for (const auto& arg : entry["arguments"]) {
            if (arg.is_string()) {
                full_cmd += arg.get<std::string>() + " ";
            }
        }
    } else if (entry.contains("command")) {
        full_cmd = entry["command"].get<std::string>();
    }

    // 提取 -D 和 -I 参数
    args.defines = extract_defines(full_cmd);
    args.includes = extract_includes(full_cmd);

    // 将 includes 中的相对路径转换为绝对路径
    for (auto& inc_path : args.includes) {
        inc_path = normalize_path(inc_path, directory);
    }

    // 将文件路径也转换为绝对路径
    args.file_path = normalize_path(args.file_path, directory);

    return args;
}

std::vector<std::string> CompDBParser::extract_defines(const std::string& cmd) {
    std::vector<std::string> defines;
    std::istringstream iss(cmd);
    std::string token;
    while (iss >> token) {
        if (token.substr(0, 2) == "-D") {
            if (token.size() > 2) {
                // -DFOO 或 -DFOO=BAR
                defines.push_back(token.substr(2));
            } else {
                // -D FOO 形式
                std::string next;
                if (iss >> next) {
                    defines.push_back(next);
                }
            }
        }
    }
    return defines;
}

std::vector<std::string> CompDBParser::extract_includes(const std::string& cmd) {
    std::vector<std::string> includes;
    std::istringstream iss(cmd);
    std::string token;
    while (iss >> token) {
        if (token.substr(0, 2) == "-I") {
            if (token.size() > 2) {
                // -I/path 或 -I /path
                includes.push_back(token.substr(2));
            } else {
                // -I /path 形式
                std::string next;
                if (iss >> next) {
                    includes.push_back(next);
                }
            }
        }
    }
    return includes;
}

std::string CompDBParser::normalize_path(const std::string& path, const std::string& base_dir) {
    if (path.empty()) return path;
    // 已经是绝对路径
    if (path[0] == '/') return path;
    // 拼接为绝对路径
    std::string result = base_dir;
    if (!result.empty() && result.back() != '/') {
        result += '/';
    }
    result += path;
    return result;
}
