// common/types.cpp - 公共类型实现，验证宏条件编译在函数内的使用
#include "common/types.h"
#include <iostream>

#ifdef DEBUG_MODE
/// @brief 调试模式下打印状态码
/// @param code 要打印的状态码枚举值
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

/// @brief 检查连接数是否超过 MAX_CONNECTIONS 上限
/// @param count 当前连接数
/// @return 连接数正常返回 OK，超过返回 ERROR
StatusCode check_connection(int count) {
#ifdef DEBUG_MODE
    debug_print_status(StatusCode::OK);
#endif
    if (count > MAX_CONNECTIONS) {
        return StatusCode::ERROR;
    }
    return StatusCode::OK;
}
