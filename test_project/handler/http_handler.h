// handler/http_handler.h - 派生类定义，验证类继承关系和成员函数提取
#ifndef HTTP_HANDLER_H
#define HTTP_HANDLER_H

#include "handler/base_handler.h"
#include <string>

// HttpHandler 继承 BaseHandler - 验证继承关系提取
class HttpHandler : public BaseHandler {
public:
    HttpHandler() = default;

    // 实现基类纯虚函数
    void handle() override;

    // 覆盖基类虚函数
    std::string name() override;

    // 派生类新增成员函数
    void set_url(const std::string& url);

private:
    std::string url_;
};

#endif // HTTP_HANDLER_H
