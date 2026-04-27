// config.cpp - 配置读取实现，验证多层函数调用关系
#include "config.h"

// 解析服务器信息 - 被 load_config() 调用，形成第3级调用链
ServerInfo parse_server_info() {
    ServerInfo info;
    info.ip = "127.0.0.1";
    info.port = 8080;
    return info;
}

// 解析日志信息 - 被 load_config() 调用，形成第3级调用链
LogInfo parse_log_info() {
    LogInfo info;
    info.path = "/var/log/test.log";
    info.level = 2;
    return info;
}

// 加载配置 - 被 init_app() 调用，内部再调用 parse_server_info() 和 parse_log_info()
Config load_config() {
    Config cfg;
    cfg.server = parse_server_info();
    cfg.log = parse_log_info();
    return cfg;
}
