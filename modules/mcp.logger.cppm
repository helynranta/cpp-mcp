/**
 * @file mcp.logger.cppm
 * @brief MCP Logger Module - Simple logging utilities
 *
 * This module provides logging functionality for the MCP framework.
 * It is a standalone module with no dependencies on other MCP modules.
 */

module;

// Global module fragment - include standard library headers
#include <chrono>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>

export module mcp.logger;

// Export the logger functionality
export namespace mcp {

enum class log_level { debug, info, warning, error };

class logger {
public:
    static logger& instance() {
        static logger instance;
        return instance;
    }

    void set_level(log_level level) {
        std::lock_guard<std::mutex> lock(mutex_);
        level_ = level;
    }

    template <typename... Args>
    void debug(Args&&... args) {
        log(log_level::debug, std::forward<Args>(args)...);
    }

    template <typename... Args>
    void info(Args&&... args) {
        log(log_level::info, std::forward<Args>(args)...);
    }

    template <typename... Args>
    void warning(Args&&... args) {
        log(log_level::warning, std::forward<Args>(args)...);
    }

    template <typename... Args>
    void error(Args&&... args) {
        log(log_level::error, std::forward<Args>(args)...);
    }

private:
    logger() : level_(log_level::info) {}

    template <typename T>
    void log_impl(std::stringstream& ss, T&& arg) {
        ss << std::forward<T>(arg);
    }

    template <typename T, typename... Args>
    void log_impl(std::stringstream& ss, T&& arg, Args&&... args) {
        ss << std::forward<T>(arg);
        log_impl(ss, std::forward<Args>(args)...);
    }

    template <typename... Args>
    void log(log_level level, Args&&... args) {
        if (level < level_) {
            return;
        }

        std::stringstream ss;

        // Add timestamp
        auto now = std::chrono::system_clock::now();
        auto now_c = std::chrono::system_clock::to_time_t(now);
        auto now_tm = std::localtime(&now_c);

        ss << std::put_time(now_tm, "%Y-%m-%d %H:%M:%S") << " ";

        // Add log level and color
        switch (level) {
            case log_level::debug:
                ss << "\033[36m[DEBUG]\033[0m "; // Cyan
                break;
            case log_level::info:
                ss << "\033[32m[INFO]\033[0m "; // Green
                break;
            case log_level::warning:
                ss << "\033[33m[WARNING]\033[0m "; // Yellow
                break;
            case log_level::error:
                ss << "\033[31m[ERROR]\033[0m "; // Red
                break;
        }

        // Add log content
        log_impl(ss, std::forward<Args>(args)...);

        // Output log
        std::lock_guard<std::mutex> lock(mutex_);
        std::cerr << ss.str() << std::endl;
    }

    log_level level_;
    std::mutex mutex_;
};

// Helper function to set log level
inline void set_log_level(log_level level) {
    mcp::logger::instance().set_level(level);
}

} // export namespace mcp

// Export convenience macros as inline functions to avoid preprocessor issues
export namespace mcp {
    template <typename... Args>
    inline void log_debug(Args&&... args) {
        logger::instance().debug(std::forward<Args>(args)...);
    }

    template <typename... Args>
    inline void log_info(Args&&... args) {
        logger::instance().info(std::forward<Args>(args)...);
    }

    template <typename... Args>
    inline void log_warning(Args&&... args) {
        logger::instance().warning(std::forward<Args>(args)...);
    }

    template <typename... Args>
    inline void log_error(Args&&... args) {
        logger::instance().error(std::forward<Args>(args)...);
    }
}
