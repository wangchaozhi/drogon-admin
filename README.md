# c_web

[![backend](https://github.com/wangchaozhi/brogon-admin/actions/workflows/backend.yml/badge.svg)](https://github.com/wangchaozhi/brogon-admin/actions/workflows/backend.yml)
[![frontend](https://github.com/wangchaozhi/brogon-admin/actions/workflows/frontend.yml/badge.svg)](https://github.com/wangchaozhi/brogon-admin/actions/workflows/frontend.yml)

基于 **Drogon** 的模块化 C++ Web 服务骨架 + **React 19 + Ant Design 6** 前端，Windows + MSVC + vcpkg 构建。

## 目录结构

```
c_web/
├── src/                        # 后端 C++ 源码
│   ├── main.cc                 # 入口，薄薄一层
│   ├── core/                   # 框架层：Bootstrap / Result / Logger
│   ├── common/                 # 通用工具：TimeUtil / CryptoUtil / JwtUtil
│   ├── filters/                # 横切过滤器：AuthFilter / CorsFilter
│   ├── plugins/                # 全局单例：ConfigPlugin
│   └── modules/                # 业务模块（自治）
│       └── user/
│           ├── UserController  # HTTP 路由（/api/auth/* 与 /api/users/*）
│           ├── UserService     # 业务逻辑（异步）
│           ├── UserRepository  # 数据访问（SQLite 异步）
│           └── dto/            # 请求 / 响应 DTO（LoginReq / CreateUserReq ...）
├── front/                      # 前端（React 19 + Vite + Ant Design 6）
│   └── src/
│       ├── pages/              # 页面：auth / home + AppRoutes
│       ├── components/         # 通用组件：AppHeader
│       ├── context/            # AuthContext
│       ├── hooks/              # useAuth / useDevice（响应式三端适配）
│       ├── theme/              # Ant Design 主题定制
│       ├── utils/              # remember 等工具
│       └── api.ts              # 前端 API 封装
├── config/config.json          # Drogon 运行配置（含 db_clients / jwt_secret）
├── sql/schema.sql              # 启动时自动应用的建表脚本
├── data/                       # SQLite 数据库文件目录（运行时自动创建）
├── build.bat / build_vs.bat    # Windows 构建脚本
├── CMakeLists.txt
└── vcpkg.json
```

## 前置依赖

### 后端
1. Visual Studio 2022（含 C++ 桌面开发组件）
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

前端通过 `src/api.ts` 统一调用后端 `http://127.0.0.1:8080/api/*` 接口，登录成功后 JWT 写入本地并由 `AuthContext` 全局维护。

## 默认接口

| Method | Path                | 鉴权 | 说明                    |
|--------|---------------------|------|-------------------------|
| GET    | /api/health         | 否   | 健康检查                |
| POST   | /api/auth/register  | 否   | 注册账号（JSON body）   |
| POST   | /api/auth/login     | 否   | 登录，返回 JWT          |
| GET    | /api/users/me       | 是   | 获取当前登录用户        |
| GET    | /api/users/{id}     | 是   | 按 id 查询用户          |
| POST   | /api/users          | 否   | 兼容旧路径，等价于注册  |

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
