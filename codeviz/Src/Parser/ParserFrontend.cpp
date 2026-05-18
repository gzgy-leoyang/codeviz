/**
 * @file ParserFrontend.cpp
 * @brief C/C++ 解析前端实现
 *
 * 使用 tree-sitter 解析 C/C++ 源文件，通过深度优先 CST 遍历
 * 提取函数/类型/宏/调用/包含等符号信息。
 * 对应设计文档 4.3.4 节。
 * 所有 visit_* 类方法为实际 CST 分发入口，辅助函数保持文件内静态。
 */

#include "Parser/ParserFrontend.h"
#include <tree_sitter/api.h>
#include <spdlog/spdlog.h>
#include <stdexcept>
#include <algorithm>
#include <cstring>
#include <sstream>

// tree-sitter 语言入口函数声明
extern "C" {
    const TSLanguage* tree_sitter_c(void);
    const TSLanguage* tree_sitter_cpp(void);
}

// ============================================================================
// 内部辅助函数（与类状态无关，保持文件内静态）
// ============================================================================

/**
 * @brief 提取 TSNode 对应的源码文本
 *
 * @param node 目标节点
 * @param source 源码字符串
 * @return 节点覆盖的源码片段
 */
static std::string node_text(TSNode node, const std::string& source) {
    uint32_t start = ts_node_start_byte(node);
    uint32_t end   = ts_node_end_byte(node);
    if (start >= end || end > source.size()) return "";
    return source.substr(start, end - start);
}

/**
 * @brief 按 field 名查找子节点
 *
 * @param node 父节点
 * @param field field 名称
 * @return 匹配的子节点，未找到时为空节点
 */
static TSNode child_by_field(TSNode node, const char* field) {
    return ts_node_child_by_field_name(node, field, (uint32_t)strlen(field));
}

/**
 * @brief 统计文件的行数信息
 *
 * 逐行扫描文件内容，区分总行数、有效代码行数和注释行数。
 * 支持块注释（斜杠星号 ... 星号斜杠）和行注释（//）的统计，
 * 正确处理注释与代码混排的行。
 *
 * @param content 文件内容
 * @param total_lines [out] 总行数
 * @param code_lines [out] 代码行数
 * @param comment_lines [out] 注释行数
 */
static void count_file_lines(const std::string& content,
                              int& total_lines, int& code_lines, int& comment_lines) {
    total_lines = code_lines = comment_lines = 0;
    bool in_block = false;
    std::istringstream stream(content);
    std::string line;
    while (std::getline(stream, line)) {
        total_lines++;
        auto trim_left = [](std::string& s) {
            s.erase(0, s.find_first_not_of(" \t\r"));
        };
        trim_left(line);
        if (line.empty()) continue;

        if (in_block) {
            comment_lines++;
            size_t end = line.find("*/");
            if (end != std::string::npos) {
                in_block = false;
                std::string after = line.substr(end + 2);
                trim_left(after);
                if (!after.empty() && after.find("//") != 0 && after.find('#') != 0) {
                    code_lines++;
                }
            }
            continue;
        }

        if (line.find("/*") == 0) {
            comment_lines++;
            size_t end = line.find("*/", 2);
            if (end != std::string::npos) {
                std::string after = line.substr(end + 2);
                trim_left(after);
                if (!after.empty() && after.find("//") != 0 && after.find('#') != 0) {
                    code_lines++;
                }
            } else {
                in_block = true;
            }
            continue;
        }

        if (line.find("//") == 0 || line.find('#') == 0) {
            comment_lines++;
            continue;
        }

        code_lines++;
    }
}

/**
 * @brief 从 #include 原始文本中提取文件路径
 *
 * 去除外围的引号或尖括号。
 * 例如 `"foo.h"` → `foo.h`，`<vector>` → `vector`。
 *
 * @param raw 原始 include 路径文本
 * @return 纯净的文件路径
 */
