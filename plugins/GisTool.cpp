#include <cstring>
#include <exception>
#include <mutex>
#include <string>

#include "PluginAPI.h"
#include "ToolRegistry.h"
#include "json.hpp"
#include "Aggregation/AggregationTool.h"
#include "Boolean/BooleanTools.h"
#include "Classification/ClassificationTool.h"
#include "Measurement/MeasurementTools.h"
#include "Transformation/TransformationTools.h"

using json = nlohmann::json;

static ToolRegistry registry;
static std::once_flag gToolsRegisterFlag;

static void EnsureToolsRegistered() {
    std::call_once(gToolsRegisterFlag, []() {
        RegisterAggregationTools(registry);
        RegisterBooleanTools(registry);
        RegisterClassificationTools(registry);
        RegisterMeasurementTools(registry);
        RegisterTransformationTools(registry);
    });
}

// Allocates a heap buffer for the response string.
// The caller (PluginAPI consumer) is responsible for freeing this memory with delete[].
static char* BuildResponseBuffer(const json& response) {
    const std::string result = response.dump();
    char* buffer = new char[result.length() + 1];
#ifdef _WIN32
    strcpy_s(buffer, result.length() + 1, result.c_str());
#else
    strcpy(buffer, result.c_str());
#endif
    return buffer;
}

const char* GetNameImpl() { return "gis-tools"; }
const char* GetVersionImpl() { return "1.7.0"; }
PluginType GetTypeImpl() { return PLUGIN_TYPE_TOOLS; }

int InitializeImpl() {
    try {
        EnsureToolsRegistered();
        return 1;
    } catch (...) {
        return 0;
    }
}

char* HandleRequestImpl(const char* req) {
    json response;
    response["content"] = json::array();

    try {
        EnsureToolsRegistered();

        if (req == nullptr || req[0] == '\0') {
            throw std::runtime_error("Request payload is empty");
        }

        const json request = json::parse(req, nullptr, false);
        if (request.is_discarded()) {
            throw std::runtime_error("Malformed JSON payload");
        }

        // Be tolerant with client variations: some callers send full JSON-RPC
        // wrapper, others may pass params directly.
        const json* params = nullptr;
        if (request.is_object() && request.contains("params") && request["params"].is_object()) {
            params = &request["params"];
        } else if (request.is_object()) {
            params = &request;
        } else {
            throw std::runtime_error("Request must be a JSON object");
        }

        std::string toolName;
        if (params->contains("name") && (*params)["name"].is_string()) {
            toolName = (*params)["name"].get<std::string>();
        } else if (params->contains("tool") && (*params)["tool"].is_string()) {
            toolName = (*params)["tool"].get<std::string>();
        } else {
            throw std::runtime_error("Missing tool name in params.name");
        }

        json arguments = json::object();
        if (params->contains("arguments") && (*params)["arguments"].is_object()) {
            arguments = (*params)["arguments"];
        } else if (params->contains("args") && (*params)["args"].is_object()) {
            arguments = (*params)["args"];
        }

        const ToolResult result = registry.Execute(toolName, arguments);

        response["content"].push_back({
            {"type", "text"},
            {"text", result.text}
        });

        // Keep structured data as text for broad MCP client compatibility.
        response["content"].push_back({
            {"type", "text"},
            {"text", result.data.dump()}
        });

        response["isError"] = false;
    } catch (const std::exception& ex) {
        response["content"].push_back({
            {"type", "text"},
            {"text", std::string("Invalid request: ") + ex.what()}
        });
        response["isError"] = true;
    } catch (...) {
        response["content"].push_back({
            {"type", "text"},
            {"text", "Invalid request: unknown error"}
        });
        response["isError"] = true;
    }

    return BuildResponseBuffer(response);
}

void ShutdownImpl() {
}

int GetToolCountImpl() {
    try {
        EnsureToolsRegistered();
        return static_cast<int>(registry.GetTools().size());
    } catch (...) {
        return 0;
    }
}

const PluginTool* GetToolImpl(int index) {
    try {
        EnsureToolsRegistered();
        const auto& tools = registry.GetTools();
        if (index < 0 || index >= static_cast<int>(tools.size())) {
            return nullptr;
        }
        return &tools[index];
    } catch (...) {
        return nullptr;
    }
}

static PluginAPI plugin = {
    GetNameImpl,
    GetVersionImpl,
    GetTypeImpl,
    InitializeImpl,
    HandleRequestImpl,
    ShutdownImpl,
    GetToolCountImpl,
    GetToolImpl,
    nullptr,
    nullptr,
    nullptr,
    nullptr
};

extern "C" PLUGIN_API PluginAPI* CreatePlugin() {
    return &plugin;
}

extern "C" PLUGIN_API void DestroyPlugin(PluginAPI*) {
    // Nothing to clean up for this plugin.
}
