// config.h - 数据结构定义（含嵌套结构体），验证数据结构包含关系提取
#ifndef CONFIG_H
#define CONFIG_H

#include <string>

// 服务器信息结构体 - 被 Config 包含
struct ServerInfo {
    std::string ip;
    int port;
};

// 日志信息结构体 - 被 Config 包含
struct LogInfo {
    std::string path;
    int level;
};

// 配置结构体 - 嵌套包含 ServerInfo 和 LogInfo，形成包含关系
struct Config {
    ServerInfo server;
    LogInfo log;
};

// 配置读取函数声明
Config load_config();
ServerInfo parse_server_info();
LogInfo parse_log_info();

#endif // CONFIG_H