static std::string extract_include_path(const std::string& raw) {
    if (raw.size() < 2) return raw;
    if ((raw.front() == '"' && raw.back() == '"') ||
        (raw.front() == '<' && raw.back() == '>')) {
        return raw.substr(1, raw.size() - 2);
    }
    return raw;
}

// ============================================================================
// ParserFrontend 实现
// ============================================================================

/**
 * @brief 解析单个源文件
 *
 * 流程：判断语言（.c → tree-sitter-c, 其他 → tree-sitter-cpp）
 * → 创建 TSParser → parse_string → CST 遍历 → 行数统计。
 *
 * @param source 源文件内容及路径
 * @param args 编译参数（当前保留扩展备用）
 * @return 文件解析结果
 */
FileParseResult ParserFrontend::parse_file(const SourceFile& source,
                                            const CompileArgs& args) {
    spdlog::info("解析源文件: {}", source.file_path);

    FileParseResult result;
    result.file_path = source.file_path;

    // 重置文件间状态
    last_comment_.clear();
    last_comment_line_ = 0;

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
        // .h 头文件统一用 C++ 解析器以便正确解析函数原型和类定义
        lang = tree_sitter_cpp();
        is_cpp_ = true;
    } else {
        spdlog::warn("不支持的文件扩展名: {} ({})", ext, source.file_path);
        lang = tree_sitter_cpp();
        is_cpp_ = true;
    }

    // 初始化解析器并解析
    init_parser(ext_lower);

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
    if (!tree) {
        spdlog::warn("tree-sitter 解析失败: {}", source.file_path);
        ts_parser_delete(parser);
        return result;
    }

    TSNode root = ts_tree_root_node(tree);
    if (!ts_node_is_null(root)) {
        std::vector<std::string> scope;
        traverse_cst(root, result, content, scope, nullptr);
    }

    ts_tree_delete(tree);
    ts_parser_delete(parser);

    // 统计行数
    count_file_lines(content, result.total_lines, result.code_lines, result.comment_lines);

    spdlog::info("解析完成: {} 个符号, {} 个包含关系 ({} 行, {} 代码行)",
                 result.symbols.size(), result.includes.size(),
                 result.total_lines, result.code_lines);
    return result;
}

/**
 * @brief 初始化解析器
 *
 * 当前版本解析器在 parse_file 中每次新建，此方法保留接口备用。
 *
 * @param file_ext 文件扩展名
 */
void ParserFrontend::init_parser(const std::string& file_ext) {
    // 解析器在 parse_file 中每次新建，无需全局初始化
}

// ============================================================================
// CST 遍历主分发
// ============================================================================

/**
 * @brief 深度优先遍历 CST 主分发
 *
 * 按节点类型分发到对应 visit_* 方法：
 * - comment → 收集 Doxygen 前导注释
 * - function_definition → visit_function_definition
 * - call_expression → 记录调用（且继续递归子节点）
 * - if/for/while/do/switch/case/?: → 分支计数
 * - field_declaration → 在复合类型体内提取字段
 * - struct/class_specifier → 对应 visit 方法
 * - access_specifier → 跟踪访问修饰符状态
 * - preproc_def/preproc_function_def/preproc_include → 宏和包含处理
 * - 其他节点 → 递归遍历子节点
 *
 * @param node 当前 CST 节点
 * @param result 解析结果
 * @param source 源码字符串
 * @param scope 当前作用域栈
 * @param current_func 当前正在遍历的函数（可能为 nullptr）
 */
