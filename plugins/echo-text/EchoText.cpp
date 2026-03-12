#include <algorithm>
#include <cctype>
#include <string>

#include "PluginAPI.h"
#include "json.hpp"

using json = nlohmann::json;

static PluginTool methods[] = {
    {
        "echo_text",
        "Return the input text converted to uppercase.",
        R"({
            "$schema": "http://json-schema.org/draft-07/schema#",
            "type": "object",
            "properties": {
                "text": {
                    "type": "string",
                    "description": "Text to convert to uppercase."
                }
            },
            "required": ["text"],
            "additionalProperties": false
        })"
    }
};

const char* GetNameImpl() { return "echo-tools"; }
const char* GetVersionImpl() { return "1.0.0"; }
PluginType GetTypeImpl() { return PLUGIN_TYPE_TOOLS; }

int InitializeImpl() {
    return 1;
}

char* HandleRequestImpl(const char* req) {
    auto request = json::parse(req);

    std::string inputText = request["params"]["arguments"]["text"].get<std::string>();
    std::transform(inputText.begin(), inputText.end(), inputText.begin(),
        [](unsigned char ch) { return static_cast<char>(std::toupper(ch)); });

    json responseContent;
    responseContent["type"] = "text";
    responseContent["text"] = inputText;

    json response;
    response["content"] = json::array();
    response["content"].push_back(responseContent);
    response["isError"] = false;

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
    return sizeof(methods) / sizeof(methods[0]);
}

const PluginTool* GetToolImpl(int index) {
    if (index < 0 || index >= GetToolCountImpl()) return nullptr;
    return &methods[index];
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
