#include "ToolRegistry.h"

#include <stdexcept>

using json = nlohmann::json;

void ToolRegistry::Register(const ToolDefinition& definition) {
    const std::string toolName = definition.meta.name;
    if (handlers_.find(toolName) != handlers_.end()) {
        throw std::invalid_argument("Duplicate tool registration: " + toolName);
    }

    tools_.push_back(definition.meta);
    handlers_[toolName] = definition.handler;
}

ToolResult ToolRegistry::Execute(const std::string& toolName, const json& arguments) const {
    const auto iterator = handlers_.find(toolName);
    if (iterator == handlers_.end()) {
        throw std::invalid_argument("Unsupported tool name for gis-tools plugin: " + toolName);
    }

    return iterator->second(arguments);
}

const std::vector<PluginTool>& ToolRegistry::GetTools() const {
    return tools_;
}
