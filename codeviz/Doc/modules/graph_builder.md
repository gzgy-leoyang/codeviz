# 图构建模块

> 来源: 从设计规格说明书 4.3 节提取的 图构建模块 详细设计。
> 原始文档: Doc/Code_Visualization_Tool_Design_Spec.md

#### 4.3.6 图构建模块

##### 职责
基于 `AnalysisContext` 中的全局符号表和引用关系，构建函数调用图、头文件包含图、类型依赖图，填充边数据到 `AnalysisContext` 中，同时支持按入口函数和深度过滤调用图，计算函数的扇入扇出统计。在 BFS 剪枝前将完整调用边保存至 `full_call_edges`，供 HTML 报告前端按需展开。

##### 对外接口

```cpp
class GraphBuilder {
public:
    /**
     * 构建调用图、包含图和类型依赖图，并计算扇入扇出
     * @param ctx 分析上下文（输入输出，边数据和函数统计将被填充）
     * @param entry_function 调用图展开的入口函数名
     * @param depth 展开的最大深度
     */
    void build(AnalysisContext& ctx, const std::string& entry_function, int depth);

    /**
     * 导出为分析引擎消费的图数据
     * @param ctx 已构建边数据的上下文
     * @return 结构化的图数据
     */
    GraphData export_graph_data(const AnalysisContext& ctx);
};
```

##### 内部函数

| 函数签名 | 功能 |
| :--- | :--- |
| uint32_t find_entry_id(const AnalysisContext& ctx, const std::string& entry_name) | 根据名称定位入口函数的 Symbol ID |
| void build_call_graph(AnalysisContext& ctx, uint32_t entry_id, int max_depth) | 从入口函数 BFS 遍历构建调用图，生成 CallEdge |
| void build_include_graph(AnalysisContext& ctx) | 验证 IncludeEdge 有效性，统计被包含次数，识别热点头文件 |
| void build_type_dependency_graph(AnalysisContext& ctx) | 分析 CompositeSymbol 之间的字段类型和继承关系，生成 TypeDependencyEdge |
| void compute_fan_in(AnalysisContext& ctx) | 基于 CallEdge 统计每个函数的被调用次数 |
| void compute_fan_out(AnalysisContext& ctx) | 基于 CallEdge 统计每个函数调用的不同函数数 |
| void bfs_traverse(uint32_t start_id, int max_depth, AnalysisContext& ctx, std::vector<CallEdge>& edges) | 广度优先遍历调用关系，受深度限制 |

##### 主流程步骤
1. 定位入口函数的 Symbol ID（完全限定名 → 短名称匹配）。若未找到则记录警告，构建完整调用图（不按入口过滤）。
2. 调用 compute_fan_in / compute_fan_out 基于 **完整调用边** 统计扇入扇出（在 BFS 替换前执行，确保统计覆盖所有边）。
3. 调用 build_include_graph 验证 IncludeEdge 有效性并统计文件被包含热度。
4. 调用 build_type_dependency_graph 分析字段类型和继承关系，生成 TypeDependencyEdge。
5. 调用 build_call_graph 从入口开始 BFS 遍历，生成以入口函数为中心的调用子图，**替换** ctx.call_edges（深度由 -d 参数控制，替换前已备份扇入扇出数据）。

##### 依赖的数据结构
- 输入输出核心数据：AnalysisContext
- 输出接口数据：GraphData（通过 export_graph_data 导出）

##### 异常处理
- 入口函数未找到：抛出 std::runtime_error，由 CLI 捕获并提示用户。
- BFS 遍历过程中遇到符号缺失：记录警告，跳过该条调用引用。
- 类型依赖分析中字段类型无法解析：记录警告，忽略该类型依赖边。

