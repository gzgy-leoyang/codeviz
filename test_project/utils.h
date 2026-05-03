// utils.h - 工具函数声明，验证头文件包含链（utils.h -> config.h）
#ifndef UTILS_H
#define UTILS_H

#include "config.h"  // 包含 config.h，形成包含链
#include <string>

/// @brief 应用初始化
/// 被 main() 调用，内部调用 load_config() 和 setup_logger()
void init_app();

/// @brief 打印配置信息
/// @param cfg 应用配置结构体
void print_config(const Config& cfg);

#endif // UTILS_H
