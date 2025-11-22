#include "NativeBridge.h"
#include "Logger.h"

using namespace core;

EventBus NativeBridge::bus;

void NativeBridge::init(){
    Logger::log(LogLevel::Info, "NativeBridge initialized");
}

void NativeBridge::onPermissionResult(bool granted){
    std::string payload = granted?"granted":"denied";
    bus.publish("permissions", payload);
    Logger::log(LogLevel::Info, std::string("Permission result: ")+payload);
}

