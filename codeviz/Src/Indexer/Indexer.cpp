// Indexer/Indexer.cpp - 符号索引模块实现
// 两遍遍历 FileParseResult：第一遍建表，第二遍解析引用关系
// 对应设计文档 4.3.5 节

#include "Indexer/Indexer.h"
#include <spdlog/spdlog.h>
#include <algorithm>
#include <limits>
#include <cstring>

AnalysisContext Indexer::build_index(const std::vector<FileParseResult>& parse_results) {
    spdlog::info("开始构建符号索引，共 {} 个文件", parse_results.size());

    AnalysisContext ctx;
    init_context(ctx);

    // 第一遍：为每个文件和符号分配 ID，填充符号表
    for (const auto& file_result : parse_results) {
        spdlog::debug("第一遍处理文件: {}", file_result.file_path);

        // 为该文件创建 FileSymbol
        FileSymbol file_sym = create_file_symbol(file_result.file_path, ctx);

        // 遍历该文件中的所有原始符号
        for (const auto& raw : file_result.symbols) {
            // 分配或获取全局唯一 Symbol ID
            uint32_t sym_id = get_or_create_symbol_id(raw, ctx);

            // 将 RawSymbol 转换为核心 Symbol 存入符号表
            if (sym_id != 0 && sym_id <= ctx.symbols.size()) {
                // 已存在
            } else {
                Symbol sym = convert_to_symbol(raw, sym_id);
                ctx.symbols.push_back(sym);
                ctx.symbol_name_to_id[sym.qualified_name] = sym_id;
            }

            // 根据类型创建分类型符号对象
            if (raw.kind == RawSymbol::FUNC) {
                FunctionSymbol fsym = extract_function_detail(raw, sym_id);
                ctx.functions.push_back(fsym);
            } else if (raw.kind == RawSymbol::STRUCT || raw.kind == RawSymbol::CLASS) {
                CompositeSymbol csym = extract_composite_detail(raw, sym_id);
                ctx.composites.push_back(csym);
            }

            // 将符号 ID 关联到文件
            file_sym.symbols.push_back(sym_id);
        }

        ctx.files.push_back(file_sym);
        ctx.source_files.push_back(file_result.file_path);
    }

    // 第二遍：解析调用关系和包含关系
    process_calls(parse_results, ctx);
    process_includes(parse_results, ctx);

    // 第二遍补充：解析复合类型的基类名称为 Symbol ID
    resolve_composite_base_classes(ctx);

    // 填充反向引用
    fill_reverse_references(ctx);

    spdlog::info("符号索引构建完成: {} 个符号, {} 个调用边, {} 个包含边",
                 ctx.symbols.size(), ctx.call_edges.size(), ctx.include_edges.size());
    return ctx;
}

std::vector<SymbolMetadata> Indexer::export_metadata(const AnalysisContext& ctx) {
    spdlog::info("导出符号元数据，共 {} 个符号", ctx.symbols.size());

    std::vector<SymbolMetadata> result;
    result.reserve(ctx.symbols.size());

    for (const auto& sym : ctx.symbols) {
        SymbolMetadata meta;
        meta.symbol_id = sym.id;
        meta.name = sym.name;
        meta.qualified_name = sym.qualified_name;
        meta.file_path = sym.file_path;
        meta.line = sym.line_start;
        meta.kind = sym.kind;
        // complexity / fan_in / fan_out 由 Analyzer 填充后回传

        // 尝试从 functions 池中获取统计数据
        auto it = std::find_if(ctx.functions.begin(), ctx.functions.end(),
                               [&sym](const FunctionSymbol& f) {
                                   return f.symbol_id == sym.id;
                               });
        if (it != ctx.functions.end()) {
            meta.complexity = it->cyclomatic_complexity;
            meta.fan_in = it->fan_in;
            meta.fan_out = it->fan_out;
        }

        result.push_back(meta);
    }

    return result;
}

