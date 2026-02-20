/**
 * @file mcp_message.h
 * @brief Core definitions for the Model Context Protocol (MCP) framework
 *
 * This file contains the core structures and definitions for the MCP protocol.
 * Implements the 2025-06-18 protocol specification.
 */

#ifndef MCP_MESSAGE_H
#define MCP_MESSAGE_H

#include <atomic>
#include <functional>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

// Include the JSON library for parsing and generating JSON
#include <nlohmann/json.hpp>

namespace mcp {

// Use the nlohmann json library
using json = nlohmann::ordered_json;

// MCP version - Currently claims conformance with 2025-11-25 specification
// Core implementation based on 2025-06-18 with 2025-11-25 extensions ready
// Supported versions for protocol negotiation: 2025-03-26, 2025-06-18, 2025-11-25
constexpr const char* MCP_VERSION = "2025-11-25";

// MCP error codes (JSON-RPC 2.0 standard codes)
enum class error_code {
    parse_error = -32700,        // Invalid JSON
    invalid_request = -32600,    // Invalid Request object
    method_not_found = -32601,   // Method not found
    invalid_params = -32602,     // Invalid method parameters
    internal_error = -32603,     // Internal JSON-RPC error
    server_error_start = -32000, // Server error start
    server_error_end = -32099    // Server error end
};

// MCP exception class
class mcp_exception : public std::runtime_error {
public:
    mcp_exception(error_code code, const std::string& message) : std::runtime_error(message), code_(code) {}

    error_code code() const { return code_; }

private:
    error_code code_;
};

// JSON-RPC 2.0 Request
struct request {
    std::string jsonrpc = "2.0";
    json id;
    std::string method;
    json params;

    // Create a request
    static request create(const std::string& method, const json& params = json::object()) {
        request req;
        req.jsonrpc = "2.0";
        req.id = generate_id();
        req.method = method;
        req.params = params;
        return req;
    }

    // Create a request with a specific ID
    static request create_with_id(const json& id, const std::string& method, const json& params = json::object()) {
        request req;
        req.jsonrpc = "2.0";
        req.id = id;
        req.method = method;
        req.params = params;
        return req;
    }

    // Create a notification (no response expected)
    static request create_notification(const std::string& method, const json& params = json::object()) {
        request req;
        req.jsonrpc = "2.0";
        req.id = nullptr;
        req.method = "notifications/" + method;
        req.params = params;
        return req;
    }

    // Check if this is a notification
    bool is_notification() const { return id.is_null(); }

    // Convert to JSON
    json to_json() const {
        json j = {{"jsonrpc", jsonrpc}, {"method", method}};

        if (!params.empty()) {
            j["params"] = params;
        }

        if (!is_notification()) {
            j["id"] = id;
        }

        return j;
    }

    static request from_json(const json& j) {
        request req;
        req.jsonrpc = j["jsonrpc"].get<std::string>();
        req.id = j["id"];
        req.method = j["method"].get<std::string>();
        req.params = j["params"];
        return req;
    }

private:
    // Generate a unique ID
    static json generate_id() {
        static std::atomic<int> next_id{1};
        return next_id.fetch_add(1);
    }
};

// JSON-RPC 2.0 Response
struct response {
    std::string jsonrpc = "2.0";
    json id;
    json result;
    json error;

    // Create a success response
    static response create_success(const json& req_id, const json& result_data = json::object()) {
        response res;
        res.jsonrpc = "2.0";
        res.id = req_id;
        res.result = result_data;
        return res;
    }

    // Create an error response
    // Per MCP specification: error objects must contain only 'code' (integer) and 'message' (string)
    static response create_error(const json& req_id, error_code code, const std::string& message) {
        response res;
        res.jsonrpc = "2.0";
        res.id = req_id;
        res.error = {{"code", static_cast<int>(code)}, {"message", message}};
        return res;
    }

    // Check if this is an error response
    bool is_error() const { return !error.empty(); }

    // Convert to JSON
    json to_json() const {
        json j = {{"jsonrpc", jsonrpc}, {"id", id}};

        if (is_error()) {
            j["error"] = error;
        } else {
            j["result"] = result;
        }

        return j;
    }

    static response from_json(const json& j) {
        response res;
        res.jsonrpc = j["jsonrpc"].get<std::string>();
        res.id = j["id"];
        res.result = j["result"];
        res.error = j["error"];
        return res;
    }
};

/**
 * @brief Elicitation request parameters
 *
 * Per MCP 2025-06-18 specification:
 * - message: Human-readable prompt for the user
 * - requestedSchema: JSON Schema defining expected response structure
 *   (restricted to flat objects with primitive properties)
 */
struct elicitation_params {
    std::string message;
    json requested_schema;

    // Convert to JSON
    json to_json() const { return {{"message", message}, {"requestedSchema", requested_schema}}; }

