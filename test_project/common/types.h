// common/types.h - 公共类型定义与宏开关，验证宏定义检测和循环包含检测
#ifndef COMMON_TYPES_H
#define COMMON_TYPES_H

#include <string>

// 宏定义 - 验证宏提取功能（放在 include 之前，确保 base_handler.h 可用）
#define MAX_CONNECTIONS 100
#define API_VERSION "1.0"

/// @brief 状态码枚举
/// 定义操作返回状态，OK/ERROR/TIMEOUT
enum class StatusCode {
    OK = 0,
    ERROR = 1,
    TIMEOUT = 2
};

// 循环依赖：types.h 包含 base_handler.h，而 base_handler.h 也包含 types.h
// 使用 #ifndef 保护避免编译错误，但工具应能检测出循环包含关系
#include "handler/base_handler.h"

/// @brief 调试状态打印（仅在 DEBUG_MODE 下编译）
/// @param code 要打印的状态码
#ifdef DEBUG_MODE
void debug_print_status(StatusCode code);
#endif

/// @brief 检查连接数是否超过上限
/// @param count 当前连接数
/// @return 连接数正常返回 OK，超过 MAX_CONNECTIONS 返回 ERROR
StatusCode check_connection(int count);

#endif // COMMON_TYPES_H
