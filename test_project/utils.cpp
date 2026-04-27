// utils.cpp - 工具函数实现，验证多层函数调用链 main() -> init_app() -> load_config()/setup_logger()
#include "utils.h"
#include "logger.h"
#include <iostream>

// 打印配置信息 - 展示嵌套结构体的访问
void print_config(const Config& cfg) {
    std::cout << "Server: " << cfg.server.ip << ":" << cfg.server.port << std::endl;
    std::cout << "Log: " << cfg.log.path << " level=" << cfg.log.level << std::endl;
}

// 应用初始化 - 被 main() 调用，内部调用 load_config() 和 setup_logger()，形成调用链
void init_app() {
    std::cout << "Initializing app..." << std::endl;

    // 调用 load_config()（config.cpp），形成第2级调用
    Config cfg = load_config();
    print_config(cfg);

    // 调用 setup_logger()（logger.cpp），形成第2级调用
    setup_logger(cfg.log.path, cfg.log.level);

    std::cout << "App initialized." << std::endl;
}
