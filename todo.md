# Codeviz 实现偏差修复清单

## 一、架构/模块设计偏差（优先级：高）

### 1. ~~CMakeParser 使用正则替代 tree-sitter-cmake~~ ✅ 已修复
- **问题**: tree-sitter-cmake 遍历函数均为空桩，实际靠 `parse_with_regex()` 正则回退
- **涉及文件**: `Src/CMakeParser/CMakeParser.h`, `Src/CMakeParser/CMakeParser.cpp`
- **修改内容**:
  - 头文件: 参数类型从 `void*` 改为 `TSNode`；移除 `parse_with_regex` 声明；新增 `handle_normal_command`、`visit_cmake_minimum_required`、`visit_add_subdirectory`
  - 实现: 实现完整的 tree-sitter-cmake CST 递归遍历（`traverse_cst` → `handle_normal_command` → 各 `visit_*`）；移除正则回退；参数提取采用递归式 `flatten_arguments` 遍历隐藏的 `_paren_argument` 节点
- **验证**: 编译无警告；对 test_project 运行正常，成功提取 project_name=test_project, cmake_version=3.10, target=test_app, link_libs=pthread；HTML 报告正常生成

### 2. ParserFrontend 用自由函数替代类方法
- **问题**: 所有 `visit_*` 成员方法均为空桩，实际解析逻辑在文件内静态自由函数中
- **涉及文件**: `Src/Parser/ParserFrontend.h`, `Src/Parser/ParserFrontend.cpp`
- **目标**: 将自由函数逻辑迁移到类方法中，使类接口正常工作

### 3. Reporter 使用字符串替换而非 Inja 模板引擎
- **问题**: 手动 `std::string::find+replace` 替代了 Inja 模板渲染
- **涉及文件**: `Src/Reporter/Reporter.cpp`
- **目标**: 改用 Inja 模板引擎渲染 HTML

### 4. ECharts 未集成
- **问题**: 技术选型指定 ECharts 为补充图表库，但未实现
- **涉及文件**: `Src/Template/template.html`, `Src/Template/cytoscape_bridge.js`, `Src/Reporter/Reporter.cpp`
- **目标**: 集成 ECharts 作为补充图表（或从文档中移除该选型）

### 5. GraphBuilder::build_include_graph 为空桩
- **问题**: 函数声明存在但无实际操作
- **涉及文件**: `Src/GraphBuilder/GraphBuilder.cpp`
- **目标**: 实现 include graph 构建逻辑（或确认 Indexer 已完成该工作后清理此函数）

---

## 二、功能缺失（优先级：高）

### 6. 圈复杂度占位逻辑
- **问题**: 使用 `callees.size() + 1` 而非实际 AST 分支节点计数
- **涉及文件**: `Src/Analyzer/Analyzer.cpp`
- **目标**: 统计 `if/for/while/switch/case/&&/||` 等分支节点计算真实圈复杂度

### 7. 数据对象成员详情未提取
- **问题**: `FieldInfo` 结构体完整但 `visit_field_declaration` 为空桩，成员名/类型/偏移/字节长度均未提取
- **涉及文件**: `Src/Parser/ParserFrontend.cpp`, `Src/Parser/ParserFrontend.h`
- **目标**: 实现字段声明解析，填充 FieldInfo 数据

### 8. 外部库调用分析未实现
- **问题**: 无法分析 `.so`/`.a` 库调用，外部符号仅标记为 `UINT32_MAX`
- **涉及文件**: `Src/Indexer/Indexer.cpp`, `Src/GraphBuilder/GraphBuilder.cpp`
- **目标**: 实现外部库符号关联分析

### 9. 函数签名细节未提取
- **问题**: `return_type`、`parameters`、`is_virtual`、`is_static`、`is_inline` 均未填充
- **涉及文件**: `Src/Parser/ParserFrontend.cpp`
- **目标**: 实现 `visit_function_declarator` 提取完整函数签名

---

## 三、基础设施缺失（优先级：中）

### 10. 缺少自动部署脚本
- **问题**: 无依赖检查/自动安装脚本
- **涉及文件**: 新建 `deploy.sh` 或类似脚本
- **目标**: 检查 python3、clang、libclang-dev、graphviz、npm 等依赖并引导安装

### 11. 测试目录为空
- **问题**: `Test/` 目录无任何测试代码
- **涉及文件**: `Test/` 全目录
- **目标**: 按规范建立 `Test/CLI/`、`Test/Parser/`、`Test/Indexer/` 等测试子目录并编写测试

### 12. 未实现大规模图降级机制
- **问题**: 超过 1000 图元时未启用 WebGL 或虚拟滚动
- **涉及文件**: `Src/Template/cytoscape_bridge.js`
- **目标**: 实现 1000 节点阈值 WebGL 切换或虚拟滚动

---

## 四、微偏差（优先级：低）

| # | 问题 | 说明 |
|:---|:---|:---|
| 13 | CLI.h 额外声明 `read_file_readonly` | 未在规范内部函数表中列出，确认是否需要保留 |
| 14 | Indexer 额外方法 | `resolve_include_file`、`resolve_composite_base_classes` 未在规范中 |
| 15 | Reporter 额外方法 `find_symbol_name` | 未在规范中 |
| 16 | Analyzer `tarjan_dfs` | 未在规范内部函数表中列出（合理实现细节，酌情处理） |
