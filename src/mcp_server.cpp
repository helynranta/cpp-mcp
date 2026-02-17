/**
 * @file mcp_server.cpp
 * @brief Implementation of the MCP server
 *
 * This file implements the server-side functionality for the Model Context Protocol.
 * Follows the 2025-03-26 basic protocol specification.
 */

#include "mcp_server.h"

#include <random>

namespace mcp {

namespace {
bool interruptible_sleep(const std::stop_token& stoken, std::chrono::milliseconds duration) {
    constexpr auto kStopCheckInterval = std::chrono::milliseconds(
        100); // Balance shutdown responsiveness and CPU overhead
    while (!stoken.stop_requested() && duration > std::chrono::milliseconds::zero()) {
        auto step = duration > kStopCheckInterval ? kStopCheckInterval : duration;
        std::this_thread::sleep_for(step);
        duration -= step;
    }
    return !stoken.stop_requested();
}

int heartbeat_jitter_ms() {
    thread_local std::mt19937 rng{std::random_device{}()};
    thread_local std::uniform_int_distribution<int> dist(0, 499);
    return dist(rng);
}
} // namespace

server::server(const server::configuration& conf)
    : host_(conf.host), port_(conf.port), name_(conf.name), version_(conf.version), sse_endpoint_(conf.sse_endpoint),
      msg_endpoint_(conf.msg_endpoint), mcp_endpoint_(conf.mcp_endpoint),
      request_timeout_seconds_(conf.request_timeout_seconds), thread_pool_(conf.threadpool_size),
      validate_origin_(conf.security.validate_origin), allowed_origins_(conf.security.allowed_origins),
      enable_tool_confirmation_(conf.security.enable_tool_confirmation) {
#ifdef MCP_SSL
    if (conf.ssl.server_cert_path && conf.ssl.server_private_key_path) {
        if (!std::filesystem::exists(*conf.ssl.server_cert_path)) {
            LOG_ERROR("SSL certificate file '", *conf.ssl.server_cert_path, "' not found");
        }

        if (!std::filesystem::exists(*conf.ssl.server_private_key_path)) {
            LOG_ERROR("SSL key file '", *conf.ssl.server_private_key_path, "' not found");
        }

        http_server_ = http::create_server(true, *conf.ssl.server_cert_path, *conf.ssl.server_private_key_path);
    } else {
        http_server_ = http::create_server();
    }
#else
    http_server_ = http::create_server();
#endif
}

server::~server() {
    // Signal to all captured lambdas that we're being destroyed
    // This must be done BEFORE stop() to prevent any callback from accessing
    // server members after destruction begins
    if (alive_) {
        alive_->store(false, std::memory_order_release);
    }
    stop();
}

bool server::start(bool blocking) {
    if (running_) {
        return true; // Already running
    }

    LOG_INFO("Starting MCP server on ", host_, ":", port_);

    // Setup CORS handling with Origin validation (MCP 2025-03-26 security)
    http_server_->register_options(".*", [this](const http::request_data& req, http::response_builder& res) {
        // Handle Origin validation for OPTIONS requests
        // Note: OPTIONS requests are CORS preflight requests and should be allowed
        // even without an Origin header, as they don't carry sensitive data
        auto origin_opt = req.get_header("Origin");
        if (origin_opt.has_value()) {
            // If Origin header is present, validate it
            if (is_origin_allowed(*origin_opt)) {
                res.set_header("Access-Control-Allow-Origin", *origin_opt);
                res.set_header("Access-Control-Allow-Credentials", "true");
            } else if (validate_origin_) {
                // Origin validation enabled and origin not allowed
                res.set_status(403);
                return;
            } else {
                // Origin validation disabled
                res.set_header("Access-Control-Allow-Origin", "*");
            }
        } else {
            // No Origin header - allow with wildcard for OPTIONS preflight
            res.set_header("Access-Control-Allow-Origin", "*");
        }
        res.set_header("Access-Control-Allow-Methods", "GET, POST, DELETE, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type, Mcp-Session-Id, Accept, Origin");
        res.set_status(204); // No Content
    });

    // Setup unified MCP endpoint (Streamable HTTP transport - MCP 2025-03-26)
    http_server_->register_get(
        mcp_endpoint_.c_str(), [this](const http::request_data& req, http::response_builder& res) {
            this->handle_mcp(req, res);
            LOG_INFO(req.remote_addr, ":", req.remote_port, " - \"GET ", req.path, " HTTP/1.1\"");
        });

    http_server_->register_post(
        mcp_endpoint_.c_str(), [this](const http::request_data& req, http::response_builder& res) {
            this->handle_mcp(req, res);
            LOG_INFO(req.remote_addr, ":", req.remote_port, " - \"POST ", req.path, " HTTP/1.1\"");
        });

    http_server_->register_delete(
        mcp_endpoint_.c_str(), [this](const http::request_data& req, http::response_builder& res) {
            this->handle_mcp(req, res);
            LOG_INFO(req.remote_addr, ":", req.remote_port, " - \"DELETE ", req.path, " HTTP/1.1\"");
        });

    // Setup legacy JSON-RPC endpoint (deprecated)
    http_server_->register_post(
        msg_endpoint_.c_str(), [this](const http::request_data& req, http::response_builder& res) {
            this->handle_jsonrpc(req, res);
            LOG_INFO(req.remote_addr, ":", req.remote_port, " - \"POST ", req.path, " HTTP/1.1\"");
        });

    // Setup legacy SSE endpoint (deprecated)
    http_server_->register_get(
        sse_endpoint_.c_str(), [this](const http::request_data& req, http::response_builder& res) {
            this->handle_sse(req, res);
            LOG_INFO(req.remote_addr, ":", req.remote_port, " - \"GET ", req.path, " HTTP/1.1\"");
        });

    // Start resource check thread (only start in non-blocking mode)
    if (!blocking) {
        maintenance_thread_run_ = true;
        // Use jthread with stop_token for cooperative cancellation
        maintenance_thread_ = std::make_unique<std::jthread>([this](std::stop_token stoken) {
            while (!stoken.stop_requested()) {
                // Check inactive sessions every 60 seconds
                std::unique_lock<std::mutex> lock(maintenance_mutex_);
                auto should_exit = maintenance_cond_.wait_for(lock, std::chrono::seconds(60), [this, &stoken] {
                    return !maintenance_thread_run_ || stoken.stop_requested();
                });
                if (should_exit || stoken.stop_requested()) {
                    LOG_INFO("Maintenance thread exiting");
                    return;
                }
                lock.unlock();

                try {
                    check_inactive_sessions();
                } catch (const std::exception& e) {
                    LOG_ERROR("Exception in maintenance thread: ", e.what());
                } catch (...) {
                    LOG_ERROR("Unknown exception in maintenance thread");
                }
            }
        });
    }

    // Start server
    if (blocking) {
        LOG_INFO("Starting server in blocking mode");
        if (!http_server_->listen(host_.c_str(), port_)) {
            LOG_ERROR("Failed to start server on ", host_, ":", port_);
            return false;
        }

        // Set running to true and block until server is stopped
        // Wait on the maintenance condition variable which will be signaled when stop() is called
        std::unique_lock<std::mutex> lock(maintenance_mutex_);
        running_ = true;
        maintenance_cond_.wait(lock, [this] { return !running_; });

        return true;
    } else {
        // Start server in a separate thread - jthread for automatic joining
        server_thread_ = std::make_unique<std::jthread>([this](std::stop_token /* stoken */) {
            LOG_INFO("Starting server in separate thread");
            if (!http_server_->listen(host_.c_str(), port_)) {
                LOG_ERROR("Failed to start server on ", host_, ":", port_);
                running_ = false;
                return;
            }
        });
        running_ = true;
        return true;
    }
}

void server::stop() {
    if (!running_) {
        return;
    }

    LOG_INFO("Stopping MCP server on ", host_, ":", port_);

    // Set running_ to false while holding the mutex for proper synchronization
    // with the blocking wait in start()
    {
        std::lock_guard<std::mutex> lock(maintenance_mutex_);
        running_ = false;
    }
    // Notify any blocking wait in start() method immediately after setting running_ = false
    maintenance_cond_.notify_all();

    // Stop maintenance thread - jthread will request stop and join automatically on reset
    if (maintenance_thread_) {
        {
            std::unique_lock<std::mutex> lock(maintenance_mutex_);
            maintenance_thread_run_ = false;
        }
        maintenance_cond_.notify_one();
        maintenance_thread_->request_stop();
        maintenance_thread_.reset(); // jthread joins automatically on destruction
    }

    // Copy all dispatchers and threads to avoid holding the lock for too long
    std::vector<std::shared_ptr<event_dispatcher>> dispatchers_to_close;
    std::vector<std::unique_ptr<std::jthread>> threads_to_stop;

    {
        std::lock_guard<std::mutex> lock(mutex_);

        // Copy all dispatchers
        dispatchers_to_close.reserve(session_dispatchers_.size());
        for (const auto& [_, dispatcher] : session_dispatchers_) {
            dispatchers_to_close.push_back(dispatcher);
        }

        // Move all threads (jthread will request_stop and join on destruction)
        threads_to_stop.reserve(sse_threads_.size());
        for (auto& [_, thread] : sse_threads_) {
            if (thread) {
                threads_to_stop.push_back(std::move(thread));
            }
        }

        // Clear the maps
        session_dispatchers_.clear();
        sse_threads_.clear();
        session_lifecycle_.clear();
        session_client_capabilities_.clear();
    }

    LOG_INFO("Server stop cleanup: dispatchers=", dispatchers_to_close.size(),
             ", sse_threads=", threads_to_stop.size());

    // Close all dispatchers to gracefully disconnect SSE clients
    for (const auto& dispatcher : dispatchers_to_close) {
        if (dispatcher && !dispatcher->is_closed()) {
            dispatcher->close();
        }
    }

    // Request stop on all SSE threads - they will exit their loops
    for (auto& thread : threads_to_stop) {
        if (thread) {
            thread->request_stop();
        }
    }

    // jthread automatically joins on destruction when threads_to_stop goes out of scope
    // The threads will exit cleanly because:
    // 1. Dispatchers are closed (wait_event returns false)
    // 2. stop_requested() returns true
    // 3. alive_ is set to false (checked in lambdas)
    LOG_INFO("Server stop cleanup: joining ", threads_to_stop.size(), " SSE threads");
    threads_to_stop.clear(); // Joins all threads automatically

    // Stop server thread - jthread joins automatically
    if (server_thread_) {
        http_server_->stop();
        server_thread_->request_stop();
        server_thread_.reset(); // jthread joins automatically on destruction
    } else {
        http_server_->stop();
    }

    LOG_INFO("MCP server stopped");
}

bool server::is_running() const {
    return running_;
}

void server::set_server_info(const std::string& name, const std::string& version) {
    std::lock_guard<std::mutex> lock(mutex_);
    name_ = name;
    version_ = version;
}

void server::set_capabilities(const json& capabilities) {
    std::lock_guard<std::mutex> lock(mutex_);
    capabilities_ = capabilities;
}

void server::register_method(const std::string& method, method_handler handler) {
    std::lock_guard<std::mutex> lock(mutex_);
    method_handlers_[method] = handler;
}

void server::register_notification(const std::string& method, notification_handler handler) {
    std::lock_guard<std::mutex> lock(mutex_);
    notification_handlers_[method] = handler;
}

void server::register_resource(const std::string& path, std::shared_ptr<resource> resource) {
    std::lock_guard<std::mutex> lock(mutex_);
    resources_[path] = resource;

    // Register methods for resource access
    if (method_handlers_.find("resources/read") == method_handlers_.end()) {
        method_handlers_["resources/read"] = [this](const json& params, const std::string& session_id) -> json {
            if (!params.contains("uri")) {
                throw mcp_exception(error_code::invalid_params, "Missing 'uri' parameter");
            }

            std::string uri = params["uri"];
            auto it = resources_.find(uri);
            if (it == resources_.end()) {
                throw mcp_exception(error_code::invalid_params, "Resource not found: " + uri);
            }

            json contents = json::array();
            contents.push_back(it->second->read());

            return json{{"contents", contents}};
        };
    }

    if (method_handlers_.find("resources/list") == method_handlers_.end()) {
        method_handlers_["resources/list"] = [this](const json& params, const std::string& session_id) -> json {
            json resources = json::array();

            for (const auto& [uri, res] : resources_) {
                resources.push_back(res->get_metadata());
            }

            json result = {{"resources", resources}};

            if (params.contains("cursor")) {
                result["nextCursor"] = "";
            }

            return result;
        };
    }

    if (method_handlers_.find("resources/subscribe") == method_handlers_.end()) {
        method_handlers_["resources/subscribe"] = [this](const json& params, const std::string& session_id) -> json {
            if (!params.contains("uri")) {
                throw mcp_exception(error_code::invalid_params, "Missing 'uri' parameter");
            }

            std::string uri = params["uri"];
            auto it = resources_.find(uri);
            if (it == resources_.end()) {
                throw mcp_exception(error_code::invalid_params, "Resource not found: " + uri);
            }

            return json::object();
        };
    }

    if (method_handlers_.find("resources/templates/list") == method_handlers_.end()) {
        method_handlers_["resources/templates/list"] =
            [this](const json& params, const std::string& session_id) -> json { return json::array(); };
    }
}

void server::register_tool(const tool& tool, tool_handler handler) {
    std::lock_guard<std::mutex> lock(mutex_);
    tools_[tool.name] = std::make_pair(tool, handler);

    // Register methods for tool listing and calling
    if (method_handlers_.find("tools/list") == method_handlers_.end()) {
        method_handlers_["tools/list"] = [this](const json& params, const std::string& session_id) -> json {
            json tools_json = json::array();
            for (const auto& [name, tool_pair] : tools_) {
                tools_json.push_back(tool_pair.first.to_json());
            }
            return json{{"tools", tools_json}};
        };
    }

    if (method_handlers_.find("tools/call") == method_handlers_.end()) {
        method_handlers_["tools/call"] = [this](const json& params, const std::string& session_id) -> json {
            if (!params.contains("name")) {
                throw mcp_exception(error_code::invalid_params, "Missing 'name' parameter");
            }

            std::string tool_name = params["name"];
            auto it = tools_.find(tool_name);
            if (it == tools_.end()) {
                throw mcp_exception(error_code::invalid_params, "Tool not found: " + tool_name);
            }

            const mcp::tool& tool_def = it->second.first;

            json tool_args = params.contains("arguments") ? params["arguments"] : json::array();

            if (tool_args.is_string()) {
                try {
                    tool_args = json::parse(tool_args.get<std::string>());
                } catch (const json::exception& e) {
                    throw mcp_exception(error_code::invalid_params, "Invalid JSON arguments: " + std::string(e.what()));
                }
            }

            json tool_result = {{"isError", false}};

            try {
                // Check if tool requires confirmation (MCP 2025-03-26 safety)
                if (enable_tool_confirmation_ && tool_def.requires_confirmation) {
                    if (tool_confirmation_handler_) {
                        bool confirmed = tool_confirmation_handler_(tool_name, tool_args, session_id);
                        if (!confirmed) {
                            throw mcp_exception(error_code::invalid_request,
                                                "Tool execution denied: user confirmation required but not granted");
                        }
                    } else {
                        // If no confirmation handler is set but tool requires confirmation, deny execution
                        LOG_WARNING("Tool '", tool_name, "' requires confirmation but no handler is set");
                        throw mcp_exception(error_code::invalid_request,
                                            "Tool execution denied: confirmation required but no handler configured");
                    }
                }

                // Execute tool handler
                json handler_result = it->second.second(tool_args, session_id);

                // MCP 2025-06-18: Support both formats for backward compatibility
                // 1. New format: handler returns object with "content" and optional "structuredContent"
                // 2. Legacy format: handler returns just content array
                if (handler_result.is_object() && handler_result.contains("content")) {
                    // New format: merge handler result into tool_result
                    tool_result["content"] = handler_result["content"];
                    if (handler_result.contains("structuredContent")) {
                        tool_result["structuredContent"] = handler_result["structuredContent"];
                    }
                    // Preserve isError if set by handler (though handlers typically throw exceptions for errors)
                    if (handler_result.contains("isError")) {
                        tool_result["isError"] = handler_result["isError"];
                    }
                } else {
                    // Legacy format: handler returns just content array
                    tool_result["content"] = handler_result;
                }
            } catch (const std::exception& e) {
                tool_result["isError"] = true;
                tool_result["content"] = json::array({{{"type", "text"}, {"text", e.what()}}});
            }

            return tool_result;
        };
    }
}

void server::register_session_cleanup(const std::string& key, session_cleanup_handler handler) {
    std::lock_guard<std::mutex> lock(mutex_);
    session_cleanup_handler_[key] = handler;
}

std::vector<tool> server::get_tools() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<tool> tools;

