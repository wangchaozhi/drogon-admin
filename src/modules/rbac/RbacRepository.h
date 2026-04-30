#pragma once
//
// Rbac 仓储：协程版。所有方法返回 drogon::Task<T>，内部使用 execSqlCoro。
// buildTree 为纯内存操作保留同步接口。
//
#include "dto/RoleDto.h"
#include "dto/MenuDto.h"
#include "dto/PermissionDto.h"
#include "dto/RbacReq.h"
#include <drogon/orm/DbClient.h>
#include <drogon/utils/coroutine.h>
#include <optional>

namespace modules::rbac {

class RbacRepository {
public:
    RbacRepository() = default;

    // --------- 角色 ---------
    drogon::Task<std::vector<dto::RoleDto>>    listRoles();
    drogon::Task<std::optional<dto::RoleDto>>  getRole(int64_t id);
    drogon::Task<std::optional<dto::RoleDto>>  getRoleByCode(std::string code);
    drogon::Task<dto::RoleDto>                 createRole(dto::RoleUpsertReq r);
    drogon::Task<bool>                         updateRole(int64_t id, dto::RoleUpsertReq r);
    drogon::Task<bool>                         deleteRole(int64_t id);
    drogon::Task<std::vector<int64_t>>         getRolePermissionIds(int64_t roleId);
    drogon::Task<>                             setRolePermissions(int64_t roleId,
                                                                  std::vector<int64_t> permIds);

    // --------- 菜单 ---------
    drogon::Task<std::vector<dto::MenuDto>>    listMenus();
    drogon::Task<std::vector<dto::MenuDto>>    menuTree();
    drogon::Task<std::optional<dto::MenuDto>>  getMenu(int64_t id);
    drogon::Task<dto::MenuDto>                 createMenu(dto::MenuUpsertReq m);
    drogon::Task<bool>                         updateMenu(int64_t id, dto::MenuUpsertReq m);
    drogon::Task<bool>                         deleteMenu(int64_t id);

    // --------- 权限 ---------
    drogon::Task<std::vector<dto::PermissionDto>> listPermissions();

    // --------- 用户 ---------
    drogon::Task<std::vector<int64_t>>         getUserRoleIds(int64_t userId);
    drogon::Task<>                             setUserRoles(int64_t userId,
                                                            std::vector<int64_t> roleIds);

    // 热路径：AuthFilter / 登录时查询用户拥有的 角色 / 权限 / 菜单
    drogon::Task<std::vector<std::string>>     getUserRoleCodes(int64_t userId);
    drogon::Task<std::vector<std::string>>     getUserPermCodes(int64_t userId);
    drogon::Task<std::vector<dto::MenuDto>>    getUserMenuTree(int64_t userId);

    // 纯内存，保持同步
    static std::vector<dto::MenuDto> buildTree(std::vector<dto::MenuDto> flat);

private:
    drogon::orm::DbClientPtr db() const;
};

} // namespace modules::rbac
