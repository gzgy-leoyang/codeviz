// ============================================================
// test_cli.cpp — CLI 模块单元测试
//
// 学习指引:
//   本文件演示了如何对"读入参数 → 校验 → 扫描文件 → 读取文件"这一
//   流程中的每个步骤编写独立测试。建议按以下顺序阅读:
//
//   第1层: 辅助函数 (make_temp_dir, create_temp_file) — 测试基础设施
//   第2层: parse_arguments 测试 — 最纯粹的"输入→输出"测试
//   第3层: validate_arguments 测试 — 正常 + 异常路径
//   第4层: scan_source_files 测试 — 涉及文件系统的测试
//   第5层: read_file_readonly / ensure_output_dir 测试 — 文件读写
//
// 关键概念:
//   TEST_CASE("名称")  — 定义一个测试用例，doctest 自动注册它
//   CHECK(表达式)      — 断言表达式为真，失败不中断后续测试
//   CHECK_FALSE(expr)  — 断言表达式为假
//   CHECK_NOTHROW(fn)  — 断言函数不抛异常
//   CHECK_THROWS_AS(fn, 异常类型) — 断言函数抛出指定类型异常
//   REQUIRE(表达式)    — 类似 CHECK，但失败立即中止当前测试（用于必要条件）
//   SUBCASE            — 在一个 TEST_CASE 内细分不同场景（本文件未使用）
//
// 原则:
//   1. 每个 TEST_CASE 测试一个独立场景，互不依赖
//   2. 测试创建自己的临时文件/目录，结束后清理
//   3. 正常路径和异常路径都要覆盖
// ============================================================

// doctest.h — 单头文件测试框架。不需要 .a 或 .so 链接库，
// 只需要这个头文件就获得了完整的 TEST_CASE / CHECK 等宏。
// 注意这里没有定义 DOCTEST_CONFIG_IMPLEMENT，因为 test_main.cpp 已经定义了。
// 一个项目中只能有一个 .cpp 定义 IMPLEMENT（生成 main），
// 其他测试文件只包含头文件（注册 TEST_CASE 供 main 调用）。
#include "doctest.h"

// 被测模块的头文件——CLI.h 声明了我们即将测试的 6 个自由函数:
//   parse_arguments()     — 解析命令行参数
//   validate_arguments()  — 校验参数合法性
//   scan_source_files()   — 递归扫描源文件
//   ensure_output_dir()   — 确保输出目录存在
//   read_file_readonly()  — 以只读方式读取文件
// 以及数据结构 CommandLineArgs
#include "CLI/CLI.h"

#include <filesystem>
#include <fstream>
#include <cstdio>

namespace fs = std::filesystem;


// ============================================================
// 辅助函数
//
// 为什么需要辅助函数?
//   被测函数中，有些需要真实的文件系统（如 scan_source_files 需要读取目录）。
//   我们不能用生产代码中的真实目录做测试（数据不可控、可能误删文件），
//   所以每个测试自己创建临时目录/文件，用完即删。
//   这就是"测试夹具(Test Fixture)"——为测试搭建的可控环境。
// ============================================================

// make_temp_dir — 创建临时目录
// mkdtemp: Linux 系统调用，根据模板创建唯一临时目录并返回路径。
// 模板的结尾 XXXXXX 会被替换为随机字符，避免多测试并行时的命名冲突。
// REQUIRE: 如果创建失败，继续执行后面的测试没有意义，所以用 REQUIRE 立即中止。
static std::string make_temp_dir() {
    char dir_template[] = "/tmp/codeviz_test_XXXXXX";
    auto* dir = mkdtemp(dir_template);
    REQUIRE(dir != nullptr);
    return std::string(dir);
}

// create_temp_file — 在指定目录下创建文件并写入内容
// 用于为 read_file_readonly、scan_source_files 等函数准备输入数据。
// REQUIRE(ofs.is_open()) — 文件创建失败必须立即中止，否则后续断言都会失败。
static std::string create_temp_file(const std::string& dir, const std::string& name, const std::string& content = "") {
    auto path = dir + "/" + name;
    std::ofstream ofs(path);
    REQUIRE(ofs.is_open());
    ofs << content;
    ofs.close();
    return path;
}


