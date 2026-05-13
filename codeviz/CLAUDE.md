# codeviz — C/C++ 源码静态分析与可视化工具

基于 tree-sitter，分析 C/C++ 项目生成交互式 HTML 报告（调用图/包含图/类型图/统计分析）。

---

## 快速命令

```bash
./build.sh                          # Debug 构建，产物在 build/output/codeviz
./build.sh --release                # Release 构建
./build/output/codeviz -p /path -e main -d 5 -o report.html
```

## 项目结构

各模块按分析流水线顺序排列，数据通过 `AnalysisContext` 传递：

| 模块 | 职责 | 详细设计文档 |
|------|------|-------------|
| `CLI/` | main()、参数解析、调度全流程 | `Doc/modules/cli.md` |
| `CMakeParser/` | 解析 CMakeLists.txt 提取项目名/编译器/链接库 | `Doc/modules/cmake_parser.md` |
| `CompDBParser/` | 解析 compile_commands.json 提取编译参数 | `Doc/modules/compdb_parser.md` |
| `Parser/` | tree-sitter CST 遍历提取符号（函数/类/宏/调用/包含） | `Doc/modules/parser.md` |
| `Indexer/` | 两遍遍历：建符号表 → 解析引用边 | `Doc/modules/indexer.md` |
| `GraphBuilder/` | BFS 调用图展开、包含图、类型依赖图、扇入扇出 | `Doc/modules/graph_builder.md` |
| `Analyzer/` | 圈复杂度计算、Tarjan SCC 循环包含检测、热力图 | `Doc/modules/analyzer.md` |
| `Reporter/` | JSON 序列化 + Inja 模板渲染 → HTML 报告 | `Doc/modules/reporter.md` |

## 代码规范

- 文件/目录/类名：PascalCase，方法/变量：snake_case，成员变量后缀 `_`
- 头文件声明含对应设计文档章节号，源文件每个关键步骤输出 spdlog INFO 日志
- 跨模块数据结构定义在 `Common/DataTypes.h`，不自行发明
- 错误处理：非致命失败 `try-catch` + `spdlog::warn` 降级，致命失败 `spdlog::error` + `return 1`

## 硬约束

- **Git 工作流**：修改完成后必须经人工确认才能 merge 到 main 分支，否则保持当前分支。严禁未经确认直接合并或 force push
- **修改后验证**：每次修改完成后，自动执行验证流程：
  1. `./build.sh` 构建项目
  2. 构建成功后，`./build/output/codeviz -p /home/dd/Works/ReadSrc/test_project/ -o /tmp/codeviz_test_report.html`
  3. `firefox /tmp/codeviz_test_report.html` 打开报告
  4. 构建失败则先修复再继续

## 关键坑点（Claude 容易犯错的地方）

1. **tree-sitter-cpp parser.c 约 17MB**，首次编译约 30 秒——不要中断
2. **可执行文件在 `build/output/codeviz`**，不在 `build/` 下
3. **`NLOHMANN_JSON_NAMESPACE_NO_VERSION=1`** 必须定义，否则前向声明歧义
4. **.h 头文件统一用 tree-sitter-cpp 解析**——纯 C 项目中可能产生额外节点
5. **扇入扇出计算必须在 BFS 替换 call_edges 之前**——顺序不可互换
6. **full_call_edges**：GraphBuilder 在 BFS 剪枝前保存完整调用边，供前端按需展开
7. **Cytoscape.js 选择器**：布尔属性用 `[attr]`，不要用 `[attr="true"]`（`true != "true"`）
