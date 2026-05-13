# 编译数据库解析模块

> 来源: 从设计规格说明书 4.3 节提取的 编译数据库解析模块 详细设计。
> 原始文档: Doc/Code_Visualization_Tool_Design_Spec.md

#### 4.3.3 编译数据库解析模块
##### 职责
读取 CMake 生成的 `compile_commands.json` 文件，为每个源文件提取编译参数，包括宏定义（`-D`）、头文件搜索路径（`-I`）及其他编译选项，构建文件路径到编译参数的映射表。

##### 对外接口

```cpp
class CompDBParser {
public:
    /**
     * 解析编译数据库
     * @param build_dir 包含 compile_commands.json 的构建目录路径
     * @return 文件绝对路径到编译参数的映射表，若文件不存在则返回空映射
     */
    std::unordered_map<std::string, CompileArgs> parse(const std::string& build_dir);
};
```

##### 内部函数
| 函数签名 | 功能 |
| :--- | :--- |
| std::string find_compile_commands(const std::string& build_dir) | 定位 compile_commands.json 文件路径 |
| CompileArgs parse_entry(const json& entry) | 解析单个编译条目，提取宏、头文件路径、其他选项 |
| std::vector<std::string> extract_defines(const std::string& cmd) | 从命令字符串中提取 -D 宏定义 |
| std::vector<std::string> extract_includes(const std::string& cmd) | 从命令字符串中提取 -I 头文件路径 |
| std::string normalize_path(const std::string& path, const std::string& base_dir) | 将相对路径转换为基于 base_dir 的绝对路径 |

##### 主流程步骤
1. 在 build_dir 下查找 compile_commands.json，若不存在则记录警告并返回空映射。
2. 使用 nlohmann/json 解析 JSON 文件。
3. 遍历 JSON 数组，对每个条目：
   - 获取 file 字段作为源文件路径。
   - 获取 directory 字段作为工作目录，用于路径归一化。
   - 优先使用 arguments 数组字段；若不存在则解析 command 字符串。
   - 从命令内容中提取 -D 和 -I 参数。
   - 将所有路径转换为绝对路径。
   - 存入映射表（键为 file 绝对路径）。
4. 返回映射表。

##### 依赖的数据结构
- 输出接口数据：std::unordered_map<std::string, CompileArgs>

##### 异常处理
- compile_commands.json 不存在：记录警告，返回空映射。
- JSON 解析失败：记录错误，返回已成功解析的部分映射。
- 单个条目缺少 file 字段：记录警告，跳过该条目。