    // Create from JSON
    static elicitation_params from_json(const json& j) {
        elicitation_params params;
        params.message = j["message"].get<std::string>();
        params.requested_schema = j["requestedSchema"];
        return params;
    }
};

/**
 * @brief Elicitation response actions
 *
 * Per MCP 2025-06-18 specification:
 * - accept: User explicitly approved and submitted with data
 * - decline: User explicitly declined the request
 * - cancel: User dismissed without making an explicit choice
 */
enum class elicitation_action {
    accept,  // User accepted and provided data
    decline, // User explicitly declined
    cancel   // User dismissed without choice
};

/**
 * @brief Elicitation response result
 *
 * Per MCP 2025-06-18 specification:
 * - action: One of accept, decline, or cancel
 * - content: Data submitted by user (only present for accept action)
 */
struct elicitation_result {
    elicitation_action action;
    json content; // Only present for "accept" action

    // Convert to JSON
    json to_json() const {
        std::string action_str;
        switch (action) {
            case elicitation_action::accept:
                action_str = "accept";
                break;
            case elicitation_action::decline:
                action_str = "decline";
                break;
            case elicitation_action::cancel:
                action_str = "cancel";
                break;
        }

        json j = {{"action", action_str}};

        // Only include content for "accept" action
        if (action == elicitation_action::accept && !content.empty()) {
            j["content"] = content;
        }

        return j;
    }

    // Create from JSON
    static elicitation_result from_json(const json& j) {
        elicitation_result result;

        std::string action_str = j["action"].get<std::string>();
        if (action_str == "accept") {
            result.action = elicitation_action::accept;
        } else if (action_str == "decline") {
            result.action = elicitation_action::decline;
        } else if (action_str == "cancel") {
            result.action = elicitation_action::cancel;
        } else {
            throw mcp_exception(error_code::invalid_params, "Invalid elicitation action: " + action_str);
        }

        // Extract content for "accept" action
        if (j.contains("content")) {
            result.content = j["content"];
        }

        return result;
    }
};

/**
 * @brief Completion request parameters
 *
 * Per MCP 2025-06-18 specification:
 * - ref: Reference to a prompt or resource template for completion
 * - argument: The argument being completed (name and current value)
 * - context: Optional additional context including previously-resolved variables
 */
struct complete_request {
    // Reference type: "ref/prompt" or "ref/resource"
    std::string ref_type;

    // For prompt references (ref/prompt)
    std::string ref_name;

    // For resource template references (ref/resource)
    std::string ref_uri;

    // Argument information
    std::string argument_name;
    std::string argument_value;

    // Optional context with previously-resolved arguments
    json context;

    // Convert to JSON
    json to_json() const {
        json ref = {{"type", ref_type}};

        // Add name for prompt references or uri for resource references
        if (ref_type == "ref/prompt" && !ref_name.empty()) {
            ref["name"] = ref_name;
        } else if (ref_type == "ref/resource" && !ref_uri.empty()) {
            ref["uri"] = ref_uri;
        }

        json j = {{"ref", ref}, {"argument", {{"name", argument_name}, {"value", argument_value}}}};

        // Add context if present
        if (!context.empty()) {
            j["context"] = context;
        }

        return j;
    }

    // Create from JSON
    static complete_request from_json(const json& j) {
        complete_request req;

        // Extract ref
        const auto& ref = j["ref"];
        req.ref_type = ref["type"].get<std::string>();

        if (ref.contains("name")) {
            req.ref_name = ref["name"].get<std::string>();
        }
        if (ref.contains("uri")) {
            req.ref_uri = ref["uri"].get<std::string>();
        }

        // Extract argument
        const auto& arg = j["argument"];
        req.argument_name = arg["name"].get<std::string>();
        req.argument_value = arg["value"].get<std::string>();

        // Extract context if present
        if (j.contains("context")) {
            req.context = j["context"];
        }

        return req;
    }
};

/**
 * @brief Completion result
 *
 * Per MCP 2025-06-18 specification:
 * - values: Array of completion suggestions (max 100 items)
 * - total: Optional total number of available completions
 * - hasMore: Optional indicator of additional completions available
 * - _meta: Optional metadata about the completion
 */
struct complete_result {
    std::vector<std::string> values;
    int total = 0;
    bool has_more = false;

    // Optional _meta field for extensibility
    json meta;

    // Convert to JSON
    json to_json() const {
        json completion = {{"values", values}};

        // Add optional fields if non-default
        if (total > 0) {
            completion["total"] = total;
        }
        if (has_more) {
            completion["hasMore"] = has_more;
        }

        json j = {{"completion", completion}};

        // Add _meta if present
        if (!meta.empty()) {
            j["_meta"] = meta;
        }

        return j;
    }

    // Create from JSON
    static complete_result from_json(const json& j) {
        complete_result result;

        const auto& completion = j["completion"];

        // Extract values array
        result.values = completion["values"].get<std::vector<std::string>>();

        // Extract optional fields
        if (completion.contains("total")) {
            result.total = completion["total"].get<int>();
        }
        if (completion.contains("hasMore")) {
            result.has_more = completion["hasMore"].get<bool>();
        }

        // Extract _meta if present
        if (j.contains("_meta")) {
            result.meta = j["_meta"];
        }

        return result;
    }
};

} // namespace mcp

#endif // MCP_MESSAGE_H
