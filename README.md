# c_web

基于 **Drogon** 的模块化 C++ Web 服务骨架，Windows + MSVC + vcpkg 构建。

## 目录结构

```
src/
├── main.cc                 # 入口，薄薄一层
├── core/                   # 框架层：Bootstrap / Result / Logger
├── common/                 # 通用工具：TimeUtil ...
├── filters/                # 横切过滤器：AuthFilter / CorsFilter
├── plugins/                # 全局单例：ConfigPlugin
└── modules/                # 业务模块（自治）
    └── user/
        ├── UserController  # HTTP 路由
        ├── UserService     # 业务逻辑（异步）
        ├── UserRepository  # 数据访问（SQLite 异步）
        └── dto/            # 请求 / 响应 DTO
config/config.json          # Drogon 运行配置（含 db_clients）
sql/schema.sql              # 启动时自动应用的建表脚本
data/                       # SQLite 数据库文件目录（运行时自动创建）
```

## 前置依赖

1. Visual Studio 2022（含 C++ 桌面开发组件）
2. CMake 3.20+
3. [vcpkg](https://github.com/microsoft/vcpkg)，并设置环境变量 `VCPKG_ROOT`
4. Drogon 需启用 `sqlite3` feature（vcpkg.json 已声明，首次构建会自动触发）

## 构建与运行

在 **x64 Native Tools Command Prompt for VS 2022** 中：

```bat
set VCPKG_ROOT=C:\path\to\vcpkg
build.bat
```

构建产物：`build\Release\c_web.exe`，`config\` 会自动拷贝到输出目录旁。

运行：

```bat
cd build\Release
c_web.exe
```

## 默认接口

| Method | Path              | 说明                |
|--------|-------------------|---------------------|
| GET    | /api/health       | 健康检查            |
| GET    | /api/users/{id}   | 查询用户（预置 id=1）|
| POST   | /api/users        | 创建用户（JSON body）|

测试：

```bash
curl http://127.0.0.1:8080/api/health
curl http://127.0.0.1:8080/api/users/1
curl -X POST http://127.0.0.1:8080/api/users ^
     -H "Content-Type: application/json" ^
     -d "{\"name\":\"tom\",\"email\":\"tom@x.com\"}"
```

## 扩展新业务模块

1. 在 `src/modules/` 下新建目录，例如 `order/`
2. 按 `Controller / Service / Repository / dto` 四件套补齐
3. **无需改 CMake、无需改 main**：`file(GLOB_RECURSE …)` 自动收录，Drogon 自动注册路由

## 启用鉴权 / CORS

在 Controller 路由里追加 Filter：

```cpp
ADD_METHOD_TO(UserController::create, "/api/users",
              drogon::Post, "filters::AuthFilter");
```

或在 `config.json` 中配置全局 Filters。

## 数据库（SQLite）

- 默认使用 SQLite，数据库文件位于 `data/c_web.db`（首次启动自动创建）
- 启动时 `core::Bootstrap` 会自动执行 `sql/schema.sql`，存在的表不会重复创建
- `config/config.json` 中 `db_clients` 段控制连接参数；SQLite 写为单连接更稳妥：
  ```json
  "db_clients": [{
      "name": "default",
      "rdbms": "sqlite3",
      "filename": "data/c_web.db",
      "connection_number": 1
  }]
  ```
- 更换为 MySQL：把 `rdbms` 改成 `mysql` 并补 `host/port/user/passwd`，同时 `vcpkg.json` 改成 `drogon[mysql]` 即可，Repository 代码不用改（SQL 语法适度调整）。

## 查看数据库

- 推荐 **DB Browser for SQLite**（https://sqlitebrowser.org/）
- 或命令行：`sqlite3 data\c_web.db "SELECT * FROM users;"`
