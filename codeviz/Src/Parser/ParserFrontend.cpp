// Parser/ParserFrontend.cpp - C/C++ 解析前端实现
// 使用 tree-sitter 解析 C/C++ 源文件，提取函数/类型/宏/包含等符号信息
// 对应设计文档 4.3.4 节

#include "Parser/ParserFrontend.h"
#include <tree_sitter/api.h>
#include <spdlog/spdlog.h>
#include <stdexcept>
#include <algorithm>
#include <sstream>
#include <stack>
#include <cstring>

// tree-sitter 语言入口函数声明
extern "C" {
    const TSLanguage *tree_sitter_c(void);
    const TSLanguage *tree_sitter_cpp(void);
}

// ---------- 内部辅助：从 TSNode 提取对应的源码文本 ----------
static std::string node_text(TSNode node, const std::string& source) {
    uint32_t start = ts_node_start_byte(node);
    uint32_t end   = ts_node_end_byte(node);
    if (start >= end || end > source.size()) return "";
    return source.substr(start, end - start);
}

// ---------- 内部辅助：在子节点中按 field 名查找 ----------
static TSNode child_by_field(TSNode node, const char* field) {
    return ts_node_child_by_field_name(node, field, (uint32_t)strlen(field));
}

// ---------- 内部辅助：递归遍历，用 DFS 栈 ----------

static void traverse(TSNode node, const std::string& source,
                     FileParseResult& result,
                     std::vector<std::string>& scope,
                     RawSymbol* current_func);

// 提取 #include 路径（去掉引号或尖括号）
static std::string extract_include_path(const std::string& raw) {
    if (raw.size() < 2) return raw;
    if ((raw.front() == '"' && raw.back() == '"') ||
        (raw.front() == '<' && raw.back() == '>')) {
        return raw.substr(1, raw.size() - 2);
    }
    return raw;
}

// ---- 访问器 ----

static void handle_function_def(TSNode node, const std::string& source,
                                  FileParseResult& result,
                                  std::vector<std::string>& scope) {
    // 提取函数名：function_definition → declarator → function_declarator → declarator(identifier)
    TSNode decl = child_by_field(node, "declarator");
    if (ts_node_is_null(decl)) return;

    // 循环向内找 function_declarator
    while (!ts_node_is_null(decl)) {
        const char* t = ts_node_type(decl);
        if (strcmp(t, "function_declarator") == 0) break;
        TSNode inner = ts_node_child_by_field_name(decl, "declarator",
                                                    (uint32_t)strlen("declarator"));
        if (ts_node_is_null(inner)) break;
        decl = inner;
    }
    if (ts_node_is_null(decl)) return;

    TSNode name_node = child_by_field(decl, "declarator");
    if (ts_node_is_null(name_node)) return;

    std::string func_name = node_text(name_node, source);
    if (func_name.empty()) return;

    // 构建完全限定名
    std::string qualified = func_name;
    if (!scope.empty()) {
        std::string prefix;
        for (auto& s : scope) prefix += s + "::";
        qualified = prefix + func_name;
    }

    RawSymbol sym;
    sym.kind       = RawSymbol::FUNC;
    sym.name       = qualified;
    sym.file_path  = result.file_path;
    sym.line_start = ts_node_start_point(node).row + 1;
    sym.line_end   = ts_node_end_point(node).row + 1;

    result.symbols.push_back(sym);

    // 进入函数体，继续遍历以收集 call_expression
    scope.push_back(func_name);
    RawSymbol* func_ptr = &result.symbols.back();
    TSNode body = child_by_field(node, "body");
    if (!ts_node_is_null(body)) {
        traverse(body, source, result, scope, func_ptr);
    }
    scope.pop_back();
}

static void handle_call_expr(TSNode node, const std::string& source,
                              FileParseResult& result, RawSymbol* current_func) {
    if (!current_func) return;

    TSNode fn_node = child_by_field(node, "function");
    if (ts_node_is_null(fn_node)) return;

    std::string callee = node_text(fn_node, source);
    // 去掉空白
    callee.erase(std::remove_if(callee.begin(), callee.end(), ::isspace), callee.end());
    if (!callee.empty()) {
        current_func->callee_names.push_back(callee);
    }
}

