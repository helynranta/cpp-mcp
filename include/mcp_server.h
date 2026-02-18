/**
 * @file mcp_server.h
 * @brief MCP Server implementation
 *
 * This file implements the server-side functionality for the Model Context Protocol.
 * Follows the 2025-03-26 basic protocol specification.
 */

#ifndef MCP_SERVER_H
#define MCP_SERVER_H

#include "mcp_jsonrpc_validation.h"
#include "mcp_logger.h"
#include "mcp_message.h"
#include "mcp_progress.h"
#include "mcp_resource.h"
#include "mcp_thread_pool.h"
#include "mcp_tool.h"

// Include the HTTP abstraction layer
#include "mcp_http_factory.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <future>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>

namespace mcp {

/**
 * @enum lifecycle_state
 * @brief Lifecycle states for MCP session per 2025-03-26 spec
 */
enum class lifecycle_state {
    uninitialized, // Session created but initialize not received
    initializing,  // initialize request received, waiting for initialized notification
    ready,         // Session fully initialized and ready for operations
    shutdown       // Session shutting down
};

using method_handler = std::function<json(const json&, const std::string&)>;
using tool_handler = method_handler;
using notification_handler = std::function<void(const json&, const std::string&)>;
using auth_handler = std::function<bool(const std::string&, const std::string&)>;
using session_cleanup_handler = std::function<void(const std::string&)>;
using cancellation_handler =
    std::function<void(const json& request_id, const std::string& reason, const std::string& session_id)>;

/**
 * @brief Tool confirmation handler type
 *
 * Called before executing a tool that requires confirmation.
 * @param tool_name The name of the tool to be executed
 * @param arguments The arguments to be passed to the tool
 * @param session_id The session ID of the client requesting the tool
 * @return true if the tool should be executed, false to deny execution
 */
using tool_confirmation_handler =
    std::function<bool(const std::string& tool_name, const json& arguments, const std::string& session_id)>;

class event_dispatcher {
public:
    event_dispatcher() = default;

    ~event_dispatcher() { close(); }

    bool wait_event(http::streaming_data_sink* sink,
                    const std::chrono::milliseconds& timeout = std::chrono::milliseconds(10000)) {
        if (!sink || closed_.load(std::memory_order_acquire)) {
            return false;
        }

        std::string message_copy;
        {
            std::unique_lock<std::mutex> lk(m_);

            if (closed_.load(std::memory_order_acquire)) {
                return false;
            }

            // Wait for a message to be available in the queue
            bool result = cv_.wait_for(
                lk, timeout, [&] { return !message_queue_.empty() || closed_.load(std::memory_order_acquire); });

            if (closed_.load(std::memory_order_acquire)) {
                return false;
            }

            if (!result || message_queue_.empty()) {
                return false;
            }

            // Get the next message from the queue
            message_copy = std::move(message_queue_.front());
            message_queue_.pop();
        }

        try {
            if (!message_copy.empty()) {
                if (!sink->write(message_copy.data(), message_copy.size())) {
                    close();
                    return false;
                }
            }
            return true;
        } catch (...) {
            close();
            return false;
        }
    }

    bool send_event(const std::string& message) {
        if (closed_.load(std::memory_order_acquire) || message.empty()) {
            return false;
        }

        try {
            std::lock_guard<std::mutex> lk(m_);

            if (closed_.load(std::memory_order_acquire)) {
                return false;
            }

            // Add message to the queue
            message_queue_.push(message);

            cv_.notify_one(); // Notify waiting threads
            return true;
        } catch (...) {
            return false;
        }
    }

    void close() {
        bool was_closed = closed_.exchange(true, std::memory_order_release);
        if (was_closed) {
            return;
        }

        try {
            cv_.notify_all();
        } catch (...) {
            // Ignore exceptions
        }
    }

    bool is_closed() const { return closed_.load(std::memory_order_acquire); }

    // Get the last activity time
    std::chrono::steady_clock::time_point last_activity() const {
        std::lock_guard<std::mutex> lk(m_);
        return last_activity_;
    }

    // Update the activity time (when sending or receiving a message)
    void update_activity() {
        std::lock_guard<std::mutex> lk(m_);
        last_activity_ = std::chrono::steady_clock::now();
    }

private:
    mutable std::mutex m_;
    std::condition_variable cv_;
    std::queue<std::string> message_queue_;
    std::atomic<bool> closed_{false};
    std::chrono::steady_clock::time_point last_activity_{std::chrono::steady_clock::now()};
};

/**
 * @class server
 * @brief Main MCP server class
 *
 * The server class implements an HTTP server that handles JSON-RPC requests
 * according to the Model Context Protocol specification.
 */
class server {
public:
    /**
     * @struct configuration
     * @brief Configuration settings for the server.
     *
     * This struct holds all configurable parameters for the server, including
     * network bindings, identification, and endpoint paths. If SSL is enabled,
     * it also includes paths to the server certificate and private key.
     */
    struct configuration {
        /** Host to bind to (e.g., "localhost", "0.0.0.0") */
        std::string host{"localhost"};

