// GraphBuilder/GraphBuilder.cpp - 图构建模块实现
// 基于 AnalysisContext 构建调用图/包含图/类型依赖图，计算扇入扇出
// 对应设计文档 4.3.6 节

#include "GraphBuilder/GraphBuilder.h"
#include <spdlog/spdlog.h>
#include <stdexcept>
#include <queue>
#include <unordered_map>
#include <algorithm>
#include <limits>

void GraphBuilder::build(AnalysisContext& ctx, const std::string& entry_function, int depth) {
    spdlog::info("构建图数据: 入口函数={}, 展开深度={}", entry_function, depth);

    // 1. 定位入口函数
    uint32_t entry_id = find_entry_id(ctx, entry_function);
    if (entry_id == 0) {
        spdlog::warn("未找到入口函数 '{}', 将构建完整调用图", entry_function);
    }

    // 2. 构建调用图
    build_call_graph(ctx, entry_id, depth);

    // 3. 构建包含图
    build_include_graph(ctx);

    // 4. 构建类型依赖图
    build_type_dependency_graph(ctx);

    // 5. 计算扇入扇出
    compute_fan_in(ctx);
    compute_fan_out(ctx);

    spdlog::info("图构建完成: {} 条调用边, {} 条包含边, {} 条类型依赖边",
                 ctx.call_edges.size(), ctx.include_edges.size(), ctx.type_edges.size());
}

GraphData GraphBuilder::export_graph_data(const AnalysisContext& ctx) {
    spdlog::info("导出图数据");

    GraphData gdata;

    // 导出函数节点
    for (const auto& sym : ctx.symbols) {
        GraphNode node;
        node.id = sym.id;
        node.label = sym.name;
        switch (sym.kind) {
            case SymbolKind::FUNCTION:   node.type = GraphNode::FUNCTION;    break;
            case SymbolKind::FILE_ENTITY: node.type = GraphNode::FILE_ENTITY; break;
            case SymbolKind::STRUCT:
            case SymbolKind::CLASS:      node.type = GraphNode::STRUCT;      break;
            default:                     node.type = GraphNode::FUNCTION;    break;
        }
        gdata.nodes.push_back(node);
    }

    // 导出调用边
    for (const auto& edge : ctx.call_edges) {
        GraphEdge ge;
        ge.source_id = edge.caller_id;
        ge.target_id = edge.callee_id;
        ge.relation = GraphEdge::CALLS;
        ge.weight = edge.call_count;
        gdata.edges.push_back(ge);
    }

    // 导出包含边
    for (const auto& edge : ctx.include_edges) {
        GraphEdge ge;
        ge.source_id = edge.includer_id;
        ge.target_id = edge.includee_id;
        ge.relation = GraphEdge::INCLUDES;
        ge.weight = 1;
        gdata.edges.push_back(ge);
    }

    // 导出类型依赖边
    for (const auto& edge : ctx.type_edges) {
        GraphEdge ge;
        ge.source_id = edge.source_id;
        ge.target_id = edge.target_id;
        if (edge.relation == TypeDependencyEdge::INHERITS) {
            ge.relation = GraphEdge::INHERITS;
        } else {
            ge.relation = GraphEdge::CONTAINS;
        }
        ge.weight = 1;
        gdata.edges.push_back(ge);
    }

    spdlog::info("图数据导出完成: {} 节点, {} 边", gdata.nodes.size(), gdata.edges.size());
    return gdata;
}

uint32_t GraphBuilder::find_entry_id(const AnalysisContext& ctx, const std::string& entry_name) {
    if (entry_name.empty()) return 0;

    // 优先按完全限定名匹配
    auto it = ctx.symbol_name_to_id.find(entry_name);
    if (it != ctx.symbol_name_to_id.end()) {
        return it->second;
    }

    // 按短名称匹配（取第一个匹配的函数符号）
    for (const auto& sym : ctx.symbols) {
        if (sym.name == entry_name && sym.kind == SymbolKind::FUNCTION) {
            return sym.id;
        }
    }

    spdlog::warn("入口函数 '{}' 未在符号表中找到", entry_name);
    return 0;
}

void GraphBuilder::build_call_graph(AnalysisContext& ctx, uint32_t entry_id, int max_depth) {
    spdlog::debug("构建调用图: 入口ID={}, 最大深度={}", entry_id, max_depth);

    if (entry_id == 0) {
        // 无入口函数，保留 Indexer 填充的所有 call_edges
        return;
    }

    // BFS 遍历，生成从入口函数出发的调用子图
    std::vector<CallEdge> bfs_edges;
    bfs_traverse(entry_id, max_depth, ctx, bfs_edges);

    // 替换 ctx.call_edges 为 BFS 遍历得到的子图
    // 注意：这里保留全量边（Analyzer 需要完整统计），通过标记区分
    // 实际上按设计规范，call_edges 保持全量，BFS 仅决定前端展示范围
    // 此处实现保留全量，由 Reporter 根据深度过滤展示
    spdlog::debug("BFS 完成，展示子图含 {} 条调用边", bfs_edges.size());
}