static void handle_struct(TSNode node, const std::string& source,
                            FileParseResult& result,
                            std::vector<std::string>& scope) {
    TSNode name_node = child_by_field(node, "name");
    std::string struct_name = ts_node_is_null(name_node) ? "<anonymous>" : node_text(name_node, source);

    std::string qualified = struct_name;
    if (!scope.empty()) {
        std::string prefix;
        for (auto& s : scope) prefix += s + "::";
        qualified = prefix + struct_name;
    }

    RawSymbol sym;
    sym.kind       = RawSymbol::STRUCT;
    sym.name       = qualified;
    sym.file_path  = result.file_path;
    sym.line_start = ts_node_start_point(node).row + 1;
    sym.line_end   = ts_node_end_point(node).row + 1;

    result.symbols.push_back(sym);

    scope.push_back(struct_name);
    TSNode body = child_by_field(node, "body");
    if (!ts_node_is_null(body)) {
        traverse(body, source, result, scope, nullptr);
    }
    scope.pop_back();
}

static void handle_class(TSNode node, const std::string& source,
                           FileParseResult& result,
                           std::vector<std::string>& scope) {
    TSNode name_node = child_by_field(node, "name");
    std::string class_name = ts_node_is_null(name_node) ? "<anonymous>" : node_text(name_node, source);
    spdlog::debug("handle_class: name={}", class_name);

    std::string qualified = class_name;
    if (!scope.empty()) {
        std::string prefix;
        for (auto& s : scope) prefix += s + "::";
        qualified = prefix + class_name;
    }

    RawSymbol sym;
    sym.kind       = RawSymbol::CLASS;
    sym.name       = qualified;
    sym.file_path  = result.file_path;
    sym.line_start = ts_node_start_point(node).row + 1;
    sym.line_end   = ts_node_end_point(node).row + 1;

    // 提取基类列表
    uint32_t child_count = ts_node_child_count(node);
    for (uint32_t i = 0; i < child_count; i++) {
        TSNode child = ts_node_child(node, i);
        const char* ctype = ts_node_type(child);
        if (strcmp(ctype, "base_class_clause") == 0) {
            // 遍历 base_class_clause 的子节点，找到类型名
            uint32_t bc = ts_node_child_count(child);
            for (uint32_t j = 0; j < bc; j++) {
                TSNode base = ts_node_child(child, j);
                const char* bt = ts_node_type(base);
                if (strcmp(bt, "type_identifier") == 0 ||
                    strcmp(bt, "qualified_identifier") == 0) {
                    std::string base_name = node_text(base, source);
                    sym.base_class_names.push_back(base_name);
                    spdlog::debug("  提取基类: {} (type={})", base_name, bt);
                }
            }
        }
    }

    result.symbols.push_back(sym);

    scope.push_back(class_name);
    TSNode body = child_by_field(node, "body");
    if (!ts_node_is_null(body)) {
        traverse(body, source, result, scope, nullptr);
    }
    scope.pop_back();
}

static void handle_macro(TSNode node, const std::string& source,
                           FileParseResult& result) {
    TSNode name_node = child_by_field(node, "name");
    if (ts_node_is_null(name_node)) return;
    std::string macro_name = node_text(name_node, source);
    if (macro_name.empty()) return;

    RawSymbol sym;
    sym.kind       = RawSymbol::MACRO;
    sym.name       = macro_name;
    sym.file_path  = result.file_path;
    sym.line_start = ts_node_start_point(node).row + 1;
    result.symbols.push_back(sym);
}

static void handle_include(TSNode node, const std::string& source,
                             FileParseResult& result) {
    TSNode path_node = child_by_field(node, "path");
    if (ts_node_is_null(path_node)) return;
    std::string raw_path = node_text(path_node, source);
    std::string path = extract_include_path(raw_path);
    if (!path.empty()) {
        result.includes.emplace_back(result.file_path, path);
    }
}

// ---------- DFS 遍历主体 ----------
static void traverse(TSNode node, const std::string& source,
                     FileParseResult& result,
                     std::vector<std::string>& scope,
                     RawSymbol* current_func) {
    if (ts_node_is_null(node)) return;

    const char* type = ts_node_type(node);

    if (strcmp(type, "function_definition") == 0) {
        handle_function_def(node, source, result, scope);
        return; // 函数内部已在 handle_function_def 中递归处理
    } else if (strcmp(type, "call_expression") == 0) {
        handle_call_expr(node, source, result, current_func);
        // 继续遍历子节点（参数中可能有嵌套调用）
    } else if (strcmp(type, "struct_specifier") == 0) {
        handle_struct(node, source, result, scope);
        return;
    } else if (strcmp(type, "class_specifier") == 0) {
        handle_class(node, source, result, scope);
        return;
    } else if (strcmp(type, "preproc_def") == 0 ||
               strcmp(type, "preproc_function_def") == 0) {
        handle_macro(node, source, result);
        return;
    } else if (strcmp(type, "preproc_include") == 0) {
        handle_include(node, source, result);
        return;
    }

    // 递归遍历子节点
    uint32_t child_count = ts_node_child_count(node);
    for (uint32_t i = 0; i < child_count; i++) {
        traverse(ts_node_child(node, i), source, result, scope, current_func);
    }
}

