# CLI 入口模块

> 来源: 从设计规格说明书 4.3 节提取的 CLI 入口模块 详细设计。
> 原始文档: Doc/Code_Visualization_Tool_Design_Spec.md

#### 4.3.1 CLI 入口模块

##### 职责
解析命令行参数，校验输入，调度各模块完成分析流程。

##### 对外接口
```cpp
int main(int argc, char* argv[]);
```

##### 内部函数

| 函数签名 | 功能 |
| :--- | :--- |
| CommandLineArgs parse_arguments(int argc, char* argv[]) | 解析命令行参数（基于 CLI11）|
| void validate_arguments(const CommandLineArgs& args) | 校验参数合法性（路径/深度范围）|
| void init_logger(bool verbose) | 初始化日志等级 |
| std::vector<std::string> scan_source_files(const std::string& root) | 递归扫描源文件（跳过构建目录）|
| void ensure_output_dir(const std::string& path) | 确保输出目录存在 |
| std::string read_file_readonly(const std::string& path) | 以只读方式打开文件（O_RDONLY）|

##### 主流程步骤
1. 解析并校验命令行参数；若未指定 -o，默认输出到 `<project_path>.html`。
2. 初始化日志系统。
3. 扫描项目目录，获取源文件列表（跳过 /build/ /CMakeFiles/ 等目录）。
4. 若存在 CMakeLists.txt，调用 CMakeParser 解析（含递归处理 add_subdirectory）。
5. 若存在 compile_commands.json，调用 CompDBParser 解析；若 CMakeLists.txt 未显式指定编译器，从 command 字段推断。
6. 对每个源文件调用 ParserFrontend::parse_file（只读打开）。
7. 调用 Indexer::build_index 构建符号表。
8. 调用 GraphBuilder::build 构建图数据。
9. 调用 Analyzer::analyze 执行统计分析。
10. 调用 Reporter::generate 生成 HTML 报告并写入文件。

##### 依赖的数据结构
- CommandLineArgs（定义在 DataTypes.h 中）
- SourceFile, CMakeFile, CompileArgs（接口数据）
- AnalysisContext, BuildMetadata（核心数据）

##### 异常处理
- 参数校验失败：CLI::ParseError 通过 app.exit() 输出；其余抛出 std::invalid_argument。
- 文件读取失败：记录警告并跳过该文件，不中止后续分析。
- 解析模块异常：捕获并降级处理。
