# drogon-admin

[![backend](https://github.com/wangchaozhi/drogon-admin/actions/workflows/backend.yml/badge.svg)](https://github.com/wangchaozhi/drogon-admin/actions/workflows/backend.yml)
[![frontend](https://github.com/wangchaozhi/drogon-admin/actions/workflows/frontend.yml/badge.svg)](https://github.com/wangchaozhi/drogon-admin/actions/workflows/frontend.yml)

基于 **Drogon** 的模块化 C++ Web 服务骨架 + **React 19 + Ant Design 6** 前端，Windows + MSVC + vcpkg 构建。全链路 **C++20 协程**（`drogon::Task` / `AsyncTask` / `co_await execSqlCoro`），无同步 SQL 阻塞 event loop。

## 目录结构

```
drogon-admin/
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
├── config/config.json          # Drogon 运行配置（数据库、环境与注册策略）
├── docker/Dockerfile.musl      # 静态 musl 构建 + 非 root 运行镜像
├── compose.yml                 # 持久化数据与加固后的本地部署
├── sql/
│   ├── schema.sql              # 当前完整基线（新安装）
│   └── migrations/             # 按文件名排序的版本化增量迁移
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
4. Drogon 已启用 `sqlite3` feature，依赖：`drogon[sqlite3]` / `argon2` / `jsoncpp` / `openssl` / `zlib` / `sqlite3`（vcpkg.json 已声明，首次构建会自动触发）
5. Python 3.10+（运行跨平台 API 集成测试；仅构建可设置 `-DBUILD_TESTING=OFF`）

### 前端
1. Node.js 26+
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

构建产物：`build\Release\drogon-admin.exe`，`config\` 会自动拷贝到输出目录旁。

> Windows 侧采用 `x64-windows-static-md` triplet：vcpkg 的第三方依赖（Drogon / OpenSSL / jsoncpp / zlib / sqlite3 等）静态链接进 `drogon-admin.exe`，CRT 仍为动态（`/MD`）。因此产物在装有 VC++ Runtime 的 Win10/11 机器上可直接运行，无需随附一堆 DLL。

运行：

```bat
cd build\Release
drogon-admin.exe
```

开发环境未提供 JWT 密钥时会生成一次性随机密钥，服务重启后现有 token 失效。生产环境必须显式提供至少 32 字符的密钥，否则拒绝启动：

```bat
set DROGON_ENV=production
set DROGON_JWT_SECRET=<至少32字符的随机密钥>
drogon-admin.exe
```

`DROGON_REGISTRATION_MODE` 支持 `first-user`（默认，仅允许首个管理员注册）、`open`（开放注册，后续用户为普通角色）和 `disabled`（关闭公开注册）。密码使用 Argon2id 随机盐哈希；旧 SHA-256 哈希会在登录成功后自动升级。

登录连续失败默认 5 次后锁定账号 15 分钟，可通过
`DROGON_LOGIN_MAX_FAILURES`（3–20）和
`DROGON_LOGIN_LOCK_SECONDS`（30–86400）调整。JWT 携带会话版本；
修改密码后旧 token 立即失效，受保护请求也会实时拒绝已禁用或已删除账号。
用户可在个人中心验证当前密码后自行设置新密码；成功后所有设备的现有 token
都会立即失效，并要求重新登录。
管理员可在用户编辑页启用或禁用账号；修改邮箱、密码、状态或角色都会吊销该用户的现有 token。
系统以事务保护最后一个可用管理员，禁止删除、禁用或移除其管理员角色；内置 `admin` 角色具备系统级超级权限，且内置角色不可删除、管理员权限映射不可手工修改。
未预期的数据库和服务端异常只向客户端返回通用错误，详细原因与请求 ID
仅写入服务端日志，避免泄漏 SQL 和内部结构。用户列表分页限制为每页最多
200 条，并严格拒绝非法页码、超长关键字和非整数参数。

### CORS 与生产边界

默认不允许跨域（开发服务器已通过 Vite proxy 访问 `/api`）。前后端不同源部署时，使用逗号分隔的**精确 origin** 白名单：

```bash
DROGON_CORS_ALLOWED_ORIGINS=https://admin.example.com,https://ops.example.com
DROGON_CORS_ALLOW_CREDENTIALS=false
DROGON_CORS_MAX_AGE_SECONDS=600
```

生产环境禁止 `*`；预检只接受 `GET/POST/PUT/DELETE/PATCH` 和
`Authorization/Content-Type/X-Request-ID`。所有 API 响应（包括鉴权失败和
404）统一附加 CORS、CSP、Permissions Policy 等安全响应头，并使用
`Cache-Control: no-store`，避免账号、权限或会话数据被浏览器和代理落盘缓存。

推荐由 Nginx/Caddy/云负载均衡终止 TLS，并让 Drogon 仅对内网监听。确认所有公网流量均为 HTTPS 后再设置 `DROGON_ENABLE_HSTS=true`；否则不要提前启用 HSTS。

## 容器部署

生产镜像使用 Alpine 多阶段构建，最终运行静态 musl 可执行文件，并以 UID
10001、只读根文件系统、无 Linux capabilities 启动：

```bash
cp .env.example .env
# 编辑 .env，至少设置 32 字符以上的随机 DROGON_JWT_SECRET
docker compose up -d --build
docker compose ps
curl http://127.0.0.1:8080/api/ready
```

Compose 默认仅绑定 `127.0.0.1`，应由同机反向代理提供公网 HTTPS。SQLite、
日志和上传目录分别保存在命名卷中；升级前应备份数据卷。容器健康检查调用
`/api/ready`，而 `/api/health` 仅用于进程存活检查。

## 前端开发与构建

```bat
cd front
npm install          REM 首次执行
npm run dev          REM 本地开发（默认 http://localhost:5173）
npm test             REM Vitest 组件与认证客户端测试
npm run build        REM 生产构建，产物在 front/dist
npm run preview      REM 预览生产构建
```

前端通过 `src/api/` 下的模块统一调用后端 `http://127.0.0.1:8080/api/*` 接口，登录成功后 JWT 写入本地并由 `AuthContext` 全局维护。路由层面通过 `ProtectedRoute` 做鉴权与权限拦截，无权限时自动跳转到 `/403`。

“记住我”仅持久化邮箱，不保存密码。窗口重新获得焦点时会刷新用户角色和权限；临时网络错误会保留 token 并提供重试入口，只有明确的 401 才清除登录态。
所有页面与管理布局均按路由懒加载，生产构建会拆分为独立 chunk，避免首屏
下载完整后台管理功能。

## 默认接口

| Method | Path                               | 鉴权 | 权限要求     | 说明                    |
|--------|------------------------------------|------|--------------|-------------------------|
| GET    | /api/health                        | 否   | —            | 健康检查                |
| GET    | /api/ready                         | 否   | —            | DB/迁移就绪检查         |
| GET    | /api/metrics                       | 否   | —            | Prometheus 运行指标     |
| POST   | /api/auth/register                 | 否   | —            | 按注册策略创建账号      |
| POST   | /api/auth/login                    | 否   | —            | 登录，返回 JWT          |
| GET    | /api/users/me                      | 是   | —            | 获取当前登录用户        |
| PUT    | /api/users/me/password             | 是   | —            | 修改当前用户密码        |
| GET    | /api/users/{id}                    | 是   | —            | 按 id 查询用户          |
| POST   | /api/users                         | 是   | user:create  | 管理员创建普通用户      |
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
ctest --test-dir build -C Release --output-on-failure
```

CTest 会启动临时 Drogon 实例和独立 SQLite 数据库，黑盒验证注册/登录、密码与角色变更后的 token 吊销、登录锁定、最后管理员保护、内置角色保护以及 CORS/安全响应头。测试不会读写开发数据库。

手工调用示例：

```bash
curl http://127.0.0.1:8080/api/health

curl -X POST http://127.0.0.1:8080/api/auth/register ^
     -H "Content-Type: application/json" ^
     -d "{\"name\":\"Tom\",\"password\":\"Correct-Horse-2026!\",\"email\":\"tom@example.com\"}"

curl -X POST http://127.0.0.1:8080/api/auth/login ^
     -H "Content-Type: application/json" ^
     -d "{\"email\":\"tom@example.com\",\"password\":\"Correct-Horse-2026!\"}"

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
- **禁止反模式**：严禁在 `execSqlAsync` 的回调中直接调用 `execSqlSync`——SQLite 单 worker 线程会自锁卡死。本仓业务链路统一使用 `co_await execSqlCoro`；仅启动期版本迁移使用同步 SQL，完成后才对外提供服务。

## 日志

- 双路输出：`trantor::AsyncFileLogger` 写 `logs/drogon-admin.log`（自动按大小切片）+ 自定义 `AsyncConsoleSink` 后台线程刷 `stdout`，业务线程只入队，Windows QuickEdit"选中"阻塞 stdout 时不会拖住 HTTP 线程。
- 每个请求接受或生成 `X-Request-ID`，响应原样返回；结构化访问日志包含 request ID、方法、路径、状态码、耗时微秒和已认证用户 ID，便于反向代理与应用日志关联。
- 已应用迁移记录在 `schema_migrations`；每个迁移在 `BEGIN IMMEDIATE` 事务中执行，失败会回滚并阻止服务进入就绪状态。
- `/api/metrics` 输出 Prometheus 文本格式，包含构建版本、运行时长、活动请求数、
  各状态码类别请求数和请求耗时直方图。该端点不包含用户或路径标签，避免
  敏感信息及高基数指标；生产环境应继续通过 Compose 的 localhost 绑定或
  反向代理访问控制将其限制在监控网络。

## 首个注册用户自动晋升管理员

- 默认 `registration_mode=first-user`：仅第一个注册用户获得 admin 角色，随后关闭公开注册；
- 判断与“创建用户 + 绑定角色”位于同一个 SQLite `Immediate` 事务，避免并发注册产生多个首管理员；
- 如需开放注册可显式设置 `open`，后续用户只获得普通 `user` 角色。

## RBAC 权限设计

- **用户-角色-权限** 三元模型：`users` ↔ `user_roles` ↔ `roles` ↔ `role_permissions` ↔ `permissions`
- **菜单即权限**：每个菜单对应一组权限（`menu` 类型为页面访问权，`button` 类型为操作权），由 `menu_id` 关联
- **PermissionFilter**：在 `PermissionRegistry` 中注册每个 Controller 方法的权限码，请求时校验 JWT 中的用户是否拥有对应权限
- **前端权限**：`DynamicMenu` 根据后端返回的权限树动态渲染侧边栏；`HasPerm` 组件控制按钮级显隐；路由层 `ProtectedRoute` 控制页面级访问

## 数据库（SQLite）

- 默认使用 SQLite，数据库文件位于 `data/drogon-admin.db`（首次启动自动创建）
- 启动时先对齐 `sql/schema.sql` 基线，再按文件名执行 `sql/migrations/*.sql`；版本写入 `schema_migrations`
- schema 内置种子数据：默认角色 `admin`（全部权限）与 `user`（仅仪表盘+个人中心）
- `config/config.json` 中 `db_clients` 段控制连接参数；SQLite 写为单连接更稳妥：
  ```json
  "db_clients": [{
      "name": "default",
      "rdbms": "sqlite3",
      "filename": "data/drogon-admin.db",
      "connection_number": 1
  }]
  ```
- 当前 schema、迁移和部分 Repository 查询针对 SQLite；切换 MySQL/PostgreSQL 前需提供对应 SQL 方言与迁移实现，不能只修改连接配置。

## 查看数据库

- 推荐 **DB Browser for SQLite**（https://sqlitebrowser.org/）
- 或命令行：`sqlite3 data\drogon-admin.db "SELECT * FROM users;"`

## 持续集成与发版

- **手动构建**：main 分支上不再自动触发构建。可在 GitHub Actions 页面点击 `Run workflow` 手动触发 `backend` / `frontend` / `musl-release` 流水线。
- **PR 质量门**：面向 main 的 Pull Request 仍会自动执行构建以验证改动。
- **版本治理**：仓库根目录的 `VERSION` 是唯一发布版本源，必须与 CMake、vcpkg、前端 package 及 lockfile 保持一致。CI 会阻止版本不一致或 tag 与版本不匹配的发布；本地可运行 `python scripts/verify_release_version.py --tag v0.1.1` 预检。
- **自动发版**：推送形如 `v*`（如 `v0.1.0`、`v1.2.3-rc1`）的 tag 时，`backend` 会构建三平台 Release 产物（Windows static-md / Ubuntu 22.04 / macOS arm64），`musl-release` 会通过 Alpine 3.24 构建无动态加载器依赖的 x64 / arm64 musl 静态产物，`frontend` 会构建 `dist`，分别打包后自动上传到同名 tag 的 GitHub Release：
  - `drogon-admin-<tag>-windows-x64.zip`
  - `drogon-admin-<tag>-ubuntu22-x64.tar.gz`
  - `drogon-admin-<tag>-macos-arm64.tar.gz`（experimental）
  - `drogon-admin-<tag>-linux-musl-x64.tar.gz`
  - `drogon-admin-<tag>-linux-musl-arm64.tar.gz`
  - `drogon-admin-front-<tag>.zip`
  - 每个压缩包均附带同名 `.sha256` 校验文件，并同时保留在 workflow artifact 中。
  - 带 `-` 的 tag（如 `v1.0.0-rc1`）会被标记为 pre-release。

发一次版示例：

```bat
python scripts/verify_release_version.py --tag v0.1.1
git tag v0.1.1
git push origin v0.1.1
```
