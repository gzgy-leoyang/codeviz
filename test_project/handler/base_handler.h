// handler/base_handler.h - 抽象基类定义，验证类继承关系提取与循环包含检测
#ifndef BASE_HANDLER_H
#define BASE_HANDLER_H

#include <string>
// 循环依赖：base_handler.h 包含 types.h，而 types.h 也包含 base_handler.h
#include "common/types.h"

// 抽象基类 - HttpHandler 将继承此类
class BaseHandler {
public:
    virtual ~BaseHandler() = default;

    // 纯虚函数 - 派生类必须实现
    virtual void handle() = 0;

    // 虚函数 - 派生类可覆盖
    virtual std::string name() {
        return "BaseHandler";
    }

protected:
    StatusCode status_ = StatusCode::OK;
};

#endif // BASE_HANDLER_H
