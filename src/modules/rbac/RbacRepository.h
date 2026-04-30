#pragma once
//
// Rbac 仓储：菜单 / 角色 / 权限 / 用户-角色 / 角色-权限 的 CRUD。
// 管理端调用频率低，这里全部使用 SQLite 同步 API 以简化回调嵌套；
// 失败通过 drogon::orm::DrogonDbException 抛出，由上层 try/catch。
//
#include "dto/RoleDto.h"
#include "dto/MenuDto.h"
#include "dto/PermissionDto.h"
#include "dto/RbacReq.h"
#include <drogon/orm/DbClient.h>
#include <optional>

namespace modules::rbac {

class RbacRepository {
public:
    RbacRepository() = default;

    // --------- 角色 ---------
    std::vector<dto::RoleDto>       listRoles();
    std::optional<dto::RoleDto>     getRole(int64_t id);
    std::optional<dto::RoleDto>     getRoleByCode(const std::string& code);
    dto::RoleDto                    createRole(const dto::RoleUpsertReq& r);
    bool                            updateRole(int64_t id, const dto::RoleUpsertReq& r);
    bool                            deleteRole(int64_t id);
    std::vector<int64_t>            getRolePermissionIds(int64_t roleId);
    void                            setRolePermissions(int64_t roleId,
                                                       const std::vector<int64_t>& permIds);

    // --------- 菜单 ---------
    std::vector<dto::MenuDto>       listMenus();    // 平铺 + 按 sort 升序
    std::vector<dto::MenuDto>       menuTree();     // 全量树
    std::optional<dto::MenuDto>     getMenu(int64_t id);
    dto::MenuDto                    createMenu(const dto::MenuUpsertReq& m);
    bool                            updateMenu(int64_t id, const dto::MenuUpsertReq& m);
    bool                            deleteMenu(int64_t id);

    // --------- 权限 ---------
    std::vector<dto::PermissionDto> listPermissions();

    // --------- 用户 ---------
    std::vector<int64_t>            getUserRoleIds(int64_t userId);
    void                            setUserRoles(int64_t userId,
                                                 const std::vector<int64_t>& roleIds);

    // 热路径：AuthFilter / 登录时查询用户拥有的 角色 / 权限 / 菜单
    std::vector<std::string>        getUserRoleCodes(int64_t userId);
    std::vector<std::string>        getUserPermCodes(int64_t userId);
    std::vector<dto::MenuDto>       getUserMenuTree(int64_t userId);

    // 将扁平列表构造为树（按 parent_id / sort）
    static std::vector<dto::MenuDto> buildTree(std::vector<dto::MenuDto> flat);

private:
    drogon::orm::DbClientPtr db() const;
};

} // namespace modules::rbac
