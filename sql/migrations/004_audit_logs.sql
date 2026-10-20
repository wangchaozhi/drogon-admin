-- 管理员审计日志：保留历史，不对 actor/target 添加外键，避免主体删除后丢失审计。

CREATE TABLE IF NOT EXISTS audit_logs (
    id            INTEGER PRIMARY KEY AUTOINCREMENT,
    actor_user_id INTEGER NOT NULL,
    action        TEXT    NOT NULL,
    target_type   TEXT    NOT NULL,
    target_id     TEXT    NOT NULL,
    request_id    TEXT    NOT NULL,
    details       TEXT    NOT NULL DEFAULT '{}',
    created_at    INTEGER NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_audit_logs_created_at
    ON audit_logs(created_at DESC, id DESC);
CREATE INDEX IF NOT EXISTS idx_audit_logs_action_created
    ON audit_logs(action, created_at DESC);
CREATE INDEX IF NOT EXISTS idx_audit_logs_actor_created
    ON audit_logs(actor_user_id, created_at DESC);
CREATE INDEX IF NOT EXISTS idx_audit_logs_target
    ON audit_logs(target_type, target_id, created_at DESC);

INSERT INTO menus
    (parent_id, name, path, icon, component, sort, type, visible, created_at)
SELECT 2, '审计日志', '/system/audit-logs', 'FileSearchOutlined',
       'AuditLogPage', 24, 'menu', 1, 0
WHERE NOT EXISTS (
    SELECT 1 FROM menus WHERE path = '/system/audit-logs'
);

INSERT OR IGNORE INTO permissions
    (code, name, type, menu_id, created_at)
SELECT 'audit:view', '审计日志-查看', 'menu', id, 0
FROM menus
WHERE path = '/system/audit-logs'
ORDER BY id
LIMIT 1;

INSERT OR IGNORE INTO role_permissions(role_id, permission_id)
SELECT 1, id FROM permissions WHERE code = 'audit:view';
