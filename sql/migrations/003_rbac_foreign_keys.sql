-- 为历史安装重建 RBAC 关联表，补齐数据库级引用完整性约束。
-- 如果已有孤儿关联，复制阶段会失败并回滚，避免静默丢失数据。

CREATE TABLE user_roles_fk (
    user_id INTEGER NOT NULL,
    role_id INTEGER NOT NULL,
    PRIMARY KEY (user_id, role_id),
    FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE,
    FOREIGN KEY (role_id) REFERENCES roles(id) ON DELETE RESTRICT
);

INSERT INTO user_roles_fk(user_id, role_id)
SELECT user_id, role_id FROM user_roles;

DROP TABLE user_roles;

ALTER TABLE user_roles_fk RENAME TO user_roles;

CREATE TABLE role_permissions_fk (
    role_id INTEGER NOT NULL,
    permission_id INTEGER NOT NULL,
    PRIMARY KEY (role_id, permission_id),
    FOREIGN KEY (role_id) REFERENCES roles(id) ON DELETE CASCADE,
    FOREIGN KEY (permission_id) REFERENCES permissions(id) ON DELETE CASCADE
);

INSERT INTO role_permissions_fk(role_id, permission_id)
SELECT role_id, permission_id FROM role_permissions;

DROP TABLE role_permissions;

ALTER TABLE role_permissions_fk RENAME TO role_permissions;
