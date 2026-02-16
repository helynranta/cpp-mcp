/**
 * @file mcp_streamable_http_client.cpp
 * @brief Implementation of the MCP Streamable HTTP client
 *
 * This file implements the client-side functionality for the Model Context Protocol
 * using Streamable HTTP transport (MCP 2025-03-26 specification).
 */

#include "mcp_streamable_http_client.h"

#include "base64.hpp"

namespace mcp {

streamable_http_client::streamable_http_client(const std::string& scheme_host_port, const std::string& mcp_endpoint,
                                               bool validate_certificates, const std::string& ca_cert_path)
    : scheme_host_port_(scheme_host_port), mcp_endpoint_(mcp_endpoint) {
    init_clients(scheme_host_port, validate_certificates, ca_cert_path);
}

streamable_http_client::~streamable_http_client() {
    close_sse_connection();
}

void streamable_http_client::init_clients(const std::string& scheme_host_port, bool validate_certificates,
                                          const std::string& ca_cert_path) {
    // Create two separate clients using HTTP factory (Beast by default)
    // One for regular JSON-RPC POST requests, one for SSE GET streaming
    http_client_ = http::create_client(scheme_host_port);
    sse_client_ = http::create_client(scheme_host_port);

    // Configure timeouts for HTTP client
    http_client_->set_connection_timeout(timeout_seconds_);
    http_client_->set_read_timeout(timeout_seconds_);
    http_client_->set_write_timeout(timeout_seconds_);

    // Configure timeouts for SSE client (longer connection timeout for streaming)
    sse_client_->set_connection_timeout(timeout_seconds_ * 2);
    sse_client_->set_write_timeout(timeout_seconds_);

#ifdef MCP_SSL
    // Configure SSL/TLS if enabled
    http_client_->set_certificate_verification(validate_certificates);
    sse_client_->set_certificate_verification(validate_certificates);
    if (!ca_cert_path.empty()) {
        http_client_->set_ca_cert_path(ca_cert_path);
        sse_client_->set_ca_cert_path(ca_cert_path);
    }
#else
    // Suppress unused parameter warnings
    (void)validate_certificates;
    (void)ca_cert_path;
#endif
}

bool streamable_http_client::initialize(const std::string& client_name, const std::string& client_version) {
    LOG_INFO("Initializing MCP client with Streamable HTTP transport...");

    request req = request::create("initialize", {{"protocolVersion", MCP_VERSION},
                                                 {"capabilities", capabilities_},
                                                 {"clientInfo", {{"name", client_name}, {"version", client_version}}}});

    try {
        LOG_INFO("Opening SSE connection to ", mcp_endpoint_, "...");
        open_sse_connection();

        const auto timeout = std::chrono::milliseconds(5000);

        {
            std::unique_lock<std::mutex> lock(mutex_);

            bool success = session_cv_.wait_for(lock, timeout, [this]() {
                if (!sse_running_) {
                    LOG_WARNING("SSE connection closed, stopping wait");
                    return true;
                }
                if (!session_id_.empty()) {
                    LOG_INFO("Session ID received, stopping wait");
                    return true;
                }
                return false;
            });

            if (!success) {
                LOG_WARNING("Condition variable wait timed out");
            }

            if (!sse_running_) {
                throw std::runtime_error("SSE connection closed, failed to get session ID");
            }

            if (session_id_.empty()) {
                throw std::runtime_error("Timeout waiting for SSE connection, failed to get session ID");
            }

            LOG_INFO("Successfully got session ID: ", session_id_);
        }

        json result = send_jsonrpc(req);

        server_capabilities_ = result["capabilities"];

        request notification = request::create_notification("initialized");
        send_jsonrpc(notification);

        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("Initialization failed: ", e.what());
        close_sse_connection();
        return false;
    }
}

bool streamable_http_client::ping() {
    request req = request::create("ping", {});

    try {
        json result = send_jsonrpc(req);
        return result.empty();
    } catch (...) {
        return false;
    }
}

void streamable_http_client::set_auth_token(const std::string& token) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auth_token_ = token;
    }
    set_header("Authorization", "Bearer " + auth_token_);
}

