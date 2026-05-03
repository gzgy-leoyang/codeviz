// config.cpp - 配置读取实现，验证多层函数调用关系
#include "config.h"

/// @brief 解析服务器连接信息
/// 被 load_config() 调用，形成第 3 级调用链
/// @return 填充了默认值的 ServerInfo 结构体
ServerInfo parse_server_info() {
    ServerInfo info;
    info.ip = "127.0.0.1";
    info.port = 8080;
    return info;
}

/// @brief 解析日志配置信息
/// 被 load_config() 调用，形成第 3 级调用链
/// @return 填充了默认值的 LogInfo 结构体
LogInfo parse_log_info() {
    LogInfo info;
    info.path = "/var/log/test.log";
    info.level = 2;
    return info;
}

/// @brief 加载应用配置
/// 内部调用 parse_server_info() 和 parse_log_info()
/// @return 完整的应用配置结构体
Config load_config() {
    Config cfg;
    cfg.server = parse_server_info();
    cfg.log = parse_log_info();
    return cfg;
}
