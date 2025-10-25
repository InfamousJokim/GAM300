#pragma once
#include "ECS/ECS.hpp"
#include "Common/YAML.h"
#include "SerializationRegistry.h"
#include <fstream>

namespace Boom
{
    /**
     * High-level serialization interface that uses the SerializationRegistry.
     * This class is now much simpler and delegates to the registry.
     */
    struct BOOM_API DataSerializer
    {
        // Version tracking for backward compatibility
        static constexpr const char* SERIALIZATION_VERSION = "1.0";

        // ===== ENTITY SERIALIZATION =====
        void Serialize(EntityRegistry& scene, const std::string& path)
        {
            YAML::Emitter emitter;
            emitter << YAML::BeginMap;

            // Version info
            emitter << YAML::Key << "Version" << YAML::Value << SERIALIZATION_VERSION;

            // Entities
            emitter << YAML::Key << "ENTITIES" << YAML::Value << YAML::BeginSeq;

            for (auto entt : scene.view<entt::entity>())
            {
                emitter << YAML::BeginMap;
                SerializationRegistry::Instance().SerializeAllComponents(emitter, scene, entt);
                emitter << YAML::EndMap;
            }

            emitter << YAML::EndSeq;
            emitter << YAML::EndMap;

            // Write to file
            std::ofstream file(path);
            if (!file.is_open())
            {
                BOOM_ERROR("[DataSerializer] Failed to open file for writing: {}", path);
                return;
            }

            file << emitter.c_str();
            file.close();

            BOOM_INFO("[DataSerializer] Serialized entities to: {}", path);
        }

        void Deserialize(EntityRegistry& scene, AssetRegistry& assets, const std::string& path)
        {
            try
            {
                auto root = YAML::LoadFile(path);

                // Check version (for future compatibility)
                if (root["Version"])
                {
                    std::string version = root["Version"].as<std::string>();
                    BOOM_INFO("[DataSerializer] Loading scene version: {}", version);

                    // Future: Handle version migration here
                }

                const auto& nodes = root["ENTITIES"];
                if (!nodes)
                {
                    BOOM_ERROR("[DataSerializer] No ENTITIES node found in: {}", path);
                    return;
                }

                // Create a dummy entity (EnTT quirk - entity 0 is reserved)
                [[maybe_unused]] auto _ = scene.create();

                for (const auto& node : nodes)
                {
                    EntityID entity = scene.create();
                    SerializationRegistry::Instance().DeserializeAllComponents(
                        node, scene, entity, assets
                    );
                }

                BOOM_INFO("[DataSerializer] Deserialized {} entities from: {}",
                    nodes.size(), path);
            }
            catch (const YAML::Exception& e)
            {
                std::cout << e.what() << std::endl;

            }
        }

        // ===== ASSET SERIALIZATION =====
        void Serialize(AssetRegistry& registry, const std::string& path)
        {
            YAML::Emitter emitter;
            emitter << YAML::BeginMap;

            // Version info
            emitter << YAML::Key << "Version" << YAML::Value << SERIALIZATION_VERSION;

            // Assets
            emitter << YAML::Key << "ASSETS" << YAML::Value << YAML::BeginSeq;

            registry.View([&](Asset* asset)
                {
                    std::string typeName(magic_enum::enum_name(asset->type));

                    emitter << YAML::BeginMap;
                    emitter << YAML::Key << "Type" << YAML::Value << typeName;
                    emitter << YAML::Key << "UID" << YAML::Value << asset->uid;
                    emitter << YAML::Key << "Name" << YAML::Value << asset->name;
                    emitter << YAML::Key << "Source" << YAML::Value << asset->source;

                    // Serialize properties using registry
                    SerializationRegistry::Instance().SerializeAssetProperties(emitter, asset);

                    emitter << YAML::EndMap;
                });

            emitter << YAML::EndSeq;
            emitter << YAML::EndMap;

            // Write to file
            std::ofstream file(path);
            if (!file.is_open())
            {
                BOOM_ERROR("[DataSerializer] Failed to open file for writing: {}", path);
                return;
            }

            file << emitter.c_str();
            file.close();

            BOOM_INFO("[DataSerializer] Serialized assets to: {}", path);
        }

        void Deserialize(AssetRegistry& registry, const std::string& path)
        {
            try
            {
                auto root = YAML::LoadFile(path);
                // Check version
                if (root["Version"])
                {
                    std::string version = root["Version"].as<std::string>();
                    BOOM_INFO("[DataSerializer] Loading assets version: {}", version);
                }
                const auto& nodes = root["ASSETS"];
                if (!nodes)
                {
                    BOOM_ERROR("[DataSerializer] No ASSETS node found in: {}", path);
                    return;
                }

                int successCount = 0;
                int failCount = 0;

                for (const auto& node : nodes)
                {
                    try
                    {
                        auto props = node["Properties"];
                        auto uid = node["UID"].as<AssetID>();
                        auto name = node["Name"].as<std::string>();
                        auto source = node["Source"].as<std::string>();
                        auto typeName = node["Type"].as<std::string>();
                        auto opt = magic_enum::enum_cast<AssetType>(typeName);
                        auto type = opt.has_value() ? opt.value() : AssetType::UNKNOWN;

                        BOOM_INFO("[DataSerializer] Processing asset UID={}, Type={}", uid, typeName);

                        // Deserialize using registry
                        Asset* asset = SerializationRegistry::Instance().DeserializeAsset(
                            registry, type, uid, source, props
                        );

                        if (asset)
                        {
                            asset->source = source;
                            asset->name = name;

                            // CRITICAL: Verify the asset loaded properly
                            if (type == AssetType::MODEL) {
                                auto* modelAsset = static_cast<ModelAsset*>(asset);
                                if (!modelAsset->data) {
                                    BOOM_ERROR("[DataSerializer] Model '{}' has null data after loading from '{}'",
                                        name, source);
                                    failCount++;
                                    continue;
                                }
                            }
                            successCount++;
                        }
                        else
                        {
                            BOOM_ERROR("[DataSerializer] Failed to deserialize asset '{}' (UID={})",
                                name, uid);
                            failCount++;
                        }
                    }
                    catch (const YAML::Exception& e)
                    {
                        //BOOM_ERROR("[DataSerializer] YAML error while parsing asset: {}", e.what());
                        BOOM_ERROR("[DataSerializer] YAML error while parsing asset: {}", std::string(e.what()));
                        failCount++;
                    }
                    catch (const std::exception& e)
                    {
                        //BOOM_ERROR("[DataSerializer] Error while loading asset: {}", e.what());
                        BOOM_ERROR("[DataSerializer] Error while loading asset: {}", std::string(e.what()));
                        failCount++;
                    }
                    catch (...)
                    {
                        BOOM_ERROR("[DataSerializer] Unknown error while loading asset");
                        failCount++;
                    }
                }

                BOOM_INFO("[DataSerializer] Deserialized {} assets from: {} ({} succeeded, {} failed)",
                    nodes.size(), path, successCount, failCount);
            }
            catch (const YAML::Exception& e)
            {
                //BOOM_ERROR("[DataSerializer] Failed to load YAML file '{}': {}", path, e.what());
                BOOM_ERROR("[DataSerializer] Failed to load YAML file '{}': {}", path, std::string(e.what()));
            }
            catch (const std::exception& e)
            {
                //BOOM_ERROR("[DataSerializer] Error loading asset file '{}': {}", path, e.what());
                BOOM_ERROR("[DataSerializer] Error loading asset file '{}': {}", path, std::string(e.what()));
            }
        }
    };
}