void streamable_http_client::set_header(const std::string& key, const std::string& value) {
    std::lock_guard<std::mutex> lock(mutex_);
    default_headers_[key] = value;

    // Update default headers on both clients
    if (http_client_) {
        http::headers_map headers;
        for (const auto& [k, v] : default_headers_) {
            headers.emplace(k, v);
        }
        http_client_->set_default_headers(headers);
    }
    if (sse_client_) {
        http::headers_map headers;
        for (const auto& [k, v] : default_headers_) {
            headers.emplace(k, v);
        }
        sse_client_->set_default_headers(headers);
    }
}

void streamable_http_client::set_timeout(int timeout_seconds) {
    std::lock_guard<std::mutex> lock(mutex_);
    timeout_seconds_ = timeout_seconds;

    if (http_client_) {
        http_client_->set_connection_timeout(timeout_seconds_);
        http_client_->set_write_timeout(timeout_seconds_);
        http_client_->set_read_timeout(timeout_seconds_);
    }

    if (sse_client_) {
        sse_client_->set_connection_timeout(timeout_seconds_ * 2);
        sse_client_->set_write_timeout(timeout_seconds_);
        sse_client_->set_read_timeout(timeout_seconds_);
    }
}

void streamable_http_client::set_capabilities(const json& capabilities) {
    std::lock_guard<std::mutex> lock(mutex_);
    capabilities_ = capabilities;
}

response streamable_http_client::send_request(const std::string& method, const json& params) {
    request req = request::create(method, params);
    json result = send_jsonrpc(req);

    response res;
    res.jsonrpc = "2.0";
    res.id = req.id;
    res.result = result;

    return res;
}

void streamable_http_client::send_notification(const std::string& method, const json& params) {
    request req = request::create_notification(method, params);
    send_jsonrpc(req);
}

json streamable_http_client::get_server_capabilities() {
    return server_capabilities_;
}

json streamable_http_client::call_tool(const std::string& tool_name, const json& arguments) {
    return send_request("tools/call", {{"name", tool_name}, {"arguments", arguments}}).result;
}

std::vector<tool> streamable_http_client::get_tools() {
    json response_json = send_request("tools/list", {}).result;
    std::vector<tool> tools;

    json tools_json;
    if (response_json.contains("tools") && response_json["tools"].is_array()) {
        tools_json = response_json["tools"];
    } else if (response_json.is_array()) {
        tools_json = response_json;
    } else {
        return tools;
    }

    for (const auto& tool_json : tools_json) {
        tool t;
        t.name = tool_json["name"];
        t.description = tool_json["description"];

        if (tool_json.contains("inputSchema")) {
            t.parameters_schema = tool_json["inputSchema"];
        }

        tools.push_back(t);
    }

    return tools;
}

json streamable_http_client::get_capabilities() {
    return capabilities_;
}

json streamable_http_client::list_resources(const std::string& cursor) {
    json params = json::object();
    if (!cursor.empty()) {
        params["cursor"] = cursor;
    }
    return send_request("resources/list", params).result;
}

json streamable_http_client::read_resource(const std::string& resource_uri) {
    return send_request("resources/read", {{"uri", resource_uri}}).result;
}

json streamable_http_client::subscribe_to_resource(const std::string& resource_uri) {
    return send_request("resources/subscribe", {{"uri", resource_uri}}).result;
}

json streamable_http_client::list_resource_templates() {
    return send_request("resources/templates/list").result;
}

void streamable_http_client::set_progress_handler(progress_handler handler) {
    std::lock_guard<std::mutex> lock(mutex_);
    progress_handler_ = handler;
}

bool streamable_http_client::is_running() const {
    return sse_running_.load();
}