    for (const auto& [name, tool_pair] : tools_) {
        tools.push_back(tool_pair.first);
    }

    return tools;
}

void server::set_auth_handler(auth_handler handler) {
    std::lock_guard<std::mutex> lock(mutex_);
    auth_handler_ = handler;
}

void server::set_cancellation_handler(cancellation_handler handler) {
    std::lock_guard<std::mutex> lock(mutex_);
    cancellation_handler_ = handler;
}

void server::set_tool_confirmation_handler(tool_confirmation_handler handler) {
    std::lock_guard<std::mutex> lock(mutex_);
    tool_confirmation_handler_ = handler;
}

bool server::client_supports_elicitation(const std::string& session_id) const {
    std::lock_guard<std::mutex> lock(mutex_);

    // Check if session exists and has capabilities
    auto it = session_client_capabilities_.find(session_id);
    if (it == session_client_capabilities_.end()) {
        return false;
    }

    // Check if client declared elicitation capability
    const json& capabilities = it->second;
    return capabilities.contains("elicitation");
}

elicitation_result server::request_elicitation(const std::string& session_id, const std::string& message,
                                               const json& requested_schema) {
    // Verify client supports elicitation
    if (!client_supports_elicitation(session_id)) {
        throw mcp_exception(error_code::invalid_request,
                            "Client does not support elicitation. Client must declare 'elicitation' capability.");
    }

    // Create elicitation request
    elicitation_params params;
    params.message = message;
    params.requested_schema = requested_schema;

    request elicit_req = request::create("elicitation/create", params.to_json());
    json req_id = elicit_req.id;

    LOG_INFO("Sending elicitation request to session: ", session_id, ", request ID: ", req_id.dump());

    // Create promise and future for async response
    auto promise_ptr = std::make_shared<std::promise<elicitation_result>>();
    std::future<elicitation_result> result_future = promise_ptr->get_future();

    // Store the promise in pending requests map
    {
        std::lock_guard<std::mutex> lock(mutex_);
        pending_elicitation_requests_[req_id] = promise_ptr;
    }

    // Send the request
    send_request(session_id, elicit_req);

    // Wait for response with timeout
    auto timeout = std::chrono::seconds(request_timeout_seconds_);
    auto status = result_future.wait_for(timeout);

    // Remove from pending requests map
    {
        std::lock_guard<std::mutex> lock(mutex_);
        pending_elicitation_requests_.erase(req_id);
    }

    if (status == std::future_status::timeout) {
        LOG_ERROR("Elicitation request timed out for session: ", session_id, ", request ID: ", req_id.dump());
        throw mcp_exception(error_code::internal_error, "Elicitation request timed out. User did not respond within " +
                                                            std::to_string(request_timeout_seconds_) + " seconds.");
    }

    // Get the result
    try {
        return result_future.get();
    } catch (const std::exception& e) {
        LOG_ERROR("Error getting elicitation result: ", e.what());
        throw;
    }
}

