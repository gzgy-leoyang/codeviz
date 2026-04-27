// common/types.cpp - 公共类型实现，验证宏条件编译在函数内的使用
#include "common/types.h"
#include <iostream>

#ifdef DEBUG_MODE
// 调试模式下的状态打印 - 验证条件编译检测
void debug_print_status(StatusCode code) {
    switch (code) {
        case StatusCode::OK:
            std::cout << "[DEBUG] Status: OK" << std::endl;
            break;
        case StatusCode::ERROR:
            std::cout << "[DEBUG] Status: ERROR" << std::endl;
            break;
        case StatusCode::TIMEOUT:
            std::cout << "[DEBUG] Status: TIMEOUT" << std::endl;
            break;
    }
}
#endif

// 检查连接数是否超过 MAX_CONNECTIONS 宏定义的上限
StatusCode check_connection(int count) {
#ifdef DEBUG_MODE
    debug_print_status(StatusCode::OK);
#endif
    if (count > MAX_CONNECTIONS) {
        return StatusCode::ERROR;
    }
    return StatusCode::OK;
}
