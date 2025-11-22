#pragma once
#include <string>

namespace core {

enum class LogLevel { Debug, Info, Warn, Error };

class Logger {
public:
    static void log(LogLevel lv, const std::string &msg);
};

}

// convenience macros
#ifdef ANDROID
#include <android/log.h>
#define LOG_DEBUG(msg) core::Logger::log(core::LogLevel::Debug, msg)
#else
#include <iostream>
#define LOG_DEBUG(msg) core::Logger::log(core::LogLevel::Debug, msg)
#endif
