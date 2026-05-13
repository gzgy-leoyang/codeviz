# HTML 报告生成器

> 来源: 从设计规格说明书 4.3 节提取的 HTML 报告生成器 详细设计。
> 原始文档: Doc/Code_Visualization_Tool_Design_Spec.md

#### 4.3.8 HTML 报告生成器

##### 职责
将符号元数据、统计结果和图结构数据序列化为 JSON，注入 HTML 模板，生成自包含的可交互可视化报告文件。

##### 对外接口

```cpp
class Reporter {
public:
    /**
     * 生成 HTML 报告
     * @param symbols 符号元数据列表
     * @param stats 统计分析结果
     * @param ctx 分析上下文（含图数据）
     * @return 完整的 HTML 报告内容及建议输出路径
     */
    HTMLReport generate(const std::vector<SymbolMetadata>& symbols,
                        const AnalysisStats& stats,
                        const AnalysisContext& ctx);
};
```

##### 内部函数

| 函数签名 | 功能 |
| :--- | :--- |
| std::string load_template() | 加载内嵌的 HTML 骨架模板字符串（C++ string literal）|
| json build_json(const std::vector<SymbolMetadata>& symbols, const AnalysisStats& stats, const AnalysisContext& ctx) | 构建完整的 JSON 数据对象 |
| json convert_call_graph(const std::vector<CallEdge>& edges, const std::vector<Symbol>& symbols) | 将调用边转换为 Cytoscape.js nodes/edges 格式；按 (caller_id, callee_id) 去重合并 weight |
| json convert_include_graph(const std::vector<IncludeEdge>& edges, const std::vector<FileSymbol>& files, const std::vector<Symbol>& symbols) | 将包含边转换为 Cytoscape.js nodes/edges 格式 |
| json convert_type_graph(const std::vector<TypeDependencyEdge>& edges, const std::vector<CompositeSymbol>& composites, const std::vector<Symbol>& symbols) | 将类型依赖边转换为 Cytoscape.js nodes/edges 格式 |
| json build_hotspots(const AnalysisStats& stats) | 构建热力图数据（文件和函数的热力值及颜色映射） |
| json build_anomalies(const AnalysisStats& stats) | 构建异常检测结果数据（循环包含等） |
| std::string find_symbol_name(uint32_t id, const std::vector<Symbol>& symbols) | 根据 Symbol ID 查找名称 |

##### 主流程步骤
1. 调用 load_template 获取内嵌的 HTML 骨架字符串（定义在 Reporter.cpp 中，含 Inja 占位符 `{{ cytoscape_js }}` / `{{ data_json }}` / `{{ bridge_js }}`）。
2. 调用 build_json 将所有输入数据组装为单一 JSON 对象：
   - metadata：项目名、文件数、函数数、C/C++ 编译器、运行命令、生成时间。
   - symbols：含函数签名信息（return_type、parameters、is_virtual/is_static/is_inline）及 Doxygen 注释（comment）。
   - composites：结构体/类的字段信息（name、type、access）及 Doxygen 注释（comment）。
   - call_graph / include_graph / type_graph：通过 convert_* 函数转换。
   - hotspots / anomalies：分别通过 build_hotspots 和 build_anomalies 构建。
   - external_refs：外部符号引用列表（caller_name、callee_name、推测的 library）。
   - stats：文件统计和函数统计（供前端直接渲染）。
3. 使用 Inja 模板引擎（`inja::Environment::render`）将 JSON 数据、Cytoscape.js 库、桥接 JS 注入模板占位符。
4. 返回 HTMLReport，包含完整 HTML 字符串。

##### 前端交互说明
- **节点形状**：函数节点使用圆角矩形（round-rectangle），入口函数使用圆形（ellipse）以示区分；文件节点（FILE_ENTITY）使用圆角矩形；宽度自适应文字内容；函数节点标签显示"函数名\n(文件名)"，包含图节点只显示文件名。入口节点额外使用粗体黄色文字（不设独立边框色，边框与其他节点一致）。
- **节点点击**：非叶节点首次单击展开子节点（径向扇形定位，半径 150px，108° 向下弧），再次单击显示详情框；叶节点单击直接显示详情框。右击已展开节点可折叠其子图。
- **悬停高亮**：鼠标悬停节点时，其扇入/扇出连线高亮为 `#E1F656`；悬停连线时，该连线及两端节点边框同时高亮为 `#E1F656`。移出后恢复样式表颜色。
- **节点信息面板**：点击节点后，视图左上角显示信息框，包含类型、文件、行号、被调用/调用数、圈复杂度、展开状态，以及 Doxygen 注释内容（若存在）。
- **边信息面板**：单击调用边弹出信息框，展示 `caller → callee` 标题及被调用函数的返回类型和参数列表。
- **侧边栏**：位于视图右侧，折叠后仅保留展开按钮（位于 canvas 区域右上角），点击展开后显示 280px 宽的符号搜索列表。
- **性能降级**：节点超过 1000 时自动启用大图模式（hideEdgesOnViewport、motionBlur）。

##### 依赖的数据结构
- 输入接口数据：std::vector<SymbolMetadata>、AnalysisStats
- 输入核心数据：AnalysisContext（用于提取图边数据）
- 输出接口数据：HTMLReport

##### 异常处理
- 模板加载失败（模板字符串为空）：抛出 std::runtime_error，由 CLI 捕获并退出。
- JSON 序列化失败：使用 nlohmann/json 的异常机制向上抛出。
- 图数据转换时遇到孤立引用（节点 ID 在符号表中不存在）：记录警告，跳过该条边。