void streamable_http_client::open_sse_connection() {
    sse_running_ = true;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        session_id_.clear();
        session_cv_.notify_all();
    }

    std::string connection_info = "Base URL: " + scheme_host_port_ + ", MCP Endpoint: " + mcp_endpoint_;
    LOG_INFO("Attempting to establish SSE connection: ", connection_info);

    sse_thread_ = std::make_unique<std::jthread>([this](std::stop_token stoken) {
        int retry_count = 0;
        const int max_retries = 5;
        const int retry_delay_base = 1000;

        while (sse_running_ && !stoken.stop_requested()) {
            try {
                LOG_INFO("SSE thread: Attempting to connect to ", mcp_endpoint_);

                std::string buffer;
                // Stream SSE data using client interface
                auto res = sse_client_->get_stream(
                    mcp_endpoint_, [&, this](const char* data, size_t data_length) -> bool {
                        buffer.append(data, data_length);

                        // Normalize CRLF to LF
                        size_t crlf_pos = buffer.find("\r\n");
                        while (crlf_pos != std::string::npos) {
                            buffer.replace(crlf_pos, 2, "\n");
                            crlf_pos = buffer.find("\r\n", crlf_pos + 1);
                        }

                        // Process complete events in buffer
                        size_t start_pos = 0;
                        while ((start_pos = buffer.find("\n\n", start_pos)) != std::string::npos) {
                            size_t end_pos = start_pos + 2;
                            std::string event = buffer.substr(0, start_pos);
                            buffer.erase(0, end_pos);
                            start_pos = 0;

                            if (!parse_sse_data(event.data(), event.size())) {
                                LOG_ERROR("SSE thread: Failed to parse event");
                            }
                        }

                        return sse_running_.load();
                    });

                // Extract Mcp-Session-Id from response headers
                if (res && res.status_code / 100 == 2) {
                    auto it = res.headers.find("Mcp-Session-Id");
                    if (it != res.headers.end()) {
                        std::lock_guard<std::mutex> lock(mutex_);
                        session_id_ = it->second;
                        session_cv_.notify_all();
                        LOG_INFO("SSE thread: Got session ID from header: ", session_id_);
                    }
                }

                // Check client result
                if (!res || res.status_code / 100 != 2) {
                    std::string error_msg = "SSE connection failed: ";
                    if (!res.success) {
                        error_msg += res.error_message;
                    } else {
                        error_msg += "HTTP " + std::to_string(res.status_code);
                    }
                    throw std::runtime_error(error_msg);
                }

                retry_count = 0;
                LOG_INFO("SSE thread: Connection successful");
            } catch (const std::exception& e) {
                if (!sse_running_) {
                    LOG_INFO("SSE connection actively closed, no retry needed");
                    break;
                }

                if (++retry_count > max_retries) {
                    LOG_ERROR("Maximum retry count reached, stopping SSE connection attempts");
                    break;
                }

                // Log first attempt as INFO since connection drops during shutdown are normal
                if (retry_count == 1) {
                    LOG_INFO("SSE connection closed: ", e.what());
                } else {
                    LOG_WARNING("SSE connection error (attempt ", retry_count, "): ", e.what());
                }

                int delay = retry_delay_base * (1 << (retry_count - 1));
                LOG_INFO("Will retry in ", delay, " ms (attempt ", retry_count, "/", max_retries, ")");

                const int check_interval = 100;
                for (int waited = 0; waited < delay && sse_running_; waited += check_interval) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(check_interval));
                }

                if (!sse_running_) {
                    LOG_INFO("SSE connection actively closed during retry wait, stopping retry");
                    break;
                }
            }
        }

        LOG_INFO("SSE thread: Exiting");
    });
}

