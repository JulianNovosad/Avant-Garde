// Centralized network port configuration
#pragma once

#include <cstdint>

namespace blacknode {
    // Engine / native defaults
    constexpr uint16_t VIDEO_PORT = 50000;         // MJPEG video UDP
    constexpr uint16_t TELEMETRY_BASE = 50010;     // telemetry UDP base (50010-50014)
    constexpr uint16_t CONTROL_PORT = 50020;       // control outbound JSON

    // Desktop-harness defaults
    constexpr uint16_t DESKTOP_UDP_PORT = 40321;   // desktop UDP receiver
    constexpr uint16_t DESKTOP_TCP_PORT = 40322;   // desktop TCP client
    constexpr uint16_t DESKTOP_WS_PORT = 8087;     // example WS server used in desktop URL

    // Web defaults
    constexpr uint16_t DEFAULT_WS_PORT = 80;
}
