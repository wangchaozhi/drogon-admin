-- drogon-admin 初始化建表脚本（SQLite）
-- 启动时由 core::Bootstrap 执行一次，存在即跳过。

CREATE TABLE IF NOT EXISTS users (
    id            INTEGER PRIMARY KEY AUTOINCREMENT,
    name          TEXT    NOT NULL,
    email         TEXT    NOT NULL UNIQUE,
    password_hash TEXT    NOT NULL DEFAULT '',
    created_at    INTEGER NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_users_email ON users(email);

-- 迁移：老库存在但缺 password_hash 列时补齐（Bootstrap 用 try/catch 吞异常）
ALTER TABLE users ADD COLUMN password_hash TEXT NOT NULL DEFAULT '';

-- ===================== RBAC =====================

CREATE TABLE IF NOT EXISTS roles (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    code        TEXT    NOT NULL UNIQUE,
    name        TEXT    NOT NULL,
    description TEXT    NOT NULL DEFAULT '',
    created_at  INTEGER NOT NULL DEFAULT 0
);

CREATE TABLE IF NOT EXISTS menus (
    id         INTEGER PRIMARY KEY AUTOINCREMENT,
    parent_id  INTEGER NOT NULL DEFAULT 0,
    name       TEXT    NOT NULL,
    path       TEXT    NOT NULL DEFAULT '',
    icon       TEXT    NOT NULL DEFAULT '',
    component  TEXT    NOT NULL DEFAULT '',
    sort       INTEGER NOT NULL DEFAULT 0,
    type       TEXT    NOT NULL DEFAULT 'menu',  -- dir | menu
    visible    INTEGER NOT NULL DEFAULT 1,
    created_at INTEGER NOT NULL DEFAULT 0
);

CREATE INDEX IF NOT EXISTS idx_menus_parent ON menus(parent_id);

CREATE TABLE IF NOT EXISTS permissions (
    id         INTEGER PRIMARY KEY AUTOINCREMENT,
    code       TEXT    NOT NULL UNIQUE,
    name       TEXT    NOT NULL,
    type       TEXT    NOT NULL DEFAULT 'button', -- menu | button
    menu_id    INTEGER NOT NULL DEFAULT 0,
    created_at INTEGER NOT NULL DEFAULT 0
);

CREATE INDEX IF NOT EXISTS idx_perms_menu ON permissions(menu_id);

CREATE TABLE IF NOT EXISTS user_roles (
    user_id INTEGER NOT NULL,
    role_id INTEGER NOT NULL,
    PRIMARY KEY (user_id, role_id)
);

CREATE TABLE IF NOT EXISTS role_permissions (
    role_id       INTEGER NOT NULL,
    permission_id INTEGER NOT NULL,
    PRIMARY KEY (role_id, permission_id)
);

-- ===================== 种子数据 =====================

-- 角色
INSERT OR IGNORE INTO roles (id, code, name, description, created_at) VALUES (1, 'admin', '系统管理员', '拥有全部权限', 0);
INSERT OR IGNORE INTO roles (id, code, name, description, created_at) VALUES (2, 'user',  '普通用户',   '仅可访问基础功能', 0);

-- 菜单（id 固定便于幂等；type=dir 为目录，type=menu 为可点击页面）
INSERT OR IGNORE INTO menus (id, parent_id, name, path, icon, component, sort, type, visible, created_at) VALUES (1, 0, '仪表盘',    '/dashboard',     'DashboardOutlined', 'DashboardPage',  10, 'menu', 1, 0);
INSERT OR IGNORE INTO menus (id, parent_id, name, path, icon, component, sort, type, visible, created_at) VALUES (2, 0, '系统管理',  '/system',        'SettingOutlined',   '',               20, 'dir',  1, 0);
INSERT OR IGNORE INTO menus (id, parent_id, name, path, icon, component, sort, type, visible, created_at) VALUES (3, 2, '用户管理',  '/system/users',  'TeamOutlined',      'UserListPage',   21, 'menu', 1, 0);
INSERT OR IGNORE INTO menus (id, parent_id, name, path, icon, component, sort, type, visible, created_at) VALUES (4, 2, '角色管理',  '/system/roles',  'SafetyOutlined',    'RoleListPage',   22, 'menu', 1, 0);
INSERT OR IGNORE INTO menus (id, parent_id, name, path, icon, component, sort, type, visible, created_at) VALUES (5, 2, '菜单管理',  '/system/menus',  'MenuOutlined',      'MenuListPage',   23, 'menu', 1, 0);
INSERT OR IGNORE INTO menus (id, parent_id, name, path, icon, component, sort, type, visible, created_at) VALUES (6, 0, '个人中心',  '/profile',       'UserOutlined',      'ProfilePage',    90, 'menu', 1, 0);

-- 权限：menu 类型 + 按钮类型
INSERT OR IGNORE INTO permissions (code, name, type, menu_id, created_at) VALUES ('dashboard:view', '仪表盘-查看', 'menu',   1, 0);
INSERT OR IGNORE INTO permissions (code, name, type, menu_id, created_at) VALUES ('system:view',    '系统管理',   'menu',   2, 0);

INSERT OR IGNORE INTO permissions (code, name, type, menu_id, created_at) VALUES ('user:view',         '用户-查看',     'menu',   3, 0);
INSERT OR IGNORE INTO permissions (code, name, type, menu_id, created_at) VALUES ('user:create',       '用户-新增',     'button', 3, 0);
INSERT OR IGNORE INTO permissions (code, name, type, menu_id, created_at) VALUES ('user:update',       '用户-编辑',     'button', 3, 0);
INSERT OR IGNORE INTO permissions (code, name, type, menu_id, created_at) VALUES ('user:delete',       '用户-删除',     'button', 3, 0);
INSERT OR IGNORE INTO permissions (code, name, type, menu_id, created_at) VALUES ('user:assign-role',  '用户-分配角色', 'button', 3, 0);

INSERT OR IGNORE INTO permissions (code, name, type, menu_id, created_at) VALUES ('role:view',   '角色-查看',     'menu',   4, 0);
INSERT OR IGNORE INTO permissions (code, name, type, menu_id, created_at) VALUES ('role:create', '角色-新增',     'button', 4, 0);
INSERT OR IGNORE INTO permissions (code, name, type, menu_id, created_at) VALUES ('role:update', '角色-编辑',     'button', 4, 0);
INSERT OR IGNORE INTO permissions (code, name, type, menu_id, created_at) VALUES ('role:delete', '角色-删除',     'button', 4, 0);
INSERT OR IGNORE INTO permissions (code, name, type, menu_id, created_at) VALUES ('role:assign', '角色-分配权限', 'button', 4, 0);

INSERT OR IGNORE INTO permissions (code, name, type, menu_id, created_at) VALUES ('menu:view',   '菜单-查看', 'menu',   5, 0);
INSERT OR IGNORE INTO permissions (code, name, type, menu_id, created_at) VALUES ('menu:create', '菜单-新增', 'button', 5, 0);
INSERT OR IGNORE INTO permissions (code, name, type, menu_id, created_at) VALUES ('menu:update', '菜单-编辑', 'button', 5, 0);
INSERT OR IGNORE INTO permissions (code, name, type, menu_id, created_at) VALUES ('menu:delete', '菜单-删除', 'button', 5, 0);

INSERT OR IGNORE INTO permissions (code, name, type, menu_id, created_at) VALUES ('profile:view', '个人中心', 'menu', 6, 0);

-- admin 授予所有权限
INSERT OR IGNORE INTO role_permissions (role_id, permission_id)
SELECT 1, id FROM permissions;

-- user 仅授予 dashboard:view + profile:view + system-none
INSERT OR IGNORE INTO role_permissions (role_id, permission_id)
SELECT 2, id FROM permissions WHERE code IN ('dashboard:view', 'profile:view');
