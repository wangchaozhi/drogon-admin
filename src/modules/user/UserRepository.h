#pragma once
//
// Repository：SQLite 异步实现。通过 Drogon 默认 DbClient 访问。
// 对外接口采用回调形式，保持与 Drogon 异步模型一致，避免在 HTTP 线程阻塞。
//
#include "dto/UserDto.h"
#include <drogon/orm/DbClient.h>
#include <functional>
#include <memory>
#include <optional>
#include <string>

namespace modules::user {

using DbErrCb = std::function<void(const std::string& /*errMsg*/)>;

class UserRepository {
public:
    UserRepository();

    // 查询：成功回调 optional 为空表示记录不存在
    void findById(int64_t id,
                  std::function<void(std::optional<dto::UserDto>)> onOk,
                  DbErrCb onErr);

    // 插入：成功回调返回含自增 id 的 UserDto
    void insert(const std::string& name,
                const std::string& email,
                std::function<void(dto::UserDto)> onOk,
                DbErrCb onErr);

private:
    drogon::orm::DbClientPtr db() const;
};

} // namespace modules::user
