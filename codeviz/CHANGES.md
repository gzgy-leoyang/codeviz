# codeviz 项目修改汇总

> 首次提交: 2026-04-27
> 代码重组织: 2026-05-02 (21251c3)
> 最近更新: 2026-05-03

---

## 2026-05-03 修改汇总

共 **15 个提交**，涉及 **15 个文件**，新增 **2442 行**，删除 **1879 行**。

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

## 文件变更总览（2026-05-03）

| 文件 | 变更类型 | 说明 |
|------|---------|------|
| `Src/Parser/ParserFrontend.cpp` | 576 行改动 | AST 分支统计、字段/签名提取、行数统计 |
| `Src/Parser/ParserFrontend.h` | 51 行改动 | void*→TSNode, 新增方法声明 |
| `Src/CLI/CLI.cpp` | 87 行改动 | --help 增强、-o 默认值、compiler 检测、command_line |
| `Src/Common/DataTypes.h` | 18 行新增 | ExternalRef, branch_count, lines 字段, command_line |
| `Src/GraphBuilder/GraphBuilder.cpp` | 69 行改动 | build_include_graph 实现, BFS depth fix |
| `Src/Indexer/Indexer.cpp` | 22 行改动 | 外部符号收集, 行数回填, 目录过滤 |
| `Src/Analyzer/Analyzer.cpp` | 23 行改动 | 新圈复杂度计算, 清理空循环 |
| `Src/Reporter/Reporter.cpp` | 119 行改动 | 字段/签名/外部引用序列化 |
| `Src/Template/template.html` | 20 行改动 | 垂直布局, 侧边栏折叠, 大图降级, 命令参数显示 |
| `deploy.sh` | **新文件** 131 行 | 自动部署/环境检测脚本 |
| `build.sh` | 2 行改动 | 构建路径提示同步 |
| `Src/CMakeLists.txt` | 3 行新增 | 输出目录分离 |
| `Doc/Code_Visualization_Tool_Design_Spec.md` | 2778 + 398 行改动 | 设计规范同步（两轮） |