void server::handle_sse(const http::request_data& req, http::response_builder& res) {
    std::string session_id = generate_session_id();
    std::string session_uri = msg_endpoint_ + "?session_id=" + session_id;

    // Setup SSE response headers
    res.set_header("Content-Type", "text/event-stream");
    res.set_header("Cache-Control", "no-cache");
    res.set_header("Connection", "keep-alive");
    res.set_header("Access-Control-Allow-Origin", "*");

    // Create session-specific event dispatcher
    auto session_dispatcher = std::make_shared<event_dispatcher>();

    // Initialize activity time
    session_dispatcher->update_activity();

    // Add session dispatcher to mapping table
    {
        std::lock_guard<std::mutex> lock(mutex_);
        session_dispatchers_[session_id] = session_dispatcher;
        // Initialize session to uninitialized lifecycle state
        session_lifecycle_[session_id] = lifecycle_state::uninitialized;
    }

    // Create session thread - use jthread with stop_token for cooperative cancellation
    // Capture alive_ for safe access in the thread
    auto alive = alive_;
    auto thread = std::make_unique<std::jthread>([this, alive, session_id, session_uri,
                                                  session_dispatcher](std::stop_token stoken) {
        try {
            // Send initial session URI
            if (!interruptible_sleep(stoken, std::chrono::milliseconds(500))) {
                return;
            }

            // Check if server is still alive or stop requested
            if (stoken.stop_requested() || !alive || !alive->load(std::memory_order_acquire)) {
                return;
            }

            std::stringstream ss;
            ss << "event: endpoint\r\ndata: " << session_uri << "\r\n\r\n";
            session_dispatcher->send_event(ss.str());

            // Update activity time (after sending message)
            session_dispatcher->update_activity();

            // Send periodic heartbeats to detect connection status
            int heartbeat_count = 0;
            while (!stoken.stop_requested() && alive && alive->load(std::memory_order_acquire) && running_ &&
                   !session_dispatcher->is_closed()) {
                auto heartbeat_sleep = std::chrono::seconds(5) + std::chrono::milliseconds(heartbeat_jitter_ms());
                if (!interruptible_sleep(stoken,
                                         std::chrono::duration_cast<std::chrono::milliseconds>(heartbeat_sleep))) {
                    break;
                } // NOTE: DO NOT set it the same as the timeout of wait_event

                if (stoken.stop_requested() || !alive || !alive->load(std::memory_order_acquire) ||
                    session_dispatcher->is_closed() || !running_) {
                    break;
                }

                std::stringstream heartbeat;
                heartbeat << "event: heartbeat\r\ndata: " << heartbeat_count++ << "\r\n\r\n";

                try {
                    bool sent = session_dispatcher->send_event(heartbeat.str());
                    if (!sent) {
                        if (alive && alive->load(std::memory_order_acquire)) {
                            LOG_WARNING("Failed to send heartbeat, client may have closed connection: ", session_id);
                        }
                        break;
                    }

                    // Update activity time (heartbeat successful)
                    session_dispatcher->update_activity();
                } catch (const std::exception& e) {
                    if (alive && alive->load(std::memory_order_acquire)) {
                        LOG_ERROR("Failed to send heartbeat: ", e.what());
                    }
                    break;
                }
            }
        } catch (const std::exception& e) {
            if (alive && alive->load(std::memory_order_acquire)) {
                LOG_ERROR("SSE session thread exception: ", session_id, ", ", e.what());
            }
        }

        // Only call close_session if server is still alive
        if (alive && alive->load(std::memory_order_acquire)) {
            close_session(session_id);
        }
    });

    // Store thread
    {
        std::lock_guard<std::mutex> lock(mutex_);
        sse_threads_[session_id] = std::move(thread);
    }

    // Setup chunked content provider - capture alive_ by value (shared_ptr) for safe access
    // Note: 'alive' was already captured above for the SSE thread
    res.set_chunked_content_provider("text/event-stream", [this, alive, session_id, session_dispatcher](
                                                              size_t /* offset */, http::streaming_data_sink& sink) {
        try {
            // Check if server is still alive before accessing any members
            if (!alive || !alive->load(std::memory_order_acquire)) {
                return false;
            }

            // Check if session is closed - directly get status from dispatcher, reduce lock contention
            if (session_dispatcher->is_closed()) {
                return false;
            }

            // Update activity time (received request)
            session_dispatcher->update_activity();

            // Wait for event
            bool result = session_dispatcher->wait_event(&sink);
            if (!result) {
                // Check alive again before accessing server methods
                if (alive && alive->load(std::memory_order_acquire)) {
                    LOG_WARNING("Failed to wait for event, closing connection: ", session_id);
                    close_session(session_id);
                }
                return false;
            }

            // Update activity time (successfully received message)
            session_dispatcher->update_activity();

            return true;
        } catch (const std::exception& e) {
            // Check alive before logging/calling server methods
            if (alive && alive->load(std::memory_order_acquire)) {
                LOG_ERROR("SSE content provider exception: ", e.what());
                close_session(session_id);
            }
            return false;
        }
    });
}