// ============================================================================
// ParserFrontend 公有接口实现
// ============================================================================

FileParseResult ParserFrontend::parse_file(const SourceFile& source, const CompileArgs& args) {
    spdlog::info("解析源文件: {}", source.file_path);

    FileParseResult result;
    result.file_path = source.file_path;

    // 根据扩展名判断语言类型
    std::string ext;
    auto dot_pos = source.file_path.rfind('.');
    if (dot_pos != std::string::npos) {
        ext = source.file_path.substr(dot_pos);
    }
    std::string ext_lower = ext;
    std::transform(ext_lower.begin(), ext_lower.end(), ext_lower.begin(), ::tolower);

    const TSLanguage* lang = nullptr;
    if (ext_lower == ".c") {
        lang = tree_sitter_c();
        is_cpp_ = false;
    } else if (ext_lower == ".cpp" || ext_lower == ".cxx" || ext_lower == ".cc" ||
               ext_lower == ".hpp" || ext_lower == ".hxx" || ext_lower == ".hh") {
        lang = tree_sitter_cpp();
        is_cpp_ = true;
    } else if (ext_lower == ".h") {
        // .h 文件可能是 C 或 C++ 头文件，优先用 C++ 解析（C++ 兼容 C 语法）
        lang = tree_sitter_cpp();
        is_cpp_ = true;
    } else {
        spdlog::warn("不支持的文件扩展名: {} ({})", ext, source.file_path);
        lang = tree_sitter_cpp();
        is_cpp_ = true;
    }

    TSParser* parser = ts_parser_new();
    if (!parser) {
        spdlog::error("创建 tree-sitter parser 失败");
        return result;
    }

    if (lang) {
        if (!ts_parser_set_language(parser, lang)) {
            spdlog::warn("设置语言失败 (ABI 不匹配?): {}", source.file_path);
        }
    }

    const std::string& content = source.content;
    TSTree* tree = ts_parser_parse_string(parser, nullptr,
                                          content.c_str(),
                                          static_cast<uint32_t>(content.size()));
    if (tree == nullptr) {
        spdlog::warn("tree-sitter 解析失败: {}", source.file_path);
        ts_parser_delete(parser);
        return result;
    }

    TSNode root = ts_tree_root_node(tree);
    if (ts_node_is_null(root)) {
        spdlog::warn("CST 根节点为空: {}", source.file_path);
    } else {
        std::vector<std::string> scope;
        traverse(root, content, result, scope, nullptr);
    }

    ts_tree_delete(tree);
    ts_parser_delete(parser);

    spdlog::info("解析完成: {} 个符号, {} 个包含关系",
                 result.symbols.size(), result.includes.size());
    return result;
}

// 以下为头文件声明的私有方法——逻辑已内联到 traverse() 中
// 保留空实现避免链接错误

void ParserFrontend::init_parser(const std::string& file_ext) {}

void ParserFrontend::traverse_cst(void* node_ctx, FileParseResult& result,
                                   const std::string& source,
                                   std::vector<std::string>& scope) {}

void ParserFrontend::visit_function_definition(void* node, FileParseResult& result,
                                                const std::string& source,
                                                std::vector<std::string>& scope) {}

void ParserFrontend::visit_function_declarator(void* node, RawSymbol& sym,
                                                const std::string& source) {}

void ParserFrontend::visit_call_expression(void* node, FileParseResult& result,
                                            const std::string& source,
                                            RawSymbol* current_func) {}

void ParserFrontend::visit_struct_specifier(void* node, FileParseResult& result,
                                             const std::string& source,
                                             std::vector<std::string>& scope) {}

void ParserFrontend::visit_class_specifier(void* node, FileParseResult& result,
                                            const std::string& source,
                                            std::vector<std::string>& scope) {}

void ParserFrontend::visit_field_declaration(void* node, RawSymbol& sym,
                                              const std::string& source) {}

void ParserFrontend::visit_preproc_def(void* node, FileParseResult& result,
                                        const std::string& source) {}

void ParserFrontend::visit_preproc_include(void* node, FileParseResult& result,
                                            const std::string& source) {}

std::string ParserFrontend::get_node_text(void* node, const std::string& source) {
    return "";
}

std::string ParserFrontend::get_qualified_name(const std::string& base,
                                                const std::vector<std::string>& scope) {
    std::string result;
    for (const auto& s : scope) result += s + "::";
    result += base;
    return result;
}

void ParserFrontend::enter_scope(std::vector<std::string>& scope, const std::string& name) {
    scope.push_back(name);
}

void ParserFrontend::exit_scope(std::vector<std::string>& scope) {
    if (!scope.empty()) scope.pop_back();
}
