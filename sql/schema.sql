-- c_web 初始化建表脚本（SQLite）
-- 启动时由 core::Bootstrap 执行一次，存在即跳过。

CREATE TABLE IF NOT EXISTS users (
    id            INTEGER PRIMARY KEY AUTOINCREMENT,
    name          TEXT    NOT NULL,
    email         TEXT    NOT NULL UNIQUE,
    password_hash TEXT    NOT NULL DEFAULT '',
    created_at    INTEGER NOT NULL
);

-- 索引：按邮箱登录需要快速查询（UNIQUE 已隐式建立）
CREATE INDEX IF NOT EXISTS idx_users_email ON users(email);

-- 迁移：老库存在但缺 password_hash 列时补齐（列已存在时 SQLite 会报错，
-- 由 Bootstrap 的 try/catch 吞掉不影响启动，因此无需 IF NOT EXISTS）。
ALTER TABLE users ADD COLUMN password_hash TEXT NOT NULL DEFAULT '';
