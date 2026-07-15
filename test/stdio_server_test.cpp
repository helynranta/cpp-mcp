#include "mcp_stdio_server.h"

#include <atomic>
#include <boost/test/unit_test.hpp>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace {
using json = nlohmann::ordered_json;

auto parse_lines(const std::string& output) -> std::vector<json> {
    std::vector<json> messages;
    std::istringstream lines(output);
    for (std::string line; std::getline(lines, line);) {
        if (!line.empty()) {
            messages.push_back(json::parse(line));
        }
    }
    return messages;
}
} // namespace

BOOST_AUTO_TEST_SUITE(StdioServerTests)

BOOST_AUTO_TEST_CASE(stdio_cancellation_notification_interrupts_a_running_tool_call) {
    std::ostringstream input_text;
    input_text << json{{"jsonrpc", "2.0"},
                       {"id", 1},
                       {"method", "initialize"},
                       {"params",
                        {{"protocolVersion", "2025-11-25"},
                         {"capabilities", json::object()},
                         {"clientInfo", {{"name", "stdio-test"}, {"version", "1.0"}}}}}}
                      .dump()
               << '\n';
    input_text << json{{"jsonrpc", "2.0"}, {"method", "notifications/initialized"}}.dump() << '\n';
    input_text << json{{"jsonrpc", "2.0"},
                       {"id", 2},
                       {"method", "tools/call"},
                       {"params", {{"name", "wait"}, {"arguments", json::object()}}}}
                      .dump()
               << '\n';
    input_text << json{{"jsonrpc", "2.0"},
                       {"method", "notifications/cancelled"},
                       {"params", {{"requestId", 2}, {"reason", "client stopped waiting"}}}}
                      .dump()
               << '\n';

    std::istringstream input(input_text.str());
    std::ostringstream output;
    std::ostringstream errors;
    mcp::stdio_server server({.input = &input, .output = &output, .error = &errors});
    std::mutex cancellation_mutex;
    std::condition_variable cancellation_condition;
    bool cancelled = false;
    std::atomic_bool tool_finished = false;
    std::atomic_bool interrupted_before_completion = false;
    server.set_cancellation_handler([&](const json& request_id, const std::string& reason, const std::string&) {
        BOOST_CHECK_EQUAL(request_id, 2);
        BOOST_CHECK_EQUAL(reason, "client stopped waiting");
        interrupted_before_completion.store(!tool_finished.load());
        {
            std::lock_guard lock(cancellation_mutex);
            cancelled = true;
        }
        cancellation_condition.notify_all();
    });
    server.register_tool(mcp::tool_builder("wait").with_description("Wait until cancelled").build(),
                         [&](const json&, const std::string&) -> json {
                             std::unique_lock lock(cancellation_mutex);
                             cancellation_condition.wait_for(lock, std::chrono::milliseconds(250),
                                                             [&] { return cancelled; });
                             tool_finished.store(true);
                             return json{{"content", json::array()}};
                         });

    BOOST_REQUIRE(server.start());

    BOOST_CHECK(interrupted_before_completion.load());
    const auto messages = parse_lines(output.str());
    BOOST_REQUIRE_EQUAL(messages.size(), 2u);
    BOOST_CHECK_EQUAL(messages[0].at("id"), 1);
    BOOST_CHECK_EQUAL(messages[1].at("id"), 2);
}