void ParserFrontend::traverse_cst(TSNode node, FileParseResult& result,
                                   const std::string& source,
                                   std::vector<std::string>& scope,
                                   RawSymbol* current_func) {
    if (ts_node_is_null(node)) return;

    const char* type = ts_node_type(node);

    if (strcmp(type, "comment") == 0) {
        std::string text = node_text(node, source);
        // 仅支持 Doxygen 格式注释（/// 或 /**）
        if (text.rfind("///", 0) == 0 || text.rfind("/**", 0) == 0) {
            if (last_comment_.empty()) {
                last_comment_ = text;
            } else {
                last_comment_ += "\n" + text;
            }
            last_comment_line_ = ts_node_end_point(node).row + 1;
        }
        return;
    } else if (strcmp(type, "function_definition") == 0) {
        visit_function_definition(node, result, source, scope);
        return;
    } else if (strcmp(type, "call_expression") == 0) {
        visit_call_expression(node, result, source, current_func);
        // 继续遍历子节点（参数中可能有嵌套调用）
    } else if (current_func && (
        strcmp(type, "if_statement") == 0 ||
        strcmp(type, "for_statement") == 0 ||
        strcmp(type, "while_statement") == 0 ||
        strcmp(type, "do_statement") == 0 ||
        strcmp(type, "switch_statement") == 0 ||
        strcmp(type, "case_statement") == 0 ||
        strcmp(type, "conditional_expression") == 0)) {
        current_func->branch_count++;
    } else if (strcmp(type, "field_declaration") == 0) {
        if (current_composite_idx_ != SIZE_MAX) {
            visit_field_declaration(node, result.symbols[current_composite_idx_], source);
        }
        return;
    } else if (strcmp(type, "struct_specifier") == 0) {
        visit_struct_specifier(node, result, source, scope);
        return;
    } else if (strcmp(type, "class_specifier") == 0) {
        visit_class_specifier(node, result, source, scope);
        return;
    } else if (strcmp(type, "access_specifier") == 0) {
        std::string text = node_text(node, source);
        if (text == "public") current_access_ = AccessSpecifier::PUBLIC;
        else if (text == "protected") current_access_ = AccessSpecifier::PROTECTED;
        else if (text == "private") current_access_ = AccessSpecifier::PRIVATE;
        return;
    } else if (strcmp(type, "preproc_def") == 0 ||
               strcmp(type, "preproc_function_def") == 0) {
        visit_preproc_def(node, result, source);
        return;
    } else if (strcmp(type, "preproc_include") == 0) {
        visit_preproc_include(node, result, source);
        return;
    }

    // 递归遍历子节点
    uint32_t child_count = ts_node_child_count(node);
    for (uint32_t i = 0; i < child_count; i++) {
        traverse_cst(ts_node_child(node, i), result, source, scope, current_func);
    }
}

// ============================================================================
// 各 visit_* 处理函数
// ============================================================================

/**
 * @brief 处理函数定义节点
 *
 * 从 function_definition → declarator → function_declarator 链
 * 提取函数名，从 type 子节点提取返回类型。
 * 进入作用域后递归遍历函数体以收集调用表达式和分支节点数。
 *
 * @param node function_definition 节点
 * @param result 解析结果
 * @param source 源码字符串
 * @param scope 作用域栈
 */
void ParserFrontend::visit_function_definition(TSNode node,
                                                FileParseResult& result,
                                                const std::string& source,
                                                std::vector<std::string>& scope) {
    TSNode decl = child_by_field(node, "declarator");
    if (ts_node_is_null(decl)) return;

    // 循环向内定位 function_declarator（处理指针/数组修饰符嵌套）
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
    std::string qualified = get_qualified_name(func_name, scope);

    RawSymbol sym;
    sym.kind       = RawSymbol::FUNC;
    sym.name       = qualified;
    sym.file_path  = result.file_path;
    sym.line_start = ts_node_start_point(node).row + 1;
    sym.line_end   = ts_node_end_point(node).row + 1;

    // 提取返回类型
    TSNode type_node = child_by_field(node, "type");
    if (!ts_node_is_null(type_node)) {
        sym.return_type = node_text(type_node, source);
    }

    // 提取函数签名（参数、虚/静/内联标记）
    visit_function_declarator(decl, sym, source);

    // 关联前导注释（同一行或上一行）
    if (!last_comment_.empty() && sym.line_start - last_comment_line_ <= 2) {
        sym.comment = last_comment_;
    }
    last_comment_.clear();

    result.symbols.push_back(sym);

    // 进入函数体，继续遍历以收集 call_expression
    scope.push_back(func_name);
    RawSymbol* func_ptr = &result.symbols.back();
    TSNode body = child_by_field(node, "body");
    if (!ts_node_is_null(body)) {
        last_comment_.clear();  // 防止函数体内注释泄漏
        traverse_cst(body, result, source, scope, func_ptr);
    }
    scope.pop_back();
}

