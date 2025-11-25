#pragma once
#include "Core.h"
#include "Graphics/Buffers/Vertex.h"
#include "Graphics/Models/Animation.h"
#include <assimp/scene.h>
#include <vector>
#include <string>
#include <memory>
#include <YAML-CPP/yaml.h>

namespace Boom {
    // Forward declarations
    using AssetID = uint64_t;
    enum class AssetType : uint8_t;

    /**
     * @brief Intermediate data structures to hold CPU-side loaded asset data
     * before GPU upload. These are populated on worker threads and consumed
     * on the main thread during GPU resource creation.
     */

    /**
     * @brief Holds raw texture data loaded from disk (CPU only)
     */
    struct TextureLoadContext {
        std::string filePath;
        std::vector<uint8_t> pixelData;
        int32_t width = 0;
        int32_t height = 0;
        int32_t channels = 0;
        bool isHDR = false;
        bool isCompressed = false;
        bool isFlipY = false;

        // Texture properties from assets.yaml
        bool isCompileAsCompressed = true;
        float quality = 0.95f;
        int32_t alphaThreshold = 127;
        int32_t mipLevel = 0;
        bool isGamma = false;
    };

    /**
     * @brief Holds parsed static model data (CPU only)
     */
    struct StaticModelLoadContext {
        std::string filePath;
        std::vector<MeshData<ShadedVert>> meshes;
        bool loadSuccess = false;
        std::string errorMessage;
    };

    /**
     * @brief Holds parsed skeletal model data (CPU only)
     */
    struct SkeletalModelLoadContext {
        std::string filePath;
        std::vector<MeshData<SkeletalVertex>> meshes;
        std::shared_ptr<Joint> rootJoint;
        std::vector<std::shared_ptr<struct AnimationClip>> clips;
        glm::mat4 globalTransform;
        uint32_t jointCount = 0;
        bool loadSuccess = false;
        std::string errorMessage;
    };

    /**
     * @brief Union type for any asset load context
     */
    struct AssetLoadContext {
        AssetType type;
        AssetID uid;
        std::string name;
        std::string source;

        // Type-specific context (only one is valid at a time)
        std::unique_ptr<TextureLoadContext> textureContext;
        std::unique_ptr<StaticModelLoadContext> staticModelContext;
        std::unique_ptr<SkeletalModelLoadContext> skeletalModelContext;

        // For assets that don't need CPU loading (materials, prefabs, etc.)
        // Store YAML as string to avoid copy issues
        std::string yamlPropsStr;

        // Properties for models/textures
        bool hasJoints = false;

        AssetLoadContext() = default;
        AssetLoadContext(AssetLoadContext&&) = default;
        AssetLoadContext& operator=(AssetLoadContext&&) = default;

        // Disable copy
        AssetLoadContext(const AssetLoadContext&) = delete;
        AssetLoadContext& operator=(const AssetLoadContext&) = delete;
    };

} // namespace Boom
