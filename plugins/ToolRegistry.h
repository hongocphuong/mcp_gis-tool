#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "PluginAPI.h"
#include "json.hpp"

struct ToolResult {
    std::string text;
    nlohmann::json data;
};

using ToolHandler = ToolResult(*)(const nlohmann::json& arguments);

struct ToolDefinition {
    PluginTool meta;
    ToolHandler handler;
};

class ToolRegistry {
public:
    void Register(const ToolDefinition& definition);
    ToolResult Execute(const std::string& toolName, const nlohmann::json& arguments) const;
    const std::vector<PluginTool>& GetTools() const;

private:
    std::vector<PluginTool> tools_;
    std::unordered_map<std::string, ToolHandler> handlers_;
};
