#include "RbacService.h"

namespace modules::rbac {

RbacService& RbacService::instance() {
    static RbacService inst;
    return inst;
}

RbacService::UserCache RbacService::getUserView(int64_t userId) {
    {
        std::lock_guard<std::mutex> lk(mu_);
        auto it = viewCache_.find(userId);
        if (it != viewCache_.end()) return it->second;
    }
    // 读 DB 时不持锁，避免阻塞
    UserCache v;
    v.roles = repo_.getUserRoleCodes(userId);
    v.perms = repo_.getUserPermCodes(userId);
    std::string s;
    s.reserve(v.perms.size() * 16 + 1);
    s.push_back('|');
    for (const auto& c : v.perms) { s.append(c); s.push_back('|'); }
    v.permsStr = std::move(s);

    std::lock_guard<std::mutex> lk(mu_);
    viewCache_[userId] = v;
    return v;
}

std::vector<dto::MenuDto> RbacService::getUserMenus(int64_t userId) {
    {
        std::lock_guard<std::mutex> lk(mu_);
        auto it = menuCache_.find(userId);
        if (it != menuCache_.end()) return it->second;
    }
    auto tree = repo_.getUserMenuTree(userId);

    std::lock_guard<std::mutex> lk(mu_);
    menuCache_[userId] = tree;
    return tree;
}

void RbacService::invalidateUser(int64_t userId) {
    std::lock_guard<std::mutex> lk(mu_);
    viewCache_.erase(userId);
    menuCache_.erase(userId);
}

void RbacService::invalidateAll() {
    std::lock_guard<std::mutex> lk(mu_);
    viewCache_.clear();
    menuCache_.clear();
}

} // namespace modules::rbac