// ============================================================
// parse_arguments 测试
//
// 被测函数签名:
//   CommandLineArgs parse_arguments(int argc, char* argv[])
//
// 这个函数是"纯函数"——给定 argc/argv，输出 CommandLineArgs。
// 不读写文件，不依赖全局状态，是最容易测试的一类函数。
// 策略: 构造不同的 argv 数组，调用 parse_arguments，检查返回值每个字段是否正确。
//
// 特别注意:
//   parse_arguments 内部使用了 CLI::App 库。
//   它给 -p 参数添加了 CLI::ExistingDirectory 校验，
//   所以测试时 -p 必须传一个真实存在的目录（如 /tmp）。
//   如果传不存在的目录，CLI::App 会调用 exit() 终止进程，测试也会被杀死。
// ============================================================

TEST_CASE("CLI::parse_arguments - 基本参数") {
    // argc/argv 模拟用户在命令行输入: codeviz -p /tmp
    // argv[0] 是程序名（惯例），argv[1] 起是实际参数
    const char* argv[] = {"codeviz", "-p", "/tmp"};

    // 调用被测函数，argc = 3（数组元素个数）
    auto args = parse_arguments(3, const_cast<char**>(argv));

    // CHECK 逐个验证返回值的每个字段是否符合预期
    // 如果某个 CHECK 失败，不会中断测试——其他 CHECK 仍然执行
    CHECK(args.project_path == "/tmp");     // -p 指定的路径
    CHECK(args.expand_depth == 2);          // 未指定 -d，应为默认值 2
    CHECK(args.entry_function == "main");   // 未指定 -e，应为默认值 "main"
    CHECK_FALSE(args.verbose);              // 未指定 -v，应为 false
    CHECK(args.output_path == "");          // 未指定 -o，应为空字符串（main 中会补默认值）
}

TEST_CASE("CLI::parse_arguments - 全参数") {
    // 所有参数都指定，验证每个字段都能正确解析
    // 注意 argc = 8，因为 argv 有 8 个元素（下标 0~7）
    const char* argv[] = {"codeviz", "-p", "/tmp", "-e", "run", "-d", "5", "-v"};
    auto args = parse_arguments(8, const_cast<char**>(argv));

    CHECK(args.project_path == "/tmp");     // -p /tmp
    CHECK(args.entry_function == "run");    // -e run
    CHECK(args.expand_depth == 5);          // -d 5
    CHECK(args.verbose);                    // -v 存在，应为 true
}

TEST_CASE("CLI::parse_arguments - 长选项") {
    // 验证长选项格式 (--xxx) 也能正确解析
    const char* argv[] = {"codeviz", "--project", "/tmp", "--entry", "run",
                          "--depth", "3", "--output", "/tmp/r.html"};
    auto args = parse_arguments(9, const_cast<char**>(argv));

    CHECK(args.project_path == "/tmp");
    CHECK(args.entry_function == "run");
    CHECK(args.expand_depth == 3);
    CHECK(args.output_path == "/tmp/r.html");
}


// ============================================================
// validate_arguments 测试
//
// 被测函数签名:
//   void validate_arguments(const CommandLineArgs& args)
//
// 这个函数校验参数是否合法: 路径存在且是目录、depth 在 1~20 之间。
// 校验失败时抛出 std::invalid_argument。
//
// 测试策略:
//   正常路径: CHECK_NOTHROW — 断言函数执行不抛异常
//   异常路径: CHECK_THROWS_AS — 断言函数抛出指定类型的异常
//
// 注意:
//   validate_arguments 成功时内部会调 spdlog::info 输出日志，
//   这就是为什么 test_main.cpp 中要先调 init_logger(false)。
// ============================================================

TEST_CASE("CLI::validate_arguments - 有效参数") {
    CommandLineArgs args;
    args.project_path = "/tmp";     // /tmp 总是存在且是目录
    args.expand_depth = 5;          // 在 [1, 20] 范围内

    // CHECK_NOTHROW: 验证函数没有抛异常
    CHECK_NOTHROW(validate_arguments(args));
}

TEST_CASE("CLI::validate_arguments - depth 越下界") {
    CommandLineArgs args;
    args.project_path = "/tmp";
    args.expand_depth = 0;           // 小于 1，应抛异常

    // CHECK_THROWS_AS: 验证函数抛出了指定类型的异常
    // 这里验证异常类型是 std::invalid_argument
    CHECK_THROWS_AS(validate_arguments(args), std::invalid_argument);
}

