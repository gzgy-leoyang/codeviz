// config.h - 数据结构定义（含嵌套结构体），验证数据结构包含关系提取
#ifndef CONFIG_H
#define CONFIG_H

#include <string>

/// @brief 服务器连接信息
/// 包含 IP 地址和端口号，被 Config 结构体嵌套包含
struct ServerInfo {
    std::string ip;
    int port;
};

/// @brief 日志配置信息
/// 包含日志文件路径和日志级别，被 Config 结构体嵌套包含
struct LogInfo {
    std::string path;
    int level;
};

/// @brief 应用配置
/// 嵌套包含 ServerInfo 和 LogInfo，验证数据结构包含关系提取
struct Config {
    ServerInfo server;
    LogInfo log;
};

/// @brief 加载完整配置
/// 内部调用 parse_server_info() 和 parse_log_info()
/// @return 填充完成的 Config 结构体
Config load_config();

/// @brief 解析服务器信息
/// @return 服务器信息结构体
ServerInfo parse_server_info();

/// @brief 解析日志信息
/// @return 日志信息结构体
LogInfo parse_log_info();

#endif // CONFIG_H
