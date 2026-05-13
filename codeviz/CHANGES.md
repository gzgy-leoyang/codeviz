# codeviz 项目修改汇总

> 首次提交: 2026-04-27
> 代码重组织: 2026-05-02 (21251c3)
> 最近更新: 2026-05-13（第三轮）

---

## 2026-05-03 修改汇总

共 **20 个提交**，涉及 **25 个文件**，新增 **2645 行**，删除 **2072 行**。

### 一、核心分析引擎改进

#### 1. 圈复杂度改用 AST 分支节点真实统计
- **提交**: `8aa6af0`
- **文件**: ParserFrontend.cpp/.h, Analyzer.cpp, DataTypes.h
- **内容**: 圈复杂度从依赖 callees 数量的间接估算，改为在 CST 遍历中真实统计 `if/for/while/do/switch/case/ternary` 等分支节点数量，结果写入 `FunctionSymbol::branch_count`，Analyzer 用 `branch_count + 1` 计算最终值。

#### 2. 实现 `build_include_graph` 与文件行数统计
- **提交**: `a7f6f22`
- **文件**: GraphBuilder.cpp, DataTypes.h, ParserFrontend.cpp, Indexer.cpp, Analyzer.cpp
- **内容**:
  - `GraphBuilder::build_include_graph` 从空桩实现为边验证与热度统计
  - `FileParseResult` 新增 `total_lines / code_lines / comment_lines` 字段
  - `ParserFrontend` 新增 `count_file_lines()` 统计文件行数
  - `Indexer` 回填行数到 `FileSymbol`

#### 3. 字段提取、外部库追踪与函数签名
- **提交**: `235a4be`
- **文件**: ParserFrontend.cpp, Indexer.cpp, Reporter.cpp, DataTypes.h
- **内容**:
  - `visit_field_declaration` 提取字段类型/名字
  - `visit_function_declarator` 提取参数表、虚/静/内联修饰、返回类型
  - `Indexer` 收集外部符号引用，按命名空间前缀推测库名
  - `Reporter` 序列化字段数据、外部符号引用、函数签名到 JSON
  - `DataTypes` 新增 `ExternalRef` 结构体

#### 4. `-d` 深度参数实际控制调用图展开
- **提交**: `691ba0e`
- **文件**: GraphBuilder.cpp
- **内容**: BFS 子图结果之前被丢弃（`ctx.call_edges` 未替换），现在将 BFS 结果写回 `call_edges`，扇入扇出计算移至替换前使用全量边。验证：depth=1→4边, depth=2→8边, depth=3→10边。

### 二、新功能

#### 5. HTML 报告头部显示运行命令参数
- **提交**: `9c4f904`
- **文件**: DataTypes.h, CLI.cpp, Reporter.cpp
- **内容**: `AnalysisContext` 新增 `command_line` 字段；CLI 侧填充命令字符串；Reporter 序列化到 JSON metadata；页面显示"运行命令: codeviz -p ..."灰色提示。

#### 6. 增强 `--help` 帮助信息
- **提交**: `b679220`
- **文件**: CLI.cpp
- **内容**: 添加使用描述、用法格式、各选项详细说明/默认值/约束范围、使用示例。修复 CLI::ParseError 改用 `app.exit()` 正确输出。

#### 7. 侧边栏折叠/展开 + 头部垂直布局
- **提交**: `4ae16b9`
- **文件**: template.html
- **内容**: 页面头部改为垂直排列（标题在上，项目信息和参数在下）；侧边栏添加折叠/展开按钮，窄窗口时隐藏符号搜索，切换时自动调整 Cytoscape 尺寸。

#### 8. 大规模图降级机制
- **提交**: `935c9c3`
- **文件**: template.html
- **内容**: `initCytoscape` 在节点 >1000 时启用 `hideEdgesOnViewport` / `motionBlur` / `textEvents:no` 等性能优化，并显示"大图模式"提示。

### 三、Bug 修复

#### 9. 排除构建目录文件，消除重复 main 函数
- **提交**: `4e04dbb`
- **文件**: Indexer.cpp
- **内容**: `scan_source_files` 跳过 `/build/`、`/CMakeFiles/` 等构建目录，防止 `CMakeCompilerId.cpp` 等构建产物干扰，函数热力图不再出现重复 main 条目。

