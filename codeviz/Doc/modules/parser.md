# C/C++ 解析前端

> 来源: 从设计规格说明书 4.3 节提取的 C/C++ 解析前端 详细设计。
> 原始文档: Doc/Code_Visualization_Tool_Design_Spec.md

#### 4.3.4 C/C++ 解析前端

##### 职责

基于 tree-sitter 对单个源文件进行语法分析，遍历 CST 提取函数定义、函数调用、结构体/类定义、宏定义、include 指令等原始符号信息，产出 FileParseResult 供符号索引模块消费。

##### 对外接口

```cpp
class ParserFrontend {
public:
    /**
     * 解析单个源文件
     * @param source 源文件内容及路径
     * @param args 编译参数（宏、头文件路径，当前版本保留以备后续扩展）
     * @return 该文件的原始符号和引用关系
     */
    FileParseResult parse_file(const SourceFile& source, const CompileArgs& args);
};
```

##### 内部函数

| 函数签名 | 功能 |
| :--- | :--- |
| void init_parser(const std::string& file_ext) | 根据扩展名选择 tree-sitter-c 或 tree-sitter-cpp 解析器 |
| void traverse_cst(TSNode node, FileParseResult& result, const std::string& source, std::vector<std::string>& scope, RawSymbol* current_func) | 深度优先遍历 CST，维护作用域栈；current_func 指向当前函数符号（收集 callee 和分支用）|
| void visit_function_definition(TSNode node, FileParseResult& result, const std::string& source, std::vector<std::string>& scope) | 处理函数定义，创建 RawSymbol(kind=FUNC)，提取返回类型和签名，进入函数体遍历 |
| void visit_function_declarator(TSNode node, RawSymbol& sym, const std::string& source) | 提取函数签名（参数列表、virtual/static/inline 修饰）|
| void visit_call_expression(TSNode node, FileParseResult& result, const std::string& source, RawSymbol* current_func) | 处理函数调用，追加到 current_func 的 callee_names |
| void visit_struct_specifier(TSNode node, FileParseResult& result, const std::string& source, std::vector<std::string>& scope) | 处理结构体定义，设置 current_composite_ 后遍历字段体 |
| void visit_class_specifier(TSNode node, FileParseResult& result, const std::string& source, std::vector<std::string>& scope) | 处理类定义，含基类列表（base_class_clause）|
| void visit_field_declaration(TSNode node, RawSymbol& sym, const std::string& source) | 提取成员字段信息（类型、字段名、访问修饰符）|
| void visit_preproc_def(TSNode node, FileParseResult& result, const std::string& source) | 处理宏定义，创建 RawSymbol(kind=MACRO) |
| void visit_preproc_include(TSNode node, FileParseResult& result, const std::string& source) | 处理 #include 指令，记录 (includer, includee) |
| static void count_file_lines(const std::string& content, int& total, int& code, int& comment) | 统计文件总行数、代码行数、注释行数 |
| std::string get_node_text(TSNode node, const std::string& source) | 提取节点对应的源码文本 |
| std::string get_qualified_name(const std::string& base, const std::vector<std::string>& scope) | 拼接完全限定名 |
| void enter_scope(std::vector<std::string>& scope, const std::string& name) | 进入作用域 |
| void exit_scope(std::vector<std::string>& scope) | 退出作用域 |

##### 成员变量
| 变量 | 类型 | 用途 |
| :--- | :--- | :--- |
| is_cpp_ | bool | 标记当前解析语言是否为 C++ |
| current_access_ | AccessSpecifier | 当前类体内的访问修饰符（public/protected/private）|
| current_composite_ | RawSymbol* | 当前正在遍历的结构体/类符号（字段收集目标）|
| last_comment_ | std::string | 最近一个 Doxygen comment 节点内容 |
| last_comment_line_ | uint32_t | 最近一个 Doxygen comment 的结束行号 |

##### 主流程步骤

1. 根据源文件扩展名选择对应的 tree-sitter 语言（.c → C，.cpp/.hpp → C++），初始化解析器，重置 last_comment_。
2. 调用 ts_parser_parse_string 解析 SourceFile::content，获取 CST 根节点。
3. 从根节点开始深度优先遍历 CST，维护作用域栈和当前函数指针：
   - comment → 仅提取 Doxygen 格式注释（`///` 或 `/**`），累积连续多行到 last_comment_。
   - function_definition → visit_function_definition：创建 RawSymbol(kind=FUNC)，关联前导 Doxygen 注释；提取返回类型、参数、virtual/static/inline 标记；进入函数体前清除 last_comment_ 防止体内注释泄漏。
   - call_expression → visit_call_expression：提取被调用者名称，追加到 current_func->callee_names。
   - if/for/while/do/switch/case/conditional_expression → current_func->branch_count++（分支节点统计，用于圈复杂度）。
   - field_declaration → visit_field_declaration：当 current_composite_ 非空时提取字段信息。
   - struct_specifier → 创建 RawSymbol(kind=STRUCT)，设置 current_composite_ 后遍历字段体。
   - class_specifier → 创建 RawSymbol(kind=CLASS)，提取基类列表（base_class_clause），设置 current_composite_ 后遍历字段体。
   - preproc_def / preproc_function_def → 创建 RawSymbol(kind=MACRO)。
   - preproc_include → 记录 (includer, includee) 包含关系。
   - access_specifier → 更新 current_access_ 为对应的枚举值。
4. 调用 count_file_lines 统计文件行数并写入 FileParseResult。
5. 返回 FileParseResult（内含 RawSymbol、includes、行数统计）。

##### 依赖的数据结构

输入接口数据：SourceFile、CompileArgs

输出接口数据：FileParseResult（内含 std::vector<RawSymbol> 和 includes 列表）

##### 异常处理

- 不支持的源文件扩展名：抛出 std::runtime_error，由 CLI 捕获并跳过该文件。
- tree-sitter 解析失败（返回 NULL 或含 ERROR 节点）：记录警告，返回空的 FileParseResult。
- 节点文本提取失败：记录警告，使用空字符串或占位符代替，不中止解析。
