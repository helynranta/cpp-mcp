/**
 * @file elicitation_example.cpp
 * @brief Example demonstrating MCP 2025-06-18 elicitation (human-in-the-loop) feature
 *
 * This example shows how to:
 * 1. Declare elicitation capability in client
 * 2. Request user input during tool execution
 * 3. Handle different response actions (accept, decline, cancel)
 * 4. Use JSON Schema to validate user input
 *
 * Note: This example demonstrates the API structure. Full request-response
 * handling would require implementing a complete client-server interaction loop.
 */

#include "mcp_message.h"
#include "mcp_server.h"
#include "mcp_tool.h"

#include <iostream>
#include <string>

using namespace mcp;

int main() {
    std::cout << "=== MCP 2025-06-18 Elicitation (Human-in-the-Loop) Example ===" << std::endl;
    std::cout << std::endl;

    // ========================================================================
    // Part 1: Client Capability Declaration
    // ========================================================================
    std::cout << "Part 1: Client declares elicitation capability" << std::endl;
    std::cout << "------------------------------------------------" << std::endl;

    // Client declares support for elicitation during initialization
    json client_capabilities = {{"elicitation", json::object()} // Empty object indicates support
    };

    std::cout << "Client capabilities: " << client_capabilities.dump(2) << std::endl;
    std::cout << std::endl;

    // ========================================================================
    // Part 2: Simple Text Input Request
    // ========================================================================
    std::cout << "Part 2: Server requests simple text input" << std::endl;
    std::cout << "------------------------------------------" << std::endl;

    // Server wants to ask user for their API key
    elicitation_params simple_params;
    simple_params.message = "Please provide your API key to continue";
    simple_params.requested_schema = {{"type", "object"},
                                       {"properties", {{"api_key", {{"type", "string"}, {"description", "Your API key"}}}}},
                                       {"required", json::array({"api_key"})}};

    std::cout << "Elicitation request params:" << std::endl;
    std::cout << simple_params.to_json().dump(2) << std::endl;
    std::cout << std::endl;

    // ========================================================================
    // Part 3: User Accepts and Provides Data
    // ========================================================================
    std::cout << "Part 3: User accepts and provides data" << std::endl;
    std::cout << "---------------------------------------" << std::endl;

    elicitation_result accept_result;
    accept_result.action = elicitation_action::accept;
    accept_result.content = {{"api_key", "sk-test-1234567890"}};

    std::cout << "User response (accept):" << std::endl;
    std::cout << accept_result.to_json().dump(2) << std::endl;
    std::cout << std::endl;

    // ========================================================================
    // Part 4: User Declines Request
    // ========================================================================
    std::cout << "Part 4: User declines the request" << std::endl;
    std::cout << "-----------------------------------" << std::endl;

    elicitation_result decline_result;
    decline_result.action = elicitation_action::decline;

    std::cout << "User response (decline):" << std::endl;
    std::cout << decline_result.to_json().dump(2) << std::endl;
    std::cout << std::endl;

    // ========================================================================
    // Part 5: User Cancels (Dismisses Dialog)
    // ========================================================================
    std::cout << "Part 5: User cancels/dismisses" << std::endl;
    std::cout << "-------------------------------" << std::endl;

    elicitation_result cancel_result;
    cancel_result.action = elicitation_action::cancel;

    std::cout << "User response (cancel):" << std::endl;
    std::cout << cancel_result.to_json().dump(2) << std::endl;
    std::cout << std::endl;

    // ========================================================================
    // Part 6: Complex Multi-Field Form
    // ========================================================================
    std::cout << "Part 6: Complex multi-field form with validation" << std::endl;
    std::cout << "------------------------------------------------" << std::endl;

    elicitation_params complex_params;
    complex_params.message = "Please provide your profile information";
    complex_params.requested_schema = {
        {"type", "object"},
        {"properties",
         {{"name", {{"type", "string"}, {"minLength", 3}, {"maxLength", 50}, {"description", "Your full name"}}},
          {"email", {{"type", "string"}, {"format", "email"}, {"description", "Your email address"}}},
          {"age", {{"type", "integer"}, {"minimum", 18}, {"maximum", 120}, {"description", "Your age"}}},
          {"newsletter", {{"type", "boolean"}, {"default", false}, {"description", "Subscribe to newsletter?"}}},
          {"role",
           {{"type", "string"},
            {"enum", json::array({"developer", "designer", "manager"})},
            {"enumNames", json::array({"Software Developer", "UI/UX Designer", "Project Manager"})},
            {"description", "Your role"}}}}},
        {"required", json::array({"name", "email"})}};

    std::cout << "Complex form schema:" << std::endl;
    std::cout << complex_params.to_json().dump(2) << std::endl;
    std::cout << std::endl;

    // User provides complete data
    elicitation_result complex_result;
    complex_result.action = elicitation_action::accept;
    complex_result.content = {
        {"name", "Alice Johnson"}, {"email", "alice@example.com"}, {"age", 28}, {"newsletter", true}, {"role", "developer"}};

    std::cout << "User response (complex form accepted):" << std::endl;
    std::cout << complex_result.to_json().dump(2) << std::endl;
    std::cout << std::endl;

    // ========================================================================
    // Part 7: Server-Side Usage Pattern
    // ========================================================================
    std::cout << "Part 7: Server checking client capability" << std::endl;
    std::cout << "------------------------------------------" << std::endl;

    // Create a minimal server to demonstrate capability checking
    server::configuration config;
    config.host = "localhost";
    config.port = 9000;
    config.name = "ElicitationExampleServer";
    config.version = "1.0";

    server srv(config);

    std::string fake_session_id = "example-session-123";

    // Check if client supports elicitation
    bool supports_elicitation = srv.client_supports_elicitation(fake_session_id);
    std::cout << "Client supports elicitation: " << (supports_elicitation ? "yes" : "no") << std::endl;
    std::cout << "(Note: Returns false because no actual client is connected)" << std::endl;
    std::cout << std::endl;

    // ========================================================================
    // Part 8: Tool Using Elicitation
    // ========================================================================
    std::cout << "Part 8: Example tool that would use elicitation" << std::endl;
    std::cout << "-----------------------------------------------" << std::endl;

    // Define a tool that could use elicitation for missing parameters
    tool database_query_tool = tool_builder("query_database")
                                   .with_description("Query the database - requires credentials")
                                   .with_string_param("query", "SQL query to execute", false)
                                   .with_string_param("credentials", "Database credentials (optional)", false)
                                   .build();

    std::cout << "Tool definition:" << std::endl;
    std::cout << database_query_tool.to_json().dump(2) << std::endl;
    std::cout << std::endl;

    std::cout << "In a real implementation, the tool handler would:" << std::endl;
    std::cout << "1. Check if credentials are provided in params" << std::endl;
    std::cout << "2. If not, check if client supports elicitation" << std::endl;
    std::cout << "3. Request credentials via elicitation/create" << std::endl;
    std::cout << "4. Handle user response (accept/decline/cancel)" << std::endl;
    std::cout << "5. Execute query if credentials obtained" << std::endl;
    std::cout << std::endl;

    // ========================================================================
    // Summary
    // ========================================================================
    std::cout << "=== Summary ===" << std::endl;
    std::cout << "Elicitation enables multi-turn interactions where servers can:" << std::endl;
    std::cout << "- Request additional information from users" << std::endl;
    std::cout << "- Provide structured forms with validation" << std::endl;
    std::cout << "- Handle user decisions (accept/decline/cancel)" << std::endl;
    std::cout << "- Maintain control flow within tool execution" << std::endl;
    std::cout << std::endl;
    std::cout << "Key Benefits:" << std::endl;
    std::cout << "- Better user experience with guided input" << std::endl;
    std::cout << "- Client-controlled data sharing" << std::endl;
    std::cout << "- Type-safe input with JSON Schema" << std::endl;
    std::cout << "- Clear action semantics (accept vs decline vs cancel)" << std::endl;

    return 0;
}
