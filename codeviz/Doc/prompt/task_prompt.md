请按照设计文档:
Code_Visualization_Tool Design Spec.md
逐步实现以下模块,每个模块实现完毕后，输出完整的 .h 和 .cpp 文件内容。

## 实现顺序
请严格按照以下顺序实现：

### 第 0 步：项目骨架搭建
1. 创建目录结构
2. 编写顶层 CMakeLists.txt
3. 编写 Src/CMakeLists.txt
4. 编写 build.sh
5. 编写 .gitignore
6. 编写 README.md

### 第 1 步：核心数据结构定义
在 Src/ 下创建 Common/ 目录，实现以下头文件：
- Common/DataTypes.h → 包含所有核心数据结构（4.2.2 节）和接口数据结构（4.2.1 节）
- 不需要 .cpp 文件（纯头文件）

### 第 2 步：编译数据库解析模块（依赖最少）
实现文件：
- CompDBParser/CompDBParser.h
- CompDBParser/CompDBParser.cpp

### 第 3 步：CMake 解析模块
实现文件：
- CMakeParser/CMakeParser.h
- CMakeParser/CMakeParser.cpp

### 第 4 步：C/C++ 解析前端
实现文件：
- Parser/ParserFrontend.h
- Parser/ParserFrontend.cpp

### 第 5 步：符号索引模块
实现文件：
- Indexer/Indexer.h
- Indexer/Indexer.cpp

### 第 6 步：图构建模块
实现文件：
- GraphBuilder/GraphBuilder.h
- GraphBuilder/GraphBuilder.cpp

### 第 7 步：分析引擎
实现文件：
- Analyzer/Analyzer.h
- Analyzer/Analyzer.cpp

### 第 8 步：HTML 报告生成器
实现文件：
- Reporter/Reporter.h
- Reporter/Reporter.cpp
- Template/template.html → 内嵌的 HTML 模板字符串常量
- Template/cytoscape_bridge.js → 内嵌的 JS 桥接代码

### 第 9 步：CLI 入口模块
实现文件：
- CLI/CLI.h
- CLI/CLI.cpp
- 包含 main 函数

## 核对清单
每完成一个模块，请对照设计文档确认：
- [ ] 类名和方法签名与 4.3 节"对外接口"一致
- [ ] 使用 4.2 节定义的数据结构，不自行发明新结构
- [ ] 异常处理策略与 4.3 节"异常处理"一致
- [ ] 当前模块的输入/输出与 3.1 节逻辑视图的数据流一致