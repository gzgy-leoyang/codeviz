// handler/base_handler.h - 抽象基类定义，验证类继承关系提取与循环包含检测
#ifndef BASE_HANDLER_H
#define BASE_HANDLER_H

#include <string>
// 循环依赖：base_handler.h 包含 types.h，而 types.h 也包含 base_handler.h
#include "common/types.h"

/// @brief 请求处理抽象基类
/// HttpHandler 将继承此类，验证类继承关系提取
class BaseHandler {
public:
    virtual ~BaseHandler() = default;

    /// @brief 处理请求（纯虚函数）
    /// 派生类必须实现此方法
    virtual void handle() = 0;

    /// @brief 获取处理器名称
    /// @return 返回 "BaseHandler"，派生类可覆盖
    virtual std::string name() {
        return "BaseHandler";
    }

protected:
    StatusCode status_ = StatusCode::OK;  ///< 当前处理状态
};

#endif // BASE_HANDLER_H
