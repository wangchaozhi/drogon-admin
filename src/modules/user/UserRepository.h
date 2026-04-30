#pragma once
//
// Repository：基于 Drogon 协程的 SQLite 访问层。所有方法均返回
// drogon::Task<T>，在 co_await 下执行 execSqlCoro，不阻塞任何 event loop。
//
#include "dto/UserDto.h"
#include <drogon/orm/DbClient.h>
#include <drogon/utils/coroutine.h>
#include <optional>
#include <string>
#include <vector>

namespace modules::user {

// 带密码哈希的用户记录（仅服务端内部使用，不对外暴露）
struct UserRecord {
    dto::UserDto user;
    std::string  passwordHash;
};

class UserRepository {
public:
    UserRepository() = default;

    drogon::Task<std::optional<dto::UserDto>> findById(int64_t id);

    drogon::Task<std::optional<UserRecord>>   findByEmail(std::string email);

    drogon::Task<dto::UserDto>                insert(std::string name,
                                                     std::string email,
                                                     std::string passwordHash);

    struct PageResult {
        std::vector<dto::UserDto> items;
        int64_t total{0};
    };

    drogon::Task<PageResult> listPaged(int page, int pageSize, std::string keyword);

    drogon::Task<bool> deleteById(int64_t id);

    // 更新基础资料；passwordHash 为空时不改密码
    drogon::Task<std::optional<dto::UserDto>> updateById(int64_t id,
                                                         std::string name,
                                                         std::string email,
                                                         std::string passwordHash);

    // 系统是否已存在管理员（user_roles 至少一条 role_id=1 的绑定）
    drogon::Task<bool> hasAnyAdmin();

private:
    drogon::orm::DbClientPtr db() const;
};

} // namespace modules::user