void Indexer::init_context(AnalysisContext& ctx) {
    ctx.symbols.clear();
    ctx.symbol_name_to_id.clear();
    ctx.functions.clear();
    ctx.composites.clear();
    ctx.files.clear();
    ctx.call_edges.clear();
    ctx.include_edges.clear();
    ctx.type_edges.clear();
    ctx.references.clear();
    ctx.source_files.clear();
    unresolved_base_classes_.clear();
    next_id_ = 1;
}

uint32_t Indexer::get_or_create_symbol_id(const RawSymbol& raw, AnalysisContext& ctx) {
    // 使用完全限定名（文件路径 + 名称 + 行号）作为唯一 key
    std::string key = raw.file_path + "::" + raw.name + "@" + std::to_string(raw.line_start);

    auto it = ctx.symbol_name_to_id.find(key);
    if (it != ctx.symbol_name_to_id.end()) {
        return it->second;
    }

    uint32_t new_id = next_id_++;
    ctx.symbol_name_to_id[key] = new_id;

    // 也建立短名到 ID 的映射（可能会覆盖）
    if (ctx.symbol_name_to_id.find(raw.name) == ctx.symbol_name_to_id.end()) {
        ctx.symbol_name_to_id[raw.name] = new_id;
    }

    return new_id;
}

Symbol Indexer::convert_to_symbol(const RawSymbol& raw, uint32_t id) {
    Symbol sym;
    sym.id = id;
    sym.name = raw.name;
    sym.qualified_name = raw.file_path + "::" + raw.name;
    sym.file_path = raw.file_path;
    sym.line_start = raw.line_start;
    sym.line_end = raw.line_end;
    sym.access = raw.access;

    // 转换 kind
    switch (raw.kind) {
        case RawSymbol::FUNC:       sym.kind = SymbolKind::FUNCTION;    break;
        case RawSymbol::STRUCT:     sym.kind = SymbolKind::STRUCT;      break;
        case RawSymbol::CLASS:      sym.kind = SymbolKind::CLASS;       break;
        case RawSymbol::ENUM_KIND:  sym.kind = SymbolKind::ENUM;        break;
        case RawSymbol::VAR:        sym.kind = SymbolKind::VARIABLE;    break;
        case RawSymbol::MACRO:      sym.kind = SymbolKind::MACRO;       break;
        default:                    sym.kind = SymbolKind::FUNCTION;    break;
    }

    return sym;
}

FunctionSymbol Indexer::extract_function_detail(const RawSymbol& raw, uint32_t symbol_id) {
    FunctionSymbol fsym;
    fsym.symbol_id = symbol_id;
    fsym.return_type = raw.return_type;
    fsym.parameters = raw.parameters;
    fsym.is_virtual = raw.is_virtual;
    fsym.is_static = raw.is_static;
    fsym.is_inline = raw.is_inline;
    // cyclomatic_complexity / fan_in / fan_out 由 Analyzer 填充
    return fsym;
}

CompositeSymbol Indexer::extract_composite_detail(const RawSymbol& raw, uint32_t symbol_id) {
    CompositeSymbol csym;
    csym.symbol_id = symbol_id;
    csym.fields = raw.fields;
    // 暂存基类名称字符串，在第二遍解析为 Symbol ID
    unresolved_base_classes_[symbol_id] = raw.base_class_names;
    return csym;
}

FileSymbol Indexer::create_file_symbol(const std::string& file_path, AnalysisContext& ctx) {
    FileSymbol fsym;

    uint32_t file_id = next_id_++;
    std::string key = "FILE::" + file_path;
    ctx.symbol_name_to_id[key] = file_id;

    // 创建对应的 Symbol 表条目
    Symbol sym;
    sym.id = file_id;
    sym.name = file_path;
    sym.qualified_name = key;
    sym.file_path = file_path;
    sym.kind = SymbolKind::FILE_ENTITY;
    sym.access = AccessSpecifier::NONE;
    ctx.symbols.push_back(sym);

    fsym.symbol_id = file_id;
    return fsym;
}