#### 10. `compile_commands.json` 自动检测编译器
- **提交**: `8a4952a`
- **文件**: CLI.cpp, CompDBParser
- **内容**: 当 CMakeLists.txt 未显式 `set` 编译器时，从 `compile_commands.json` 的 `command` 字段提取编译器路径，项目概况中 C/C++ 编译器信息正确显示。

#### 11. `-o` 默认输出到项目目录同级 .html
- **提交**: `564b301`
- **文件**: CLI.cpp
- **内容**: 未指定 `-o` 时默认输出到 `<project_path>.html`，更新帮助说明和示例。

### 四、构建与部署

#### 12. 添加自动部署脚本 deploy.sh
- **提交**: `09938c9` → `07d0d75` → `935c9c3`
- **文件**: deploy.sh（新文件，131 行）
- **内容**:
  - 核心依赖检查（cmake/g++/make），失败时引导安装
  - 可选依赖检查（python3/git/clang/libclang-dev/node/npm/graphviz）
  - 一次性检测所有依赖，汇总列出后再 y/n 确认安装
  - 仅检测运行环境，不包含自动构建步骤
  - 后续移除构建步骤和编译依赖，仅检测可选运行时工具

#### 13. 构建输出目录分离
- **提交**: `e9e04d4`
- **文件**: Src/CMakeLists.txt, build.sh
- **内容**: `CMAKE_RUNTIME_OUTPUT_DIRECTORY` 设为 `build/output/`，分离中间产物与最终可执行文件。

### 五、文档与杂项

#### 14. 同步设计规范与代码实现（第一轮）
- **提交**: `8ec3eff`
- **文件**: Doc/Code_Visualization_Tool_Design_Spec.md
- **内容**: 7 项同步（WebGL→Cytoscape.js、firefox 依赖检查、移除 ECharts 引用、构建路径、数据结构字段对齐、CMakeParser API、TSNode 类型、build_include_graph 描述、Inja 确认）。

#### 15. 删除已完成的任务清单
- **提交**: `0d454ba`
- **内容**: 所有偏差已修复或关闭，不再需要独立跟踪。

---

### 六、设计规格深度同步（2026-05-03 第二轮）

**提交**: 工作区未提交
**文件**: Doc/Code_Visualization_Tool_Design_Spec.md（398 行 diff）

18 项修正，对齐源码实际状态：

| # | 区域 | 修正内容 |
|---|------|---------|
| 1 | **3.2 物理视图** | `index.html` → 通用描述；运行时/查看环境更新 |
| 2 | **3.3 运行视图** | "写入 index.html" → "写入 HTML 报告文件" |
| 3 | **3.5 开发视图** | 移除不存在的 `Test/`、`Architecture.md`；添加 `deploy.sh` |
| 4 | **3.5 表格** | 添加 `Src/CMakeLists.txt`、`deploy.sh` 条目；更新 `Template/`、`build.sh` 描述 |
| 5 | **3.7 处理模块** | C/C++ 解析：移除 Clang LibTooling、AST→CST、补充字段声明/宏 |
| 6 | **3.8 前端渲染** | 移除 D3.js；添加侧边栏折叠 |
| 7 | **4.1.2 图形框架** | 移除未实现的 Vis.js 备选计划 |
| 8 | **4.2.1 RawSymbol** | Kind 枚举补充 `CLASS`/`ENUM_KIND`；新增 9 个字段 |
| 9 | **4.2.1 IndexedData** | 数据流更新为 AnalysisContext 直接传递 |
| 10 | **4.2.2 AnalysisContext** | 添加 `command_line` 字段 |
| 11 | **4.2.2 数据流图** | `IndexedData` → `AnalysisContext` |
| 12 | **4.3.1 CLI** | 添加 `read_file_readonly`；编译目录过滤/编译器推断 |
| 13 | **4.3.4 ParserFrontend** | 签名补 `current_func`；添加 `count_file_lines`/成员变量；分支计数 |
| 14 | **4.3.5 Indexer** | 添加 `resolve_include_file`/`resolve_composite_base_classes`；ExternalRef |
| 15 | **4.3.6 GraphBuilder** | **关键**：扇入扇出移至 BFS 替换前 |
| 16 | **4.3.7 Analyzer** | 添加 `tarjan_dfs`；`compute_hotspots` 修正为仅扇入排序 |
| 17 | **4.3.8 Reporter** | 添加 `find_symbol_name`；update convert_\* 签名；JSON 数据补充 |
| 18 | **4.2.1 异常处理** | CLI::ParseError 改用 app.exit()；解析异常降级 |