/**
 * @brief 提取函数签名信息
 *
 * 从 function_declarator 提取参数列表。
 * 通过检查父节点（function_definition）的子节点
 * 识别 virtual/static/inline 修饰符。
 *
 * @param node function_declarator 节点
 * @param sym 目标原始符号（输出参数）
 * @param source 源码字符串
 */
void ParserFrontend::visit_function_declarator(TSNode node, RawSymbol& sym,
                                                const std::string& source) {
    // 提取参数列表
    TSNode param_list = child_by_field(node, "parameters");
    if (!ts_node_is_null(param_list)) {
        uint32_t n = ts_node_child_count(param_list);
        for (uint32_t i = 0; i < n; i++) {
            TSNode child = ts_node_child(param_list, i);
            const char* type = ts_node_type(child);
            if (strcmp(type, "parameter_declaration") == 0) {
                sym.parameters.push_back(node_text(child, source));
            }
        }
    }

    // 从父节点 function_definition 检查 virtual/static/inline 修饰符
    TSNode parent = ts_node_parent(node);
    if (!ts_node_is_null(parent)) {
        uint32_t n = ts_node_child_count(parent);
        for (uint32_t i = 0; i < n; i++) {
            TSNode child = ts_node_child(parent, i);
            const char* ctype = ts_node_type(child);
            if (strcmp(ctype, "virtual") == 0 || strcmp(ctype, "virtual_specifier") == 0) {
                sym.is_virtual = true;
            } else if (strcmp(ctype, "static") == 0) {
                sym.is_static = true;
            } else if (strcmp(ctype, "inline") == 0) {
                sym.is_inline = true;
            }
        }
    }
}

/**
 * @brief 处理函数调用表达式
 *
 * 从 "function" field 提取被调用函数名，
 * 去除所有空白字符后追加到 current_func 的 callee_names 列表。
 *
 * @param node call_expression 节点
 * @param result 解析结果
 * @param source 源码字符串
 * @param current_func 当前函数指针（为 nullptr 时忽略）
 */
void ParserFrontend::visit_call_expression(TSNode node,
                                            FileParseResult& result,
                                            const std::string& source,
                                            RawSymbol* current_func) {
    if (!current_func) return;

    TSNode fn_node = child_by_field(node, "function");
    if (ts_node_is_null(fn_node)) return;

    std::string callee = node_text(fn_node, source);
    callee.erase(std::remove_if(callee.begin(), callee.end(), ::isspace), callee.end());
    if (!callee.empty()) {
        current_func->callee_names.push_back(callee);
    }
}

/**
 * @brief 处理结构体定义
 *
 * 提取结构体名称（支持匿名结构体，命名为 "<anonymous>"），
 * 关联前导注释，进入 field_declaration_list 递归提取成员字段。
 *
 * @param node struct_specifier 节点
 * @param result 解析结果
 * @param source 源码字符串
 * @param scope 作用域栈
 */
