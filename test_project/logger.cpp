// logger.cpp - 日志实现，验证条件编译在函数实现中的使用
#include "logger.h"
#include <iostream>

/// @brief 初始化日志系统
/// 根据 DEBUG_MODE 宏决定是否输出调试信息
/// @param path 日志文件路径
/// @param level 日志级别
void setup_logger(const std::string& path, int level) {
#ifdef DEBUG_MODE
    std::cout << "[DEBUG] Logger setup: path=" << path << ", level=" << level << std::endl;
#else
    std::cout << "Logger setup complete." << std::endl;
#endif
}

/// @brief 输出格式化的日志消息
/// @param level 日志级别
/// @param msg 日志内容
void log_message(const std::string& level, const std::string& msg) {
    std::cout << "[" << level << "] " << msg << std::endl;
}
