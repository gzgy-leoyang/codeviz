# CMake 解析模块

> 来源: 从设计规格说明书 4.3 节提取的 CMake 解析模块 详细设计。
> 原始文档: Doc/Code_Visualization_Tool_Design_Spec.md

#### 4.3.2 CMake 解析模块

##### 职责
解析项目中的 `CMakeLists.txt` 文件，提取构建目标、链接库依赖和编译工具链信息，累积填充到 `BuildMetadata` 结构中。

##### 对外接口

```cpp
class CMakeParser {
public:
    /**
     * 解析单个 CMakeLists.txt 文件，将提取的信息累积到 meta 中
     * @param file CMakeLists.txt 文件内容及路径
     * @param meta 构建元数据（输入输出参数，支持多文件累加）
     * @return 0 表示成功，-1 表示解析失败
     */
    int parse(const CMakeFile& file, BuildMetadata& meta);
};
```
##### 内部函数
| 函数签名 | 功能 |
| :--- | :--- |
| void traverse_cst(TSNode root, BuildMetadata& meta, const std::string& source)	|递归遍历 CST，遇到 normal_command 时派发|
| void handle_normal_command(TSNode node, BuildMetadata& meta, const std::string& source)	|查找 identifier + argument_list，分发到 visit_*|
| void visit_project(TSNode arg_list, BuildMetadata& meta, const std::string& source)	|提取项目名称和版本|
| void visit_cmake_minimum_required(TSNode arg_list, BuildMetadata& meta, const std::string& source)	|提取 cmake_minimum_required 版本号|
| void visit_add_executable(TSNode arg_list, BuildMetadata& meta, const std::string& source)	|提取可执行目标名|
| void visit_add_library(TSNode arg_list, BuildMetadata& meta, const std::string& source)	|提取库目标名|
| void visit_target_link_libraries(TSNode arg_list, BuildMetadata& meta, const std::string& source)	|提取目标及其链接库列表|
| void visit_set_compiler(TSNode arg_list, BuildMetadata& meta, const std::string& source)	|提取 CMAKE_C_COMPILER / CMAKE_CXX_COMPILER|
| void visit_add_subdirectory(TSNode arg_list, BuildMetadata& meta, const std::string& source)	|提取子目录路径|
| std::string get_node_text(TSNode node, const std::string& source)	|从源码中提取节点文本|

##### 主流程步骤
1. 创建 tree-sitter 解析器，设置 tree-sitter-cmake 语言。
2. 调用 ts_parser_parse_string 解析 CMakeFile::content，获取 CST 根节点。
3. traverse_cst 递归遍历 CST，遇到 normal_command 节点时通过 handle_normal_command 分发到 visit_* 处理函数。
4. 各 visit_* 函数通过 flatten_arguments 递归收集 argument_list 中的参数。
5. 将提取的信息填充到 BuildMetadata 中。
6. 对于 add_subdirectory 指令，记录子目录路径（实际读取和递归由 CLI 模块负责）。
7. 返回 0 表示成功，-1 表示解析失败。

##### 依赖的数据结构
- 输入接口数据：CMakeFile
- 输出核心数据：BuildMetadata

##### 异常处理
tree-sitter 解析失败：抛出 std::runtime_error，由 CLI 模块捕获并跳过该文件。
