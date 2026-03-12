#include <exception>
#include <string>

#include "PluginAPI.h"
#include "ToolRegistry.h"
#include "json.hpp"
#include "tools/GeoJsonTools.h"
#include "tools/Wgs84Tools.h"

using json = nlohmann::json;

static ToolRegistry registry;
static bool gToolsRegistered = false;

static void EnsureToolsRegistered() {
    if (gToolsRegistered) {
        return;
    }

    RegisterWgs84Tools(registry);
    RegisterGeoJsonTools(registry);
    gToolsRegistered = true;
}

const char* GetNameImpl() { return "gis-tools"; }
const char* GetVersionImpl() { return "1.7.0"; }
PluginType GetTypeImpl() { return PLUGIN_TYPE_TOOLS; }

int InitializeImpl() {
    EnsureToolsRegistered();
    return 1;
}

char* HandleRequestImpl(const char* req) {
    json response;
    response["content"] = json::array();

    try {
        EnsureToolsRegistered();
        auto request = json::parse(req);
        const std::string toolName = request["params"]["name"].get<std::string>();
        const json arguments = request["params"].contains("arguments") ? request["params"]["arguments"] : json::object();

        const ToolResult result = registry.Execute(toolName, arguments);

        json textContent;
        textContent["type"] = "text";
        textContent["text"] = result.text;

        json jsonAsTextContent;
        jsonAsTextContent["type"] = "text";
        jsonAsTextContent["text"] = result.data.dump();

        response["content"].push_back(textContent);
        response["content"].push_back(jsonAsTextContent);

        response["isError"] = false;
    } catch (const std::exception& ex) {
        json responseContent;
        responseContent["type"] = "text";
        responseContent["text"] = std::string("Invalid request: ") + ex.what();

        response["content"].push_back(responseContent);
        response["isError"] = true;
    }

    std::string result = response.dump();
    char* buffer = new char[result.length() + 1];
#ifdef _WIN32
    strcpy_s(buffer, result.length() + 1, result.c_str());
#else
    strcpy(buffer, result.c_str());
#endif

    return buffer;
}

void ShutdownImpl() {
}

int GetToolCountImpl() {
    EnsureToolsRegistered();
    return static_cast<int>(registry.GetTools().size());
}

const PluginTool* GetToolImpl(int index) {
    EnsureToolsRegistered();
    if (index < 0 || index >= GetToolCountImpl()) return nullptr;
    return &registry.GetTools()[index];
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