void ParserFrontend::visit_struct_specifier(TSNode node,
                                             FileParseResult& result,
                                             const std::string& source,
                                             std::vector<std::string>& scope) {
    TSNode name_node = child_by_field(node, "name");
    std::string struct_name = ts_node_is_null(name_node)
        ? "<anonymous>" : node_text(name_node, source);

    std::string qualified = get_qualified_name(struct_name, scope);

    RawSymbol sym;
    sym.kind       = RawSymbol::STRUCT;
    sym.name       = qualified;
    sym.file_path  = result.file_path;
    sym.line_start = ts_node_start_point(node).row + 1;
    sym.line_end   = ts_node_end_point(node).row + 1;

    // 关联前导注释
    if (!last_comment_.empty() && sym.line_start - last_comment_line_ <= 2) {
        sym.comment = last_comment_;
    }
    last_comment_.clear();

    result.symbols.push_back(sym);

    scope.push_back(struct_name);
    // 通过子节点类型查找 field_declaration_list（C 语法）或 body（C++ 语法）
    TSNode field_list = {};
    uint32_t nc = ts_node_child_count(node);
    for (uint32_t i = 0; i < nc; i++) {
        TSNode child = ts_node_child(node, i);
        const char* t = ts_node_type(child);
        if (strcmp(t, "field_declaration_list") == 0 || strcmp(t, "body") == 0) {
            field_list = child;
            break;
        }
    }
    if (!ts_node_is_null(field_list)) {
        current_access_ = AccessSpecifier::PUBLIC;
        current_composite_idx_ = result.symbols.size() - 1;
        last_comment_.clear();  // 防止结构体体内注释泄漏
        traverse_cst(field_list, result, source, scope, nullptr);
        current_composite_idx_ = SIZE_MAX;
    }
    scope.pop_back();
}

/**
 * @brief 处理类定义
 *
 * 提取类名和基类列表（从 base_class_clause 子节点），
 * 关联前导注释，进入类体递归提取成员字段和嵌套类。
 * 设置默认访问修饰符为 PRIVATE（C++ 类默认私有）。
 *
 * @param node class_specifier 节点
 * @param result 解析结果
 * @param source 源码字符串
 * @param scope 作用域栈
 */
