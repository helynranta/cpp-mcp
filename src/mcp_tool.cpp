/**
 * @file mcp_tool.cpp
 * @brief Implementation of the MCP tools
 *
 * This file implements the tool-related functionality for the MCP protocol.
 * Follows the 2025-03-26 basic protocol specification.
 */

#include "mcp_tool.h"

#include <random>
#include <sstream>

namespace mcp {

// Implementation for tool_builder
tool_builder::tool_builder(const std::string& name) : name_(name) {}

tool_builder& tool_builder::with_description(const std::string& description) {
    description_ = description;
    return *this;
}

tool_builder& tool_builder::add_param(const std::string& name, const std::string& description, const std::string& type,
                                      bool required) {
    json param = {{"type", type}, {"description", description}};

    parameters_["properties"][name] = param;

    if (required) {
        required_params_.push_back(name);
    }

    return *this;
}

tool_builder& tool_builder::with_string_param(const std::string& name, const std::string& description, bool required) {
    return add_param(name, description, "string", required);
}

tool_builder& tool_builder::with_number_param(const std::string& name, const std::string& description, bool required) {
    return add_param(name, description, "number", required);
}

tool_builder& tool_builder::with_boolean_param(const std::string& name, const std::string& description, bool required) {
    return add_param(name, description, "boolean", required);
}

tool_builder& tool_builder::with_array_param(const std::string& name, const std::string& description,
                                             const std::string& item_type, bool required) {
    json param = {{"type", "array"}, {"description", description}, {"items", {{"type", item_type}}}};

    parameters_["properties"][name] = param;

    if (required) {
        required_params_.push_back(name);
    }

    return *this;
}

tool_builder& tool_builder::with_object_param(const std::string& name, const std::string& description,
                                              const json& properties, bool required) {
    json param = {{"type", "object"}, {"description", description}, {"properties", properties}};

    parameters_["properties"][name] = param;

    if (required) {
        required_params_.push_back(name);
    }

    return *this;
}

tool_builder& tool_builder::with_read_only(bool value) {
    has_read_only_ = true;
    read_only_value_ = value;
    return *this;
}

tool_builder& tool_builder::with_destructive(bool value) {
    has_destructive_ = true;
    destructive_value_ = value;
    return *this;
}

tool_builder& tool_builder::with_cost(double value) {
    has_cost_ = true;
    cost_value_ = value;
    return *this;
}

tool_builder& tool_builder::with_latency(int value) {
    has_latency_ = true;
    latency_value_ = value;
    return *this;
}

tool_builder& tool_builder::with_confirmation_required(bool value) {
    requires_confirmation_ = value;
    return *this;
}

tool_builder& tool_builder::with_title(const std::string& title) {
    has_title_ = true;
    title_ = title;
    return *this;
}

tool_builder& tool_builder::with_output_schema(const json& schema) {
    has_output_schema_ = true;
    output_schema_ = schema;
    return *this;
}

tool tool_builder::build() const {
    tool t;
    t.name = name_;
    t.description = description_;

    // Create the parameters schema
    json schema = parameters_;
    schema["type"] = "object";
    if (!schema.contains("properties") || !schema["properties"].is_object()) {
        schema["properties"] = json::object();
    }

    if (!required_params_.empty()) {
        schema["required"] = required_params_;
    }

    t.parameters_schema = schema;

    // Set metadata annotations
    t.has_read_only = has_read_only_;
    t.read_only_value = read_only_value_;

    t.has_destructive = has_destructive_;
    t.destructive_value = destructive_value_;

    t.has_cost = has_cost_;
    t.cost_value = cost_value_;

    t.has_latency = has_latency_;
    t.latency_value = latency_value_;

    // Set security flags (MCP 2025-03-26)
    t.requires_confirmation = requires_confirmation_;

    // MCP 2025-06-18: Set title and output schema if present
    t.has_title = has_title_;
    if (has_title_) {
        t.title = title_;
    }

    t.has_output_schema = has_output_schema_;
    if (has_output_schema_) {
        t.output_schema = output_schema_;
    }

    return t;
}

} // namespace mcp
