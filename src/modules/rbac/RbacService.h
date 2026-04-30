#pragma once
//
// Rbac 业务层 + 用户级别缓存（协程版）。
// 管理接口透传给 RbacRepository；热路径（AuthFilter / 登录）使用缓存
// 避免每次请求打 DB。任何会影响用户权限的写操作都应显式 invalidate。
//
#include "RbacRepository.h"
#include <drogon/utils/coroutine.h>
#include <mutex>
#include <unordered_map>
#include <string>
#include <vector>

namespace modules::rbac {

class RbacService {
public:
    static RbacService& instance();

    RbacRepository& repo() { return repo_; }

    struct UserCache {
        std::vector<std::string> roles;      // 角色 code 列表
        std::vector<std::string> perms;      // 权限 code 列表
        std::string              permsStr;   // "|dashboard:view|user:view|..."
    };

    // 带缓存的用户权限视图
    drogon::Task<UserCache> getUserView(int64_t userId);

    // 带缓存的用户菜单树（随权限一起失效）
    drogon::Task<std::vector<dto::MenuDto>> getUserMenus(int64_t userId);

    // 用户被分配角色 / 用户被删除 → 调用（纯内存，同步）
    void invalidateUser(int64_t userId);
    // 角色权限 / 菜单 发生变动 → 全量失效（纯内存，同步）
    void invalidateAll();

private:
    RbacService() = default;

    RbacRepository repo_;
    std::mutex mu_;
    std::unordered_map<int64_t, UserCache> viewCache_;
    std::unordered_map<int64_t, std::vector<dto::MenuDto>> menuCache_;
};

} // namespace modules::rbac