---

### 七、2026-05-03 及后续补录

#### 16. ParserFrontend visit_* 迁移自由函数到类方法
- **提交**: `52d8ec5`
- **文件**: ParserFrontend.cpp/.h, todo.md
- **内容**: 所有 visit_* 成员方法从空桩改为实际逻辑，移除文件内静态自由函数；头文件 void* 改用 TSNode 类型，traverse_cst 新增 current_func 参数。

#### 17. Reporter 改用 Inja 模板引擎替代手动字符串替换
- **提交**: `1b4e0a7`
- **文件**: Reporter.cpp, todo.md
- **内容**: 模板占位符从硬编码 `find+replace` 改为 Inja 语法（`{{ var_name }}`），`generate()` 使用 `inja::Environment::render()` 渲染。

#### 18. HTML 节点圆角矩形 + 文件名显示 + 选中色 #D5EE2E
- **提交**: `a634991`
- **文件**: Reporter.cpp, cytoscape_bridge.js, template.html
- **内容**: 节点改为圆角矩形，标签显示"函数名\n(文件名)"，扇入/扇出改为"被调用/调用"，选中边框色和信息框关联色统一为 #D5EE2E，启用 text-wrap 多行标签。

#### 19. Doxygen 注释提取 + HTML 节点信息展示注释 + 布局优化
- **提交**: `61d5771`
- **文件**: ParserFrontend.cpp/.h, DataTypes.h, Indexer.cpp, Reporter.cpp, template.html, cytoscape_bridge.js, test_project 测试文件（8 个）, 设计规格文档
- **内容**: ParserFrontend 提取 Doxygen 注释（`///` 或 `/** */`），comment 字段贯穿 RawSymbol → FunctionSymbol/CompositeSymbol；节点选中时信息框显示完整注释；侧边栏移至右侧，折叠按钮置于 canvas 右上角；节点信息框移至左上角，#D5EE2E 边框关联。

#### 20. 添加 .gitignore，停止跟踪 .html 文件
- **提交**: `88ac359`
- **文件**: .gitignore（新文件）
- **内容**: 创建 `.gitignore` 忽略所有 `*.html` 文件，从 Git 索引中移除已跟踪的 4 个 `.html` 文件，保持磁盘文件不变。

---

### 八、2026-05-12 — 调用图按需展开（dev 分支）

共 **4 个提交**，涉及 **5 个文件**。

#### 1. 调用图懒展开 + 叶节点灰色 + 入口节点高亮
- **文件**: DataTypes.h, GraphBuilder.cpp, Reporter.cpp, cytoscape_bridge.js, template.html
- **后端改动**:
  - `AnalysisContext` 新增 `entry_function_id` 和 `full_call_edges`，GraphBuilder 在 BFS 剪枝前保存完整调用边
  - Reporter 序列化完整调用图 + 入口 ID 到 JSON 元数据
- **前端改动**:
  - **入口节点**: 页面打开仅显示入口函数节点，函数名金色 `#FFD700` 加粗高亮，边框金色
  - **按需展开**: 单击节点后从完整数据中取出下一级边/目标节点，增量 `cy.add()` 添加到图中
  - **径向定位**: 子节点以父节点为中心，固定半径 150px 扇形展开（108° 向下弧），不再全图 dagre 重排
  - **叶节点灰色**: 无下级调用的节点标记 `isDeadEnd` 属性，边框灰色 `#555555`
  - **高亮覆盖**: 节点选中时显示 `#D5EE2E` 绿色高亮，取消选中恢复原色（灰色或红色）
  - **回退模式**: 当 `-e` 指定的入口函数不存在时，全量显示调用图但仍扫描所有节点标记叶节点灰色
- **Bug 修复**: 修复 Cytoscape.js 选择器 `[attr="true"]` 因布尔值 `true != "true"` 样式不匹配的问题，改用存在性选择器 `[attr]`

---

## 文件变更总览（2026-05-12）

