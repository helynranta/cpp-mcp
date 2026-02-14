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
tool_builder::tool_builder(const std::string& name)
    : name_(name) {
}

tool_builder& tool_builder::with_description(const std::string& description) {
    description_ = description;
    return *this;
}

tool_builder& tool_builder::add_param(const std::string& name, 
                                     const std::string& description, 
                                     const std::string& type, 
                                     bool required) {
    json param = {
        {"type", type},
        {"description", description}
    };
    
    parameters_["properties"][name] = param;
    
    if (required) {
        required_params_.push_back(name);
    }
    
    return *this;
}

tool_builder& tool_builder::with_string_param(const std::string& name, 
                                             const std::string& description, 
                                             bool required) {
    return add_param(name, description, "string", required);
}

tool_builder& tool_builder::with_number_param(const std::string& name, 
                                             const std::string& description, 
                                             bool required) {
    return add_param(name, description, "number", required);
}

tool_builder& tool_builder::with_boolean_param(const std::string& name, 
                                              const std::string& description, 
                                              bool required) {
    return add_param(name, description, "boolean", required);
}

tool_builder& tool_builder::with_array_param(const std::string& name, 
                                            const std::string& description,
                                            const std::string& item_type,
                                            bool required) {
    json param = {
        {"type", "array"},
        {"description", description},
        {"items", {
            {"type", item_type}
        }}
    };
    
    parameters_["properties"][name] = param;
    
    if (required) {
        required_params_.push_back(name);
    }
    
    return *this;
}

tool_builder& tool_builder::with_object_param(const std::string& name, 
                                             const std::string& description,
                                             const json& properties,
                                             bool required) {
    json param = {
        {"type", "object"},
        {"description", description},
        {"properties", properties}
    };
    
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

tool tool_builder::build() const {
    tool t;
    t.name = name_;
    t.description = description_;
    
    // Create the parameters schema
    json schema = parameters_;
    schema["type"] = "object";;
    
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
    
    return t;
}

} // namespace mcp