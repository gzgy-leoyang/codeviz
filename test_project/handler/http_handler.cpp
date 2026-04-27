// handler/http_handler.cpp - 派生类实现，验证派生类方法与基类方法的调用关系
#include "handler/http_handler.h"
#include <iostream>

// 实现基类纯虚函数 - 派生类具体行为
void HttpHandler::handle() {
    // 调用基类方法 name()，形成派生类→基类的方法调用关系
    std::cout << "Handling request for handler: " << name() << std::endl;
    std::cout << "URL: " << url_ << std::endl;
}

// 覆盖基类虚函数 - 返回派生类名称
std::string HttpHandler::name() {
    return "HttpHandler";
}

// 派生类新增方法 - 设置请求 URL
void HttpHandler::set_url(const std::string& url) {
    url_ = url;
}
