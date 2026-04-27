// logger.cpp - 日志实现，验证条件编译在函数实现中的使用
#include "logger.h"
#include <iostream>

// 日志初始化 - 被 init_app() 调用
void setup_logger(const std::string& path, int level) {
#ifdef DEBUG_MODE
    std::cout << "[DEBUG] Logger setup: path=" << path << ", level=" << level << std::endl;
#else
    std::cout << "Logger setup complete." << std::endl;
#endif
}

// 日志输出 - 根据 level 参数输出日志
void log_message(const std::string& level, const std::string& msg) {
    std::cout << "[" << level << "] " << msg << std::endl;
}
