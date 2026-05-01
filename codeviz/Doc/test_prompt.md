
# 生成测试项目 (test_project/) 的 AI Agent 提示词

## 任务目标
生成一个简单的 C++ 被测项目，用于全面验证“源码可视化分析工具”的各项功能。项目需包含函数调用、数据结构包含、类继承、头文件包含（含循环依赖）、宏定义、条件编译、库依赖等场景。

## 项目结构要求

test_project/
├── CMakeLists.txt # 顶层构建配置
├── main.cpp # 程序入口
├── utils.h # 工具函数声明
├── utils.cpp # 工具函数实现
├── config.h # 数据结构定义 (包含嵌套结构体)
├── config.cpp # 配置读取实现
├── logger.h # 日志模块 (宏定义)
├── logger.cpp # 日志实现
├── handler/
│ ├── base_handler.h # 基类定义
│ ├── base_handler.cpp # 基类实现
│ ├── http_handler.h # 派生类定义
│ └── http_handler.cpp # 派生类实现
├── common/
│ ├── types.h # 公共类型定义 (宏开关)
│ └── types.cpp # 公共类型实现
└── build/ # 构建产物 (自动生成)


## 详细设计约束

### 1. CMakeLists.txt
- 项目名为 `test_project`，C++17 标准。
- 将源文件组织为可执行文件 `test_app`。
- `target_link_libraries` 添加系统库 `pthread`（或显式不链接任何外部库），用于验证库依赖提取。
- 定义宏 `DEBUG_MODE` 通过 `target_compile_definitions`。

### 2. 函数调用关系 (多层调用与回溯)
- `main()` 调用 `init_app()` (在 utils.cpp 中定义)
- `init_app()` 调用 `load_config()` (config.cpp) 和 `setup_logger()` (logger.cpp)
- `load_config()` 内部调用 `parse_server_info()` 和 `parse_log_info()`
- 产生清晰的3级以上调用链，便于测试展开深度与回溯。

### 3. 数据结构包含关系
- `config.h` 中定义：
  - `struct ServerInfo { std::string ip; int port; };`
  - `struct LogInfo { std::string path; int level; };`
  - `struct Config { ServerInfo server; LogInfo log; };`
- 形成嵌套包含关系，`Config` 包含 `ServerInfo` 和 `LogInfo`。

### 4. C++ 类继承与成员函数
- `handler/base_handler.h` 中定义抽象基类 `BaseHandler`，包含纯虚函数 `void handle() = 0;` 和虚函数 `std::string name()`。
- `handler/http_handler.h` 中定义 `HttpHandler : public BaseHandler`，实现 `handle()` 和 `name()`，并增加成员函数 `void set_url(const std::string& url)`。
- 派生类中调用基类方法，形成类关系图。

### 5. 头文件包含与循环依赖
- `utils.h` 包含 `config.h`，`logger.h` 包含 `utils.h`，形成一条包含链。
- 创造循环依赖：`common/types.h` 包含 `handler/base_handler.h`，而 `handler/base_handler.h` 又包含 `common/types.h`（使用 `#ifndef` 保护，但工具应能检测出循环包含）。

### 6. 宏定义与条件编译
- `logger.h` 中使用 `#ifdef DEBUG_MODE` 条件编译输出不同日志级别。
- `common/types.h` 中定义 `#define MAX_CONNECTIONS 100` 和 `#define API_VERSION "1.0"`。
- 在函数内部使用宏包裹部分代码。

### 7. 静态库引用
- CMakeLists.txt 中显式添加 `target_link_libraries(test_app PRIVATE pthread)`（或选择一个常见库如 `jsoncpp`），确保编译数据库中有 `-l` 信息。

### 8. 编译数据库生成
- CMakeLists.txt 开启 `set(CMAKE_EXPORT_COMPILE_COMMANDS ON)`，确保构建后生成 `compile_commands.json`。

## 生成要求
- 所有代码文件必须语法正确，能够编译通过。
- 需要在每个 `.cpp` 和 `.h` 文件中添加简要注释，说明其在测试场景中的作用。
- 输出项目时，请将文件内容以 Markdown 代码块形式给出，并标注文件路径。

请开始生成测试项目。