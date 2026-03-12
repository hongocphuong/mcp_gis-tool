#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "PluginAPI.h"
#include "json.hpp"

using json = nlohmann::json;

struct ToolResult {
    std::string text;
    json data;
};

using ToolHandler = ToolResult(*)(const json& arguments);

struct ToolDefinition {
    PluginTool meta;
    ToolHandler handler;
};

class ToolRegistry {
public:
    void Register(const ToolDefinition& definition);
    ToolResult Execute(const std::string& toolName, const json& arguments) const;
    const std::vector<PluginTool>& GetTools() const;

private:
    std::vector<PluginTool> tools_;
    std::unordered_map<std::string, ToolHandler> handlers_;
};
