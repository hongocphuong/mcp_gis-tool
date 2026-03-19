//  The MIT License
//
//  Copyright (C) 2025 Giuseppe Mastrangelo
//
//  Permission is hereby granted, free of charge, to any person obtaining
//  a copy of this software and associated documentation files (the
//  'Software'), to deal in the Software without restriction, including
//  without limitation the rights to use, copy, modify, merge, publish,
//  distribute, sublicense, and/or sell copies of the Software, and to
//  permit persons to whom the Software is furnished to do so, subject to
//  the following conditions:
//
//  The above copyright notice and this permission notice shall be
//  included in all copies or substantial portions of the Software.
//
//  THE SOFTWARE IS PROVIDED 'AS IS', WITHOUT WARRANTY OF ANY KIND,
//  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
//  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
//  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
//  CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
//  TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
//  SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

#include "PluginsLoader.h"

#include <algorithm>
#include <cctype>
#include <unordered_set>
#include <vector>

namespace vx::mcp {

    namespace {
        std::string ToLower(std::string value) {
            std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
                return static_cast<char>(std::tolower(c));
            });
            return value;
        }

        int BuildConfigPriority(const std::filesystem::path& path) {
            const std::string lowered = ToLower(path.string());
            if (lowered.find("\\debug\\") != std::string::npos || lowered.find("/debug/") != std::string::npos) {
                return 0;
            }
            if (lowered.find("\\relwithdebinfo\\") != std::string::npos ||
                lowered.find("/relwithdebinfo/") != std::string::npos) {
                return 1;
            }
            if (lowered.find("\\minsizerel\\") != std::string::npos || lowered.find("/minsizerel/") != std::string::npos) {
                return 2;
            }
            if (lowered.find("\\release\\") != std::string::npos || lowered.find("/release/") != std::string::npos) {
                return 3;
            }
            return 4;
        }
    }

    PluginsLoader::PluginsLoader() = default;

    PluginsLoader::~PluginsLoader() {
        UnloadPlugins();
    }

    bool PluginsLoader::LoadPlugins(const std::string& directory) {
        try {
            std::vector<std::filesystem::path> candidates;
            for (const auto& entry : std::filesystem::recursive_directory_iterator(directory)) {
                if (entry.is_regular_file()) {
                    std::string extension = entry.path().extension().string();

                    // Check if this is a shared library
#ifdef _WIN32
                    if (extension == ".dll")
#else
    #ifdef __APPLE__
                    if (extension == ".dylib" || extension == ".so")
    #else
                    if (extension == ".so")
    #endif
#endif
                    {
                        candidates.push_back(entry.path());
                    }
                }
            }

            std::sort(candidates.begin(), candidates.end(), [](const std::filesystem::path& a, const std::filesystem::path& b) {
                const std::string aName = ToLower(a.filename().string());
                const std::string bName = ToLower(b.filename().string());
                if (aName != bName) {
                    return aName < bName;
                }

                const int aPriority = BuildConfigPriority(a);
                const int bPriority = BuildConfigPriority(b);
                if (aPriority != bPriority) {
                    return aPriority < bPriority;
                }

                return a.string() < b.string();
            });

            std::unordered_set<std::string> loadedFileNames;
            for (const auto& candidate : candidates) {
                const std::string normalizedName = ToLower(candidate.filename().string());
                if (loadedFileNames.find(normalizedName) != loadedFileNames.end()) {
                    LOG(WARNING) << "Skipping duplicate plugin binary: " << candidate.string() << std::endl;
                    continue;
                }

                if (LoadPlugin(candidate.string())) {
                    loadedFileNames.insert(normalizedName);
                }
            }

            return true;
        } catch (const std::exception& ex) {
            LOG(ERROR) << "Error loading plugins: " << ex.what() << std::endl;
            return false;
        }
    }

    bool PluginsLoader::LoadPlugin(const std::string& path) {
        PluginEntry entry;
        entry.path = path;

        // Load the shared library
#ifdef _WIN32
        entry.handle = LoadLibraryA(path.c_str());
        if (!entry.handle) {
            DWORD error = GetLastError();
            char errorMsg[256] = {0};
            FormatMessageA(
                    FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                    nullptr,
                    error,
                    MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                    errorMsg,
                    sizeof(errorMsg),
                    nullptr
            );
            LOG(ERROR) << "Failed to load plugin: " << path
                       << " - Error " << error << ": " << errorMsg << std::endl;
            return false;
        }
        // Get function pointers
        entry.createFunc = (PluginAPI * (*)())GetProcAddress(entry.handle, "CreatePlugin");
        entry.destroyFunc = (void (*)(PluginAPI *))GetProcAddress(entry.handle, "DestroyPlugin");
#else
        entry.handle = dlopen(path.c_str(), RTLD_LAZY);
        if (!entry.handle) {
            LOG(ERROR) << "Failed to load plugin: " << path << " - " << dlerror() << std::endl;
            return false;
        }

        // Get function pointers
        entry.createFunc = (PluginAPI * (*)())dlsym(entry.handle, "CreatePlugin");
        entry.destroyFunc = (void (*)(PluginAPI *))dlsym(entry.handle, "DestroyPlugin");
#endif

        // Check if required functions were found
        if (!entry.createFunc || !entry.destroyFunc) {
            LOG(ERROR) << "Plugin does not export required functions: " << path << std::endl;

#ifdef _WIN32
            FreeLibrary(entry.handle);
#else
            dlclose(entry.handle);
#endif

            return false;
        }

        // Create plugin instance
        entry.instance = entry.createFunc();

        // Initialize the plugin
        if (!entry.instance->Initialize()) {
            LOG(ERROR) << "Plugin initialization failed: " << path << std::endl;
            entry.destroyFunc(entry.instance);

#ifdef _WIN32
            FreeLibrary(entry.handle);
#else
            dlclose(entry.handle);
#endif

            return false;
        }

        // Add to a plugin list
        m_plugins.push_back(entry);
        LOG(INFO) << "Loaded plugin: " << entry.instance->GetName()
                  << " v" << entry.instance->GetVersion() << std::endl;

        return true;
    }

    void PluginsLoader::UnloadPlugins() {
        for (auto& entry : m_plugins) {
            UnloadPlugin(entry);
        }
        m_plugins.clear();
    }

    void PluginsLoader::UnloadPlugin(PluginEntry& entry) {
        if (entry.instance) {
            // Shutdown the plugin
            entry.instance->Shutdown();

            // Destroy the plugin instance
            entry.destroyFunc(entry.instance);
            entry.instance = nullptr;
        }

        // Unload the library
        if (entry.handle) {
#ifdef _WIN32
            FreeLibrary(entry.handle);
#else
            dlclose(entry.handle);
#endif
            entry.handle = nullptr;
        }
    }

    const std::vector<PluginEntry>& PluginsLoader::GetPlugins() const {
        return m_plugins;
    }

}