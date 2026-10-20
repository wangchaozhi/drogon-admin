//
// drogon-admin 入口：把启动流程下放给 core::Bootstrap，main 只保持薄薄一层。
// Controller / Filter / Plugin 由 Drogon 的 IoC 自动注册，main 不需要 include 它们。
//
#include "core/Bootstrap.h"
#include <drogon/drogon.h>

#include <cstdio>
#include <cstdlib>
#include <exception>

#ifdef _WIN32
#include <windows.h>
#endif

int main() {
#ifdef _WIN32
    // 让控制台输出 UTF-8 中文不乱码
    SetConsoleOutputCP(65001);
    SetConsoleCP(65001);
#endif

    bool bootstrapInitialized = false;
    try {
        core::Bootstrap::init("config/config.json");
        bootstrapInitialized = true;
        drogon::app().run();
        core::Bootstrap::shutdown();
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "startup failed: %s\n", error.what());
        std::fflush(stderr);
        if (bootstrapInitialized) {
            try {
                core::Bootstrap::shutdown();
            } catch (...) {
                // The original startup error remains the actionable failure.
            }
        }
        return EXIT_FAILURE;
    } catch (...) {
        std::fprintf(stderr, "startup failed: unknown error\n");
        std::fflush(stderr);
        if (bootstrapInitialized) {
            try {
                core::Bootstrap::shutdown();
            } catch (...) {
            }
        }
        return EXIT_FAILURE;
    }
}