        /** Port to listen on */
        int port{8080};

        /** Server name */
        std::string name{"MCP Server"};

        /** Server version */
        std::string version{"0.0.1"};

        /** SSE endpoint path (legacy, deprecated) */
        std::string sse_endpoint{"/sse"};

        /** Message endpoint path (legacy, deprecated) */
        std::string msg_endpoint{"/message"};

        /** MCP unified endpoint path (Streamable HTTP transport) */
        std::string mcp_endpoint{"/mcp"};

        unsigned int threadpool_size{std::thread::hardware_concurrency()};

        /** Request timeout in seconds (0 = no timeout) */
        unsigned int request_timeout_seconds{0};

        /**
         * @brief Security configuration for HTTP transport (MCP 2025-03-26)
         */
        struct {
            /**
             * Enable Origin header validation (DNS rebinding mitigation).
             * When enabled, validates Origin header against allowed_origins list.
             * Default: true for localhost bindings, false for 0.0.0.0
             */
            bool validate_origin{true};

            /**
             * List of allowed origins for Origin header validation.
             * Default: localhost origins (http://localhost, https://localhost, etc.)
             * Use empty vector to allow all origins (not recommended for production)
             */
            std::vector<std::string> allowed_origins{"http://localhost", "https://localhost", "http://127.0.0.1",
                                                     "https://127.0.0.1"};

            /**
             * Enable tool execution confirmation hooks.
             * When enabled, tools can be marked for user confirmation before execution.
             */
            bool enable_tool_confirmation{false};
        } security;

#ifdef MCP_SSL
        /**
         * @brief SSL configuration settings.
         *
         * Contains optional paths to the server certificate and private key.
         * These are used when SSL support is enabled.
         */
        struct {
            /** Path to the server certificate */
            std::optional<std::string> server_cert_path{std::nullopt};

            /** Path to the server private key */
            std::optional<std::string> server_private_key_path{std::nullopt};
        } ssl;
#endif
    };

    /**
     * @brief Constructor
     * @param conf The server configuration
     */
    server(const server::configuration& conf);

    /**
     * @brief Destructor
     */
    ~server();

    /**
     * @brief Start the server
     * @param blocking If true, this call blocks until the server stops
     * @return True if the server started successfully
     */
    bool start(bool blocking = true);

    /**
     * @brief Stop the server
     */
    void stop();

    /**
     * @brief Check if the server is running
     * @return True if the server is running
     */
    bool is_running() const;

    /**
     * @brief Set server information
     * @param name The name of the server
     * @param version The version of the server
     */
    void set_server_info(const std::string& name, const std::string& version);

    /**
     * @brief Set server capabilities
     * @param capabilities The capabilities of the server
     */
    void set_capabilities(const json& capabilities);

    /**
     * @brief Register a method handler
     * @param method The method name
     * @param handler The function to call when the method is invoked
     */
    void register_method(const std::string& method, method_handler handler);

    /**
     * @brief Register a notification handler
     * @param method The notification method name
     * @param handler The function to call when the notification is received
     */
    void register_notification(const std::string& method, notification_handler handler);

    /**
     * @brief Register a resource
     * @param path The path to mount the resource at
     * @param resource The resource to register
     */
    void register_resource(const std::string& path, std::shared_ptr<resource> resource);

    /**
     * @brief Register a tool
     * @param tool The tool to register
     * @param handler The function to call when the tool is invoked
     */
    void register_tool(const tool& tool, tool_handler handler);

    /**
     * @brief Register a session cleanup handler
     * @param key Tool or resource name to be cleaned up
     * @param handler The function to call when the session is closed
     */
    void register_session_cleanup(const std::string& key, session_cleanup_handler handler);

    /**
     * @brief Get the list of available tools
     * @return JSON array of available tools
     */
    std::vector<tool> get_tools() const;

    /**
     * @brief Set authentication handler
     * @param handler Function that takes a token and returns true if valid
     * @note The handler should return true if the token is valid, otherwise false
     * @note Not used in the current implementation
     */
    void set_auth_handler(auth_handler handler);