void server::handle_jsonrpc(const http::request_data& req, http::response_builder& res) {
    // Setup response headers
    res.set_header("Content-Type", "application/json");
    res.set_header("Access-Control-Allow-Origin", "*");
    res.set_header("Access-Control-Allow-Methods", "POST, OPTIONS");
    res.set_header("Access-Control-Allow-Headers", "Content-Type");

    // Handle OPTIONS request (CORS pre-flight)
    if (req.method == "OPTIONS") {
        res.set_status(204); // No Content
        return;
    }

    // Get session ID
    auto it = req.params.find("session_id");
    std::string session_id = it != req.params.end() ? it->second : "";

    // Update session activity time
    if (!session_id.empty()) {
        std::shared_ptr<event_dispatcher> dispatcher;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto disp_it = session_dispatchers_.find(session_id);
            if (disp_it != session_dispatchers_.end()) {
                dispatcher = disp_it->second;
            }
        }

        if (dispatcher) {
            dispatcher->update_activity();
        }
    }

    // Parse request
    json req_json;
    try {
        req_json = json::parse(req.body);
    } catch (const json::exception& e) {
        LOG_ERROR("Failed to parse JSON request: ", e.what());
        res.set_status(400);
        res.set_content("{\"error\":\"Invalid JSON\"}", "application/json");
        return;
    }

    // MCP 2025-06-18: JSON-RPC batching is NOT supported
    // Reject batch requests (arrays) with appropriate error
    if (req_json.is_array()) {
        LOG_ERROR("Batch requests not supported per MCP 2025-06-18 (received array with ", req_json.size(), " items)");
        res.set_status(400);
        json error_response = response::create_error(nullptr, // No ID for batch errors
                                                     error_code::invalid_request,
                                                     "JSON-RPC batching is not supported in MCP 2025-06-18+. Please "
                                                     "send individual requests instead of arrays.")
                                  .to_json();
        res.set_content(error_response.dump(), "application/json");
        return;
    }

    // Handle single request (existing logic)
    // Check if session exists
    std::shared_ptr<event_dispatcher> dispatcher;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto disp_it = session_dispatchers_.find(session_id);
        if (disp_it == session_dispatchers_.end()) {
            // Handle ping request
            if (req_json["method"] == "ping") {
                res.set_status(202);
                res.set_content("Accepted", "text/plain");
                return;
            }
            LOG_ERROR("Session not found: ", session_id);
            res.set_status(404);
            res.set_content("{\"error\":\"Session not found\"}", "application/json");
            return;
        }
        dispatcher = disp_it->second;
    }

    // Create request object
    request mcp_req;
    try {
        // Validate the JSON-RPC message first
        std::string validation_error;
        if (!validate_request_message(req_json, validation_error)) {
            LOG_ERROR("Invalid JSON-RPC request: ", validation_error);
            res.set_status(400);
            json error_response = response::create_error(req_json.contains("id") ? req_json["id"] : nullptr,
                                                         error_code::invalid_request, validation_error)
                                      .to_json();
            res.set_content(error_response.dump(), "application/json");
            return;
        }

        mcp_req.jsonrpc = req_json["jsonrpc"].get<std::string>();
        if (req_json.contains("id") && !req_json["id"].is_null()) {
            mcp_req.id = req_json["id"];

            // Check for duplicate request ID
            if (!request_id_tracker_.add_request_id(session_id, mcp_req.id)) {
                LOG_ERROR("Duplicate request ID: ", mcp_req.id.dump());
                res.set_status(400);
                json error_response =
                    response::create_error(mcp_req.id, error_code::invalid_request, "Duplicate request ID").to_json();
                res.set_content(error_response.dump(), "application/json");
                return;
            }
        }
        mcp_req.method = req_json["method"].get<std::string>();
        if (req_json.contains("params")) {
            mcp_req.params = req_json["params"];
        }
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to create request object: ", e.what());
        res.set_status(400);
        res.set_content("{\"error\":\"Invalid request format\"}", "application/json");
        return;
    }

    // If it is a notification (no ID), process it directly and return 202 status code
    if (mcp_req.is_notification()) {
        // Process it asynchronously in the thread pool
        thread_pool_.enqueue([this, mcp_req, session_id]() { process_request(mcp_req, session_id); });

        // Return 202 Accepted
        res.set_status(202);
        res.set_content("Accepted", "text/plain");
        return;
    }

    // For requests with ID, process it asynchronously in the thread pool and return the result via SSE
    thread_pool_.enqueue([this, mcp_req, session_id, dispatcher]() {
        // Process the request
        json response_json = process_request(mcp_req, session_id);

        // Remove request ID from tracker after processing
        if (!mcp_req.is_notification()) {
            request_id_tracker_.remove_request_id(session_id, mcp_req.id);
        }

        // Send response via SSE
        std::stringstream ss;
        ss << "event: message\r\ndata: " << response_json.dump() << "\r\n\r\n";
        bool result = dispatcher->send_event(ss.str());

        if (!result) {
            LOG_ERROR("Failed to send response via SSE: session_id=", session_id);
        }
    });

    // Return 202 Accepted
    res.set_status(202);
    res.set_content("Accepted", "text/plain");
}

void server::handle_mcp(const http::request_data& req, http::response_builder& res) {
    // Route to appropriate handler based on HTTP method
    if (req.method == "GET") {
        handle_mcp_get(req, res);
    } else if (req.method == "POST") {
        handle_mcp_post(req, res);
    } else if (req.method == "DELETE") {
        handle_mcp_delete(req, res);
    } else {
        // Method not allowed
        res.set_status(405);
        res.set_header("Allow", "GET, POST, DELETE");
        res.set_content("{\"error\":\"Method not allowed\"}", "application/json");
    }
}

void server::handle_mcp_get(const http::request_data& req, http::response_builder& res) {
    // GET request establishes SSE connection for receiving responses
    // This is the same as the legacy /sse endpoint but with Mcp-Session-Id header support

    // Extract session ID from header (if client wants to reconnect to existing session)
    std::string session_id = extract_session_id(req);

    // If no session ID provided, generate a new one
    if (session_id.empty()) {
        session_id = generate_session_id();
    }

    // Set session ID in response header
    set_session_id_header(res, session_id);

    // Check if session already exists (reconnection scenario)
    bool is_new_session = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto disp_it = session_dispatchers_.find(session_id);
        if (disp_it == session_dispatchers_.end()) {
            is_new_session = true;
        }
    }

    // For new sessions, use the MCP endpoint for messages
    std::string session_uri = mcp_endpoint_ + "?session_id=" + session_id;

    // Setup SSE response headers
    res.set_header("Content-Type", "text/event-stream");
    res.set_header("Cache-Control", "no-cache");
    res.set_header("Connection", "keep-alive");

    // Set CORS headers based on Origin validation (MCP 2025-03-26 security)
    auto origin = req.get_header("Origin");
    if (origin && is_origin_allowed(*origin)) {
        res.set_header("Access-Control-Allow-Origin", *origin);
        res.set_header("Access-Control-Allow-Credentials", "true");
    } else if (!validate_origin_) {
        // Only use wildcard if Origin validation is disabled
        res.set_header("Access-Control-Allow-Origin", "*");
    }
    res.set_header("Access-Control-Expose-Headers", "Mcp-Session-Id");

    // Create or retrieve session-specific event dispatcher
    std::shared_ptr<event_dispatcher> session_dispatcher;

    if (is_new_session) {
        session_dispatcher = std::make_shared<event_dispatcher>();
        session_dispatcher->update_activity();

        // Add session dispatcher to mapping table
        {
            std::lock_guard<std::mutex> lock(mutex_);
            session_dispatchers_[session_id] = session_dispatcher;
            // Initialize session to uninitialized lifecycle state
            session_lifecycle_[session_id] = lifecycle_state::uninitialized;
        }

        // Create session thread for heartbeats - use jthread for automatic joining
        auto alive = alive_;
        auto thread = std::make_unique<std::jthread>([this, alive, session_id, session_uri,
                                                      session_dispatcher](std::stop_token stoken) {
            try {
                // Send initial session endpoint
                if (!interruptible_sleep(stoken, std::chrono::milliseconds(500))) {
                    return;
                }

                // Check if server is still alive or stop requested
                if (stoken.stop_requested() || !alive || !alive->load(std::memory_order_acquire)) {
                    return;
                }

                std::stringstream ss;
                ss << "event: endpoint\r\ndata: " << session_uri << "\r\n\r\n";
                session_dispatcher->send_event(ss.str());
                session_dispatcher->update_activity();

                // Send periodic heartbeats
                int heartbeat_count = 0;
                while (!stoken.stop_requested() && alive && alive->load(std::memory_order_acquire) && running_ &&
                       !session_dispatcher->is_closed()) {
                    auto heartbeat_sleep = std::chrono::seconds(5) + std::chrono::milliseconds(heartbeat_jitter_ms());
                    if (!interruptible_sleep(stoken,
                                             std::chrono::duration_cast<std::chrono::milliseconds>(heartbeat_sleep))) {
                        break;
                    }

                    if (stoken.stop_requested() || !alive || !alive->load(std::memory_order_acquire) ||
                        session_dispatcher->is_closed() || !running_) {
                        break;
                    }

                    std::stringstream heartbeat;
                    heartbeat << "event: heartbeat\r\ndata: " << heartbeat_count++ << "\r\n\r\n";

                    try {
                        bool sent = session_dispatcher->send_event(heartbeat.str());
                        if (!sent) {
                            if (alive && alive->load(std::memory_order_acquire)) {
                                LOG_WARNING("Failed to send heartbeat, client may have closed connection: ",
                                            session_id);
                            }
                            break;
                        }
                        session_dispatcher->update_activity();
                    } catch (const std::exception& e) {
                        if (alive && alive->load(std::memory_order_acquire)) {
                            LOG_ERROR("Failed to send heartbeat: ", e.what());
                        }
                        break;
                    }
                }
            } catch (const std::exception& e) {
                if (alive && alive->load(std::memory_order_acquire)) {
                    LOG_ERROR("SSE session thread exception: ", session_id, ", ", e.what());
                }
            }

            // Only call close_session if server is still alive
            if (alive && alive->load(std::memory_order_acquire)) {
                close_session(session_id);
            }
        });

        // Store thread
        {
            std::lock_guard<std::mutex> lock(mutex_);
            sse_threads_[session_id] = std::move(thread);
        }
    } else {
        // Retrieve existing dispatcher
        std::lock_guard<std::mutex> lock(mutex_);
        auto disp_it = session_dispatchers_.find(session_id);
        if (disp_it != session_dispatchers_.end()) {
            session_dispatcher = disp_it->second;
        } else {
            // Session no longer exists
            res.set_status(404);
            res.set_content("{\"error\":\"Session not found\"}", "application/json");
            return;
        }
    }

    // Setup chunked content provider for SSE - capture alive_ for safe access
    auto alive = alive_;
    res.set_chunked_content_provider("text/event-stream", [this, alive, session_id, session_dispatcher](
                                                              size_t /* offset */, http::streaming_data_sink& sink) {
        try {
            // Check if server is still alive before accessing any members
            if (!alive || !alive->load(std::memory_order_acquire)) {
                return false;
            }

            if (session_dispatcher->is_closed()) {
                return false;
            }

            session_dispatcher->update_activity();

            bool result = session_dispatcher->wait_event(&sink);
            if (!result) {
                if (alive && alive->load(std::memory_order_acquire)) {
                    LOG_WARNING("Failed to wait for event, closing connection: ", session_id);
                    close_session(session_id);
                }
                return false;
            }

            session_dispatcher->update_activity();
            return true;
        } catch (const std::exception& e) {
            if (alive && alive->load(std::memory_order_acquire)) {
                LOG_ERROR("SSE content provider exception: ", e.what());
                close_session(session_id);
            }
            return false;
        }
    });
}