BOOST_AUTO_TEST_CASE(stdio_server_stays_available_for_a_complete_client_session_until_input_eof) {
    std::ostringstream input_text;
    input_text << json{{"jsonrpc", "2.0"},
                       {"id", 1},
                       {"method", "initialize"},
                       {"params",
                        {{"protocolVersion", "2025-11-25"},
                         {"capabilities", json::object()},
                         {"clientInfo", {{"name", "stdio-test"}, {"version", "1.0"}}}}}}
                      .dump()
               << '\n';
    input_text << json{{"jsonrpc", "2.0"}, {"method", "notifications/initialized"}}.dump() << '\n';
    input_text << json{{"jsonrpc", "2.0"}, {"id", 2}, {"method", "tools/list"}}.dump() << '\n';
    input_text << json{{"jsonrpc", "2.0"},
                       {"id", 3},
                       {"method", "tools/call"},
                       {"params", {{"name", "echo"}, {"arguments", {{"text", "hello"}}}}}}
                      .dump()
               << '\n';

    std::istringstream input(input_text.str());
    std::ostringstream output;
    std::ostringstream errors;
    mcp::stdio_server server({.input = &input, .output = &output, .error = &errors});
    server.set_server_info("stdio-test-server", "1.0");
    server.set_capabilities(json{{"tools", {{"listChanged", true}}}});
    server.register_tool(
        mcp::tool_builder("echo").with_description("Echo text").with_string_param("text", "Text to echo").build(),
        [](const json& args, const std::string&) -> json {
            return json{{"content", json::array({json{{"type", "text"}, {"text", args.at("text")}}})}};
        });

    BOOST_REQUIRE(server.start());

    const auto messages = parse_lines(output.str());
    BOOST_TEST_MESSAGE("stdio output: " << output.str());
    BOOST_REQUIRE_EQUAL(messages.size(), 3u);
    BOOST_CHECK_EQUAL(messages[0].at("id"), 1);
    BOOST_CHECK_EQUAL(messages[0].at("result").at("serverInfo").at("name"), "stdio-test-server");
    BOOST_CHECK_EQUAL(messages[1].at("id"), 2);
    BOOST_REQUIRE_EQUAL(messages[1].at("result").at("tools").size(), 1u);
    BOOST_CHECK_EQUAL(messages[1].at("result").at("tools")[0].at("name"), "echo");
    BOOST_CHECK_EQUAL(messages[2].at("id"), 3);
    BOOST_CHECK_EQUAL(messages[2].at("result").at("content")[0].at("text"), "hello");
    BOOST_CHECK(output.str().ends_with('\n'));
}

BOOST_AUTO_TEST_CASE(stdio_server_emits_tool_list_changes_without_non_protocol_stdout) {
    std::ostringstream input_text;
    input_text << json{{"jsonrpc", "2.0"},
                       {"id", 1},
                       {"method", "initialize"},
                       {"params",
                        {{"protocolVersion", "2025-11-25"},
                         {"capabilities", json::object()},
                         {"clientInfo", {{"name", "stdio-test"}, {"version", "1.0"}}}}}}
                      .dump()
               << '\n';
    input_text << json{{"jsonrpc", "2.0"}, {"method", "notifications/initialized"}}.dump() << '\n';
    input_text << json{{"jsonrpc", "2.0"}, {"id", 2}, {"method", "catalog/change"}}.dump() << '\n';

    std::istringstream input(input_text.str());
    std::ostringstream output;
    std::ostringstream errors;
    mcp::stdio_server server({.input = &input, .output = &output, .error = &errors});

    server.register_tool(mcp::tool_builder("temporary").with_description("Temporary tool").build(),
                         [](const json&, const std::string&) -> json { return json::array(); });
    server.register_method("catalog/change", [&server](const json&, const std::string&) -> json {
        BOOST_REQUIRE(server.unregister_tool("temporary"));
        BOOST_CHECK(!server.unregister_tool("temporary"));
        return json::object();
    });

    BOOST_REQUIRE(server.start());

    const auto messages = parse_lines(output.str());
    BOOST_TEST_MESSAGE("dynamic stdio output: " << output.str());
    BOOST_REQUIRE_EQUAL(messages.size(), 3u);
    BOOST_CHECK_EQUAL(messages[0].at("id"), 1);
    BOOST_CHECK_EQUAL(messages[1].at("jsonrpc"), "2.0");
    BOOST_CHECK_EQUAL(messages[1].at("method"), "notifications/tools/list_changed");
    BOOST_CHECK(!messages[1].contains("id"));
    BOOST_CHECK_EQUAL(messages[2].at("id"), 2);
    BOOST_CHECK(server.get_tools().empty());
}