| 文件 | 变更类型 | 说明 |
|------|---------|------|
| `Src/Parser/ParserFrontend.cpp` | 1013 行改动 | AST 分支统计、字段/签名提取、行数统计、visit_* 迁移、Doxygen 注释提取 |
| `Src/Parser/ParserFrontend.h` | 102 行改动 | void*→TSNode, 新增方法声明、current_func 参数 |
| `Src/CLI/CLI.cpp` | 87 行改动 | --help 增强、-o 默认值、compiler 检测、command_line |
| `Src/Common/DataTypes.h` | 21 行新增 | ExternalRef, branch_count, lines 字段, command_line, comment 字段 |
| `Src/GraphBuilder/GraphBuilder.cpp` | 69 行改动 | build_include_graph 实现, BFS depth fix |
| `Src/Indexer/Indexer.cpp` | 24 行改动 | 外部符号收集, 行数回填, 目录过滤, comment 映射 |
| `Src/Analyzer/Analyzer.cpp` | 23 行改动 | 新圈复杂度计算, 清理空循环 |
| `Src/Reporter/Reporter.cpp` | 233 行改动 | Inja 模板引擎、字段/签名/外部引用/comment 序列化、节点 label 格式 |
| `Src/Template/template.html` | 62 行改动 | 垂直布局, 侧边栏折叠, 大图降级, 命令参数显示, 信息框位置/样式 |
| `Src/Template/cytoscape_bridge.js` | 42 行改动 | 圆角矩形、多行标签、选中色、侧边栏位置 |
| `deploy.sh` | **新文件** 131 行 | 自动部署/环境检测脚本 |
| `build.sh` | 2 行改动 | 构建路径提示同步 |
| `Src/CMakeLists.txt` | 3 行新增 | 输出目录分离 |
| `Doc/Code_Visualization_Tool_Design_Spec.md` | 2778 + 417 行改动 | 设计规范同步（两轮 + Doxygen 同步） |
| `.gitignore` | **新文件** 1 行 | 忽略所有 *.html 文件 |
| `Src/Common/DataTypes.h` | 4 行新增 | entry_function_id, full_call_edges 字段 |
| `Src/GraphBuilder/GraphBuilder.cpp` | 3 行改动 | 保存完整边和入口 ID |
| `Src/Reporter/Reporter.cpp` | 大量改动 | BRIDGE_JS 重写：懒展开/径向定位/叶节点灰色/入口高亮 |
| `Src/Template/cytoscape_bridge.js` | 大量改动 | 同步 Reporter.cpp BRIDGE_JS 逻辑 |
| `Src/Template/template.html` | 1 行新增 | ni-expand-row 节点信息行 |

---

### 九、2026-05-12 — 布局重构 + Gruvbox 主题 + 浅色模式（第二轮）

#### 页面布局重构
- 左侧调用图 / 右侧卡片面板左右分栏，中间可拖拽分割线（4px，20%~80%）
- 调用图默认 70% / 右侧 30%
- 操作按钮移到标题栏右侧（重置布局 ⟳ / 适应窗口 ⛶ / 主题切换 ☀）
- 调用图始终可见，右侧卡片切换：包含图、类型图、统计分析、符号查询
- 滚轮缩放灵敏度降至 0.15，缩放更细腻

#### Gruvbox 深色主题
- 全面更换配色：页面背景 `#282828`，面板 `#3c3836`，边框 `#504945`
- 强调色 `#d65d0e`（橙），选中 `#fe8019`（亮橙），入口 `#fabd2f`（亮黄）
- 头文件边框 `#83a598`（蓝），源文件 `#b8bb26`（绿），叶节点 `#928374`（灰）

#### Solarized 浅色模式
- 点击「☀ 浅色」按钮切换，背景 `#fdf6e3`，文字 `#657b83`
- 所有页面元素 + Cytoscape 节点/边颜色同步切换
- 按钮图标化，悬停显示 tooltip 提示

#### 文件名显示
- 函数节点：函数名白色 `#ebdbb2`，文件名灰色 `#928374` 单独显示在节点下方
- 文件名格式采用 `fileName.ext`，无括号/横线包裹

---

### 十、2026-05-13 — 节点交互优化 + 边信息面板 + 圆形函数节点（dev 分支）

#### 节点点击交互改进
- **非叶节点**：首次单击仅展开子节点，再次单击才显示详情框
- **叶节点**：单击直接显示详情框
- 移除了旧版每次单击都同时展开 + 显示详情的逻辑

#### 边信息面板
- 单击调用边（箭头）弹出 `#edge-info` 面板，显示 `caller → callee` 标题
- 面板展示被调用函数的返回类型、参数列表（带序号）或"参数: 无"
- 面板在 light 主题下同步适配 Solarized 配色