TEST_CASE("CLI::validate_arguments - depth 越上界") {
    CommandLineArgs args;
    args.project_path = "/tmp";
    args.expand_depth = 21;          // 大于 20，应抛异常

    CHECK_THROWS_AS(validate_arguments(args), std::invalid_argument);
}

TEST_CASE("CLI::validate_arguments - 路径不存在") {
    CommandLineArgs args;
    args.project_path = "/tmp/_codeviz_nonexistent_xyz_";  // 故意写一个不存在的路径
    args.expand_depth = 2;

    CHECK_THROWS_AS(validate_arguments(args), std::invalid_argument);
}

TEST_CASE("CLI::validate_arguments - 路径是文件而非目录") {
    auto tmp_dir = make_temp_dir();
    auto file_path = create_temp_file(tmp_dir, "test.txt");  // 创建文件

    CommandLineArgs args;
    args.project_path = file_path;      // 路径指向一个文件，不是目录
    args.expand_depth = 2;

    // validate_arguments 内部会检查 fs::is_directory()，文件应导致异常
    CHECK_THROWS_AS(validate_arguments(args), std::invalid_argument);

    // 清理: 删除临时目录
    // 这个 cleanup 无论测试通过与否都会执行（C++ 局部变量析构）
    fs::remove_all(tmp_dir);
}


// ============================================================
// scan_source_files 测试
//
// 被测函数签名:
//   std::vector<std::string> scan_source_files(const std::string& root)
//
// 功能: 递归扫描目录，返回所有 C/C++ 源文件路径列表。
// 过滤规则:
//   1. 只识别 .c/.cpp/.cxx/.cc/.h/.hpp/.hxx/.hh 扩展名
//   2. 跳过 /build/ /CMakeFiles/ 等构建目录下的文件
//
// 测试策略:
//   创建临时目录，放入不同文件组合，验证返回值是否符合过滤规则。
//   这称为"基于状态的测试"——构造输入状态，验证输出状态。
// ============================================================

TEST_CASE("CLI::scan_source_files - 空目录") {
    auto tmp_dir = make_temp_dir();     // 创建空目录

    auto files = scan_source_files(tmp_dir);

    CHECK(files.empty());               // 空目录应返回空列表

    fs::remove_all(tmp_dir);
}

TEST_CASE("CLI::scan_source_files - 包含源文件") {
    auto tmp_dir = make_temp_dir();
    create_temp_file(tmp_dir, "main.cpp");     // 合法扩展名
    create_temp_file(tmp_dir, "utils.h");      // 合法扩展名
    create_temp_file(tmp_dir, "README.md");    // 非法扩展名（应被过滤掉）

    auto files = scan_source_files(tmp_dir);

    // 只应该找到 2 个文件（.cpp 和 .h），.md 被过滤
    CHECK(files.size() == 2);

    // 验证返回值中确实包含 .cpp 和 .h 文件
    bool has_cpp = false, has_h = false;
    for (const auto& f : files) {
        if (f.find(".cpp") != std::string::npos) has_cpp = true;
        if (f.find(".h") != std::string::npos) has_h = true;
    }
    CHECK(has_cpp);
    CHECK(has_h);

    fs::remove_all(tmp_dir);
}

TEST_CASE("CLI::scan_source_files - 跳过构建目录") {
    auto tmp_dir = make_temp_dir();
    create_temp_file(tmp_dir, "main.cpp");               // 正常源文件
    fs::create_directory(tmp_dir + "/build");
    create_temp_file(tmp_dir + "/build", "generated.cpp"); // 构建产物（应被跳过）

    auto files = scan_source_files(tmp_dir);

    // 应该只找到 1 个文件: main.cpp
    // generated.cpp 在 build/ 下，应被过滤
    CHECK(files.size() == 1);

    // 进一步验证: 结果中不包含 "build" 路径
    CHECK(files[0].find("build") == std::string::npos);

    fs::remove_all(tmp_dir);
}

TEST_CASE("CLI::scan_source_files - 跳过 CMakeFiles 目录") {
    auto tmp_dir = make_temp_dir();
    create_temp_file(tmp_dir, "main.cpp");
    fs::create_directories(tmp_dir + "/CMakeFiles");
    create_temp_file(tmp_dir + "/CMakeFiles", "CMakeCompilerId.cpp"); // 应被跳过

    auto files = scan_source_files(tmp_dir);

    // 应该只找到 main.cpp
    CHECK(files.size() == 1);

    fs::remove_all(tmp_dir);
}