void Indexer::process_calls(const std::vector<FileParseResult>& results, AnalysisContext& ctx) {
    spdlog::debug("处理函数调用关系");

    for (const auto& file_result : results) {
        for (const auto& raw : file_result.symbols) {
            if (raw.kind != RawSymbol::FUNC) continue;

            // 获取调用者 ID
            std::string caller_key = raw.file_path + "::" + raw.name + "@" + std::to_string(raw.line_start);
            auto caller_it = ctx.symbol_name_to_id.find(caller_key);
            if (caller_it == ctx.symbol_name_to_id.end()) continue;
            uint32_t caller_id = caller_it->second;

            // 为每个被调用者创建边
            for (const auto& callee_name : raw.callee_names) {
                // 尝试按短名查找被调用者
                auto callee_it = ctx.symbol_name_to_id.find(callee_name);
                uint32_t callee_id;
                if (callee_it != ctx.symbol_name_to_id.end()) {
                    callee_id = callee_it->second;
                } else {
                    // 未解析的外部符号，使用特殊 ID
                    callee_id = std::numeric_limits<uint32_t>::max();
                    spdlog::debug("外部符号: {} (调用者: {})", callee_name, raw.name);
                }

                // 创建 CallEdge
                CallEdge edge;
                edge.caller_id = caller_id;
                edge.callee_id = callee_id;
                edge.call_count = 1;
                edge.file_path = file_result.file_path;
                ctx.call_edges.push_back(edge);

                // 创建 SymbolRef
                if (callee_id != std::numeric_limits<uint32_t>::max()) {
                    SymbolRef ref;
                    ref.from_symbol_id = caller_id;
                    ref.to_symbol_id = callee_id;
                    ref.file_path = file_result.file_path;
                    ctx.references.push_back(ref);
                }

                // 回填 FunctionSymbol::callees
                auto fsym_it = std::find_if(ctx.functions.begin(), ctx.functions.end(),
                                            [caller_id](const FunctionSymbol& f) {
                                                return f.symbol_id == caller_id;
                                            });
                if (fsym_it != ctx.functions.end() &&
                    callee_id != std::numeric_limits<uint32_t>::max()) {
                    fsym_it->callees.push_back(callee_id);
                }
            }
        }
    }

    spdlog::debug("调用关系处理完成: {} 条调用边", ctx.call_edges.size());
}

void Indexer::process_includes(const std::vector<FileParseResult>& results, AnalysisContext& ctx) {
    spdlog::debug("处理头文件包含关系");

    // 构建 file_path → FILE::key 映射，方便模糊查找
    std::vector<std::string> all_file_paths;
    for (const auto& [key, id] : ctx.symbol_name_to_id) {
        if (key.rfind("FILE::", 0) == 0) {
            all_file_paths.push_back(key.substr(6)); // strip "FILE::"
        }
    }

    for (const auto& file_result : results) {
        // 获取包含者的 FileSymbol ID
        std::string includer_key = "FILE::" + file_result.file_path;
        auto includer_it = ctx.symbol_name_to_id.find(includer_key);
        if (includer_it == ctx.symbol_name_to_id.end()) continue;
        uint32_t includer_id = includer_it->second;

        for (const auto& [includer_path, includee_path] : file_result.includes) {
            // 跳过系统头文件（以 < > 包围）
            if (!includee_path.empty() && includee_path.front() == '<') continue;
            if (includee_path.find('/') == std::string::npos &&
                (includee_path.find(".h") == std::string::npos &&
                 includee_path.find(".hpp") == std::string::npos)) {
                // 无路径分隔符且无头文件扩展名 → 系统头文件
                continue;
            }

            // 解析 includee_path → 完整文件路径
            uint32_t includee_id = resolve_include_file(includee_path, file_result.file_path,
                                                         all_file_paths, ctx);
            if (includee_id == 0) {
                spdlog::debug("包含的文件未在项目中找到: {}", includee_path);
                continue;
            }

            IncludeEdge edge;
            edge.includer_id = includer_id;
            edge.includee_id = includee_id;
            edge.is_system = false;
            ctx.include_edges.push_back(edge);

            // 回填 FileSymbol::includes
            auto file_sym_it = std::find_if(ctx.files.begin(), ctx.files.end(),
                                             [includer_id](const FileSymbol& f) {
                                                 return f.symbol_id == includer_id;
                                             });
            if (file_sym_it != ctx.files.end()) {
                file_sym_it->includes.push_back(includee_id);
            }
        }
    }

    spdlog::debug("包含关系处理完成: {} 条包含边", ctx.include_edges.size());
}

