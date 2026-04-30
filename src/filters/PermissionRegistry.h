#pragma once
//
// 路由 → 权限码 映射全局单例。
// 由各 Controller 在构造函数里调用 PermissionRegistry::instance().bind(...) 注册，
// 运行时 PermissionFilter 查表：命中则校验 attributes 里的 permissions，否则放行。
//
#include <mutex>
#include <optional>
#include <regex>
#include <string>
#include <vector>

namespace filters {

class PermissionRegistry {
public:
    static PermissionRegistry& instance();

    // pathPattern 采用 regex 语法：如 "/api/roles" 或 "/api/roles/(\\d+)"
    void bind(const std::string& method,
              const std::string& pathPattern,
              const std::string& permCode);

    // 查找匹配的权限码。无匹配返回 nullopt（代表无需额外权限）。
    std::optional<std::string> lookup(const std::string& method,
                                      const std::string& path) const;

private:
    struct Entry {
        std::string method;
        std::regex  re;
        std::string code;
    };
    mutable std::mutex mu_;
    std::vector<Entry> entries_;
};

} // namespace filters
