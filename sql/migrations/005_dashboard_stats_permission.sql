-- 系统概览统计仅供显式授权的管理员查看。

INSERT OR IGNORE INTO permissions
    (code, name, type, menu_id, created_at)
SELECT 'dashboard:stats', '仪表盘-系统概览', 'button', id, 0
FROM menus
WHERE path = '/dashboard'
ORDER BY id
LIMIT 1;

INSERT OR IGNORE INTO role_permissions(role_id, permission_id)
SELECT 1, id FROM permissions WHERE code = 'dashboard:stats';