uint32_t Indexer::resolve_include_file(const std::string& includee_path,
                                        const std::string& includer_file,
                                        const std::vector<std::string>& all_file_paths,
                                        const AnalysisContext& ctx) {
    // 方法1：精确匹配
    std::string key = "FILE::" + includee_path;
    auto it = ctx.symbol_name_to_id.find(key);
    if (it != ctx.symbol_name_to_id.end()) return it->second;

    // 方法2：以 includee_path 后缀匹配所有已知文件路径
    for (const auto& fp : all_file_paths) {
        if (fp.size() >= includee_path.size() &&
            fp.substr(fp.size() - includee_path.size()) == includee_path) {
            key = "FILE::" + fp;
            auto it2 = ctx.symbol_name_to_id.find(key);
            if (it2 != ctx.symbol_name_to_id.end()) return it2->second;
        }
    }

    return 0; // 未找到
}

void Indexer::resolve_composite_base_classes(AnalysisContext& ctx) {
    spdlog::debug("解析复合类型基类名称");

    // 构建 名称 → Symbol ID 映射
    std::unordered_map<std::string, uint32_t> name_to_id;
    for (const auto& sym : ctx.symbols) {
        name_to_id[sym.name] = sym.id;
        name_to_id[sym.qualified_name] = sym.id;
        // 提取最后一段名称
        size_t pos = sym.name.rfind("::");
        if (pos != std::string::npos) {
            name_to_id[sym.name.substr(pos + 2)] = sym.id;
        }
    }

    for (auto& csym : ctx.composites) {
        auto it = unresolved_base_classes_.find(csym.symbol_id);
        if (it == unresolved_base_classes_.end()) continue;

        for (const auto& base_name : it->second) {
            // 尝试多种匹配方式
            auto match_it = name_to_id.find(base_name);
            if (match_it == name_to_id.end()) {
                // 尝试去掉前缀（如 class Foo 的 "Foo"）
                std::string cleaned = base_name;
                for (const auto& prefix : {"class ", "struct ", "enum "}) {
                    if (cleaned.rfind(prefix, 0) == 0) {
                        cleaned = cleaned.substr(strlen(prefix));
                        break;
                    }
                }
                match_it = name_to_id.find(cleaned);
            }

            if (match_it != name_to_id.end()) {
                csym.base_classes.push_back(match_it->second);
                TypeDependencyEdge edge;
                edge.source_id = csym.symbol_id;
                edge.target_id = match_it->second;
                edge.relation = TypeDependencyEdge::INHERITS;
                ctx.type_edges.push_back(edge);
                spdlog::debug("  {} 继承 {}", csym.symbol_id, match_it->second);
            } else {
                spdlog::debug("  未找到基类: {}", base_name);
            }
        }
    }

    spdlog::debug("复合类型基类解析完成: {} 条类型依赖边", ctx.type_edges.size());
}

void Indexer::fill_reverse_references(AnalysisContext& ctx) {
    spdlog::debug("填充反向引用关系");

    for (const auto& ref : ctx.references) {
        // 找到被引用的符号，将引用者的 SymbolRef 索引添加进去
        auto it = std::find_if(ctx.symbols.begin(), ctx.symbols.end(),
                               [&ref](const Symbol& s) {
                                   return s.id == ref.to_symbol_id;
                               });
        if (it != ctx.symbols.end()) {
            it->references.push_back(ref.from_symbol_id);
        }
    }
}
