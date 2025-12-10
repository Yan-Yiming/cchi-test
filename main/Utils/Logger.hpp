// cchi-test/main/Utils/Logger.hpp
#pragma once

#include <iostream>
#include <string>

// 定义日志级别
enum class LogLevel {
    ERROR = 0,  // 错误 (总是输出)
    INFO  = 1,  // 关键信息 (默认输出)
    DEBUG = 2,  // 调试信息 (详细过程)
    TRACE = 3   // 追踪信息 (极度详细，包含每拍的信号)
};

// 全局日志级别变量 (在 main.cpp 中定义)
extern LogLevel g_CurrentLogLevel;

// 简单的日志包装器
struct LogMessage {
    LogLevel level;
    bool should_log;

    LogMessage(LogLevel l) : level(l) {
        should_log = (l <= g_CurrentLogLevel);
        if (should_log) {
            // 根据级别打印前缀
            switch (l) {
                case LogLevel::ERROR: std::cout << "\033[1;31m[ERROR]\033[0m "; break; // 红色
                case LogLevel::INFO:  std::cout << "[INFO ] "; break;
                case LogLevel::DEBUG: std::cout << "\033[1;34m[DEBUG]\033[0m "; break; // 蓝色
                case LogLevel::TRACE: std::cout << "\033[1;30m[TRACE]\033[0m "; break; // 灰色
            }
        }
    }

    ~LogMessage() {
        if (should_log) {
            std::cout << std::endl; // 自动换行
        }
    }

    // 重载 << 运算符，支持像 cout 一样拼接字符串和数字
    template <typename T>
    LogMessage& operator<<(const T& msg) {
        if (should_log) {
            std::cout << msg;
        }
        return *this;
    }
};

// 宏定义，方便使用
// 用法: LOG(DEBUG) << "Value is " << x;
#define LOG(level) LogMessage(LogLevel::level)