void server::handle_mcp_post(const http::request_data& req, http::response_builder& res) {
    // POST request sends JSON-RPC messages (requests or notifications)
    // This is similar to the legacy /message endpoint but with enhanced header support

    // Validate Origin header for DNS rebinding mitigation (MCP 2025-03-26 security)
    if (should_validate_origin(req)) {
        auto origin = req.get_header("Origin");
        if (origin) {
            if (!is_origin_allowed(*origin)) {
                LOG_WARNING("POST /mcp rejected due to invalid Origin: ", *origin);
                res.set_status(403); // Forbidden
                res.set_header("Content-Type", "application/json");
                res.set_content("{\"error\":\"Origin not allowed\"}", "application/json");
                return;
            }
        }
    }

    // Setup response headers
    // Set CORS headers based on Origin validation
    auto origin = req.get_header("Origin");
    if (origin && is_origin_allowed(*origin)) {
        res.set_header("Access-Control-Allow-Origin", *origin);
    } else if (!validate_origin_) {
        // Only use wildcard if Origin validation is disabled
        res.set_header("Access-Control-Allow-Origin", "*");
    }
    res.set_header("Access-Control-Allow-Methods", "POST, OPTIONS");
    res.set_header("Access-Control-Allow-Headers", "Content-Type, Mcp-Session-Id, Accept");
    res.set_header("Access-Control-Expose-Headers", "Mcp-Session-Id");

    // Handle OPTIONS request (CORS pre-flight)
    if (req.method == "OPTIONS") {
        res.set_status(204); // No Content
        return;
    }

    // Validate Accept header per MCP 2025-03-26 Streamable HTTP specification
    // The Accept header should include application/json and/or text/event-stream
    auto accept = req.get_header("Accept");
    if (accept) {
        // Check if Accept header includes supported types
        bool accepts_json = accept->find("application/json") != std::string::npos ||
                            accept->find("*/*") != std::string::npos;
        bool accepts_sse = accept->find("text/event-stream") != std::string::npos ||
                           accept->find("*/*") != std::string::npos;

        if (!accepts_json && !accepts_sse) {
            LOG_WARNING("POST /mcp received unsupported Accept header: ", *accept);
            res.set_status(406); // Not Acceptable
            res.set_header("Content-Type", "application/json");
            res.set_content(
                "{\"error\":\"Not Acceptable. Accept header must include application/json or text/event-stream\"}",
                "application/json");
            return;
        }
    }

    // Extract session ID from header or query parameter
    std::string session_id = extract_session_id(req);

    // Debug: Log session ID extraction
    if (session_id.empty()) {
        LOG_WARNING("No session ID found in request. Headers present: ");
        for (const auto& [key, value] : req.headers) {
            LOG_WARNING("  ", key, ": ", value.substr(0, std::min(size_t(50), value.size())));
        }
    }

    // Validate MCP-Protocol-Version header (MCP 2025-06-18+)
    // This must happen AFTER session ID extraction but BEFORE processing the request
    if (!validate_protocol_version_header(req, session_id, res)) {
        // Validation failed, response already set by validator
        return;
    }

    // Update session activity time
    if (!session_id.empty()) {
        std::shared_ptr<event_dispatcher> dispatcher;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto disp_it = session_dispatchers_.find(session_id);
            if (disp_it != session_dispatchers_.end()) {
                dispatcher = disp_it->second;
            }
        }

        if (dispatcher) {
            dispatcher->update_activity();
        }
    }

    // Parse request
    json req_json;
    try {
        req_json = json::parse(req.body);
    } catch (const json::exception& e) {
        LOG_ERROR("Failed to parse JSON request: ", e.what());
        res.set_status(400);
        res.set_header("Content-Type", "application/json");
        res.set_content("{\"error\":\"Invalid JSON\"}", "application/json");
        return;
    }

    // MCP 2025-06-18: JSON-RPC batching is NOT supported
    // Reject batch requests (arrays) with appropriate error
    if (req_json.is_array()) {
        LOG_ERROR("Batch requests not supported per MCP 2025-06-18 (received array with ", req_json.size(), " items)");
        res.set_status(400);
        res.set_header("Content-Type", "application/json");
        json error_response = response::create_error(nullptr, // No ID for batch errors
                                                     error_code::invalid_request,
                                                     "JSON-RPC batching is not supported in MCP 2025-06-18+. Please "
                                                     "send individual requests instead of arrays.")
                                  .to_json();
        res.set_content(error_response.dump(), "application/json");
        return;
    }

    // Handle single request
    // Check if session exists, or create a temporary one for stateless operation
    std::shared_ptr<event_dispatcher> dispatcher;
    bool is_stateless_request = session_id.empty();

    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto disp_it = session_dispatchers_.find(session_id);
        if (disp_it == session_dispatchers_.end()) {
            // Handle ping request (allowed without session)
            if (req_json.contains("method") && req_json["method"] == "ping") {
                res.set_status(202);
                res.set_header("Content-Type", "text/plain");
                res.set_content("Accepted", "text/plain");
                return;
            }

            // Stateless mode: create a temporary session for this request
            if (is_stateless_request) {
                session_id = generate_session_id();
                LOG_INFO("Stateless request detected, creating temporary session: ", session_id);
                dispatcher = std::make_shared<event_dispatcher>();
                session_dispatchers_[session_id] = dispatcher;
            } else {
                // Session ID was provided but not found - return 404
                LOG_ERROR("Session not found: ", session_id);
                res.set_status(404);
                res.set_header("Content-Type", "application/json");
                res.set_content("{\"error\":\"Session not found\"}", "application/json");
                return;
            }
        } else {
            dispatcher = disp_it->second;
        }
    }

    // Set session ID in response
    set_session_id_header(res, session_id);

    // Create request object
    request mcp_req;
    try {
        // Validate the JSON-RPC message first
        std::string validation_error;
        if (!validate_request_message(req_json, validation_error)) {
            LOG_ERROR("Invalid JSON-RPC request: ", validation_error);
            res.set_status(400);
            res.set_header("Content-Type", "application/json");
            json error_response = response::create_error(req_json.contains("id") ? req_json["id"] : nullptr,
                                                         error_code::invalid_request, validation_error)
                                      .to_json();
            res.set_content(error_response.dump(), "application/json");
            return;
        }

        mcp_req.jsonrpc = req_json["jsonrpc"].get<std::string>();
        if (req_json.contains("id") && !req_json["id"].is_null()) {
            mcp_req.id = req_json["id"];

            // Check for duplicate request ID
            if (!request_id_tracker_.add_request_id(session_id, mcp_req.id)) {
                LOG_ERROR("Duplicate request ID: ", mcp_req.id.dump());
                res.set_status(400);
                res.set_header("Content-Type", "application/json");
                json error_response =
                    response::create_error(mcp_req.id, error_code::invalid_request, "Duplicate request ID").to_json();
                res.set_content(error_response.dump(), "application/json");
                return;
            }
        }
        mcp_req.method = req_json["method"].get<std::string>();
        if (req_json.contains("params")) {
            mcp_req.params = req_json["params"];
        }
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to create request object: ", e.what());
        res.set_status(400);
        res.set_header("Content-Type", "application/json");
        res.set_content("{\"error\":\"Invalid request\"}", "application/json");
        return;
    }

    // Check if this is a notification (no ID)
    if (mcp_req.is_notification()) {
        // Process notification asynchronously
        thread_pool_.enqueue([this, mcp_req, session_id, dispatcher]() {
            try {
                this->process_request(mcp_req, session_id);
            } catch (const std::exception& e) {
                LOG_ERROR("Exception processing notification: ", e.what());
            }
        });

        // Return 202 Accepted for notifications
        res.set_status(202);
        res.set_header("Content-Type", "text/plain");
        res.set_content("Accepted", "text/plain");
        return;
    }

    // For stateless requests, process synchronously and return response directly
    if (is_stateless_request) {
        try {
            json result = this->process_request(mcp_req, session_id);

            // Remove request ID from tracker
            if (!mcp_req.is_notification()) {
                request_id_tracker_.remove_request_id(session_id, mcp_req.id);
            }

            // Clean up temporary session
            {
                std::lock_guard<std::mutex> lock(mutex_);
                session_dispatchers_.erase(session_id);
                session_lifecycle_.erase(session_id);
                LOG_INFO("Cleaned up temporary stateless session: ", session_id);
            }

            // Return response directly in HTTP body
            res.set_status(200);
            res.set_header("Content-Type", "application/json");
            res.set_content(result.dump(), "application/json");
            return;
        } catch (const std::exception& e) {
            LOG_ERROR("Exception processing stateless request: ", e.what());

            // Remove request ID from tracker on error
            if (!mcp_req.is_notification()) {
                request_id_tracker_.remove_request_id(session_id, mcp_req.id);
            }

            // Clean up temporary session
            {
                std::lock_guard<std::mutex> lock(mutex_);
                session_dispatchers_.erase(session_id);
                session_lifecycle_.erase(session_id);
            }

            // Send error response directly
            json error_response = response::create_error(mcp_req.is_notification() ? nullptr : mcp_req.id,
                                                         error_code::internal_error,
                                                         std::string("Internal error: ") + e.what())
                                      .to_json();
            res.set_status(500);
            res.set_header("Content-Type", "application/json");
            res.set_content(error_response.dump(), "application/json");
            return;
        }
    }

    // For stateful requests, process asynchronously and send response via SSE
    thread_pool_.enqueue([this, mcp_req, session_id, dispatcher]() {
        try {
            json result = this->process_request(mcp_req, session_id);

            // Remove request ID from tracker
            if (!mcp_req.is_notification()) {
                request_id_tracker_.remove_request_id(session_id, mcp_req.id);
            }

            // Send response via SSE
            std::stringstream ss;
            ss << "event: message\r\ndata: " << result.dump() << "\r\n\r\n";
            bool send_result = dispatcher->send_event(ss.str());

            if (!send_result) {
                LOG_ERROR("Failed to send response via SSE: session_id=", session_id,
                          ", request_id=", mcp_req.id.dump());
            }
        } catch (const std::exception& e) {
            LOG_ERROR("Exception processing request: ", e.what());

            // Remove request ID from tracker on error
            if (!mcp_req.is_notification()) {
                request_id_tracker_.remove_request_id(session_id, mcp_req.id);
            }

            // Send error response
            json error_response = response::create_error(mcp_req.is_notification() ? nullptr : mcp_req.id,
                                                         error_code::internal_error,
                                                         std::string("Internal error: ") + e.what())
                                      .to_json();

            std::stringstream ss;
            ss << "event: message\r\ndata: " << error_response.dump() << "\r\n\r\n";
            dispatcher->send_event(ss.str());
        }
    });

    // Return 202 Accepted (response will be sent via SSE)
    res.set_status(202);
    res.set_header("Content-Type", "text/plain");
    res.set_content("Accepted", "text/plain");
}