bool streamable_http_client::parse_sse_data(const char* data, size_t length) {
    try {
        // Split into lines and process event fields
        std::istringstream stream(std::string(data, length));
        std::string line;
        std::string event_type = "message";
        std::vector<std::string> data_lines;

        while (std::getline(stream, line)) {
            // Trim trailing CR if present
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }

            if (line.substr(0, 7) == "event: ") {
                event_type = line.substr(7);
            } else if (line.substr(0, 6) == "data: ") {
                data_lines.push_back(line.substr(6));
            } else if (line.empty()) {
                break; // End of event
            }
        }

        if (data_lines.empty()) {
            return true;
        }

        // Join data lines with newlines
        std::string data_content;
        for (size_t i = 0; i < data_lines.size(); ++i) {
            if (i > 0)
                data_content += '\n';
            data_content += data_lines[i];
        }

        if (event_type == "heartbeat") {
            return true;
        } else if (event_type == "endpoint") {
            // For Streamable HTTP, extract session ID from the endpoint URL
            // Format: /mcp?session_id=<session-id>
            LOG_INFO("Received endpoint event: ", data_content);

            size_t session_param_pos = data_content.find("session_id=");
            if (session_param_pos != std::string::npos) {
                std::string session_from_url = data_content.substr(session_param_pos + 11); // Skip "session_id="
                // Remove any trailing query params or fragments
                size_t end_pos = session_from_url.find_first_of("&#");
                if (end_pos != std::string::npos) {
                    session_from_url = session_from_url.substr(0, end_pos);
                }

                std::lock_guard<std::mutex> lock(mutex_);
                session_id_ = session_from_url;
                session_cv_.notify_all();
                LOG_INFO("Extracted session ID from endpoint: ", session_id_);
            }
            return true;
        } else if (event_type == "message") {
            try {
                json response = json::parse(data_content);

                // Check if this is a notification (no id field or id is null)
                if (response.contains("jsonrpc") && response.contains("method") &&
                    (!response.contains("id") || response["id"].is_null())) {
                    // Handle notifications
                    std::string method = response["method"].get<std::string>();

                    if (method == "notifications/progress") {
                        // Handle progress notification
                        if (response.contains("params")) {
                            progress_handler handler_copy;
                            {
                                std::lock_guard<std::mutex> lock(mutex_);
                                handler_copy = progress_handler_;
                            }

                            if (handler_copy) {
                                try {
                                    progress_notification notif = progress_notification::from_params(
                                        response["params"]);
                                    handler_copy(notif);
                                } catch (const std::exception& e) {
                                    LOG_ERROR("Error handling progress notification: ", e.what());
                                }
                            }
                        }
                    } else {
                        LOG_INFO("Received notification: ", method);
                    }
                } else if (response.contains("jsonrpc") && response.contains("id") && !response["id"].is_null()) {
                    json id = response["id"];

                    std::lock_guard<std::mutex> lock(response_mutex_);
                    auto it = pending_requests_.find(id);
                    if (it != pending_requests_.end()) {
                        if (response.contains("result")) {
                            it->second.set_value(response["result"]);
                        } else if (response.contains("error")) {
                            json error_result = {{"isError", true}, {"error", response["error"]}};
                            it->second.set_value(error_result);
                        } else {
                            it->second.set_value(json::object());
                        }

                        pending_requests_.erase(it);
                    } else {
                        LOG_WARNING("Received response for unknown request ID: ", id);
                    }
                } else {
                    LOG_WARNING("Received invalid JSON-RPC response: ", response.dump());
                }
            } catch (const json::exception& e) {
                LOG_ERROR("Failed to parse JSON-RPC response: ", e.what());
            }
            return true;
        } else {
            LOG_WARNING("Received unknown event type: ", event_type);
            return true;
        }
    } catch (const std::exception& e) {
        LOG_ERROR("Error parsing SSE data: ", e.what());
        return false;
    }
}

