// ============================================================
// test_main.cpp — 单元测试入口文件
//
// 作用:
//   本文件是整个测试程序（codeviz_test）的入口点。
//   它替代了常规程序的 main()，转而启动 doctest 测试运行器。
//
// doctest 工作机制:
//   1. 在包含 doctest.h 之前定义 DOCTEST_CONFIG_IMPLEMENT，
//      告诉 doctest "本文件负责生成 main() 的实现代码"
//   2. 其他测试文件（如 test_cli.cpp）只写 #include "doctest.h"（不定义 IMPLEMENT），
//      它们只注册 TEST_CASE，不生成 main()
//   3. 链接时，所有 TEST_CASE 自动注册到全局，由本文件的 ctx.run() 统一执行
//
// 对比常规 main()——常规程序:
//   int main() { 解析参数; 扫描文件; 分析代码; 生成报告; return 0; }
//
// 测试 main()——本文件:
//   int main() { 初始化; 启动测试运行器; return 测试结果; }
//   它不再执行业务逻辑，而是让 doctest 去发现和执行所有 TEST_CASE
// ============================================================

// DOCTEST_CONFIG_IMPLEMENT: "我负责生成 main()"
// 不加这个宏的话，doctest.h 只是声明了 TEST_CASE 等宏，但不生成 main()
// 整个项目中只能有一个 .cpp 文件定义这个宏
#define DOCTEST_CONFIG_IMPLEMENT

// doctest 是单头文件框架——只引入这一个头文件就获得了全部功能
// 它提供了: TEST_CASE / CHECK / CHECK_THROWS / REQUIRE / SUBCASE 等宏
#include "doctest.h"

// 被测模块的头文件——测试需要调用它的函数
// CLI.h 声明了 parse_arguments()、validate_arguments() 等函数
#include "CLI/CLI.h"

// main() 函数 —— 测试程序的入口
// argc/argv 由 ctest 或用户在命令行传入，
// 例如: ./codeviz_test --success  （显示每个通过的断言）
//        ./codeviz_test --help    （查看 doctest 所有选项）
//        ./codeviz_test -ts="CLI" （只运行名称包含 "CLI" 的测试）
int main(int argc, char** argv) {
    // --------------------------------------------------------
    // 初始化 spdlog 日志系统
    // 被测函数（如 validate_arguments、scan_source_files）内部调用了
    // spdlog::info() 来输出日志。如果不提前初始化，spdlog 可能崩溃。
    // 因此测试启动时先调一次 init_logger(false)，让日志系统就绪。
    // --------------------------------------------------------
    init_logger(false);

    // --------------------------------------------------------
    // doctest::Context — 测试运行上下文
    // 类比 CLI::App 是命令行参数解析器，doctest::Context 是测试运行器。
    // 它负责:
    //   1. 解析命令行参数（--help, --success, -ts 等）
    //   2. 执行所有已注册的 TEST_CASE
    //   3. 汇总测试结果并返回退出码
    // --------------------------------------------------------
    doctest::Context ctx;

    // 将命令行参数传递给 doctest，让它处理自身的参数
    // 例如: ./codeviz_test --success -ts="CLI::parse"
    // doctest 会从中识别 --success 和 -ts 并相应调整行为
    ctx.applyCommandLine(argc, argv);

    // no-breaks: 测试失败时不触发调试器断点
    // 默认情况下，如果测试在调试器中运行，失败时会触发断点。
    // 设 true 后即使附加了调试器也不中断，适合命令行自动运行。
    ctx.setOption("no-breaks", true);

    // --------------------------------------------------------
    // ctx.run() — 执行所有测试用例
    // 这是核心调用——doctest 遍历所有已注册的 TEST_CASE，
    // 逐一执行，收集每个 CHECK / CHECK_THROWS 的断言结果。
    //
    // 返回: 0 表示全部通过，非 0 表示有失败
    // 这个返回值直接作为进程退出码，ctest 根据退出码判断测试是否通过
    // --------------------------------------------------------
    return ctx.run();

    // 执行流程总结:
    // build.sh → ctest → codeviz_test → main() [本文件]
    //   → init_logger(false)
    //   → doctest::Context::run()
    //     → TEST_CASE("CLI::parse_arguments - 基本参数") [来自 test_cli.cpp]
    //       → parse_arguments(3, argv)    // 调用被测函数
    //       → CHECK(args.project_path == "/tmp")  // 验证结果
    //     → TEST_CASE("CLI::validate_arguments - depth 越下界")
    //       → ...
    //     → 所有 TEST_CASE 执行完毕
    //   → return 0 / return 1
}
