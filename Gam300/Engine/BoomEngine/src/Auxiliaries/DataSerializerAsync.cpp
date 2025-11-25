#include "Core.h"
#include "Auxiliaries/DataSerializer.h"
#include "Auxiliaries/AsyncAssetLoader.h"
#include "Auxiliaries/AssetLoadContext.h"
#include "Auxiliaries/Assets.h"
#include <thread>
#include <chrono>

namespace Boom {

void DataSerializer::DeserializeAsync(AssetRegistry& registry, const std::string& path, GLFWwindow* win, size_t numThreads) {
    try {
        auto root = YAML::LoadFile(path);

        if (root["Version"]) {
            std::string version = root["Version"].as<std::string>();
            BOOM_INFO("[DataSerializer] Loading assets version: {}", version);
        }

        const auto& nodes = root["ASSETS"];
        if (!nodes) {
            BOOM_ERROR("[DataSerializer] No ASSETS node found in: {}", path);
            return;
        }

        BOOM_INFO("[DataSerializer] Starting multithreaded asset loading ({} assets, {} threads)",
            nodes.size(), numThreads == 0 ? "auto" : std::to_string(numThreads));

        // Create async loader
        AsyncAssetLoader loader(numThreads);

        // Phase 1: Queue all assets for CPU loading (parallel)
        for (const auto& node : nodes) {
            try {
                auto uid = node["UID"].as<AssetID>();
                auto name = node["Name"].as<std::string>();
                auto source = node["Source"].as<std::string>();
                auto typeName = node["Type"].as<std::string>();
                auto opt = magic_enum::enum_cast<AssetType>(typeName);
                auto type = opt.has_value() ? opt.value() : AssetType::UNKNOWN;
                auto props = node["Properties"];

                // Only queue assets that need CPU loading (textures, models)
                if (type == AssetType::TEXTURE) {
                    loader.QueueAsset(type, uid, name, source, false);
                }
                else if (type == AssetType::MODEL) {
                    // Determine if skeletal or static
                    bool hasJoints = false;
                    if (props["HasJoints"]) {
                        hasJoints = props["HasJoints"].as<bool>();
                    }
                    else if (props["hasJoints"]) {
                        hasJoints = props["hasJoints"].as<bool>();
                    }
                    else {
                        // Fallback: Auto-detect joints by checking the file
                        hasJoints = Model::CheckForJoints(source);
                    }
                    loader.QueueAsset(type, uid, name, source, hasJoints);
                }
            }
            catch (const YAML::Exception& e) {
                BOOM_ERROR("[DataSerializer] YAML error parsing asset: {}", std::string(e.what()));
            }
            catch (const std::exception& e) {
                BOOM_ERROR("[DataSerializer] Error parsing asset: {}", std::string(e.what()));
            }
        }

        // Phase 2: Wait for all CPU loading to complete (with progress rendering)
        BOOM_INFO("[DataSerializer] Waiting for CPU loading phase to complete...");

        // Render loading progress while waiting
        while (!loader.IsComplete()) {
            float progress = loader.GetProgress();
            AppWindow::RenderLoading(win, progress * 0.8f); // 0-80% for CPU loading
            std::this_thread::sleep_for(std::chrono::milliseconds(16)); // ~60 FPS
        }

        auto loadedContexts = loader.WaitForAll();

        BOOM_INFO("[DataSerializer] CPU loading complete. Starting GPU upload phase...");

        // Phase 3: Upload to GPU on main thread and create remaining assets
        int successCount = 0;
        int failCount = 0;
        int assetIndex = 0;

        // Create a lookup for loaded contexts
        std::unordered_map<AssetID, AssetLoadContext*> contextMap;
        for (auto& context : loadedContexts) {
            contextMap[context.uid] = &context;
        }

        for (const auto& node : nodes) {
            try {
                auto props = node["Properties"];
                auto uid = node["UID"].as<AssetID>();
                auto name = node["Name"].as<std::string>();
                auto source = node["Source"].as<std::string>();
                auto typeName = node["Type"].as<std::string>();
                auto opt = magic_enum::enum_cast<AssetType>(typeName);
                auto type = opt.has_value() ? opt.value() : AssetType::UNKNOWN;

                Asset* asset = nullptr;

                // Check if this asset was pre-loaded
                auto contextIt = contextMap.find(uid);
                if (contextIt != contextMap.end()) {
                    auto& context = *contextIt->second;

                    // Upload to GPU using pre-loaded context
                    if (type == AssetType::TEXTURE && context.textureContext) {
                        asset = static_cast<Asset*>(
                            registry.AddTextureFromContext(uid, source, *context.textureContext).get()
                        );
                    }
                    else if (type == AssetType::MODEL) {
                        if (context.skeletalModelContext && context.skeletalModelContext->loadSuccess) {
                            asset = static_cast<Asset*>(
                                registry.AddSkeletalModelFromContext(uid, source, *context.skeletalModelContext).get()
                            );
                        }
                        else if (context.staticModelContext && context.staticModelContext->loadSuccess) {
                            asset = static_cast<Asset*>(
                                registry.AddModelFromContext(uid, source, *context.staticModelContext).get()
                            );
                        }
                        else {
                            BOOM_ERROR("[DataSerializer] Model '{}' failed to load on worker thread", name);
                            failCount++;
                            continue;
                        }
                    }
                }
                else {
                    // Asset wasn't pre-loaded, use normal synchronous loading
                    asset = SerializationRegistry::Instance().DeserializeAsset(
                        registry, type, uid, source, props
                    );
                }

                if (asset) {
                    asset->source = source;
                    asset->name = name;

                    // Verify model loaded properly
                    if (type == AssetType::MODEL) {
                        auto* modelAsset = static_cast<ModelAsset*>(asset);
                        if (!modelAsset->data) {
                            BOOM_ERROR("[DataSerializer] Model '{}' (UID={}) has null data after loading", name, uid);
                            failCount++;
                            continue;
                        }
                    }
                    successCount++;
                }
                else {
                    BOOM_ERROR("[DataSerializer] Failed to deserialize asset '{}' (UID={})", name, uid);
                    failCount++;
                }

                // Render progress (80-100% for GPU upload)
                assetIndex++;
                float gpuProgress = 0.8f + (0.2f * ((float)assetIndex / (float)nodes.size()));
                AppWindow::RenderLoading(win, gpuProgress);
            }
            catch (const YAML::Exception& e) {
                BOOM_ERROR("[DataSerializer] YAML error while processing asset: {}", std::string(e.what()));
                failCount++;
            }
            catch (const std::exception& e) {
                BOOM_ERROR("[DataSerializer] Error while loading asset: {}", std::string(e.what()));
                failCount++;
            }
            catch (...) {
                BOOM_ERROR("[DataSerializer] Unknown error while loading asset");
                failCount++;
            }
        }

        BOOM_INFO("[DataSerializer] Asset loading complete: {} succeeded, {} failed", successCount, failCount);

        // Log summary of loaded models (for debugging)
        int staticModels = 0;
        int skeletalModels = 0;
        registry.View([&](Asset* asset) {
            if (asset->type == AssetType::MODEL) {
                auto* modelAsset = static_cast<ModelAsset*>(asset);
                if (modelAsset->hasJoints) {
                    skeletalModels++;
                }
                else {
                    staticModels++;
                }
            }
        });
        BOOM_INFO("[DataSerializer] Loaded {} static models, {} skeletal models", staticModels, skeletalModels);
    }
    catch (const YAML::Exception& e) {
        BOOM_ERROR("[DataSerializer] Failed to load YAML file '{}': {}", path, std::string(e.what()));
    }
    catch (const std::exception& e) {
        BOOM_ERROR("[DataSerializer] Error loading asset file '{}': {}", path, std::string(e.what()));
    }
}

} // namespace Boom
