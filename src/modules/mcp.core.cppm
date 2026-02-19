/**
 * @file mcp.core.cppm
 * @brief MCP Core Module - Core protocol types and definitions
 *
 * This module contains the fundamental types for the Model Context Protocol (MCP).
 * Implements the 2025-11-25 protocol specification.
 */

module;

// Global module fragment - include standard library and third-party headers
#include <atomic>
#include <functional>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

// Include nlohmann/json in global module fragment (third-party code)
#include <nlohmann/json.hpp>

export module mcp.core;

// Export core MCP types and definitions
export namespace mcp {

// Use the nlohmann json library
using json = nlohmann::ordered_json;

// MCP version constant
inline constexpr const char* MCP_VERSION = "2025-11-25";

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

// Elicitation request parameters
struct elicitation_params {
    std::string message;
    json requested_schema;

    json to_json() const { return {{"message", message}, {"requestedSchema", requested_schema}}; }

    static elicitation_params from_json(const json& j) {
        elicitation_params params;
        params.message = j["message"].get<std::string>();
        params.requested_schema = j["requestedSchema"];
        return params;
    }
};

// Elicitation response actions
enum class elicitation_action {
    accept,  // User accepted and provided data
    decline, // User explicitly declined
    cancel   // User dismissed without choice
};

// Elicitation response result
struct elicitation_result {
    elicitation_action action;
    json content;

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

        if (action == elicitation_action::accept && !content.empty()) {
            j["content"] = content;
        }

        return j;
    }

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

        if (j.contains("content")) {
            result.content = j["content"];
        }

        return result;
    }
};

// Completion request parameters
struct complete_request {
    std::string ref_type;
    std::string ref_name;
    std::string ref_uri;
    std::string argument_name;
    std::string argument_value;
    json context;

    json to_json() const {
        json ref = {{"type", ref_type}};

        if (ref_type == "ref/prompt" && !ref_name.empty()) {
            ref["name"] = ref_name;
        } else if (ref_type == "ref/resource" && !ref_uri.empty()) {
            ref["uri"] = ref_uri;
        }

        json j = {{"ref", ref}, {"argument", {{"name", argument_name}, {"value", argument_value}}}};

        if (!context.empty()) {
            j["context"] = context;
        }

        return j;
    }

    static complete_request from_json(const json& j) {
        complete_request req;

        const auto& ref = j["ref"];
        req.ref_type = ref["type"].get<std::string>();

        if (ref.contains("name")) {
            req.ref_name = ref["name"].get<std::string>();
        }
        if (ref.contains("uri")) {
            req.ref_uri = ref["uri"].get<std::string>();
        }

        const auto& arg = j["argument"];
        req.argument_name = arg["name"].get<std::string>();
        req.argument_value = arg["value"].get<std::string>();

        if (j.contains("context")) {
            req.context = j["context"];
        }

        return req;
    }
};

// Completion result
struct complete_result {
    std::vector<std::string> values;
    int total = 0;
    bool has_more = false;
    json meta;

    json to_json() const {
        json completion = {{"values", values}};

        if (total > 0) {
            completion["total"] = total;
        }
        if (has_more) {
            completion["hasMore"] = has_more;
        }

        json j = {{"completion", completion}};

        if (!meta.empty()) {
            j["_meta"] = meta;
        }

        return j;
    }

    static complete_result from_json(const json& j) {
        complete_result result;

        const auto& completion = j["completion"];
        result.values = completion["values"].get<std::vector<std::string>>();

        if (completion.contains("total")) {
            result.total = completion["total"].get<int>();
        }
        if (completion.contains("hasMore")) {
            result.has_more = completion["hasMore"].get<bool>();
        }

        if (j.contains("_meta")) {
            result.meta = j["_meta"];
        }

        return result;
    }
};

} // namespace mcp
