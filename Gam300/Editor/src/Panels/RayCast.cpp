#include "RayCast.h"
#include "Context/Context.h"
#include "ECS/ECS.hpp"
#include "Auxiliaries/Assets.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>

namespace EditorUI {

    RayCast::RayCast(Boom::AppContext* context)
        : m_Context(context) {
    }

    entt::entity RayCast::CastRayFromScreen(float screenX, float screenY,
        const glm::mat4& viewMatrix,
        const glm::mat4& projectionMatrix,
        const glm::vec3& cameraPosition,
        const glm::vec2& viewportSize) {

        if (!m_Context) return entt::null;

        // Convert screen coordinates to world space ray
        glm::vec3 rayDirection = ScreenToWorldRay(screenX, screenY, viewMatrix, projectionMatrix, viewportSize);

        // Use provided camera position
        glm::vec3 rayOrigin = cameraPosition;

        // Find the closest hit
        RayHit hit = GetClosestHit(rayOrigin, rayDirection);

        return hit.entity;
    }

    RayCast::RayHit RayCast::GetClosestHit(const glm::vec3& rayOrigin,
        const glm::vec3& rayDirection) {
        RayHit closestHit;
        closestHit.distance = FLT_MAX;

        if (!m_Context) return closestHit;

        auto& registry = m_Context->scene;

        // Iterate through all entities with TransformComponent and ModelComponent (renderable)
        auto view = registry.view<Boom::TransformComponent, Boom::ModelComponent>();

        for (auto entity : view) {
            // Perform ray intersection test
            float hitDistance;
            if (RayIntersectsEntity(entity, rayOrigin, rayDirection, hitDistance)) {
                if (hitDistance < closestHit.distance) {
                    closestHit.entity = entity;
                    closestHit.distance = hitDistance;
                    closestHit.hit = true;
                    closestHit.point = rayOrigin + rayDirection * hitDistance;
                }
            }
        }

        return closestHit;
    }

    bool RayCast::RayIntersectsEntity(entt::entity entity,
        const glm::vec3& rayOrigin,
        const glm::vec3& rayDirection,
        float& hitDistance) {

        if (!m_Context) return false;

        auto& registry = m_Context->scene;

        if (!registry.all_of<Boom::TransformComponent, Boom::ModelComponent>(entity)) {
            return false;
        }

        // Get entity's bounding box in world space
        glm::vec3 aabbMin, aabbMax;
        GetEntityAABB(entity, aabbMin, aabbMax);

        // Perform AABB intersection test and get actual hit distance
        return RayAABBIntersection(rayOrigin, rayDirection, aabbMin, aabbMax, hitDistance);
    }

    glm::mat4 RayCast::TransformToMatrix(const Boom::Transform3D& transform) {
        // Convert Transform3D to world matrix
        glm::mat4 matrix = glm::mat4(1.0f);

        // Apply translation
        matrix = glm::translate(matrix, transform.translate);

        // Apply rotation (assuming rotate is in degrees and represents Euler angles)
        matrix = glm::rotate(matrix, glm::radians(transform.rotate.x), glm::vec3(1.0f, 0.0f, 0.0f));
        matrix = glm::rotate(matrix, glm::radians(transform.rotate.y), glm::vec3(0.0f, 1.0f, 0.0f));
        matrix = glm::rotate(matrix, glm::radians(transform.rotate.z), glm::vec3(0.0f, 0.0f, 1.0f));

        // Apply scale
        matrix = glm::scale(matrix, transform.scale);

        return matrix;
    }

    void RayCast::GetEntityAABB(entt::entity entity, glm::vec3& aabbMin, glm::vec3& aabbMax) {
        if (!m_Context) return;

        auto& registry = m_Context->scene;
        const auto& transformComp = registry.get<Boom::TransformComponent>(entity);
        const auto& modelComp = registry.get<Boom::ModelComponent>(entity);

        // Convert Transform3D to world matrix
        glm::mat4 worldMatrix = TransformToMatrix(transformComp.transform);

        // Try to get model-specific bounds from asset registry
        glm::vec3 modelMin, modelMax;
        if (GetModelBounds(modelComp, modelMin, modelMax)) {
            // Use actual model bounds (same as before)
            std::vector<glm::vec3> corners = {
                glm::vec3(modelMin.x, modelMin.y, modelMin.z),
                glm::vec3(modelMax.x, modelMin.y, modelMin.z),
                glm::vec3(modelMin.x, modelMax.y, modelMin.z),
                glm::vec3(modelMax.x, modelMax.y, modelMin.z),
                glm::vec3(modelMin.x, modelMin.y, modelMax.z),
                glm::vec3(modelMax.x, modelMin.y, modelMax.z),
                glm::vec3(modelMin.x, modelMax.y, modelMax.z),
                glm::vec3(modelMax.x, modelMax.y, modelMax.z)
            };

            aabbMin = glm::vec3(FLT_MAX);
            aabbMax = glm::vec3(-FLT_MAX);

            for (const auto& corner : corners) {
                glm::vec3 worldCorner = worldMatrix * glm::vec4(corner, 1.0f);
                aabbMin = glm::min(aabbMin, worldCorner);
                aabbMax = glm::max(aabbMax, worldCorner);
            }
        }
        else {
            // Fallback: use a larger scale-based AABB that better encompasses typical models
            glm::vec3 scale = transformComp.transform.scale;

            // Use a more generous base size - many models are larger than 2x2x2 units
            float baseSize = 5.0f; // Adjust this based on your typical model sizes
            glm::vec3 localMin = glm::vec3(-baseSize) * scale;
            glm::vec3 localMax = glm::vec3(baseSize) * scale;

            // Transform AABB corners to world space
            std::vector<glm::vec3> corners = {
                glm::vec3(localMin.x, localMin.y, localMin.z),
                glm::vec3(localMax.x, localMin.y, localMin.z),
                glm::vec3(localMin.x, localMax.y, localMin.z),
                glm::vec3(localMax.x, localMax.y, localMin.z),
                glm::vec3(localMin.x, localMin.y, localMax.z),
                glm::vec3(localMax.x, localMin.y, localMax.z),
                glm::vec3(localMin.x, localMax.y, localMax.z),
                glm::vec3(localMax.x, localMax.y, localMax.z)
            };

            aabbMin = glm::vec3(FLT_MAX);
            aabbMax = glm::vec3(-FLT_MAX);

            for (const auto& corner : corners) {
                glm::vec3 worldCorner = worldMatrix * glm::vec4(corner, 1.0f);
                aabbMin = glm::min(aabbMin, worldCorner);
                aabbMax = glm::max(aabbMax, worldCorner);
            }
        }

        // Expand AABB slightly to make selection easier
        const float expandAmount = 0.1f;
        aabbMin -= glm::vec3(expandAmount);
        aabbMax += glm::vec3(expandAmount);
    }

