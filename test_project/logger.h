// logger.h - 日志模块声明，验证宏定义与条件编译检测，以及包含链（logger.h -> utils.h -> config.h）
#ifndef LOGGER_H
#define LOGGER_H

#include "utils.h"  // 包含 utils.h，形成包含链 logger.h -> utils.h -> config.h
#include <string>

// 条件编译 - 根据 DEBUG_MODE 宏输出不同日志级别
#ifdef DEBUG_MODE
    #define LOG_LEVEL "DEBUG"
    #define LOG_DEBUG(msg) log_message("DEBUG", msg)
#else
    #define LOG_LEVEL "RELEASE"
    #define LOG_DEBUG(msg)
#endif

#define LOG_INFO(msg) log_message("INFO", msg)
#define LOG_ERROR(msg) log_message("ERROR", msg)

// 日志初始化函数 - 被 init_app() 调用
void setup_logger(const std::string& path, int level);

// 日志输出函数
void log_message(const std::string& level, const std::string& msg);

#endif // LOGGER_H