void ParserFrontend::visit_class_specifier(TSNode node,
                                            FileParseResult& result,
                                            const std::string& source,
                                            std::vector<std::string>& scope) {
    TSNode name_node = child_by_field(node, "name");
    std::string class_name = ts_node_is_null(name_node)
        ? "<anonymous>" : node_text(name_node, source);
    spdlog::debug("handle_class: name={}", class_name);

    std::string qualified = get_qualified_name(class_name, scope);

    RawSymbol sym;
    sym.kind       = RawSymbol::CLASS;
    sym.name       = qualified;
    sym.file_path  = result.file_path;
    sym.line_start = ts_node_start_point(node).row + 1;
    sym.line_end   = ts_node_end_point(node).row + 1;

    // 关联前导注释
    if (!last_comment_.empty() && sym.line_start - last_comment_line_ <= 2) {
        sym.comment = last_comment_;
    }
    last_comment_.clear();

    // 提取基类列表
    uint32_t child_count = ts_node_child_count(node);
    for (uint32_t i = 0; i < child_count; i++) {
        TSNode child = ts_node_child(node, i);
        const char* ctype = ts_node_type(child);
        if (strcmp(ctype, "base_class_clause") == 0) {
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
    TSNode field_list = {};
    uint32_t nc = ts_node_child_count(node);
    for (uint32_t i = 0; i < nc; i++) {
        TSNode child = ts_node_child(node, i);
        const char* t = ts_node_type(child);
        if (strcmp(t, "field_declaration_list") == 0 || strcmp(t, "body") == 0) {
            field_list = child;
            break;
        }
    }
    if (!ts_node_is_null(field_list)) {
        current_access_ = AccessSpecifier::PRIVATE;
        current_composite_idx_ = result.symbols.size() - 1;
        last_comment_.clear();  // 防止类体内注释泄漏
        traverse_cst(field_list, result, source, scope, nullptr);
        current_composite_idx_ = SIZE_MAX;
    }
    scope.pop_back();
}

/**
 * @brief 提取成员字段信息
 *
 * 从 field_declaration 节点识别字段类型（type_identifier/primitive_type 等），
 * 然后提取一个或多个字段名（支持 int a, b, c; 形式）。
 * 字段的访问修饰符由 current_access_ 状态决定。
 *
 * @param node field_declaration 节点
 * @param sym 目标复合类型符号（输出：fields 将被填充）
 * @param source 源码字符串
 */
void ParserFrontend::visit_field_declaration(TSNode node, RawSymbol& sym,
                                              const std::string& source) {
    // 提取字段类型
    std::string field_type;
    uint32_t n = ts_node_child_count(node);
    for (uint32_t i = 0; i < n; i++) {
        TSNode child = ts_node_child(node, i);
        const char* ctype = ts_node_type(child);
        if (strcmp(ctype, "type_identifier") == 0 ||
            strcmp(ctype, "primitive_type") == 0 ||
            strcmp(ctype, "sized_type_specifier") == 0 ||
            strcmp(ctype, "qualified_identifier") == 0 ||
            strcmp(ctype, "namespace_identifier") == 0) {
            field_type = node_text(child, source);
            break;
        }
    }
    if (field_type.empty()) return;

    // 提取字段名（可能含多个：int a, b, c;）
    for (uint32_t i = 0; i < n; i++) {
        TSNode child = ts_node_child(node, i);
        const char* ctype = ts_node_type(child);
        std::string field_name;

        if (strcmp(ctype, "field_identifier") == 0) {
            field_name = node_text(child, source);
        } else if (strcmp(ctype, "declarator") == 0) {
            TSNode inner = child_by_field(child, "declarator");
            if (!ts_node_is_null(inner)) {
                field_name = node_text(inner, source);
            }
        }

        if (!field_name.empty()) {
            FieldInfo fi;
            fi.name = field_name;
            fi.type = field_type;
            fi.access = current_access_;
            sym.fields.push_back(fi);
        }
    }
}

/**
 * @brief 处理宏定义
 *
 * 从 preproc_def 或 preproc_function_def 节点提取宏名，
 * 创建 MACRO 类型的原始符号。
 *
 * @param node 宏定义节点
 * @param result 解析结果
 * @param source 源码字符串
 */
void ParserFrontend::visit_preproc_def(TSNode node,
                                        FileParseResult& result,
                                        const std::string& source) {
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

/**
 * @brief 处理 #include 预处理指令
 *
 * 从 path field 提取包含路径，去除引号/尖括号，
 * 将 (includer_path, includee_path) 记录到解析结果。
 *
 * @param node preproc_include 节点
 * @param result 解析结果
 * @param source 源码字符串
 */
void ParserFrontend::visit_preproc_include(TSNode node,
                                            FileParseResult& result,
                                            const std::string& source) {
    TSNode path_node = child_by_field(node, "path");
    if (ts_node_is_null(path_node)) return;
    std::string raw_path = node_text(path_node, source);
    std::string path = extract_include_path(raw_path);
    if (!path.empty()) {
        result.includes.emplace_back(result.file_path, path);
    }
}

// ============================================================================
// 工具方法
// ============================================================================

std::string ParserFrontend::get_node_text(TSNode node,
                                           const std::string& source) {
    return node_text(node, source);
}

std::string ParserFrontend::get_qualified_name(
    const std::string& base,
    const std::vector<std::string>& scope) {
    if (scope.empty()) return base;
    std::string result;
    for (const auto& s : scope) result += s + "::";
    result += base;
    return result;
}

void ParserFrontend::enter_scope(std::vector<std::string>& scope,
                                  const std::string& name) {
    scope.push_back(name);
}

void ParserFrontend::exit_scope(std::vector<std::string>& scope) {
    if (!scope.empty()) scope.pop_back();
}
