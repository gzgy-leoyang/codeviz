# /review-project — 项目审视 + CHANGES.md 更新 + 规范同步 + Git 提交

## 步骤

### 1. 项目审视

- 执行 `git status` 和 `git log --oneline -30` 了解当前工作区和提交历史
- 执行 `./build.sh` 确认项目构建通过
- 检查是否存在未跟踪文件、构建警告等异常
- 若构建失败，输出错误日志并停止

### 2. CHANGES.md 更新

- 读取 `CHANGES.md`，定位最近更新日期和末尾位置
- 分析 `git log` 中自上次 CHANGES.md 更新以来的所有提交（`git log --oneline --since="<last_date>"` 或比对 HEAD~ 范围）
- 按现有格式（分节、编号、文件变更表格）整理新增修改到 CHANGES.md
- 更新文件头部「最近更新」日期

### 3. 设计规范同步

根据本次修改涉及的源文件，按模块映射更新对应的 Doc 文件：

| 源文件 | 对应 Doc 文档 |
|--------|-------------|
| `Src/CLI/*` | `Doc/modules/cli.md` |
| `Src/CMakeParser/*` | `Doc/modules/cmake_parser.md` |
| `Src/CompDBParser/*` | `Doc/modules/compdb_parser.md` |
| `Src/Parser/*` | `Doc/modules/parser.md` |
| `Src/Indexer/*` | `Doc/modules/indexer.md` |
| `Src/GraphBuilder/*` | `Doc/modules/graph_builder.md` |
| `Src/Analyzer/*` | `Doc/modules/analyzer.md` |
| `Src/Reporter/*` + `Src/Template/*` | `Doc/modules/reporter.md` |
| `Src/Common/DataTypes.h` | `Doc/data_types.md` |
| 跨模块功能变更 | `Doc/architecture.md`、`Doc/prd.md` |

- 遍历修改的文件，识别涉及的模块，更新对应的 Doc/*.md 文件以匹配当前代码实现

### 4. 用户确认（硬约束）

- 汇总展示所有待提交的变更（git diff --stat）
- **暂停执行，等待用户确认后方可继续**

### 5. Git 提交到 main

- `git add` 相关变更文件
- 提交信息格式：`docs: 项目审视 + CHANGES.md 更新 + 设计规范同步`，附详细变更说明
- 汇报结果：提交哈希、变更文件数、新增/删除行数

### 6. 更新内存状态记录

- 在 `~/.claude/projects/-home-dd-Works-ReadSrc/memory/project_state.md` 中更新 HEAD 提交、日期和变更摘要
- 确保 MEMORY.md 中的索引行反映最新状态
