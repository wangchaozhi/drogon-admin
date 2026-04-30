-- c_web 初始化建表脚本（SQLite）
-- 启动时由 core::Bootstrap 执行一次，存在即跳过。

CREATE TABLE IF NOT EXISTS users (
    id         INTEGER PRIMARY KEY AUTOINCREMENT,
    name       TEXT    NOT NULL,
    email      TEXT    NOT NULL UNIQUE,
    created_at INTEGER NOT NULL
);

-- 预置一条演示数据（若不存在）
INSERT OR IGNORE INTO users(id, name, email, created_at)
VALUES (1, 'admin', 'admin@example.com', strftime('%s','now'));
