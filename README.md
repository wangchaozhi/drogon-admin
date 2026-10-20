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
│       └── audit/                   # 审计日志 Controller / Repository / DTO
│       └── dashboard/               # 管理员系统概览统计
├── front/                      # 前端（React 19 + Vite + Ant Design 6 + React Router）
│   └── src/
│       ├── pages/              # 页面：login / dashboard / profile / admin（users/roles/menus/audit）/ error
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

生产部署推荐把密钥写入独立文件，并只设置文件路径，避免密钥出现在容器配置、
进程环境或诊断输出中：

```bash
export DROGON_ENV=production
export DROGON_JWT_SECRET_FILE=/run/secrets/drogon_jwt_secret
./drogon-admin
```

`DROGON_JWT_SECRET` 与 `DROGON_JWT_SECRET_FILE` 互斥，同时设置会拒绝启动。
密钥文件必须是可读普通文件，最多 4096 字节且仅含一行；生成工具附带的单个
行尾换行会被安全移除。应用日志只记录密钥来源，不记录路径或内容。

`DROGON_REGISTRATION_MODE` 支持 `first-user`（默认，仅允许首个管理员注册）、`open`（开放注册，后续用户为普通角色）和 `disabled`（关闭公开注册）。密码使用 Argon2id 随机盐哈希；旧 SHA-256 哈希会在登录成功后自动升级。

登录连续失败默认 5 次后锁定账号 15 分钟，可通过
`DROGON_LOGIN_MAX_FAILURES`（3–20）和
`DROGON_LOGIN_LOCK_SECONDS`（30–86400）调整。JWT 携带会话版本；
不存在的邮箱也会执行等成本的 Argon2id 虚拟校验，并与密码错误返回相同响应，
降低通过登录耗时枚举有效账号的风险。
应用还在密码校验前执行进程级登录预算：默认每 60 秒全局 120 次、每个规范化
邮箱 10 次。超限返回 HTTP 429、稳定错误码 `4292` 和 `Retry-After`，避免随机
邮箱撞库持续消耗 Argon2id CPU/内存；标识符只以 SHA-256 指纹存放在有界内存表
中，不记录邮箱明文。可通过 `DROGON_LOGIN_RATE_LIMIT_GLOBAL`（10–100000）、
`DROGON_LOGIN_RATE_LIMIT_PER_IDENTIFIER`（3–全局上限）和
`DROGON_LOGIN_RATE_LIMIT_WINDOW_SECONDS`（1–3600）调整。限流状态是单进程内存
状态；多副本部署仍应在入口网关增加共享来源限流。
修改密码后旧 token 立即失效，受保护请求也会实时拒绝已禁用或已删除账号。
用户可在个人中心验证当前密码后自行设置新密码；成功后所有设备的现有 token
都会立即失效，并要求重新登录。
顶部“退出登录”会调用服务端注销接口递增会话版本，主动吊销该账号当前签发的
全部 token；即使 Token 被复制到其他设备，也无法在注销后继续使用。
邮箱登录标识会去除首尾空白并统一为小写，数据库通过 `COLLATE NOCASE`
唯一索引阻止仅大小写不同的重复账号。
管理员可在用户编辑页启用或禁用账号；修改邮箱、密码、状态或角色都会吊销该用户的现有 token。
用户管理页还提供“强制下线”，可按需吊销目标账号全部现有 Token，并写入审计日志。
系统以事务保护最后一个可用管理员，禁止删除、禁用或移除其管理员角色；内置 `admin` 角色具备系统级超级权限，且内置角色不可删除、管理员权限映射不可手工修改。
用户、角色和菜单的关键写操作以及自助改密会写入不可随主体删除的审计日志，
记录操作者、操作类型、目标、请求 ID、时间和经过白名单筛选的安全详情。
审计写入采用 best-effort 语义：失败会在服务端输出醒目告警，但不会把已经成功
的主业务响应改成失败；密码、密码哈希和 Token 永不写入审计详情。
未预期的数据库和服务端异常只向客户端返回通用错误，详细原因与请求 ID
仅写入服务端日志，避免泄漏 SQL 和内部结构。用户列表分页限制为每页最多
200 条，并严格拒绝非法页码、超长关键字和非整数参数。
认证与 RBAC DTO 会严格检查 JSON 字段类型、长度、ID 数量和标识符格式，
不接受 JsonCpp 的隐式数字/字符串转换。

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
mkdir -p secrets
openssl rand -hex 32 > secrets/jwt_secret
docker compose up -d --build
docker compose ps
curl http://127.0.0.1:8080/api/ready
```

Compose 会把 `./secrets/jwt_secret` 只挂载到容器的
`/run/secrets/drogon_jwt_secret`，不会把密钥值写入环境变量。若密钥文件位于
其他位置，在 `.env` 中设置 `DROGON_JWT_SECRET_PATH`；`secrets/` 已被 Git 和
镜像构建上下文忽略。密钥文件丢失、为空、包含多行或过长时，应用会在监听端口
前失败关闭。

Compose 默认仅绑定 `127.0.0.1`，应由同机反向代理提供公网 HTTPS。SQLite、
日志和上传目录分别保存在命名卷中；升级前应备份数据卷。容器健康检查调用
`/api/ready`，而 `/api/health` 仅用于进程存活检查。

### 优雅停机与流量摘除

收到 `SIGTERM` 或 `SIGINT` 后，服务立即进入 `draining`：`/api/ready` 返回
503，普通业务请求返回 503 与 `Retry-After`，但 `/api/health` 和
`/api/metrics` 在排空期间仍可用于诊断。进程会等待已进入处理链的请求完成，
随后停止 Drogon 并排空异步控制台日志；超过截止时间则记录剩余请求数并强制
退出。再次发送停机信号会跳过等待。

默认排空时间为 10 秒，可通过 `DROGON_SHUTDOWN_GRACE_SECONDS`（1–300）调整；
它必须小于容器编排器的强杀期限。`compose.yml` 已设置 15 秒
`stop_grace_period`，为默认排空和日志收尾预留 5 秒。部署到 Kubernetes 时，
应分别将 `/api/ready` 配为 readinessProbe、`/api/health` 配为 livenessProbe，
并确保 `terminationGracePeriodSeconds` 大于应用排空时间。

### SQLite 在线备份与恢复演练

不要直接复制运行中的 `drogon-admin.db`：WAL 中可能仍有未 checkpoint 的已提交
事务，裸拷贝主文件会得到过期或不可恢复的快照。仓库提供的备份工具使用 SQLite
Online Backup API，可在服务继续读写时生成事务一致的快照，并在原子发布前执行
`integrity_check` 和 `foreign_key_check`：

```bash
# 容器部署：默认保留最近 14 份成功备份到 ./backups
docker compose --profile ops run --rm backup