void server::handle_mcp_delete(const http::request_data& req, http::response_builder& res) {
    // DELETE request terminates a session

    // Validate Origin header for DNS rebinding mitigation (MCP 2025-03-26 security)
    if (should_validate_origin(req)) {
        auto origin = req.get_header("Origin");
        if (origin) {
            if (!is_origin_allowed(*origin)) {
                LOG_WARNING("DELETE /mcp rejected due to invalid Origin: ", *origin);
                res.set_status(403); // Forbidden
                res.set_header("Content-Type", "application/json");
                res.set_content("{\"error\":\"Origin not allowed\"}", "application/json");
                return;
            }
        }
    }

    // Setup response headers
    // Set CORS headers based on Origin validation
    auto origin = req.get_header("Origin");
    if (origin && is_origin_allowed(*origin)) {
        res.set_header("Access-Control-Allow-Origin", *origin);
    } else if (!validate_origin_) {
        // Only use wildcard if Origin validation is disabled
        res.set_header("Access-Control-Allow-Origin", "*");
    }
    res.set_header("Access-Control-Expose-Headers", "Mcp-Session-Id");

    // Extract session ID from header or query parameter
    std::string session_id = extract_session_id(req);

    if (session_id.empty()) {
        // No session ID provided
        res.set_status(400);
        res.set_header("Content-Type", "application/json");
        res.set_content("{\"error\":\"Session ID required\"}", "application/json");
        return;
    }

    // Check if session exists
    bool session_exists = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto disp_it = session_dispatchers_.find(session_id);
        if (disp_it != session_dispatchers_.end()) {
            session_exists = true;
        }
    }

    if (!session_exists) {
        // Session not found
        res.set_status(404);
        res.set_header("Content-Type", "application/json");
        res.set_content("{\"error\":\"Session not found\"}", "application/json");
        return;
    }

    // Set session ID in response
    set_session_id_header(res, session_id);

    // Close the session
    close_session(session_id);

    // Return 204 No Content on success
    res.set_status(204);
}

json server::process_request(const request& req, const std::string& session_id) {
    // Check if it is a notification
    if (req.is_notification()) {
        if (req.method == "notifications/initialized") {
            // Transition from initializing to ready state
            auto current_state = get_session_lifecycle_state(session_id);
            if (current_state == lifecycle_state::initializing) {
                set_session_lifecycle_state(session_id, lifecycle_state::ready);
                LOG_INFO("Session ", session_id, " transitioned to ready state");
            } else {
                LOG_WARNING("Received initialized notification in unexpected state for session: ", session_id);
            }
        } else if (req.method == "notifications/cancelled") {
            // Handle cancellation notification per MCP 2025-03-26
            if (req.params.contains("requestId")) {
                json request_id = req.params["requestId"];
                std::string reason = req.params.contains("reason") && req.params["reason"].is_string()
                                         ? req.params["reason"].get<std::string>()
                                         : "No reason provided";

                LOG_INFO("Received cancellation notification for request: ", request_id.dump(), ", reason: ", reason,
                         ", session: ", session_id);

                // Call cancellation handler if registered
                cancellation_handler handler;
                {
                    std::lock_guard<std::mutex> lock(mutex_);
                    handler = cancellation_handler_;
                }

                if (handler) {
                    try {
                        handler(request_id, reason, session_id);
                    } catch (const std::exception& e) {
                        LOG_ERROR("Exception in cancellation handler: ", e.what());
                    } catch (...) {
                        LOG_ERROR("Unknown exception in cancellation handler");
                    }
                }
            } else {
                LOG_WARNING("Received malformed cancellation notification (missing requestId) for session: ",
                            session_id);
            }
        }
        // Handle other notifications registered via register_notification
        else {
            notification_handler handler;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                auto it = notification_handlers_.find(req.method);
                if (it != notification_handlers_.end()) {
                    handler = it->second;
                }
            }

            if (handler) {
                try {
                    handler(req.params, session_id);
                } catch (const std::exception& e) {
                    LOG_ERROR("Exception in notification handler: ", e.what());
                } catch (...) {
                    LOG_ERROR("Unknown exception in notification handler");
                }
            }
        }
        return json::object();
    }

    // Check if this is a response to a pending elicitation request
    if (!req.id.is_null()) {
        std::shared_ptr<std::promise<elicitation_result>> promise_ptr;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = pending_elicitation_requests_.find(req.id);
            if (it != pending_elicitation_requests_.end()) {
                promise_ptr = it->second;
                // Don't erase yet - let request_elicitation() do that after getting the result
            }
        }

        // If we found a pending elicitation request, this is a response to it
        if (promise_ptr) {
            LOG_INFO("Received elicitation response for request ID: ", req.id.dump());

            try {
                // Parse the elicitation result from the request params
                // The client sends back a JSON-RPC request with the elicitation response
                if (req.method == "elicitation/response" || req.params.contains("action")) {
                    elicitation_result result = elicitation_result::from_json(req.params);
                    promise_ptr->set_value(result);
                    return json::object(); // Return empty for responses
                } else {
                    // If not a proper elicitation response, treat as error
                    throw mcp_exception(error_code::invalid_params,
                                        "Expected elicitation response but got: " + req.method);
                }
            } catch (const std::exception& e) {
                LOG_ERROR("Error parsing elicitation response: ", e.what());
                try {
                    promise_ptr->set_exception(std::current_exception());
                } catch (...) {
                    // Promise already set, ignore
                }
                return response::create_error(req.id, error_code::invalid_params,
                                              "Invalid elicitation response: " + std::string(e.what()))
                    .to_json();
            }
        }
    }

    // Process method call
    try {
        LOG_INFO("Processing method call: ", req.method);

        // Get current lifecycle state
        auto current_state = get_session_lifecycle_state(session_id);

        // Enforce lifecycle rules per MCP 2025-03-26
        if (req.method == "initialize") {
            // Initialize can only be sent when uninitialized
            if (current_state != lifecycle_state::uninitialized) {
                LOG_ERROR("Initialize request received in invalid state: ", session_id);
                return response::create_error(req.id, error_code::invalid_request,
                                              "Initialize already called for this session")
                    .to_json();
            }
            return handle_initialize(req, session_id);
        } else if (req.method == "ping") {
            // Ping is allowed in any state per MCP 2025-03-26
            return response::create_success(req.id, json::object()).to_json();
        }

        // All other requests require ready state (after initialized notification)
        if (current_state != lifecycle_state::ready) {
            LOG_WARNING("Request received before session ready: ", session_id, ", method: ", req.method);
            return response::create_error(req.id, error_code::invalid_request,
                                          "Session not ready - initialize handshake not complete")
                .to_json();
        }

        // Find registered method handler
        method_handler handler;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = method_handlers_.find(req.method);
            if (it != method_handlers_.end()) {
                handler = it->second;
            }
        }

        if (handler) {
            // Call handler
            LOG_INFO("Calling method handler: ", req.method);
            json result = handler(req.params, session_id);

            // Create success response
            LOG_INFO("Method call successful: ", req.method);
            return response::create_success(req.id, result).to_json();
        }

        // Method not found
        LOG_WARNING("Method not found: ", req.method);
        return response::create_error(req.id, error_code::method_not_found, "Method not found: " + req.method)
            .to_json();
    } catch (const mcp_exception& e) {
        // MCP exception
        LOG_ERROR("MCP exception: ", e.what(), ", code: ", static_cast<int>(e.code()));
        return response::create_error(req.id, e.code(), e.what()).to_json();
    } catch (const std::exception& e) {
        // Other exceptions
        LOG_ERROR("Exception while processing request: ", e.what());
        return response::create_error(req.id, error_code::internal_error, "Internal error: " + std::string(e.what()))
            .to_json();
    } catch (...) {
        // Unknown exception
        LOG_ERROR("Unknown exception while processing request");
        return response::create_error(req.id, error_code::internal_error, "Unknown internal error").to_json();
    }
}

