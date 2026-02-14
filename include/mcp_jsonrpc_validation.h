/**
 * @file mcp_jsonrpc_validation.h
 * @brief JSON-RPC 2.0 message validation utilities
 * 
 * This file provides validation utilities for JSON-RPC 2.0 messages
 * to ensure conformance with the specification and MCP requirements.
 */

#ifndef MCP_JSONRPC_VALIDATION_H
#define MCP_JSONRPC_VALIDATION_H

#include "mcp_message.h"
#include <string>
#include <set>
#include <mutex>

namespace mcp {

/**
 * @brief Validates JSON-RPC 2.0 request ID
 * 
 * According to JSON-RPC 2.0:
 * - ID MUST be a String, Number, or NULL value if included
 * - For requests (expecting a response), ID MUST NOT be null
 * - For notifications (no response expected), ID MUST NOT be present
 * 
 * @param id_json The ID field from the JSON message
 * @param is_notification True if this is a notification, false if it's a request
 * @param error_message Output parameter for error description
 * @return true if valid, false otherwise
 */
inline bool validate_request_id(const json& id_json, bool is_notification, std::string& error_message) {
    // Notifications MUST NOT have an ID field
    if (is_notification) {
        error_message = "Notifications must not have an 'id' field";
        return false;
    }
    
    // Requests MUST have a valid ID (not null)
    if (id_json.is_null()) {
        error_message = "Request 'id' must not be null";
        return false;
    }
    
    // ID must be string, number (integer or float), per JSON-RPC 2.0 spec
    if (!id_json.is_string() && !id_json.is_number()) {
        error_message = "Request 'id' must be a string or number";
        return false;
    }
    
    return true;
}

/**
 * @brief Validates that a JSON-RPC message has required fields
 * 
 * @param msg_json The JSON message to validate
 * @param error_message Output parameter for error description
 * @return true if valid, false otherwise
 */
inline bool validate_jsonrpc_message(const json& msg_json, std::string& error_message) {
    // Must be an object
    if (!msg_json.is_object()) {
        error_message = "JSON-RPC message must be an object";
        return false;
    }
    
    // Must have jsonrpc field
    if (!msg_json.contains("jsonrpc")) {
        error_message = "Missing 'jsonrpc' field";
        return false;
    }
    
    // jsonrpc must be "2.0"
    if (!msg_json["jsonrpc"].is_string() || msg_json["jsonrpc"].get<std::string>() != "2.0") {
        error_message = "Field 'jsonrpc' must be \"2.0\"";
        return false;
    }
    
    // Must have method field (for requests/notifications)
    if (!msg_json.contains("method")) {
        // Could be a response, check for result or error
        if (!msg_json.contains("result") && !msg_json.contains("error")) {
            error_message = "Missing 'method' field (not a valid request or response)";
            return false;
        }
        return true; // Valid response message
    }
    
    // method must be a string
    if (!msg_json["method"].is_string()) {
        error_message = "Field 'method' must be a string";
        return false;
    }
    
    return true;
}

/**
 * @brief Validates a JSON-RPC request message
 * 
 * @param msg_json The JSON message to validate
 * @param error_message Output parameter for error description
 * @return true if valid, false otherwise
 */
inline bool validate_request_message(const json& msg_json, std::string& error_message) {
    // First do basic validation
    if (!validate_jsonrpc_message(msg_json, error_message)) {
        return false;
    }
    
    // Must have method (checked in validate_jsonrpc_message)
    if (!msg_json.contains("method")) {
        error_message = "Request must have a 'method' field";
        return false;
    }
    
    // Check if ID field is present
    bool has_id = msg_json.contains("id");
    
    if (has_id) {
        // If ID is present, it must be valid (not null, and string/number)
        // Per JSON-RPC 2.0: id MUST contain a String, Number, or NULL value
        // But MCP 2025-03-26 requires: for requests, ID MUST NOT be null
        // For notifications, ID MUST NOT be present (not even as null)
        if (msg_json["id"].is_null()) {
            error_message = "Request 'id' must not be null (for notifications, omit the 'id' field entirely)";
            return false;
        }
        
        if (!validate_request_id(msg_json["id"], false, error_message)) {
            return false;
        }
    }
    // If no ID field at all, this is a valid notification
    
    // Params field is optional, but if present must be structured
    if (msg_json.contains("params")) {
        const auto& params = msg_json["params"];
        if (!params.is_object() && !params.is_array()) {
            error_message = "Field 'params' must be an object or array";
            return false;
        }
    }
    
    return true;
}

/**
 * @brief Validates a JSON-RPC response message
 * 
 * According to JSON-RPC 2.0:
 * - Response MUST contain exactly one of "result" or "error"
 * - If error is present, it must have "code" (integer) and "message" (string)
 * 
 * @param msg_json The JSON message to validate
 * @param error_message Output parameter for error description
 * @return true if valid, false otherwise
 */
inline bool validate_response_message(const json& msg_json, std::string& error_message) {
    // Must be an object
    if (!msg_json.is_object()) {
        error_message = "JSON-RPC response must be an object";
        return false;
    }
    
    // Must have jsonrpc field with value "2.0"
    if (!msg_json.contains("jsonrpc") || !msg_json["jsonrpc"].is_string() || 
        msg_json["jsonrpc"].get<std::string>() != "2.0") {
        error_message = "Response must have 'jsonrpc' field with value \"2.0\"";
        return false;
    }
    
    // Must have id field
    if (!msg_json.contains("id")) {
        error_message = "Response must have an 'id' field";
        return false;
    }
    
    // Must have exactly one of result or error
    bool has_result = msg_json.contains("result");
    bool has_error = msg_json.contains("error");
    
    if (has_result && has_error) {
        error_message = "Response must not have both 'result' and 'error'";
        return false;
    }
    
    if (!has_result && !has_error) {
        error_message = "Response must have either 'result' or 'error'";
        return false;
    }
    
    // If error is present, validate its structure
    if (has_error) {
        const auto& error = msg_json["error"];
        
        if (!error.is_object()) {
            error_message = "Field 'error' must be an object";
            return false;
        }
        
        if (!error.contains("code")) {
            error_message = "Error object must have 'code' field";
            return false;
        }
        
        if (!error["code"].is_number_integer()) {
            error_message = "Error 'code' must be an integer";
            return false;
        }
        
        if (!error.contains("message")) {
            error_message = "Error object must have 'message' field";
            return false;
        }
        
        if (!error["message"].is_string()) {
            error_message = "Error 'message' must be a string";
            return false;
        }
    }
    
    return true;
}

/**
 * @brief Tracks request IDs to ensure uniqueness per session
 */
class request_id_tracker {
public:
    /**
     * @brief Add a request ID for a session
     * @param session_id The session identifier
     * @param request_id The request ID to track
     * @return true if ID was unique and added, false if duplicate
     */
    bool add_request_id(const std::string& session_id, const json& request_id) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        // Create a unique key combining session and request ID
        std::string key = session_id + ":" + request_id.dump();
        
        // Check if already exists
        auto it = active_ids_.find(key);
        if (it != active_ids_.end()) {
            return false; // Duplicate ID
        }
        
        // Add to active set
        active_ids_.insert(key);
        return true;
    }
    
    /**
     * @brief Remove a request ID when response is sent
     * @param session_id The session identifier
     * @param request_id The request ID to remove
     */
    void remove_request_id(const std::string& session_id, const json& request_id) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::string key = session_id + ":" + request_id.dump();
        active_ids_.erase(key);
    }
    
    /**
     * @brief Clear all request IDs for a session (on disconnect)
     * @param session_id The session identifier
     */
    void clear_session(const std::string& session_id) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        // Remove all IDs for this session
        // Use prefix matching to find all keys starting with "session_id:"
        std::string prefix = session_id + ":";
        auto it = active_ids_.begin();
        while (it != active_ids_.end()) {
            if (it->rfind(prefix, 0) == 0) {
                it = active_ids_.erase(it);
            } else {
                ++it;
            }
        }
    }
    
private:
    std::mutex mutex_;
    std::set<std::string> active_ids_;
};

} // namespace mcp

#endif // MCP_JSONRPC_VALIDATION_H
