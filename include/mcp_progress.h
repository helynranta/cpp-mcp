/**
 * @file mcp_progress.h
 * @brief Progress notification support for MCP
 *
 * This file implements progress tracking for long-running operations
 * according to the Model Context Protocol specification (2025-03-26).
 *
 * Progress notifications allow clients and servers to report the status
 * of ongoing operations with optional messages and completion percentages.
 */

#ifndef MCP_PROGRESS_H
#define MCP_PROGRESS_H

#include "mcp_message.h"

#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>

namespace mcp {

/**
 * @struct progress_notification
 * @brief Represents a progress notification
 *
 * Progress notifications include:
 * - progressToken: The token from the original request
 * - progress: Current progress value (must always increase)
 * - total: Optional total value (can be omitted if unknown)
 * - message: Optional human-readable status message
 */
struct progress_notification {
    /** Token identifying the operation (from the original request) */
    json progress_token;

    /** Current progress value (must always increase) */
    double progress;

    /** Optional total value (std::nullopt if unknown) */
    std::optional<double> total;

    /** Optional human-readable status message */
    std::optional<std::string> message;

    /**
     * @brief Create a progress notification
     * @param token The progress token from the request
     * @param current Current progress value
     * @param total_val Optional total value
     * @param msg Optional status message
     */
    static progress_notification create(const json& token, double current,
                                        std::optional<double> total_val = std::nullopt,
                                        std::optional<std::string> msg = std::nullopt) {
        progress_notification notif;
        notif.progress_token = token;
        notif.progress = current;
        notif.total = total_val;
        notif.message = msg;
        return notif;
    }

    /**
     * @brief Convert to JSON-RPC notification params
     * @return JSON object with progress notification parameters
     */
    json to_params() const {
        json params = {{"progressToken", progress_token}, {"progress", progress}};

        if (total.has_value()) {
            params["total"] = total.value();
        }

        if (message.has_value()) {
            params["message"] = message.value();
        }

        return params;
    }

    /**
     * @brief Create from JSON params
     * @param params JSON object with progress notification parameters
     * @return Progress notification object
     */
    static progress_notification from_params(const json& params) {
        progress_notification notif;
        notif.progress_token = params["progressToken"];
        notif.progress = params["progress"].get<double>();

        if (params.contains("total")) {
            notif.total = params["total"].get<double>();
        }

        if (params.contains("message")) {
            notif.message = params["message"].get<std::string>();
        }

        return notif;
    }
};

/**
 * @brief Handler function for progress notifications
 * @param notification The progress notification
 */
using progress_handler = std::function<void(const progress_notification&)>;

/**
 * @class progress_tracker
 * @brief Tracks active progress tokens and manages progress reporting
 *
 * This class manages progress tokens from requests and provides
 * functionality to send progress notifications.
 */
class progress_tracker {
public:
    progress_tracker() = default;

    /**
     * @brief Extract progress token from request params
     * @param params The request parameters
     * @return Optional progress token if present in _meta
     */
    static std::optional<json> extract_progress_token(const json& params) {
        if (params.contains("_meta") && params["_meta"].contains("progressToken")) {
            return std::optional<json>{json(params["_meta"]["progressToken"])};
        }
        return std::nullopt;
    }

    /**
     * @brief Register an active progress token
     * @param token The progress token
     * @param request_id The request ID associated with this token
     */
    void register_token(const json& token, const json& request_id) {
        std::lock_guard<std::mutex> lock(mutex_);
        active_tokens_[token.dump()] = request_id;
    }

    /**
     * @brief Unregister a progress token (when operation completes)
     * @param token The progress token
     */
    void unregister_token(const json& token) {
        std::lock_guard<std::mutex> lock(mutex_);
        active_tokens_.erase(token.dump());
    }

    /**
     * @brief Check if a progress token is active
     * @param token The progress token
     * @return True if the token is registered and active
     */
    bool is_token_active(const json& token) const {
        std::lock_guard<std::mutex> lock(mutex_);
        return active_tokens_.find(token.dump()) != active_tokens_.end();
    }

    /**
     * @brief Get the request ID associated with a token
     * @param token The progress token
     * @return Optional request ID if token is active
     */
    std::optional<json> get_request_id(const json& token) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = active_tokens_.find(token.dump());
        if (it != active_tokens_.end()) {
            return std::optional<json>{json(it->second)};
        }
        return std::nullopt;
    }

    /**
     * @brief Clear all active tokens
     */
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        active_tokens_.clear();
    }

private:
    mutable std::mutex mutex_;
    std::map<std::string, json> active_tokens_;
};

/**
 * @brief Create a progress notification request
 * @param notification The progress notification to send
 * @return A JSON-RPC notification request
 */
inline request create_progress_notification(const progress_notification& notification) {
    return request::create_notification("progress", notification.to_params());
}

} // namespace mcp

#endif // MCP_PROGRESS_H