    /**
     * @brief Set cancellation handler
     * @param handler Function to call when a cancellation notification is received
     * @note Handler receives request_id, reason, and session_id
     */
    void set_cancellation_handler(cancellation_handler handler);

    /**
     * @brief Set tool confirmation handler (MCP 2025-03-26 safety)
     * @param handler Function to call before executing tools that require confirmation
     * @note Handler receives tool_name, arguments, and session_id; returns true to allow execution
     * @note Only called for tools marked as requiring confirmation (destructive tools, etc.)
     */
    void set_tool_confirmation_handler(tool_confirmation_handler handler);

    /**
     * @brief Request information from the user via elicitation (MCP 2025-06-18)
     * @param session_id The session ID of the client
     * @param message Human-readable prompt for the user
     * @param requested_schema JSON Schema defining expected response structure
     * @return Elicitation result with action (accept/decline/cancel) and optional content
     * @throws mcp_exception if client doesn't support elicitation or request fails
     * @note This is a synchronous call that blocks until user responds
     * @note The client must declare "elicitation" capability to use this feature
     */
    elicitation_result request_elicitation(const std::string& session_id, const std::string& message,
                                           const json& requested_schema);

    /**
     * @brief Check if a client supports elicitation
     * @param session_id The session ID of the client
     * @return true if client has declared elicitation capability
     */
    bool client_supports_elicitation(const std::string& session_id) const;

    /**
     * @brief Send a request (or notification) to a client
     * @param session_id The session ID of the client
     * @param req The request to send
     */
    void send_request(const std::string& session_id, const request& req);

    /**
     * @brief Send a progress notification to a client
     * @param session_id The session ID of the client
     * @param notification The progress notification to send
     */
    void send_progress(const std::string& session_id, const progress_notification& notification);

    /**
     * @brief Set mount point for server
     * @param mount_point The mount point to set
     * @param dir The directory to serve from the mount point
     * @param headers Optional headers to include in the response
     * @return True if the mount point was set successfully
     */
    bool set_mount_point(const std::string& mount_point, const std::string& dir,
                         http::headers_map headers = http::headers_map());

    /**
     * @brief Set session state data
     * @param session_id The session ID
     * @param state The state data to store (arbitrary JSON)
     */
    void set_session_state(const std::string& session_id, const json& state);

    /**
     * @brief Get session state data
     * @param session_id The session ID
     * @return The session state data (empty JSON if session doesn't exist)
     */
    json get_session_state(const std::string& session_id) const;

    /**
     * @brief Clear session state data
     * @param session_id The session ID
     */
    void clear_session_state(const std::string& session_id);

private:
    std::string host_;
    int port_;
    std::string name_;
    std::string version_;
    json capabilities_;

    // The HTTP server
    std::unique_ptr<http::server_interface> http_server_;

    // Server thread (for non-blocking mode) - using jthread for automatic joining
    std::unique_ptr<std::jthread> server_thread_;

    // SSE threads - using jthread for automatic joining and cooperative cancellation
    std::map<std::string, std::unique_ptr<std::jthread>> sse_threads_;

    // Event dispatcher for server-sent events
    event_dispatcher sse_dispatcher_;

    // Session-specific event dispatchers
    std::map<std::string, std::shared_ptr<event_dispatcher>> session_dispatchers_;

    // Track stateless sessions (no SSE connection)
    std::unordered_set<std::string> stateless_sessions_;

    // Server-sent events endpoint (legacy)
    std::string sse_endpoint_;
    std::string msg_endpoint_;

    // MCP unified endpoint (Streamable HTTP transport)
    std::string mcp_endpoint_;

    // Method handlers
    std::map<std::string, method_handler> method_handlers_;

    // Notification handlers
    std::map<std::string, notification_handler> notification_handlers_;

    // Resources map (path -> resource)
    std::map<std::string, std::shared_ptr<resource>> resources_;

    // Tools map (name -> handler)
    std::map<std::string, std::pair<tool, tool_handler>> tools_;

    // Authentication handler
    auth_handler auth_handler_;

    // Cancellation handler
    cancellation_handler cancellation_handler_;

    // Tool confirmation handler (MCP 2025-03-26 safety)
    tool_confirmation_handler tool_confirmation_handler_;

    // Pending elicitation requests (MCP 2025-06-18)
    // Maps request ID to promise for async elicitation responses
    std::map<json, std::shared_ptr<std::promise<elicitation_result>>> pending_elicitation_requests_;

    // Request timeout in seconds
    unsigned int request_timeout_seconds_;

    // Mutex for thread safety
    mutable std::mutex mutex_;

