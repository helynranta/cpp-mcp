/**
 * @file completion_example.cpp
 * @brief Example demonstrating MCP 2025-06-18 completion support with _meta and context fields
 *
 * This example shows:
 * 1. Server declaring completions capability
 * 2. Registering completion handler for prompts
 * 3. Handling completion requests with context field
 * 4. Returning completion results with _meta field
 */

#include "mcp_message.h"
#include "mcp_server.h"
#include <iostream>
#include <string>
#include <vector>

using namespace mcp;

int main() {
    std::cout << "=== MCP Completion Example ===" << std::endl;
    std::cout << std::endl;

    // Part 1: Server declares completions capability
    std::cout << "Part 1: Server declares completions capability" << std::endl;
    std::cout << "-----------------------------------------------" << std::endl;

    json capabilities = {
        {"tools", json::object()},
        {"completions", json::object()} // Declare completion support
    };

    json capabilities_json = capabilities;
    std::cout << "Server capabilities: " << capabilities_json.dump(2) << std::endl;
    std::cout << std::endl;

    // Part 2: Example completion request with context
    std::cout << "Part 2: Completion request with context field" << std::endl;
    std::cout << "----------------------------------------------" << std::endl;

    complete_request req;
    req.ref_type = "ref/prompt";
    req.ref_name = "code_review";
    req.argument_name = "language";
    req.argument_value = "py";
    
    // Add context with previously-resolved arguments
    req.context = json::object();
    req.context["arguments"] = json::object();
    req.context["arguments"]["repo"] = "cpp-mcp";
    req.context["arguments"]["branch"] = "main";

    json req_json = req.to_json();
    std::cout << "Completion request:" << std::endl;
    std::cout << req_json.dump(2) << std::endl;
    std::cout << std::endl;

    // Part 3: Simulated completion handler
    std::cout << "Part 3: Processing completion request" << std::endl;
    std::cout << "-------------------------------------" << std::endl;

    // In a real implementation, this would be a registered method handler
    // that checks the ref type and uses the context to provide relevant suggestions
    
    std::vector<std::string> all_languages = {
        "python", "pytorch", "pyside", "pylint", "pyqt",
        "java", "javascript", "go", "rust", "cpp"
    };

    // Filter based on the current input value
    std::vector<std::string> matching_languages;
    std::string prefix = req.argument_value;
    
    std::cout << "Filtering languages starting with: '" << prefix << "'" << std::endl;
    for (const auto& lang : all_languages) {
        if (lang.find(prefix) == 0) {
            matching_languages.push_back(lang);
        }
    }

    std::cout << "Found " << matching_languages.size() << " matches" << std::endl;
    
    // Use context to add repo-specific suggestions
    if (req.context.contains("arguments") && 
        req.context["arguments"].contains("repo")) {
        std::string repo = req.context["arguments"]["repo"].get<std::string>();
        std::cout << "Repository context: " << repo << std::endl;
        
        // For cpp-mcp repo, we might prioritize C++ related completions
        if (repo == "cpp-mcp" && prefix == "py") {
            std::cout << "Note: cpp-mcp is a C++ project, but Python also used for tooling" << std::endl;
        }
    }
    std::cout << std::endl;

    // Part 4: Create completion result with _meta field
    std::cout << "Part 4: Completion result with _meta field" << std::endl;
    std::cout << "------------------------------------------" << std::endl;

    complete_result result;
    result.values = matching_languages;
    result.total = static_cast<int>(all_languages.size());
    result.has_more = false; // We returned all matches
    
    // Add _meta field with additional metadata
    result.meta["source"] = "builtin";
    result.meta["cached"] = false;
    result.meta["timestamp"] = "2026-02-17T05:30:00Z";
    result.meta["context_used"] = !req.context.empty();

    json result_json = result.to_json();
    std::cout << "Completion result:" << std::endl;
    std::cout << result_json.dump(2) << std::endl;
    std::cout << std::endl;

    // Part 5: Example with resource template reference
    std::cout << "Part 5: Completion for resource template" << std::endl;
    std::cout << "----------------------------------------" << std::endl;

    complete_request resource_req;
    resource_req.ref_type = "ref/resource";
    resource_req.ref_uri = "file:///{repo}/src/{file}";
    resource_req.argument_name = "file";
    resource_req.argument_value = "mcp_";
    
    // Add context with previously-resolved template variables
    resource_req.context = json::object();
    resource_req.context["arguments"] = json::object();
    resource_req.context["arguments"]["repo"] = "/home/user/projects/cpp-mcp";

    json resource_req_json = resource_req.to_json();
    std::cout << "Resource completion request:" << std::endl;
    std::cout << resource_req_json.dump(2) << std::endl;
    std::cout << std::endl;

    // Simulated file completions
    std::vector<std::string> files = {
        "mcp_server.cpp", "mcp_client.cpp", "mcp_message.h", 
        "mcp_tool.h", "mcp_resource.h"
    };

    complete_result file_result;
    file_result.values = files;
    file_result.total = static_cast<int>(files.size());
    file_result.has_more = false;
    
    // Add _meta with file system metadata
    file_result.meta["source"] = "filesystem";
    file_result.meta["base_path"] = resource_req.context["arguments"]["repo"].get<std::string>() + "/src";
    file_result.meta["scanned_at"] = "2026-02-17T05:35:00Z";

    json file_result_json = file_result.to_json();
    std::cout << "File completion result:" << std::endl;
    std::cout << file_result_json.dump(2) << std::endl;
    std::cout << std::endl;

    // Part 6: Example completion handler implementation
    std::cout << "Part 6: Example completion handler registration" << std::endl;
    std::cout << "-----------------------------------------------" << std::endl;

    // This shows how you would register a completion handler in a real server
    std::cout << "// Register completion handler" << std::endl;
    std::cout << "server.register_method(\"completion/complete\", " << std::endl;
    std::cout << "    [](const json& params, const std::string& session_id) -> json {" << std::endl;
    std::cout << "        // Parse request" << std::endl;
    std::cout << "        auto req = complete_request::from_json(params);" << std::endl;
    std::cout << "        " << std::endl;
    std::cout << "        // Generate completions based on ref type and context" << std::endl;
    std::cout << "        complete_result result;" << std::endl;
    std::cout << "        " << std::endl;
    std::cout << "        if (req.ref_type == \"ref/prompt\") {" << std::endl;
    std::cout << "            // Handle prompt argument completion" << std::endl;
    std::cout << "            result.values = get_prompt_argument_completions(req);" << std::endl;
    std::cout << "        } else if (req.ref_type == \"ref/resource\") {" << std::endl;
    std::cout << "            // Handle resource template variable completion" << std::endl;
    std::cout << "            result.values = get_resource_variable_completions(req);" << std::endl;
    std::cout << "        }" << std::endl;
    std::cout << "        " << std::endl;
    std::cout << "        // Add metadata" << std::endl;
    std::cout << "        result.meta[\"session_id\"] = session_id;" << std::endl;
    std::cout << "        result.meta[\"timestamp\"] = current_timestamp();" << std::endl;
    std::cout << "        " << std::endl;
    std::cout << "        return result.to_json();" << std::endl;
    std::cout << "    }" << std::endl;
    std::cout << ");" << std::endl;
    std::cout << std::endl;

    // Part 7: Benefits of _meta and context
    std::cout << "Part 7: Benefits of _meta and context fields" << std::endl;
    std::cout << "--------------------------------------------" << std::endl;
    std::cout << std::endl;

    std::cout << "Benefits of the context field:" << std::endl;
    std::cout << "  - Provides previously-resolved variables for better suggestions" << std::endl;
    std::cout << "  - Enables stateful multi-step completion flows" << std::endl;
    std::cout << "  - Allows context-aware filtering (e.g., repo-specific languages)" << std::endl;
    std::cout << "  - Supports template variable completion in resource URIs" << std::endl;
    std::cout << std::endl;

    std::cout << "Benefits of the _meta field:" << std::endl;
    std::cout << "  - Communicates additional metadata without extending schema" << std::endl;
    std::cout << "  - Useful for debugging (timestamps, sources, cache info)" << std::endl;
    std::cout << "  - Enables vendor-specific extensions" << std::endl;
    std::cout << "  - Helps clients make informed decisions about completions" << std::endl;
    std::cout << std::endl;

    std::cout << "=== Example Complete ===" << std::endl;

    return 0;
}