void GraphBuilder::bfs_traverse(uint32_t start_id, int max_depth, AnalysisContext& ctx,
                                 std::vector<CallEdge>& edges) {
    // BFS 队列：(symbol_id, 当前深度)
    std::queue<std::pair<uint32_t, int>> bfs_queue;
    std::unordered_set<uint32_t> visited;

    bfs_queue.push({start_id, 0});
    visited.insert(start_id);

    // 构建调用者→被调用者的快速索引
    std::unordered_map<uint32_t, std::vector<uint32_t>> callee_map;
    for (const auto& edge : ctx.call_edges) {
        if (edge.callee_id != std::numeric_limits<uint32_t>::max()) {
            callee_map[edge.caller_id].push_back(edge.callee_id);
        }
    }

    while (!bfs_queue.empty()) {
        auto [cur_id, cur_depth] = bfs_queue.front();
        bfs_queue.pop();

        if (cur_depth >= max_depth) continue;

        auto it = callee_map.find(cur_id);
        if (it == callee_map.end()) continue;

        for (uint32_t callee_id : it->second) {
            // 记录调用边
            CallEdge edge;
            edge.caller_id = cur_id;
            edge.callee_id = callee_id;
            edge.call_count = 1;
            edges.push_back(edge);

            if (visited.find(callee_id) == visited.end()) {
                visited.insert(callee_id);
                bfs_queue.push({callee_id, cur_depth + 1});
            }
        }
    }
}

void GraphBuilder::build_include_graph(AnalysisContext& ctx) {
    size_t edge_count = ctx.include_edges.size();
    size_t file_count = ctx.files.size();
    spdlog::debug("构建包含图: {} 个文件, {} 条包含边", file_count, edge_count);

    if (edge_count == 0) return;

    // 建立文件符号 ID → FileSymbol 的快速索引
    std::unordered_map<uint32_t, size_t> file_id_to_idx;
    for (size_t i = 0; i < ctx.files.size(); i++) {
        file_id_to_idx[ctx.files[i].symbol_id] = i;
    }

    // 验证边 + 统计每个文件被包含的次数
    std::unordered_map<uint32_t, int> included_by;
    size_t valid = 0;
    for (const auto& edge : ctx.include_edges) {
        bool has_includer = file_id_to_idx.count(edge.includer_id) > 0;
        bool has_includee = file_id_to_idx.count(edge.includee_id) > 0;

        if (has_includer && has_includee) {
            valid++;
            included_by[edge.includee_id]++;
        } else {
            if (!has_includer)
                spdlog::warn("包含边引用无效的 includer_id: {}", edge.includer_id);
            if (!has_includee)
                spdlog::warn("包含边引用无效的 includee_id: {}", edge.includee_id);
        }
    }

    spdlog::debug("包含图验证: {}/{} 条边有效", valid, edge_count);

    // 找出被包含次数最多的文件（热点头文件）
    if (!included_by.empty()) {
        auto max_it = std::max_element(included_by.begin(), included_by.end(),
            [](const auto& a, const auto& b) { return a.second < b.second; });
        if (max_it != included_by.end() && max_it->second > 1) {
            // 查找对应的文件路径
            for (const auto& fs : ctx.files) {
                if (fs.symbol_id == max_it->first) {
                    spdlog::debug("热点头文件: {} (被包含 {} 次)",
                                  fs.symbol_id, max_it->second);
                    break;
                }
            }
        }
    }
}

void GraphBuilder::build_type_dependency_graph(AnalysisContext& ctx) {
    spdlog::debug("构建类型依赖图");

    for (const auto& csym : ctx.composites) {
        // 处理字段包含关系（字段类型为其他复合类型）
        for (const auto& field : csym.fields) {
            // 尝试将字段类型名解析为符号 ID
            auto it = ctx.symbol_name_to_id.find(field.type);
            if (it != ctx.symbol_name_to_id.end()) {
                uint32_t field_type_id = it->second;
                // 避免重复创建已存在的边
                bool exists = false;
                for (const auto& e : ctx.type_edges) {
                    if (e.source_id == csym.symbol_id &&
                        e.target_id == field_type_id &&
                        e.relation == TypeDependencyEdge::CONTAINS) {
                        exists = true;
                        break;
                    }
                }
                if (!exists) {
                    TypeDependencyEdge edge;
                    edge.source_id = csym.symbol_id;
                    edge.target_id = field_type_id;
                    edge.relation = TypeDependencyEdge::CONTAINS;
                    ctx.type_edges.push_back(edge);
                }
            }
        }
    }

    spdlog::debug("类型依赖图构建完成: {} 条依赖边", ctx.type_edges.size());
}

void GraphBuilder::compute_fan_in(AnalysisContext& ctx) {
    spdlog::debug("计算扇入");

    // 统计每个函数被调用的次数
    std::unordered_map<uint32_t, int> fan_in_map;
    for (const auto& edge : ctx.call_edges) {
        if (edge.callee_id != std::numeric_limits<uint32_t>::max()) {
            fan_in_map[edge.callee_id]++;
        }
    }

    // 回填到 FunctionSymbol
    for (auto& fsym : ctx.functions) {
        auto it = fan_in_map.find(fsym.symbol_id);
        if (it != fan_in_map.end()) {
            fsym.fan_in = it->second;
        }
    }
}

void GraphBuilder::compute_fan_out(AnalysisContext& ctx) {
    spdlog::debug("计算扇出");

    // 统计每个函数调用的不同函数数量
    std::unordered_map<uint32_t, std::unordered_set<uint32_t>> fan_out_map;
    for (const auto& edge : ctx.call_edges) {
        if (edge.callee_id != std::numeric_limits<uint32_t>::max()) {
            fan_out_map[edge.caller_id].insert(edge.callee_id);
        }
    }

    // 回填到 FunctionSymbol
    for (auto& fsym : ctx.functions) {
        auto it = fan_out_map.find(fsym.symbol_id);
        if (it != fan_out_map.end()) {
            fsym.fan_out = static_cast<int>(it->second.size());
        }
    }
}
