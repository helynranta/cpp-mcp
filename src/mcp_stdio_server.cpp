#include "mcp_stdio_server.h"

#include <utility>

namespace mcp {

stdio_server::stdio_server(configuration config)
    : config_(config), protocol_([] {
          server::configuration value;
          value.host = "127.0.0.1";
          value.port = 0;
          return value;
      }()) {
    if (!config_.input || !config_.output || !config_.error) {
        throw std::invalid_argument("stdio_server requires input, output, and error streams");
    }
    protocol_.set_outbound_message_handler([this](const std::string&, const json& message) { write_message(message); });
}

bool stdio_server::start() {
    if (running_.exchange(true)) {
        return false;
    }

    for (std::string line; running_.load() && std::getline(*config_.input, line);) {
        if (line.empty()) {
            continue;
        }
        try {
            const auto message = json::parse(line);
            if (const auto response = protocol_.process_jsonrpc(message, session_id)) {
                write_message(*response);
            }
        } catch (const json::exception& exception) {
            write_message(response::create_error(nullptr, error_code::parse_error, exception.what()).to_json());
        } catch (const std::exception& exception) {
            *config_.error << "stdio server error: " << exception.what() << '\n';
        }
    }
    running_.store(false);
    return true;
}

void stdio_server::stop() {
    running_.store(false);
}

void stdio_server::set_server_info(const std::string& name, const std::string& version) {
    protocol_.set_server_info(name, version);
}

void stdio_server::set_capabilities(const json& capabilities) {
    protocol_.set_capabilities(capabilities);
}

void stdio_server::register_method(const std::string& method, method_handler handler) {
    protocol_.register_method(method, std::move(handler));
}

void stdio_server::register_notification(const std::string& method, notification_handler handler) {
    protocol_.register_notification(method, std::move(handler));
}

void stdio_server::register_tool(const tool& value, tool_handler handler) {
    protocol_.register_tool(value, std::move(handler));
}

bool stdio_server::unregister_tool(const std::string& name) {
    return protocol_.unregister_tool(name);
}

bool stdio_server::replace_tools(const std::vector<tool_registration>& catalog) {
    return protocol_.replace_tools(catalog);
}

std::vector<tool> stdio_server::get_tools() const {
    return protocol_.get_tools();
}

void stdio_server::set_cancellation_handler(cancellation_handler handler) {
    protocol_.set_cancellation_handler(std::move(handler));
}

void stdio_server::set_tool_confirmation_handler(tool_confirmation_handler handler) {
    protocol_.set_tool_confirmation_handler(std::move(handler));
}

void stdio_server::set_session_state(const json& state) {
    protocol_.set_session_state(session_id, state);
}

json stdio_server::get_session_state() const {
    return protocol_.get_session_state(session_id);
}

void stdio_server::clear_session_state() {
    protocol_.clear_session_state(session_id);
}

void stdio_server::write_message(const json& message) {
    std::lock_guard<std::mutex> lock(output_mutex_);
    *config_.output << message.dump() << '\n';
    config_.output->flush();
}

} // namespace mcp
