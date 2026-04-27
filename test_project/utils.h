// utils.h - 工具函数声明，验证头文件包含链（utils.h -> config.h）
#ifndef UTILS_H
#define UTILS_H

#include "config.h"  // 包含 config.h，形成包含链
#include <string>

// 应用初始化函数 - 被 main() 调用，内部调用 load_config() 和 setup_logger()
void init_app();

// 打印配置信息
void print_config(const Config& cfg);

#endif // UTILS_H