void streamable_http_client::close_sse_connection() {
    if (!sse_running_) {
        LOG_INFO("SSE connection already closed");
        return;
    }

    LOG_INFO("Actively closing SSE connection (normal exit flow)...");

    // Signal the thread to stop
    sse_running_ = false;

    // Stop the SSE client connection to unblock any pending reads
    if (sse_client_) {
        sse_client_->stop();
    }

    // Give the stream time to detect the shutdown and return from get_stream()
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // jthread automatically requests stop and joins when destroyed
    if (sse_thread_) {
        LOG_INFO("Waiting for SSE thread to end...");
        sse_thread_->request_stop();
        sse_thread_.reset();
        LOG_INFO("SSE thread successfully ended");
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);
        session_id_.clear();
        session_cv_.notify_all();
    }

    LOG_INFO("SSE connection successfully closed (normal exit flow)");
}

void streamable_http_client::close_session() {
    // Unique to Streamable HTTP: Send DELETE request to explicitly close session
    if (session_id_.empty()) {
        LOG_WARNING("Cannot close session: no session ID");
        return;
    }

    LOG_INFO("Sending DELETE request to close session: ", session_id_);

    // Note: DELETE method not yet implemented in Beast client
    // For now, just close the SSE connection
    // TODO: Implement DELETE when Beast adapter supports it
    close_sse_connection();
}

http::headers_map streamable_http_client::build_request_headers() {
    http::headers_map headers;

    // Add Content-Type
    headers.emplace("Content-Type", "application/json");

    // Add Accept header (Streamable HTTP requirement)
    headers.emplace("Accept", "application/json, text/event-stream");

    // Add Mcp-Session-Id header if we have a session
    if (!session_id_.empty()) {
        headers.emplace("Mcp-Session-Id", session_id_);
    }

    // Add default headers
    for (const auto& [key, value] : default_headers_) {
        headers.emplace(key, value);
    }

    return headers;
}

json streamable_http_client::send_jsonrpc(const request& req) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (session_id_.empty()) {
        throw mcp_exception(error_code::internal_error, "Session ID not set, SSE connection may not be established");
    }

    json req_json = req.to_json();
    std::string req_body = req_json.dump();

    // Build headers with Mcp-Session-Id
    http::headers_map headers = build_request_headers();

    if (req.is_notification()) {
        // Send POST request to MCP endpoint
        auto result = http_client_->post(mcp_endpoint_, headers, req_body, "application/json");

        if (!result.success) {
            LOG_ERROR("JSON-RPC request failed: ", result.error_message);
            throw mcp_exception(error_code::internal_error, result.error_message);
        }

        // Streamable HTTP should return 202 Accepted for notifications
        if (result.status_code != 202) {
            LOG_WARNING("Unexpected status code for notification: ", result.status_code);
        }

        return json::object();
    }

    std::promise<json> response_promise;
    std::future<json> response_future = response_promise.get_future();

    {
        std::lock_guard<std::mutex> response_lock(response_mutex_);
        pending_requests_[req.id] = std::move(response_promise);
    }

    // Send POST request
    auto result = http_client_->post(mcp_endpoint_, headers, req_body, "application/json");

    if (!result.success) {
        std::lock_guard<std::mutex> response_lock(response_mutex_);
        pending_requests_.erase(req.id);
        LOG_ERROR("JSON-RPC request failed: ", result.error_message);
        throw mcp_exception(error_code::internal_error, result.error_message);
    }

    // Streamable HTTP should return 202 Accepted for async processing
    if (result.status_code != 202) {
        LOG_WARNING("Unexpected status code: ", result.status_code);
    }

    // Wait for response via SSE
    auto status = response_future.wait_for(std::chrono::seconds(timeout_seconds_));

    if (status == std::future_status::timeout) {
        std::lock_guard<std::mutex> response_lock(response_mutex_);
        pending_requests_.erase(req.id);
        throw mcp_exception(error_code::internal_error, "Request timeout");
    }

    json response = response_future.get();

    if (response.contains("isError") && response["isError"].get<bool>()) {
        throw mcp_exception(error_code::internal_error, response["error"].dump());
    }

    return response;
}

} // namespace mcp
