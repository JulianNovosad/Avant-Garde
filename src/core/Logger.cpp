#include "Logger.h"
#include <iostream>
#ifdef ANDROID
#include <android/log.h>
#endif

using namespace core;

void Logger::log(LogLevel lv, const std::string &msg){
#ifdef ANDROID
    int priority = ANDROID_LOG_INFO;
    switch(lv){ case LogLevel::Debug: priority=ANDROID_LOG_DEBUG; break; case LogLevel::Info: priority=ANDROID_LOG_INFO; break; case LogLevel::Warn: priority=ANDROID_LOG_WARN; break; case LogLevel::Error: priority=ANDROID_LOG_ERROR; break; }
    __android_log_print(priority, "Blacknode", "%s", msg.c_str());
#else
    const char* prefix = "[I]";
    switch(lv){ case LogLevel::Debug: prefix="[D]"; break; case LogLevel::Info: prefix="[I]"; break; case LogLevel::Warn: prefix="[W]"; break; case LogLevel::Error: prefix="[E]"; break; }
    std::cout<<prefix<<" "<<msg<<"\n";
#endif
}
