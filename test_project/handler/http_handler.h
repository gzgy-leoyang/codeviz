// handler/http_handler.h - 派生类定义，验证类继承关系和成员函数提取
#ifndef HTTP_HANDLER_H
#define HTTP_HANDLER_H

#include "handler/base_handler.h"
#include <string>

/// @brief HTTP 请求处理器
/// 继承自 BaseHandler，验证类继承关系和派生类方法提取
class HttpHandler : public BaseHandler {
public:
    HttpHandler() = default;

    /// @brief 实现基类纯虚函数，处理 HTTP 请求
    void handle() override;

    /// @brief 覆盖基类虚函数，返回派生类名称
    /// @return "HttpHandler"
    std::string name() override;

    /// @brief 设置请求 URL（派生类新增方法）
    /// @param url 目标 URL
    void set_url(const std::string& url);

private:
    std::string url_;  ///< 请求 URL
};

#endif // HTTP_HANDLER_H