    // Running flag
    bool running_ = false;

    // Shared alive flag for safe lambda capture - lambdas can check this instead of
    // accessing raw `this` pointer. When server is destroyed, this is set to false
    // and lambdas can safely detect that the server is gone.
    std::shared_ptr<std::atomic<bool>> alive_ = std::make_shared<std::atomic<bool>>(true);

    // Thread pool for async method handlers
    thread_pool thread_pool_;

    // Map to track session lifecycle state (session_id -> state)
    std::map<std::string, lifecycle_state> session_lifecycle_;

    // Map to track client capabilities per session (session_id -> capabilities)
    std::map<std::string, json> session_client_capabilities_;

    // Map to track custom session state (session_id -> state data)
    std::map<std::string, json> session_state_;

    // Handle SSE requests (legacy)
    void handle_sse(const http::request_data& req, http::response_builder& res);

    // Handle incoming JSON-RPC requests (legacy)
    void handle_jsonrpc(const http::request_data& req, http::response_builder& res);

    // Handle unified MCP endpoint (Streamable HTTP transport)
    void handle_mcp(const http::request_data& req, http::response_builder& res);

    // Handle MCP GET request (SSE connection establishment)
    void handle_mcp_get(const http::request_data& req, http::response_builder& res);

    // Handle MCP POST request (JSON-RPC messages)
    void handle_mcp_post(const http::request_data& req, http::response_builder& res);

    // Handle MCP DELETE request (session termination)
    void handle_mcp_delete(const http::request_data& req, http::response_builder& res);

    // Send a JSON-RPC message to a client
    void send_jsonrpc(const std::string& session_id, const json& message);

    // Process a JSON-RPC request
    json process_request(const request& req, const std::string& session_id);

    // Handle initialization request
    json handle_initialize(const request& req, const std::string& session_id);

    // Get session lifecycle state
    lifecycle_state get_session_lifecycle_state(const std::string& session_id) const;

    // Set session lifecycle state
    void set_session_lifecycle_state(const std::string& session_id, lifecycle_state state);

    // Check if a session is initialized (for backward compatibility)
    bool is_session_initialized(const std::string& session_id) const;

    // Set session initialization status (for backward compatibility)
    void set_session_initialized(const std::string& session_id, bool initialized);

    // Generate a random session ID
    std::string generate_session_id() const;

    // Extract session ID from request (Mcp-Session-Id header or query parameter)
    std::string extract_session_id(const http::request_data& req) const;

    // Set session ID in response header
    void set_session_id_header(http::response_builder& res, const std::string& session_id) const;

    // Set protocol version header in response
    void set_protocol_version_header(http::response_builder& res, const std::string& session_id) const;

    // Auxiliary function to create an async handler from a regular handler
    template <typename F>
    std::function<std::future<json>(const json&, const std::string&)> make_async_handler(F&& handler) {
        return [handler = std::forward<F>(handler)](const json& params,
                                                    const std::string& session_id) -> std::future<json> {
            return std::async(std::launch::async,
                              [handler, params, session_id]() -> json { return handler(params, session_id); });
        };
    }

    // Helper class to simplify lock management
    class auto_lock {
    public:
        explicit auto_lock(std::mutex& mutex) : lock_(mutex) {}

    private:
        std::lock_guard<std::mutex> lock_;
    };

    // Get auto lock
    auto_lock get_lock() const { return auto_lock(mutex_); }

    // Session management and maintenance
    void check_inactive_sessions();

    std::mutex maintenance_mutex_;
    std::condition_variable maintenance_cond_;
    std::unique_ptr<std::jthread> maintenance_thread_;
    bool maintenance_thread_run_ = false;

    // Session cleanup handler
    std::map<std::string, session_cleanup_handler> session_cleanup_handler_;

    // Request ID tracker for uniqueness validation
    request_id_tracker request_id_tracker_;

    // Close session
    void close_session(const std::string& session_id);

    // Security configuration (MCP 2025-03-26)
    bool validate_origin_;
    std::vector<std::string> allowed_origins_;
    bool enable_tool_confirmation_;

    // Validate Origin header for DNS rebinding mitigation
    bool is_origin_allowed(const std::string& origin) const;

    // Check if origin validation should be performed for this request
    bool should_validate_origin(const http::request_data& req) const;

    // Validate MCP-Protocol-Version header (MCP 2025-06-18+)
    bool validate_protocol_version_header(const http::request_data& req, const std::string& session_id,
                                          http::response_builder& res) const;
};

} // namespace mcp

#endif // MCP_SERVER_H
