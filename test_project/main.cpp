// main.cpp - 程序入口，验证完整调用链：main() -> init_app() -> load_config()/setup_logger() -> parse_*()
#include "utils.h"
#include "logger.h"
#include "handler/http_handler.h"
#include "common/types.h"
#include <iostream>

/// @brief 程序入口
/// 验证完整调用链：main() -> init_app() -> load_config()/setup_logger() -> parse_*()
/// @return 0 表示正常退出
int main() {
    // 调用 init_app()，触发整个调用链
    init_app();

    // 使用 HttpHandler，验证类继承关系
    HttpHandler handler;
    handler.set_url("http://localhost:8080/api");
    handler.handle();

    // 使用公共类型和宏
    StatusCode status = check_connection(50);
    std::cout << "API Version: " << API_VERSION << std::endl;
    std::cout << "Max Connections: " << MAX_CONNECTIONS << std::endl;

    // 使用日志宏
    LOG_INFO("Application started successfully");
    LOG_DEBUG("This is a debug message");

    return 0;
}