    bool RayCast::GetModelBounds(const Boom::ModelComponent& modelComp, glm::vec3& min, glm::vec3& max) {
        if (!m_Context || modelComp.modelID == EMPTY_ASSET) {
            return false;
        }

        // Try to get the model asset from the asset registry
        auto& assetRegistry = *m_Context->assets;

        // Check if the model asset exists and has data
        auto* modelAsset = assetRegistry.TryGet<ModelAsset>(modelComp.modelID);
        if (!modelAsset || !modelAsset->data) {
            return false;
        }

        // Try to cast to StaticModel first (most common case)
        auto staticModel = std::dynamic_pointer_cast<Boom::StaticModel>(modelAsset->data);
        if (staticModel) {
            return CalculateMeshBounds(staticModel, min, max);
        }

        // Try SkeletalModel as fallback
        auto skeletalModel = std::dynamic_pointer_cast<Boom::SkeletalModel>(modelAsset->data);
        if (skeletalModel) {
            return CalculateSkeletalMeshBounds(skeletalModel, min, max);
        }

        return false;
    }

    // Add this new helper method to your RayCast class
    bool RayCast::CalculateMeshBounds(std::shared_ptr<Boom::StaticModel> model, glm::vec3& min, glm::vec3& max) {
        const auto& meshData = model->GetMeshData();

        if (meshData.empty()) {
            return false;
        }

        min = glm::vec3(FLT_MAX);
        max = glm::vec3(-FLT_MAX);
        bool foundVertices = false;

        // Iterate through all submeshes and find the overall bounds
        for (const auto& mesh : meshData) {
            for (const auto& vertex : mesh.vtx) {
                min = glm::min(min, vertex.pos);
                max = glm::max(max, vertex.pos);
                foundVertices = true;
            }
        }

        return foundVertices;
    }

    // Add this new helper method for skeletal models (if needed)
    bool RayCast::CalculateSkeletalMeshBounds(std::shared_ptr<Boom::SkeletalModel> model, glm::vec3& min, glm::vec3& max) {
        // For skeletal models, you might need to access the mesh data differently
        // This is a placeholder - you'll need to adapt based on your SkeletalModel implementation

        // If SkeletalModel also has GetMeshData() or similar, use it
        // Otherwise, you might need to access the internal mesh data differently

        // For now, return false to use fallback bounds
        return false;
    }

    glm::vec3 RayCast::ScreenToWorldRay(float screenX, float screenY,
        const glm::mat4& viewMatrix,
        const glm::mat4& projectionMatrix,
        const glm::vec2& viewportSize) {

        // Convert to normalized device coordinates
        float x = (2.0f * screenX) / viewportSize.x - 1.0f;
        float y = 1.0f - (2.0f * screenY) / viewportSize.y;

        // Convert to homogeneous clip coordinates
        glm::vec4 rayClip = glm::vec4(x, y, -1.0f, 1.0f);

        // Convert to eye coordinates
        glm::vec4 rayEye = glm::inverse(projectionMatrix) * rayClip;
        rayEye = glm::vec4(rayEye.x, rayEye.y, -1.0f, 0.0f);

        // Convert to world coordinates
        glm::vec3 rayWorld = glm::vec3(glm::inverse(viewMatrix) * rayEye);
        rayWorld = glm::normalize(rayWorld);

        return rayWorld;
    }

    bool RayCast::RayAABBIntersection(const glm::vec3& rayOrigin,
        const glm::vec3& rayDirection,
        const glm::vec3& aabbMin,
        const glm::vec3& aabbMax,
        float& t) {

        glm::vec3 invDir = 1.0f / rayDirection;

        // Calculate intersections with slab planes
        glm::vec3 t1 = (aabbMin - rayOrigin) * invDir;
        glm::vec3 t2 = (aabbMax - rayOrigin) * invDir;

        // Find near and far intersections for each axis
        glm::vec3 tmin = glm::min(t1, t2);
        glm::vec3 tmax = glm::max(t1, t2);

        // Find the largest tmin and smallest tmax
        float tmin_val = glm::max(glm::max(tmin.x, tmin.y), tmin.z);
        float tmax_val = glm::min(glm::min(tmax.x, tmax.y), tmax.z);

        // Check if intersection is valid and get the actual hit distance
        if (tmax_val >= tmin_val && tmax_val >= 0) {
            t = tmin_val > 0 ? tmin_val : tmax_val; // Use the closest positive intersection
            return true;
        }

        return false;
    }

} // namespace EditorUI