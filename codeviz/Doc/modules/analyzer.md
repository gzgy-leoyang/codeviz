# 分析引擎

> 来源: 从设计规格说明书 4.3 节提取的 分析引擎 详细设计。
> 原始文档: Doc/Code_Visualization_Tool_Design_Spec.md

#### 4.3.7 分析引擎

##### 职责
基于 `AnalysisContext` 中的图数据和符号表进行统计计算，产出文件级和函数级的统计指标，检测循环包含等异常关系，为热力图渲染和异常标记提供数据支持。

##### 对外接口

```cpp
class Analyzer {
public:
    /**
     * 执行统计分析
     * @param ctx 已构建图数据的分析上下文
     * @param build_meta 构建元数据（来自 CMake 解析，用于报告中展示）
     * @return 统计结果
     */
    AnalysisStats analyze(const AnalysisContext& ctx, const BuildMetadata& build_meta);
};
```

##### 内部函数

| 函数签名 | 功能 |
| :--- | :--- |
| int compute_cyclomatic_complexity(const FunctionSymbol& func) | 基于函数的分支节点数计算圈复杂度（branch_count + 1）|
| FileStats compute_file_stats(const FileSymbol& file, const AnalysisContext& ctx) | 聚合文件级统计（总行数、代码行数、复杂度总和）|
| std::vector<CircularInclude> detect_circular_includes(const std::vector<IncludeEdge>& edges, int file_count) | 使用 Tarjan 算法检测有向图中的强连通分量，报告循环包含 |
| std::vector<FunctionStats> compute_function_stats(const AnalysisContext& ctx) | 汇总所有函数的扇入、扇出、圈复杂度；回填圈复杂度到 FunctionSymbol |
| void compute_hotspots(AnalysisStats& stats) | 对函数统计按扇入（被调用次数）降序排序 |
| void tarjan_dfs(...) | Tarjan 强连通分量递归 DFS 内部函数 |

##### 主流程步骤
1. 遍历 ctx.files，对每个文件调用 compute_file_stats（总行数、代码行数、复杂度总和），填充 AnalysisStats::file_stats。
2. 调用 compute_function_stats：遍历 ctx.functions，获取已在 GraphBuilder 中回填的扇入扇出，计算圈复杂度（branch_count + 1），回填到 FunctionSymbol，汇总为 FunctionStats 列表。
3. 调用 detect_circular_includes，对 include_edges 有向图运行 Tarjan SCC 算法，大小 > 1 的强连通分量报告为 CircularInclude。
4. 调用 compute_hotspots，按扇入降序排序函数统计（供报告热力图前 20 条呈现）。
5. 返回完整的 AnalysisStats。

##### 依赖的数据结构
- 输入核心数据：AnalysisContext
- 输入接口数据：BuildMetadata
- 输出接口数据：AnalysisStats

##### 异常处理
- 函数缺少圈复杂度计算所需的节点信息：默认圈复杂度为 1。
- 包含图过大导致 SCC 算法耗时：设置最大递归深度或节点数上限，超限时记录警告并跳过循环检测。
- 热力计算时指标全为零：所有热力值设为 0，不抛出异常。
