// handler/http_handler.cpp - 派生类实现，验证派生类方法与基类方法的调用关系
#include "handler/http_handler.h"
#include <iostream>

/// @brief 处理 HTTP 请求
/// 实现基类纯虚函数，调用基类 name() 方法
void HttpHandler::handle() {
    // 调用基类方法 name()，形成派生类→基类的方法调用关系
    std::cout << "Handling request for handler: " << name() << std::endl;
    std::cout << "URL: " << url_ << std::endl;
}

/// @brief 获取处理器名称
/// @return "HttpHandler"
std::string HttpHandler::name() {
    return "HttpHandler";
}

/// @brief 设置请求 URL
/// @param url 目标 URL 地址
void HttpHandler::set_url(const std::string& url) {
    url_ = url;
}