// ============================================================
// read_file_readonly 测试
//
// 被测函数签名:
//   std::string read_file_readonly(const std::string& path)
//
// 功能: 以 O_RDONLY 方式打开文件，读取全部内容返回。
// 要求: 必须只读打开（不修改文件内容和时间戳）。
//
// 测试策略:
//   用 create_temp_file 准备已知内容的文件，读取后验证内容匹配。
//   同时测试文件不存在时的异常抛出。
// ============================================================

TEST_CASE("CLI::read_file_readonly - 成功读取") {
    auto tmp_dir = make_temp_dir();
    auto file_path = create_temp_file(tmp_dir, "test.txt", "hello world");

    auto content = read_file_readonly(file_path);

    CHECK(content == "hello world");  // 读取内容应与写入内容一致

    fs::remove_all(tmp_dir);
}

TEST_CASE("CLI::read_file_readonly - 读取空文件") {
    auto tmp_dir = make_temp_dir();
    auto file_path = create_temp_file(tmp_dir, "empty.txt");  // 不传 content = 空内容

    auto content = read_file_readonly(file_path);

    CHECK(content.empty());           // 空文件应返回空字符串

    fs::remove_all(tmp_dir);
}

TEST_CASE("CLI::read_file_readonly - 文件不存在") {
    // 不存在的路径应抛 std::runtime_error
    CHECK_THROWS_AS(read_file_readonly("/tmp/_codeviz_nonexistent_file_"), std::runtime_error);
}


// ============================================================
// ensure_output_dir 测试
//
// 被测函数签名:
//   void ensure_output_dir(const std::string& path)
//
// 功能: 确保输出文件所在的目录存在，如果不存在则创建。
// 输入是"输出文件路径"，内部提取父目录路径再创建。
//
// 注意:
//   ensure_output_dir("/tmp/a/b/output.html") 会创建 /tmp/a/b/ 目录
//   如果输出文件就在已存在的目录中（如 /tmp/output.html），则什么都不做
// ============================================================

TEST_CASE("CLI::ensure_output_dir - 新建目录") {
    auto tmp_dir = make_temp_dir();
    auto sub_dir = tmp_dir + "/new_subdir";

    // 验证目录还不存在
    CHECK_FALSE(fs::exists(sub_dir));

    // 调用被测函数——传入 output.html 路径，期望它创建父目录
    ensure_output_dir(sub_dir + "/output.html");

    // 验证目录已被创建
    CHECK(fs::exists(sub_dir));
    CHECK(fs::is_directory(sub_dir));

    fs::remove_all(tmp_dir);
}

TEST_CASE("CLI::ensure_output_dir - 目录已存在") {
    auto tmp_dir = make_temp_dir();

    // 目录已存在，should not throw
    CHECK_NOTHROW(ensure_output_dir(tmp_dir + "/output.html"));

    fs::remove_all(tmp_dir);
}


// ============================================================
// 扩展阅读: doctest 更多用法
//
// 1. SUBCASE — 在一个 TEST_CASE 内细分多场景:
//      TEST_CASE("解析参数") {
//        SUBCASE("短选项") { ... }
//        SUBCASE("长选项") { ... }
//      }
//    每个 SUBCASE 独立运行，共享 TEST_CASE 的 setup/cleanup。
//
// 2. REQUIRE 与 CHECK 的区别:
//    CHECK(expr)  — 失败后继续执行（一个测试可以报告多个失败）
//    REQUIRE(expr) — 失败后立即中止当前测试（用于"没有这个值后续断言全无意义"的场景）
//
// 3. INFO / CAPTURE — 调试辅助:
//    INFO("正在测试 depth=", d);  // 断言失败时输出这段信息
//    CAPTURE(args.project_path);   // 失败时打印变量名和值
//
// 4. 浮点数比较:
//    CHECK(doc::Approx(0.1 + 0.2) == 0.3);  // 自动处理精度
//
// 5. 测试过滤:
//    ./codeviz_test -ts="CLI"    // 只运行名字包含 "CLI" 的测试
//    ./codeviz_test -ts="CLI::parse*"  // 通配符匹配
// ============================================================
