// common/types.h - 公共类型定义与宏开关，验证宏定义检测和循环包含检测
#ifndef COMMON_TYPES_H
#define COMMON_TYPES_H

#include <string>

// 宏定义 - 验证宏提取功能（放在 include 之前，确保 base_handler.h 可用）
#define MAX_CONNECTIONS 100
#define API_VERSION "1.0"

// 公共类型枚举（放在 include 之前，确保 base_handler.h 中可使用 StatusCode）
enum class StatusCode {
    OK = 0,
    ERROR = 1,
    TIMEOUT = 2
};

// 循环依赖：types.h 包含 base_handler.h，而 base_handler.h 也包含 types.h
// 使用 #ifndef 保护避免编译错误，但工具应能检测出循环包含关系
#include "handler/base_handler.h"

// 使用宏包裹的函数声明
#ifdef DEBUG_MODE
void debug_print_status(StatusCode code);
#endif

StatusCode check_connection(int count);

#endif // COMMON_TYPES_H
