//
// drogon-admin 入口：把启动流程下放给 core::Bootstrap，main 只保持薄薄一层。
// Controller / Filter / Plugin 由 Drogon 的 IoC 自动注册，main 不需要 include 它们。
//
#include "core/Bootstrap.h"
#include <drogon/drogon.h>

#ifdef _WIN32
#include <windows.h>
#endif

int main() {
#ifdef _WIN32
    // 让控制台输出 UTF-8 中文不乱码
    SetConsoleOutputCP(65001);
    SetConsoleCP(65001);
#endif

    core::Bootstrap::init("config/config.json");
    drogon::app().run();
    return 0;
}