#### 外部系统调用显示
- 无调用图子节点但有系统库调用的函数，其节点信息框中显示"系统函数调用 (N): xxx, yyy"
- 此类节点在 `expandNode` 中被标记为「已展开」状态

#### 入口节点圆形 + 边框归一化
- 入口节点形状改为 `ellipse`（圆形），与设计规格 DR_1 (2.5.2.1) 对齐
- 其余函数节点保持 `round-rectangle`（圆角矩形）
- 移除入口节点特制的 `border-width: 3` / `border-color: #fabd2f`
- 入口节点边框与其他节点一致（1px, 主题色），仅保留粗体黄色文字区分
- 主题切换 `applyCyTheme` 同步移除入口边框设置

#### 调用边去重
- `convert_call_graph` 使用 `std::map<pair<uint32_t,uint32_t>, uint32_t>` 按 `(caller_id, callee_id)` 合并重复调用边
- 合并后的 `weight` 为各边 `call_count` 之和

#### 共享子节点展开修复
- 当子节点已被其他父节点加载时，expandNode 仍画出调用连线（不因目标已可见而跳过）
- 仅对首次可见的子节点创建节点框和定位，已存在的共享子节点不移动
- 修复 BRIDGE_JS 中 expandNode 函数体末缺少 `}` 导致的语法错误

#### 悬停高亮连线
- 鼠标悬停节点时，其扇入/扇出连线高亮为 `#E1F656`（亮绿）
- 鼠标悬停连线时，该连线及两端节点边框同时高亮为 `#E1F656`
- 移出后恢复样式表颜色（深色 `#504945` / 浅色 `#93a1a1`）

#### 标题栏精简
- 删除副标题"源码可视化分析报告"，仅保留 "codeviz"
- 元信息栏移除字段名（"项目:"、"生成时间:"、"输出:"），仅显示内容值
- 删除"运行命令:"整行

#### 文件变更总览

| 文件 | 变更 |
|------|------|
| `Src/Reporter/Reporter.cpp` | tap/edge handler、entry shape+border、nodeShape、edge-info、dedup、light theme、expandNode 重构、hover 高亮、标题栏精简 |
| `Src/Template/cytoscape_bridge.js` | tap/edge handler、entry shape+border、nodeShape、external refs、theme toggle、expandNode 重构、hover 高亮、标题栏精简 |
| `Doc/Code_Visualization_Tool_Design_Spec.md` | 前端交互说明同步（悬停高亮） |
| `CHANGES.md` | 本文件 |


### 十一、2026-05-13 — 设计规格文档拆分解构 + check_sync.py 独立检查工具

共 **1 个提交** + 工作区文档重构。

#### 1. 添加 check_sync.py 独立检查工具
- **提交**: `c998815`
- **文件**: `extra-tool/check_sync.py`（新文件，401 行）
- **内容**: 独立 Python 脚本，检查源码和生成的 HTML 报告之间的一致性，作为独立验证工具

#### 2. 设计规格文档拆分解构
- 将庞大的单体设计规格 `Doc/Code_Visualization_Tool_Design_Spec.md`（1461 行）拆分为多个专注的独立文档：
  - `Doc/prd.md`（111 行）— 需求定义与场景分析
  - `Doc/architecture.md`（256 行）— 系统架构与 4+1 视图
  - `Doc/data_types.md`（362 行）— 核心数据结构与接口定义
  - `Doc/example.md`（127 行）— 使用示例
  - `Doc/tech_selection.md`（62 行）— 技术选型对比
  - `Doc/modules/`（8 个文件，共 495 行）— 各模块详细设计文档

#### 文件变更总览

| 文件 | 变更 |
|------|------|
| `Doc/Code_Visualization_Tool_Design_Spec.md` | **删除**（1461 行），拆分为多个独立 Doc 文件 |
| `Doc/prd.md` | **新文件** 111 行 |
| `Doc/architecture.md` | **新文件** 256 行 |
| `Doc/data_types.md` | **新文件** 362 行 |
| `Doc/example.md` | **新文件** 127 行 |
| `Doc/tech_selection.md` | **新文件** 62 行 |
| `Doc/modules/*.md` | **8 个新文件** 共 495 行 |
| `extra-tool/check_sync.py` | **新文件** 401 行 |
| `.claude/commands/review-project.md` | **新文件** |
| `.claude/commands/verify.md` | **新文件** |
| `CLAUDE.md` | **新文件** |
