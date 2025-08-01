//
// Created by pmacc on 9/29/2024.
//

#pragma once

#include <fstream>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <ctime>

namespace perf_boost {
    extern std::ofstream debugLogFile;

    extern uint32_t gStartTime;
    extern uint32_t GetTime();

    // Returns a human-readable timestamp string, e.g. "2024-09-29 14:33:12"
    inline std::string GetHumanTimestamp() {
        auto now = std::chrono::system_clock::now();
        std::time_t now_c = std::chrono::system_clock::to_time_t(now);
        std::tm tm_buf;
    #ifdef _WIN32
        localtime_s(&tm_buf, &now_c);
    #else
        localtime_r(&now_c, &tm_buf);
    #endif
        std::ostringstream oss;
        oss << std::put_time(&tm_buf, "%m-%d %H:%M:%S");
        return oss.str();
    }

#ifndef DEBUG_LOG_H
#define DEBUG_LOG_H

#define DEBUG_LOG(msg) debugLogFile << perf_boost::GetHumanTimestamp() << ": " << msg << std::endl
#define NEWLINE_LOG() debugLogFile << std::endl

#endif  // DEBUG_LOG_H
}