json server::handle_initialize(const request& req, const std::string& session_id) {
    const json& params = req.params;

    // Version negotiation
    if (!params.contains("protocolVersion") || !params["protocolVersion"].is_string()) {
        LOG_ERROR("Missing or invalid protocolVersion parameter");
        return response::create_error(req.id, error_code::invalid_params,
                                      "Expected string for 'protocolVersion' parameter")
            .to_json();
    }

    std::string requested_version = params["protocolVersion"].get<std::string>();
    LOG_INFO("Client requested protocol version: ", requested_version);

    if (requested_version != MCP_VERSION) {
        LOG_ERROR("Unsupported protocol version: ", requested_version, ", server supports: ", MCP_VERSION);
        return response::create_error(req.id, error_code::invalid_params, "Unsupported protocol version",
                                      {{"supported", {MCP_VERSION}}, {"requested", params["protocolVersion"]}})
            .to_json();
    }

    // Extract client info
    std::string client_name = "UnknownClient";
    std::string client_version = "UnknownVersion";

    if (params.contains("clientInfo")) {
        if (params["clientInfo"].contains("name")) {
            client_name = params["clientInfo"]["name"];
        }
        if (params["clientInfo"].contains("version")) {
            client_version = params["clientInfo"]["version"];
        }
    }

    // Log connection
    LOG_INFO("Client connected: ", client_name, " ", client_version);

    // Store client capabilities for this session
    if (params.contains("capabilities")) {
        std::lock_guard<std::mutex> lock(mutex_);
        session_client_capabilities_[session_id] = params["capabilities"];
        LOG_INFO("Stored client capabilities for session: ", session_id);
    }

    // Store negotiated protocol version for this session (MCP 2025-06-18+)
    // This will be used to validate the MCP-Protocol-Version header in subsequent requests
    {
        std::lock_guard<std::mutex> lock(mutex_);
        json version_state = {{"negotiated_version", requested_version}};
        session_state_[session_id] = version_state;
        LOG_INFO("Stored negotiated protocol version for session: ", session_id, ", version: ", requested_version);
    }

    // Transition session to initializing state
    set_session_lifecycle_state(session_id, lifecycle_state::initializing);
    LOG_INFO("Session ", session_id, " transitioned to initializing state");

    // Return server info and capabilities
    json server_info = {{"name", name_}, {"version", version_}};

    json result = {{"protocolVersion", MCP_VERSION}, {"capabilities", capabilities_}, {"serverInfo", server_info}};

    LOG_INFO("Initialization successful, waiting for notifications/initialized notification");

    return response::create_success(req.id, result).to_json();
}

void server::send_jsonrpc(const std::string& session_id, const json& message) {
    // Check if session ID is valid
    if (session_id.empty()) {
        LOG_WARNING("Cannot send message to empty session_id");
        return;
    }

    // Get session dispatcher
    std::shared_ptr<event_dispatcher> dispatcher;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = session_dispatchers_.find(session_id);
        if (it == session_dispatchers_.end()) {
            LOG_ERROR("Session not found: ", session_id);
            return;
        }
        dispatcher = it->second;
    }

    // Confirm dispatcher is still valid
    if (!dispatcher || dispatcher->is_closed()) {
        LOG_WARNING("Cannot send to closed session: ", session_id);
        return;
    }

    // Send message
    std::stringstream ss;
    ss << "event: message\r\ndata: " << message.dump() << "\r\n\r\n";
    bool result = dispatcher->send_event(ss.str());

    if (!result) {
        LOG_ERROR("Failed to send message to session: ", session_id);
    }
}

void server::send_request(const std::string& session_id, const request& req) {
    send_jsonrpc(session_id, req.to_json());
}

void server::send_progress(const std::string& session_id, const progress_notification& notification) {
    // Create a progress notification request
    request notif_req = create_progress_notification(notification);
    send_jsonrpc(session_id, notif_req.to_json());
}

lifecycle_state server::get_session_lifecycle_state(const std::string& session_id) const {
    // Check if session ID is valid
    if (session_id.empty()) {
        return lifecycle_state::uninitialized;
    }

    try {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = session_lifecycle_.find(session_id);
        if (it != session_lifecycle_.end()) {
            return it->second;
        }
        return lifecycle_state::uninitialized;
    } catch (const std::exception& e) {
        LOG_ERROR("Exception getting session lifecycle state: ", e.what());
        return lifecycle_state::uninitialized;
    }
}

void server::set_session_lifecycle_state(const std::string& session_id, lifecycle_state state) {
    // Check if session ID is valid
    if (session_id.empty()) {
        LOG_WARNING("Cannot set lifecycle state for empty session_id");
        return;
    }

    try {
        std::lock_guard<std::mutex> lock(mutex_);
        // Check if session still exists
        auto it = session_dispatchers_.find(session_id);
        if (it == session_dispatchers_.end()) {
            LOG_WARNING("Cannot set lifecycle state for non-existent session: ", session_id);
            return;
        }
        session_lifecycle_[session_id] = state;
    } catch (const std::exception& e) {
        LOG_ERROR("Exception setting session lifecycle state: ", e.what());
    }
}

bool server::is_session_initialized(const std::string& session_id) const {
    // Backward compatibility: check if session is in ready state
    auto state = get_session_lifecycle_state(session_id);
    return state == lifecycle_state::ready;
}

void server::set_session_initialized(const std::string& session_id, bool initialized) {
    // Backward compatibility: transition to ready or uninitialized state
    if (initialized) {
        set_session_lifecycle_state(session_id, lifecycle_state::ready);
    } else {
        set_session_lifecycle_state(session_id, lifecycle_state::uninitialized);
    }
}

std::string server::generate_session_id() const {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 15);

    std::stringstream ss;
    ss << std::hex;

    // UUID format: 8-4-4-4-12 hexadecimal digits
    for (int i = 0; i < 8; ++i) {
        ss << dis(gen);
    }
    ss << "-";

    for (int i = 0; i < 4; ++i) {
        ss << dis(gen);
    }
    ss << "-";

    for (int i = 0; i < 4; ++i) {
        ss << dis(gen);
    }
    ss << "-";

    for (int i = 0; i < 4; ++i) {
        ss << dis(gen);
    }
    ss << "-";

    for (int i = 0; i < 12; ++i) {
        ss << dis(gen);
    }

    return ss.str();
}

