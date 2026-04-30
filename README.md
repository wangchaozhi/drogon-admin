# c_web

[![backend](https://github.com/wangchaozhi/drogon-admin/actions/workflows/backend.yml/badge.svg)](https://github.com/wangchaozhi/drogon-admin/actions/workflows/backend.yml)
[![frontend](https://github.com/wangchaozhi/drogon-admin/actions/workflows/frontend.yml/badge.svg)](https://github.com/wangchaozhi/drogon-admin/actions/workflows/frontend.yml)

基于 **Drogon** 的模块化 C++ Web 服务骨架 + **React 19 + Ant Design 6** 前端，Windows + MSVC + vcpkg 构建。全链路 **C++20 协程**（`drogon::Task` / `AsyncTask` / `co_await execSqlCoro`），无同步 SQL 阻塞 event loop。

## 持续集成与发版

- **手动构建**：main 分支上不再自动触发构建。可在 GitHub Actions 页面点击 `Run workflow` 手动触发 `backend` / `frontend` 流水线。
- **PR 质量门**：面向 main 的 Pull Request 仍会自动执行构建以验证改动。
- **自动发版**：推送形如 `v*`（如 `v0.1.0`、`v1.2.3-rc1`）的 tag 时，`backend` 会构建三平台 Release 产物（Windows static-md / Ubuntu 22.04 / macOS arm64），`frontend` 会构建 `dist`，分别打包后自动上传到同名 tag 的 GitHub Release：
  - `c_web-<tag>-windows-x64.zip`
  - `c_web-<tag>-ubuntu22-x64.tar.gz`
  - `c_web-<tag>-macos-arm64.tar.gz`（experimental）
  - `c_web-front-<tag>.zip`
  - 带 `-` 的 tag（如 `v1.0.0-rc1`）会被标记为 pre-release。

发一次版示例：

```bat
git tag v0.1.0
git push origin v0.1.0
```

## 目录结构

```
c_web/
├── src/                        # 后端 C++ 源码
│   ├── main.cc                 # 入口，薄薄一层
│   ├── core/                   # 框架层：Bootstrap / Result / Logger
│   ├── common/                 # 通用工具：TimeUtil / CryptoUtil / JwtUtil
│   ├── filters/                # 横切过滤器：AuthFilter / CorsFilter / PermissionFilter / PermissionRegistry
│   ├── plugins/                # 全局单例：ConfigPlugin
│   └── modules/                # 业务模块（自治）
│       ├── user/
│       │   ├── UserController  # HTTP 路由（AsyncTask 协程）
│       │   ├── UserService     # 业务编排（drogon::Task）
│       │   ├── UserRepository  # 数据访问（co_await execSqlCoro）
│       │   └── dto/            # 请求 / 响应 DTO（LoginReq / CreateUserReq ...）
│       └── rbac/
│           ├── RoleController      # 角色管理（AsyncTask）
│           ├── MenuController      # 菜单管理（AsyncTask）
│           ├── PermissionController# 权限列表（AsyncTask）
│           ├── RbacService         # RBAC 业务逻辑 + 缓存（协程）
│           ├── RbacRepository      # RBAC 数据访问（协程）
│           └── dto/                # RBAC DTO
├── front/                      # 前端（React 19 + Vite + Ant Design 6 + React Router）
│   └── src/
│       ├── pages/              # 页面：login / dashboard / profile / admin（users/roles/menus）/ error
│       ├── layouts/            # AdminLayout（侧边栏 + 顶部栏）
│       ├── routes/             # 基于权限的嵌套路由（ProtectedRoute）
│       ├── components/         # AppHeader / DynamicMenu / HasPerm
│       ├── api/                # 模块化 API 封装（auth / user / role / menu / permission）
│       ├── context/            # AuthContext
│       ├── hooks/              # useAuth / useDevice（响应式三端适配）
│       ├── theme/              # Ant Design 主题定制
│       └── utils/              # remember 等工具
├── config/config.json          # Drogon 运行配置（含 db_clients / jwt_secret）
├── sql/schema.sql              # 启动时自动应用的建表脚本
├── data/                       # SQLite 数据库文件目录（运行时自动创建）
├── build.bat / build_vs.bat    # Windows 构建脚本
├── CMakeLists.txt
└── vcpkg.json
```

## 前置依赖

### 后端
1. Visual Studio 2022（含 C++ 桌面开发组件，需支持 C++20 协程）
2. CMake 3.20+
3. [vcpkg](https://github.com/microsoft/vcpkg)，并设置环境变量 `VCPKG_ROOT`
4. Drogon 已启用 `sqlite3` feature，依赖：`drogon[sqlite3]` / `jsoncpp` / `openssl` / `zlib` / `sqlite3`（vcpkg.json 已声明，首次构建会自动触发）

### 前端
1. Node.js 24+
2. npm 10+

## 后端构建与运行

在 **x64 Native Tools Command Prompt for VS 2022** 中：

```bat
REM 方式 A：直接运行（脚本默认 VCPKG_ROOT = E:\dev\vcpkg）
build.bat

REM 方式 B：显式指定其它 vcpkg 路径
set VCPKG_ROOT=C:\path\to\vcpkg
build.bat

REM 方式 C：使用 Visual Studio 生成器
build_vs.bat
```

构建产物：`build\Release\c_web.exe`，`config\` 会自动拷贝到输出目录旁。

> Windows 侧采用 `x64-windows-static-md` triplet：vcpkg 的第三方依赖（Drogon / OpenSSL / jsoncpp / zlib / sqlite3 等）静态链接进 `c_web.exe`，CRT 仍为动态（`/MD`）。因此产物在装有 VC++ Runtime 的 Win10/11 机器上可直接运行，无需随附一堆 DLL。

运行：

```bat
cd build\Release
c_web.exe
```

## 前端开发与构建

```bat
cd front
npm install          REM 首次执行
npm run dev          REM 本地开发（默认 http://localhost:5173）
npm run build        REM 生产构建，产物在 front/dist
npm run preview      REM 预览生产构建
```

前端通过 `src/api/` 下的模块统一调用后端 `http://127.0.0.1:8080/api/*` 接口，登录成功后 JWT 写入本地并由 `AuthContext` 全局维护。路由层面通过 `ProtectedRoute` 做鉴权与权限拦截，无权限时自动跳转到 `/403`。

## 默认接口

| Method | Path                               | 鉴权 | 权限要求     | 说明                    |
|--------|------------------------------------|------|--------------|-------------------------|
| GET    | /api/health                        | 否   | —            | 健康检查                |
| POST   | /api/auth/register                 | 否   | —            | 注册账号（JSON body）   |
| POST   | /api/auth/login                    | 否   | —            | 登录，返回 JWT          |
| GET    | /api/users/me                      | 是   | —            | 获取当前登录用户        |
| GET    | /api/users/{id}                    | 是   | —            | 按 id 查询用户          |
| POST   | /api/users                         | 否   | —            | 兼容旧路径，等价于注册  |
| GET    | /api/roles                         | 是   | role:view    | 角色列表                |
| POST   | /api/roles                         | 是   | role:create  | 创建角色                |
| PUT    | /api/roles/{id}                    | 是   | role:update  | 更新角色                |
| DELETE | /api/roles/{id}                    | 是   | role:delete  | 删除角色                |
| GET    | /api/roles/{id}/permissions        | 是   | role:assign  | 获取角色权限            |
| PUT    | /api/roles/{id}/permissions        | 是   | role:assign  | 设置角色权限            |
| GET    | /api/menus                         | 是   | menu:view    | 菜单列表                |
| GET    | /api/menus/tree                    | 是   | menu:view    | 菜单树                  |
| POST   | /api/menus                         | 是   | menu:create  | 创建菜单                |
| PUT    | /api/menus/{id}                    | 是   | menu:update  | 更新菜单                |
| DELETE | /api/menus/{id}                    | 是   | menu:delete  | 删除菜单                |
| GET    | /api/permissions                   | 是   | —            | 权限列表                |

测试：

```bash
curl http://127.0.0.1:8080/api/health

curl -X POST http://127.0.0.1:8080/api/auth/register ^
     -H "Content-Type: application/json" ^
     -d "{\"username\":\"tom\",\"password\":\"123456\",\"email\":\"tom@x.com\"}"

curl -X POST http://127.0.0.1:8080/api/auth/login ^
     -H "Content-Type: application/json" ^
     -d "{\"username\":\"tom\",\"password\":\"123456\"}"

curl http://127.0.0.1:8080/api/users/me ^
     -H "Authorization: Bearer <JWT>"
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

## 全链路协程架构

- **C++ 标准**：`CMAKE_CXX_STANDARD = 20`，启用 Drogon 协程支持。
- **Repository**：所有方法签名 `drogon::Task<T>`，数据库访问统一使用 `co_await db->execSqlCoro(...)`，Drogon 在 SQLite worker 线程执行后 resume 原协程。
- **Service**：
  - `UserService` / `RbacService` 方法返回 `drogon::Task<T>`，在协程内 `co_await` 串联 Repository 调用。
  - `RbacService` 采用**双检 + 短临锁**的缓存范式：进入协程先加锁读缓存 → 命中直接返回；未命中则释放锁执行 `co_await`，完成后再加锁回填。`co_await` 期间绝不持锁，避免跨线程 resume 引发死锁。
  - 登录凭证错误抛 `LoginInvalidError`，由 Controller 转 401。
- **Controller**：方法签名 `drogon::AsyncTask xxx(HttpRequestPtr req, std::function<void(const HttpResponsePtr&)> cb, ...)`。Drogon 识别协程返回类型并托管 frame 生命周期；参数按值传入 frame，安全 `co_await`。
- **Filter**：`HttpFilter::doFilter` 为虚函数签名固定为 `void`，内部调用 **free-function 协程** `loadAndContinue(...)` 启动 `AsyncTask` 完成 RBAC 视图加载后再 `fccb()`。
- **禁止反模式**：严禁在 `execSqlAsync` 的回调中直接调用 `execSqlSync`——SQLite 单 worker 线程会自锁卡死。本仓全链路已改造为 `co_await execSqlCoro`，仅保留 `Bootstrap::init` 里一次性的 schema 迁移使用 `execSqlSync`（运行在主线程启动阶段，不涉及 worker 竞争）。

## 日志

- 双路输出：`trantor::AsyncFileLogger` 写 `logs/c_web.log`（自动按大小切片）+ 自定义 `AsyncConsoleSink` 后台线程刷 `stdout`，业务线程只入队，Windows QuickEdit"选中"阻塞 stdout 时不会拖住 HTTP 线程。
- `sql/schema.sql` 的幂等迁移（如 `ALTER TABLE ADD COLUMN` 已存在列）会被降级为 DEBUG，避免 ERROR 噪音。

## 首个注册用户自动晋升管理员

- 系统里 `user_roles` 表不存在 `role_id=1` 的绑定时，下一个注册的用户自动获得 admin 角色；
- 否则按 `config.superAdminEmail` 匹配；其他走普通 `user` 角色。
- 相关逻辑见 [`UserService::assignDefaultRole`](src/modules/user/UserService.cc) 与 [`UserRepository::hasAnyAdmin`](src/modules/user/UserRepository.cc)。

## RBAC 权限设计

- **用户-角色-权限** 三元模型：`users` ↔ `user_roles` ↔ `roles` ↔ `role_permissions` ↔ `permissions`
- **菜单即权限**：每个菜单对应一组权限（`menu` 类型为页面访问权，`button` 类型为操作权），由 `menu_id` 关联
- **PermissionFilter**：在 `PermissionRegistry` 中注册每个 Controller 方法的权限码，请求时校验 JWT 中的用户是否拥有对应权限
- **前端权限**：`DynamicMenu` 根据后端返回的权限树动态渲染侧边栏；`HasPerm` 组件控制按钮级显隐；路由层 `ProtectedRoute` 控制页面级访问

## 数据库（SQLite）

- 默认使用 SQLite，数据库文件位于 `data/c_web.db`（首次启动自动创建）
- 启动时 `core::Bootstrap` 会自动执行 `sql/schema.sql`，存在的表不会重复创建
- schema 内置种子数据：默认角色 `admin`（全部权限）与 `user`（仅仪表盘+个人中心）
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
