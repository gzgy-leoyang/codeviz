# 顶层构建配置 - 验证库依赖提取与编译数据库生成
cmake_minimum_required(VERSION 3.10)
project(test_project LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 生成 compile_commands.json，用于源码可视化分析工具提取编译信息
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(test_app
    main.cpp
    utils.cpp
    config.cpp
    logger.cpp
    handler/base_handler.cpp
    handler/http_handler.cpp
    common/types.cpp
)

target_include_directories(test_app PRIVATE ${CMAKE_SOURCE_DIR})

# 通过 target_compile_definitions 定义 DEBUG_MODE 宏，验证条件编译检测
target_compile_definitions(test_app PRIVATE DEBUG_MODE)

# 显式链接 pthread 库，验证库依赖提取功能
target_link_libraries(test_app PRIVATE pthread)