std::string server::extract_session_id(const http::request_data& req) const {
    // First, try to get from Mcp-Session-Id header (Streamable HTTP transport)
    auto header = req.get_header("Mcp-Session-Id");
    if (header && !header->empty()) {
        return *header;
    }

    // Fallback to query parameter for backward compatibility
    auto param_it = req.params.find("session_id");
    if (param_it != req.params.end() && !param_it->second.empty()) {
        return param_it->second;
    }

    return "";
}

void server::set_session_id_header(http::response_builder& res, const std::string& session_id) const {
    if (!session_id.empty()) {
        res.set_header("Mcp-Session-Id", session_id);
    }
}

void server::check_inactive_sessions() {
    if (!running_)
        return;

    const auto now = std::chrono::steady_clock::now();
    const auto timeout = std::chrono::minutes(60); // 1 hour inactive then close

    std::vector<std::string> sessions_to_close;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& [session_id, dispatcher] : session_dispatchers_) {
            if (now - dispatcher->last_activity() > timeout) {
                // Exceeded idle time limit
                sessions_to_close.push_back(session_id);
            }
        }
    }

    // Close inactive sessions
    for (const auto& session_id : sessions_to_close) {
        LOG_INFO("Closing inactive session: ", session_id);

        close_session(session_id);
    }
}

bool server::set_mount_point(const std::string& mount_point, const std::string& dir, http::headers_map headers) {
    return http_server_->set_mount_point(mount_point, dir, headers);
}

void server::set_session_state(const std::string& session_id, const json& state) {
    std::lock_guard<std::mutex> lock(mutex_);
    session_state_[session_id] = state;
}

json server::get_session_state(const std::string& session_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = session_state_.find(session_id);
    if (it != session_state_.end()) {
        return it->second;
    }
    return json(); // Return null JSON
}

void server::clear_session_state(const std::string& session_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    session_state_.erase(session_id);
}

void server::close_session(const std::string& session_id) {
    // Clean up resources safely
    try {
        LOG_INFO("close_session begin: ", session_id);
        for (const auto& [key, handler] : session_cleanup_handler_) {
            handler(key);
        }

        // Copy resources to be processed
        std::shared_ptr<event_dispatcher> dispatcher_to_close;
        std::unique_ptr<std::jthread> thread_to_release;

        {
            std::lock_guard<std::mutex> lock(mutex_);

            // Get dispatcher pointer
            auto dispatcher_it = session_dispatchers_.find(session_id);
            if (dispatcher_it != session_dispatchers_.end()) {
                dispatcher_to_close = dispatcher_it->second;
                session_dispatchers_.erase(dispatcher_it);
            }

            // Get thread pointer
            auto thread_it = sse_threads_.find(session_id);
            if (thread_it != sse_threads_.end()) {
                thread_to_release = std::move(thread_it->second);
                sse_threads_.erase(thread_it);
            }

            // Clean up lifecycle state, client capabilities, and session state
            session_lifecycle_.erase(session_id);
            session_client_capabilities_.erase(session_id);
            session_state_.erase(session_id);
        }

        // Clear request IDs for this session
        request_id_tracker_.clear_session(session_id);

        // Close dispatcher outside the lock
        if (dispatcher_to_close && !dispatcher_to_close->is_closed()) {
            dispatcher_to_close->close();
        }

        // Release thread resources
        // NOTE: Don't try to join if called from the same thread (would cause deadlock)
        if (thread_to_release) {
            auto thread_id = thread_to_release->get_id();
            if (thread_id == std::this_thread::get_id()) {
                LOG_INFO("close_session self-thread cleanup: ", session_id);
                // We're being called from within the thread itself
                // Just request stop and let the thread exit naturally
                thread_to_release->request_stop();
                // Release ownership without joining (thread will clean up on exit)
                thread_to_release.release();
            } else {
                LOG_INFO("close_session joining thread for: ", session_id);
                // Safe to request stop and join
                thread_to_release->request_stop();
                thread_to_release.reset(); // jthread joins automatically
            }
        }
        LOG_INFO("close_session end: ", session_id);
    } catch (const std::exception& e) {
        LOG_WARNING("Exception while cleaning up session resources: ", session_id, ", ", e.what());
    } catch (...) {
        LOG_WARNING("Unknown exception while cleaning up session resources: ", session_id);
    }
}

// Security helper functions (MCP 2025-03-26)

bool server::is_origin_allowed(const std::string& origin) const {
    // Empty origin is not allowed
    if (origin.empty()) {
        return false;
    }

    // If allowed_origins is empty, allow all origins (not recommended for production)
    if (allowed_origins_.empty()) {
        return true;
    }

    // Check if the origin matches any allowed origin
    for (const auto& allowed : allowed_origins_) {
        if (origin == allowed) {
            return true;
        }

        // Also check with port variations for localhost
        // e.g., "http://localhost:8080" should match "http://localhost"
        if (allowed.find("localhost") != std::string::npos || allowed.find("127.0.0.1") != std::string::npos) {
            // Extract scheme and host from allowed origin
            size_t scheme_end = allowed.find("://");
            if (scheme_end != std::string::npos) {
                std::string allowed_scheme = allowed.substr(0, scheme_end);
                std::string allowed_rest = allowed.substr(scheme_end + 3);

                // Extract scheme from request origin
                size_t origin_scheme_end = origin.find("://");
                if (origin_scheme_end != std::string::npos) {
                    std::string origin_scheme = origin.substr(0, origin_scheme_end);
                    std::string origin_rest = origin.substr(origin_scheme_end + 3);

                    // Match scheme
                    if (origin_scheme == allowed_scheme) {
                        // Check if origin starts with allowed host (ignoring port)
                        size_t origin_port_pos = origin_rest.find(':');
                        std::string origin_host = origin_port_pos != std::string::npos
                                                      ? origin_rest.substr(0, origin_port_pos)
                                                      : origin_rest;

                        if (origin_host == allowed_rest ||
                            (allowed_rest == "localhost" && origin_host == "localhost") ||
                            (allowed_rest == "127.0.0.1" && origin_host == "127.0.0.1")) {
                            return true;
                        }
                    }
                }
            }
        }
    }

    return false;
}

bool server::should_validate_origin(const http::request_data& req) const {
    // Only validate if validation is enabled
    if (!validate_origin_) {
        return false;
    }

    // Only validate POST and DELETE requests (state-changing operations)
    // GET requests for SSE don't need Origin validation
    if (req.method == "POST" || req.method == "DELETE") {
        return true;
    }

    return false;
}

bool server::validate_protocol_version_header(const http::request_data& req, const std::string& session_id,
                                              http::response_builder& res) const {
    // Per MCP 2025-06-18: MCP-Protocol-Version header is REQUIRED in all HTTP requests after initialization
    //
    // However, for backward compatibility, we should:
    // 1. Accept requests without the header (assume 2025-03-26)
    // 2. Reject requests with invalid/unsupported version
    // 3. Reject requests where version doesn't match negotiated version

    // If no session ID, this is an initial connection - header not required yet
    if (session_id.empty()) {
        return true;
    }

    // Get the MCP-Protocol-Version header
    auto version_header = req.get_header("MCP-Protocol-Version");

    // Get the negotiated version from session state
    std::string negotiated_version;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto state_it = session_state_.find(session_id);
        if (state_it != session_state_.end() && state_it->second.contains("negotiated_version")) {
            negotiated_version = state_it->second["negotiated_version"].get<std::string>();
        }
    }

    // If no header provided
    if (!version_header || version_header->empty()) {
        // Backward compatibility: assume 2025-03-26 if no header
        // This allows legacy clients to continue working
        LOG_WARNING("MCP-Protocol-Version header missing for session: ", session_id,
                    ", assuming 2025-03-26 for backward compatibility");
        return true;
    }

    // List of supported versions
    const std::vector<std::string> supported_versions = {"2025-03-26", "2025-06-18", "2025-11-25"};

    // Check if the version is supported
    bool is_supported = std::find(supported_versions.begin(), supported_versions.end(), *version_header) !=
                        supported_versions.end();

    if (!is_supported) {
        LOG_ERROR("Unsupported MCP-Protocol-Version header: ", *version_header);
        res.set_status(400); // Bad Request
        res.set_header("Content-Type", "application/json");
        json error_response = {{"error", "Unsupported protocol version"},
                               {"version_received", *version_header},
                               {"supported_versions", supported_versions}};
        res.set_content(error_response.dump(), "application/json");
        return false;
    }

    // If we have a negotiated version, verify the header matches it
    if (!negotiated_version.empty() && *version_header != negotiated_version) {
        LOG_ERROR("Protocol version mismatch: header=", *version_header, ", negotiated=", negotiated_version);
        res.set_status(400); // Bad Request
        res.set_header("Content-Type", "application/json");
        json error_response = {{"error", "Protocol version mismatch"},
                               {"version_in_header", *version_header},
                               {"negotiated_version", negotiated_version}};
        res.set_content(error_response.dump(), "application/json");
        return false;
    }

    // All checks passed
    return true;
}

} // namespace mcp