BOOST_AUTO_TEST_CASE(stdio_server_replaces_a_tool_catalog_atomically_and_notifies_once_per_effective_change) {
    std::ostringstream input_text;
    input_text << json{{"jsonrpc", "2.0"},
                       {"id", 1},
                       {"method", "initialize"},
                       {"params",
                        {{"protocolVersion", "2025-11-25"},
                         {"capabilities", json::object()},
                         {"clientInfo", {{"name", "stdio-test"}, {"version", "1.0"}}}}}}
                      .dump()
               << '\n';
    input_text << json{{"jsonrpc", "2.0"}, {"method", "notifications/initialized"}}.dump() << '\n';
    input_text << json{{"jsonrpc", "2.0"}, {"id", 2}, {"method", "catalog/replace"}}.dump() << '\n';
    input_text << json{{"jsonrpc", "2.0"}, {"id", 3}, {"method", "tools/list"}}.dump() << '\n';
    input_text << json{{"jsonrpc", "2.0"},
                       {"id", 4},
                       {"method", "tools/call"},
                       {"params", {{"name", "current"}, {"arguments", json::object()}}}}
                      .dump()
               << '\n';

    std::istringstream input(input_text.str());
    std::ostringstream output;
    std::ostringstream errors;
    mcp::stdio_server server({.input = &input, .output = &output, .error = &errors});
    server.register_tool(mcp::tool_builder("stale").with_description("Stale tool").build(),
                         [](const json&, const std::string&) -> json { return json::array(); });
    server.register_method("catalog/replace", [&server](const json&, const std::string&) -> json {
        const std::vector<mcp::tool_registration> catalog{
            {mcp::tool_builder("current").with_description("Current tool").build(),
             [](const json&, const std::string&) -> json {
                 return json{{"content", json::array({json{{"type", "text"}, {"text", "current handler"}}})}};
             }},
        };
        BOOST_CHECK(server.replace_tools(catalog));
        BOOST_CHECK(!server.replace_tools(catalog));
        return json::object();
    });

    BOOST_REQUIRE(server.start());

    const auto messages = parse_lines(output.str());
    BOOST_TEST_MESSAGE("catalog replacement output: " << output.str());
    BOOST_REQUIRE_EQUAL(messages.size(), 5u);
    BOOST_CHECK_EQUAL(messages[0].at("id"), 1);
    BOOST_CHECK_EQUAL(messages[1].at("method"), "notifications/tools/list_changed");
    BOOST_CHECK_EQUAL(messages[2].at("id"), 2);
    BOOST_CHECK_EQUAL(messages[3].at("id"), 3);
    BOOST_REQUIRE_EQUAL(messages[3].at("result").at("tools").size(), 1u);
    BOOST_CHECK_EQUAL(messages[3].at("result").at("tools")[0].at("name"), "current");
    BOOST_CHECK_EQUAL(messages[4].at("id"), 4);
    BOOST_CHECK_EQUAL(messages[4].at("result").at("content")[0].at("text"), "current handler");
}

BOOST_AUTO_TEST_CASE(tool_listing_and_calls_remain_consistent_while_the_catalog_is_replaced) {
    mcp::server::configuration config;
    config.host = "127.0.0.1";
    config.port = 0;
    mcp::server server(config);
    server.set_outbound_message_handler([](const std::string&, const json&) {});

    const auto initialize = server.process_jsonrpc(
        json{{"jsonrpc", "2.0"},
             {"id", 1},
             {"method", "initialize"},
             {"params",
              {{"protocolVersion", "2025-11-25"},
               {"capabilities", json::object()},
               {"clientInfo", {{"name", "concurrency-test"}, {"version", "1.0"}}}}}},
        "concurrent-session");
    BOOST_REQUIRE(initialize.has_value());
    server.process_jsonrpc(json{{"jsonrpc", "2.0"}, {"method", "notifications/initialized"}}, "concurrent-session");

    const auto make_catalog = [](const std::string& version) {
        return std::vector<mcp::tool_registration>{
            {mcp::tool_builder("active").with_description(version).build(),
             [version](const json&, const std::string&) -> json {
                 return json{{"content", json::array({json{{"type", "text"}, {"text", version}}})}};
             }},
        };
    };
    const auto alpha = make_catalog("alpha");
    const auto beta = make_catalog("beta");
    server.replace_tools(alpha);

    std::atomic_int invalid_responses = 0;
    std::jthread replacer([&] {
        for (auto iteration = 0; iteration < 100; ++iteration) {
            server.replace_tools(iteration % 2 == 0 ? beta : alpha);
        }
    });
    std::jthread caller([&] {
        for (auto iteration = 0; iteration < 100; ++iteration) {
            const auto list = server.process_jsonrpc(json{{"jsonrpc", "2.0"}, {"id", 2}, {"method", "tools/list"}},
                                                     "concurrent-session");
            const auto call = server.process_jsonrpc(
                json{{"jsonrpc", "2.0"},
                     {"id", 3},
                     {"method", "tools/call"},
                     {"params", {{"name", "active"}, {"arguments", json::object()}}}},
                "concurrent-session");
            if (!list || !call || list->contains("error") || call->contains("error") ||
                list->at("result").at("tools").size() != 1 ||
                (list->at("result").at("tools")[0].at("description") != "alpha" &&
                 list->at("result").at("tools")[0].at("description") != "beta") ||
                (call->at("result").at("content")[0].at("text") != "alpha" &&
                 call->at("result").at("content")[0].at("text") != "beta")) {
                ++invalid_responses;
            }
        }
    });
    replacer.join();
    caller.join();

    BOOST_CHECK_EQUAL(invalid_responses.load(), 0);
}

BOOST_AUTO_TEST_SUITE_END()
