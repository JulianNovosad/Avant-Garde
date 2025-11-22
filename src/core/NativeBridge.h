#pragma once
#include <string>
#include "EventBus.h"

namespace core {

class NativeBridge {
public:
    static void init();
    static void onPermissionResult(bool granted);
    static int subscribe(const std::string &topic, EventBus::Handler h){ return bus.subscribe(topic, h); }
    static void unsubscribe(const std::string &topic, int id){ bus.unsubscribe(topic, id); }
    static void publish(const std::string &topic, const std::string &payload){ bus.publish(topic, payload); }
private:
    static EventBus bus;
};

}
