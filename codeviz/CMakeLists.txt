cmake_minimum_required(VERSION 3.16)
project(codeviz VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 第三方库
add_subdirectory(3rdparty)

# 源码
add_subdirectory(Src)