# 非容器部署
python scripts/sqlite_backup.py backup \
  --source data/drogon-admin.db --output-dir backups --retain 14

# 定期恢复演练前，重新校验文件大小、SHA-256、数据库结构和外键
python scripts/sqlite_backup.py verify \
  --database backups/drogon-admin-<UTC时间>.db
```

每份 `.db` 都有对应的 `.manifest.json`，记录 SHA-256、大小、SQLite 版本和迁移
基线；校验失败的临时快照不会发布，也不会参与保留策略。Linux 上若希望容器输出
归当前运维用户所有，可在运行前设置 `DROGON_BACKUP_UID=$(id -u)` 和
`DROGON_BACKUP_GID=$(id -g)`。定时任务只需周期执行同一个 Compose 命令，并将
`backups/` 再同步到异机或对象存储；同机备份不能代替异地容灾。

恢复时必须先运行 `verify`，再停止后端，保留当前数据目录，删除旧数据库对应的
`-wal`/`-shm` 文件，将已验证快照以临时文件写入同一数据目录并原子改名为
`drogon-admin.db`，同时保持容器运行用户 `10001:10001` 的所有权。启动后确认
`/api/ready` 为 200 并完成一次登录冒烟测试。建议每次发版前执行备份、每季度在
隔离环境完成一次恢复演练；切勿在后端运行期间替换数据库文件。

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
下载完整后台管理功能。根节点错误边界会捕获未处理的 React 渲染异常并显示
可恢复的刷新页面，避免用户只看到空白屏。
拥有 `dashboard:stats` 权限的管理员会在仪表盘看到用户状态、角色、可见菜单和
近 24 小时审计操作统计；普通用户仅看到自己的角色、权限和菜单概览。

## 默认接口

| Method | Path                               | 鉴权 | 权限要求     | 说明                    |
|--------|------------------------------------|------|--------------|-------------------------|
| GET    | /api/health                        | 否   | —            | 健康检查                |
| GET    | /api/ready                         | 否   | —            | DB/迁移就绪检查         |
| GET    | /api/metrics                       | 否   | —            | Prometheus 运行指标     |
| GET    | /api/dashboard/stats               | 是   | dashboard:stats | 管理员系统概览统计   |
| POST   | /api/auth/register                 | 否   | —            | 按注册策略创建账号      |
| POST   | /api/auth/login                    | 否   | —            | 登录，返回 JWT          |
| POST   | /api/auth/logout                   | 是   | —            | 安全退出并吊销 Token    |
| GET    | /api/users/me                      | 是   | —            | 获取当前登录用户        |
| PUT    | /api/users/me/password             | 是   | —            | 修改当前用户密码        |
| GET    | /api/users/{id}                    | 是   | —            | 按 id 查询用户          |
| POST   | /api/users                         | 是   | user:create  | 管理员创建普通用户      |
| POST   | /api/users/{id}/revoke-sessions    | 是   | user:update  | 强制下线目标用户        |
| POST   | /api/users/{id}/unlock             | 是   | user:update  | 解除目标用户登录锁定    |
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
| GET    | /api/audit-logs                    | 是   | audit:view   | 分页/按操作类型查询审计 |

测试：

```bash
ctest --test-dir build -C Release --output-on-failure
```

CTest 会启动临时 Drogon 实例和独立 SQLite 数据库，黑盒验证注册/登录、密码与角色变更后的 token 吊销、登录锁定及管理员解锁、最后管理员保护、内置角色保护、审计权限/记录/敏感数据隔离以及 CORS/安全响应头。独立的备份回归还会覆盖 WAL 快照、完整性校验、篡改拒绝和保留策略；测试不会读写开发数据库。

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
- 每个请求接受或生成 `X-Request-ID`，仅接受 8–128 位字母、数字、
  `.`、`_`、`-`，非法值会被替换；该 ID 会贯通响应头、错误日志和审计记录，
  便于反向代理、应用日志与操作审计关联。
- 访问日志以单行 JSON 输出 `request_id`、方法、路由模板、状态码、耗时微秒
  和已认证用户 ID；4xx/5xx 分别使用 WARN/ERROR。日志只记录 Drogon 匹配后的
  路由模板，未匹配请求统一记为 `<unmatched>`，不记录原始 URL、查询字符串、
  Authorization、Cookie 或请求体，避免动态标识符、token、密码及查询敏感值
  落日志。
- 已应用迁移记录在 `schema_migrations`；每个迁移在 `BEGIN IMMEDIATE` 事务中执行，失败会回滚并阻止服务进入就绪状态。记录同时保存 SQL 文件标准化换行后的 SHA-256 校验和，后续启动会拒绝被修改或缺失的历史迁移，防止环境间静默漂移；已发布迁移必须保持不可变，更正只能新增迁移文件。
- `/api/metrics` 输出 Prometheus 文本格式，包含构建版本、运行时长、活动请求数、
  就绪/排空状态、各状态码类别请求数、请求耗时直方图，以及按全局/标识符分类
  的登录限流计数。该端点不包含用户或路径标签，避免
  敏感信息及高基数指标；生产环境应继续通过 Compose 的 localhost 绑定或
  反向代理访问控制将其限制在监控网络。
- 审计查询由 `audit:view` 保护，默认仅 `admin` 拥有；普通 `user` 不会获得
  审计菜单或权限。支持 `page`、`page_size`，以及 `action`、
  `actor_user_id`、`target_type`、`target_id`、`request_id` 精确筛选。

## 首个注册用户自动晋升管理员

- 默认 `registration_mode=first-user`：仅第一个注册用户获得 admin 角色，随后关闭公开注册；
- 判断与“创建用户 + 绑定角色”位于同一个 SQLite `Immediate` 事务，避免并发注册产生多个首管理员；
- 如需开放注册可显式设置 `open`，后续用户只获得普通 `user` 角色。

## RBAC 权限设计

- **用户-角色-权限** 三元模型：`users` ↔ `user_roles` ↔ `roles` ↔ `role_permissions` ↔ `permissions`
- **菜单即权限**：每个菜单对应一组权限（`menu` 类型为页面访问权，`button` 类型为操作权），由 `menu_id` 关联
- **菜单树完整性**：创建和更新时验证父节点存在，禁止自引用及祖先挂到后代；
  含子节点的菜单必须先清空子节点后才能删除
- **PermissionFilter**：在 `PermissionRegistry` 中注册每个 Controller 方法的权限码，请求时校验 JWT 中的用户是否拥有对应权限
- **前端权限**：`DynamicMenu` 根据后端返回的权限树动态渲染侧边栏；`HasPerm` 组件控制按钮级显隐；路由层 `ProtectedRoute` 控制页面级访问

## 数据库（SQLite）

- 默认使用 SQLite，数据库文件位于 `data/drogon-admin.db`（首次启动自动创建）
- 启动时先对齐 `sql/schema.sql` 基线，再按文件名执行 `sql/migrations/*.sql`；版本写入 `schema_migrations`
- 升级旧数据库时会自动增加并回填迁移校验和；建立基线后，历史迁移内容
  变更或数据库中已应用的迁移文件缺失都会导致启动失败
- 启动时强制启用 WAL、`synchronous=NORMAL`、5 秒 busy timeout 与外键检查；
  `/api/ready` 会验证这些运行参数，缺失时返回 503
- RBAC 关联表使用外键约束，用户删除会级联清理角色映射，角色删除前必须先
  清理用户映射，防止产生孤儿权限记录
- schema 内置种子数据：默认角色 `admin`（全部权限）与 `user`（仅仪表盘+个人中心）
- `audit_logs` 按时间、操作、操作者和目标建立索引；不关联用户外键，以便账号
  或角色删除后仍保留追责链路
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

- **手动构建**：main 分支上不再自动触发构建。可在 GitHub Actions 页面点击 `Run workflow` 手动触发 `backend` / `frontend` / `musl-release` 流水线；这些组件入口只构建，不创建 tag 或发布。
- **PR 质量门**：面向 main 的 Pull Request 仍会自动执行构建以验证改动。
- **版本治理**：仓库根目录的 `VERSION` 是唯一发布版本源，必须与 CMake、vcpkg、前端 package 及 lockfile 保持一致。CI 会阻止版本不一致或 tag 与版本不匹配的发布；本地可运行 `python scripts/verify_release_version.py --tag v0.1.1` 预检。
- **手动发版**：在 GitHub Actions 中从 `main` 运行 `release` workflow 并填写与 `VERSION` 一致的新 tag。编排器只负责校验版本、创建 annotated tag，并在该 tag 上显式分发 `backend`、`frontend` 和 `musl-release`；这是因为 workflow 内置 `GITHUB_TOKEN` 推送的 tag 不会自动触发其他 workflow。相同 tag 的并发编排会被串行化，已存在 tag 默认拒绝；若首次分发中途失败，可勾选 `redispatch_existing` 恢复，但编排器只会复用仍指向当前 `main` 提交的 tag。
- **自动发版**：推送形如 `v*`（如 `v0.1.0`、`v1.2.3-rc1`）的 tag 时，`backend` 会构建三平台 Release 产物（Windows static-md / Ubuntu 22.04 / macOS arm64），`musl-release` 会通过 Alpine 3.24 构建无动态加载器依赖的 x64 / arm64 musl 静态产物，`frontend` 会构建 `dist`，分别打包后自动上传到同名 tag 的 GitHub Release：
  - `drogon-admin-<tag>-windows-x64.zip`
  - `drogon-admin-<tag>-ubuntu22-x64.tar.gz`
  - `drogon-admin-<tag>-macos-arm64.tar.gz`（experimental）
  - `drogon-admin-<tag>-linux-musl-x64.tar.gz`
  - `drogon-admin-<tag>-linux-musl-arm64.tar.gz`
  - `drogon-admin-front-<tag>.zip`
  - 每个压缩包均附带同名 `.sha256` 校验文件，并同时保留在 workflow artifact 中。
  - 带 `-` 的 tag（如 `v1.0.0-rc1`）会被标记为 pre-release。

本地直接推 tag 仍可发版（会由 tag 事件触发三个组件 workflow）：

```bat
python scripts/verify_release_version.py --tag v0.1.1
git tag v0.1.1
git push origin v0.1.1
```
