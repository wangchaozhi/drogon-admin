-- 邮箱作为登录标识按 ASCII 大小写不敏感处理。
-- 若历史数据中存在仅大小写不同的重复邮箱，本迁移会拒绝启动，
-- 以免静默合并两个不同账号；管理员需先清理冲突记录。

CREATE UNIQUE INDEX IF NOT EXISTS idx_users_email_nocase
    ON users(email COLLATE NOCASE);
