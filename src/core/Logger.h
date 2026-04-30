#pragma once
//
// 轻量日志宏，统一走 Drogon 的日志，保证进程内日志格式一致。
//
#include <trantor/utils/Logger.h>

#define APP_LOG_TRACE LOG_TRACE
#define APP_LOG_DEBUG LOG_DEBUG
#define APP_LOG_INFO  LOG_INFO
#define APP_LOG_WARN  LOG_WARN
#define APP_LOG_ERROR LOG_ERROR
#define APP_LOG_FATAL LOG_FATAL
