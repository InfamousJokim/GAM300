#pragma once
#include <entt/entt.hpp>
#include <nlohmann/json.hpp>
#include "Graphics/Utilities/Data.h"
#include "Auxiliaries/Assets.h"
#include "Physics/Utilities.h"  
#include "BoomProperties.h"
#include "AI/BehaviourTree.h"


namespace Boom {
    using EntityRegistry = entt::registry;
    using EntityID = entt::entity;
    constexpr EntityID NENTT = entt::null;

    //for now include only important components instead of all
    //new includes need to add into enum and string_view and within ComponentSelector() in InspectorPanel.cpp
    enum class ComponentID : size_t {
        INFO, TRANSFORM, CAMERA, RIGIDBODY, COLLIDER,
        MODEL, ANIMATOR, DIRECT_LIGHT, POINT_LIGHT, SPOT_LIGHT,
        SOUND, SCRIPT,
        THIRD_PERSON_CAMERA,
        NAV_AGENT_COMPONENT,
        AI_COMPONENT,
        SPRITE,
        PAUSE_MENU_TAG,
        COUNT
    };
    constexpr std::string_view COMPONENT_NAMES[]{
        "Info",                 //0
        "Transform",            //1
        "Camera",               //2
        "Rigidbody",            //3
        "Collider",             //4
        "Model",                //5
        "Animator",             //6
        "Direct Light",         //7
        "Point Light",          //8
        "Spot Light",           //9
        "Sound",                //10
        "Script",               //11
        "Third Person Camera" , //12
        "Nav Agent Component",  //13
        "AI Component",         //14
        "Sprite",               //15
        "Pause Menu Tag"        //16
    };

    // transform component
    struct TransformComponent
    {
        BOOM_INLINE TransformComponent(const TransformComponent&) = default;
        BOOM_INLINE TransformComponent() = default;
        Transform3D transform;

        XPROPERTY_DEF
        ("TransformComponent", TransformComponent
            , obj_member<"Transform", &TransformComponent::transform>
        )
    };

    // camera component
    struct CameraComponent
    {
        BOOM_INLINE CameraComponent(const CameraComponent&) = default;
        BOOM_INLINE CameraComponent() = default;
        Camera3D camera;

        // CameraComponent
        XPROPERTY_DEF(
            "CameraComponent", CameraComponent,
            obj_member<"Camera", &CameraComponent::camera>
        )
    };

    struct EnttComponent {
        BOOM_INLINE EnttComponent(const EnttComponent&) = default;
        BOOM_INLINE EnttComponent() = default;
        std::string name = "Entity";

        // EnttComponent
        XPROPERTY_DEF(
            "EnttComponent", EnttComponent,
            obj_member<"name", &EnttComponent::name>
        )
    };

    struct MeshComponent
    {
        BOOM_INLINE MeshComponent(const MeshComponent&) = default;
        BOOM_INLINE MeshComponent() = default;
        Mesh3D mesh;

        // MeshComponent
        //XPROPERTY_DEF(
        //    "MeshComponent", MeshComponent,
        //    obj_member<"mesh", &MeshComponent::mesh>
        //)
    };

    struct RigidBodyComponent
    {
        BOOM_INLINE RigidBodyComponent(const RigidBodyComponent&) = default;
        BOOM_INLINE RigidBodyComponent() = default;
        RigidBody3D RigidBody;

        // RigidBodyComponent
        XPROPERTY_DEF(
            "RigidBodyComponent", RigidBodyComponent,
            obj_member<"RigidBody", &RigidBodyComponent::RigidBody>
        )
    };

    struct ColliderComponent
    {
        BOOM_INLINE ColliderComponent(const ColliderComponent&) = default;
        BOOM_INLINE ColliderComponent() = default;
        Collider3D Collider;

        // ColliderComponent
        XPROPERTY_DEF(
            "ColliderComponent", ColliderComponent,
            obj_member<"Collider", &ColliderComponent::Collider>
        )
    };

    ////Model Component
    struct ModelComponent {

        //using AssetID = uint64_t;

        AssetID modelID{ EMPTY_ASSET };
        AssetID materialID{ EMPTY_ASSET };
        std::string modelName;
        std::string materialName;
        std::string modelSource;
        std::string materialSource;

        XPROPERTY_DEF(
            "ModelComponent", ModelComponent,
            obj_member<"ModelID", &ModelComponent::modelID>,
            obj_member<"MaterialID", &ModelComponent::materialID>,
            obj_member<"ModelName", &ModelComponent::modelName>,
            obj_member<"MaterialName", &ModelComponent::materialName>,
            obj_member<"ModelSource", &ModelComponent::modelSource>,
            obj_member<"MaterialSource", &ModelComponent::materialSource>
        )

    };

    //Animator Component
    struct AnimatorComponent
    {
        BOOM_INLINE AnimatorComponent(const AnimatorComponent&) = default;
        BOOM_INLINE AnimatorComponent() = default;
        Animator3D animator;
        //std::vector<std::string> additionalAnimFiles;
    };

    struct SkyboxComponent {
        AssetID skyboxID{ EMPTY_ASSET };

        XPROPERTY_DEF(
            "SkyboxComponent", SkyboxComponent,
            obj_member<"SkyboxID", &SkyboxComponent::skyboxID>
        )
    };

    //helpful for encapsulating information about an entity
    //can be used for entity hierarchies (linked-list)
    struct InfoComponent {
        AssetID parent{ EMPTY_ASSET };
        std::string name{ "Entity" };
        AssetID uid{ RandomU64() };

        XPROPERTY_DEF(
            "InfoComponent", InfoComponent,
            obj_member<"Parent", &InfoComponent::parent>,
            obj_member<"Name", &InfoComponent::name>,
            obj_member<"UID", &InfoComponent::uid>
        )
    };
    BOOM_INLINE entt::entity FindEntityByName(entt::registry& reg, std::string_view name) {
        auto view = reg.view<const InfoComponent>();
        for (auto [e, info] : view.each()) {
            if (info.name == name) return e;
        }
        return entt::null;
    }

    // ========== HIERARCHY HELPER FUNCTIONS ==========
    // NOTE: Parent relationships use UID (not entity ID) for serialization stability

    // Get parent entity by UID lookup (returns entt::null if no parent)
    BOOM_INLINE entt::entity GetParentEntity(entt::registry& reg, entt::entity entity) {
        if (!reg.valid(entity) || !reg.all_of<InfoComponent>(entity)) {
            return entt::null;
        }
        const auto& info = reg.get<InfoComponent>(entity);
        AssetID parentUID = info.parent;

        if (parentUID == EMPTY_ASSET || parentUID == 0) {
            return entt::null;
        }

        // Find entity with matching UID
        auto view = reg.view<InfoComponent>();
        for (auto e : view) {
            if (view.get<InfoComponent>(e).uid == parentUID) {
                // Found parent
                return e;
            }
        }

        // Parent UID is set but entity not found - this is a warning condition
        BOOM_WARN("[GetParentEntity] Entity '{}' has parent UID {} but no matching entity found!",
                 info.name, parentUID);
        return entt::null;
    }

    // Get all children of an entity (by UID)
    BOOM_INLINE std::vector<entt::entity> GetChildren(entt::registry& reg, entt::entity parent) {
        std::vector<entt::entity> children;
        if (!reg.valid(parent) || !reg.all_of<InfoComponent>(parent)) return children;

        AssetID parentUID = reg.get<InfoComponent>(parent).uid;
        auto view = reg.view<InfoComponent>();
        for (auto [e, info] : view.each()) {
            if (info.parent == parentUID) {
                children.push_back(e);
            }
        }
        return children;
    }

    // Check if 'ancestor' is in the parent chain of 'entity' (prevents circular parenting)
    BOOM_INLINE bool HasAncestor(entt::registry& reg, entt::entity entity, entt::entity ancestor) {
        if (!reg.all_of<InfoComponent>(ancestor)) return false;

        AssetID ancestorUID = reg.get<InfoComponent>(ancestor).uid;
        entt::entity current = entity;
        int depth = 0;
        const int maxDepth = 1000; // Prevent infinite loops

        while (current != entt::null && depth < maxDepth) {
            if (reg.all_of<InfoComponent>(current)) {
                if (reg.get<InfoComponent>(current).uid == ancestorUID) {
                    return true;
                }
            }
            current = GetParentEntity(reg, current);
            depth++;
        }
        return false;
    }

    // Get world transform matrix (combines local transform with parent chain)
    BOOM_INLINE glm::mat4 GetWorldMatrix(entt::registry& reg, entt::entity entity) {
        if (!reg.valid(entity) || !reg.all_of<TransformComponent>(entity)) {
            return glm::mat4(1.0f);
        }

        const auto& transform = reg.get<TransformComponent>(entity);
        glm::mat4 localMatrix = transform.transform.Matrix();

        entt::entity parent = GetParentEntity(reg, entity);
        if (parent == entt::null) {
            return localMatrix;
        }

        // Recursively get parent's world matrix and combine
        glm::mat4 parentMatrix = GetWorldMatrix(reg, parent);
        return parentMatrix * localMatrix;
    }

    // Get world position from transform hierarchy
    BOOM_INLINE glm::vec3 GetWorldPosition(entt::registry& reg, entt::entity entity) {
        glm::mat4 worldMatrix = GetWorldMatrix(reg, entity);
        return glm::vec3(worldMatrix[3]);
    }

    // --- Utility: Decompose TRS matrix into translate / euler-deg / scale ---
    //     without normalizing away scale.
    BOOM_INLINE void DecomposeTRS(const glm::mat4& m,
        glm::vec3& outT,
        glm::vec3& outRDeg,
        glm::vec3& outS)
    {
        // Translation = 4th column
        outT = glm::vec3(m[3]);

        // Basis vectors (3x3 block columns)
        glm::vec3 col0 = glm::vec3(m[0]);
        glm::vec3 col1 = glm::vec3(m[1]);
        glm::vec3 col2 = glm::vec3(m[2]);

        // Scale = lengths of basis vectors
        outS.x = glm::length(col0);
        outS.y = glm::length(col1);
        outS.z = glm::length(col2);

        // Guard against zero scale
        glm::vec3 safeS = outS;
        if (safeS.x == 0.f) safeS.x = 1.f;
        if (safeS.y == 0.f) safeS.y = 1.f;
        if (safeS.z == 0.f) safeS.z = 1.f;

        // Remove scale from basis vectors to get rotation-only matrix
        glm::mat3 rotMat;
        rotMat[0] = col0 / safeS.x;
        rotMat[1] = col1 / safeS.y;
        rotMat[2] = col2 / safeS.z;

        glm::quat q = glm::quat_cast(rotMat);
        glm::vec3 eulerRad = glm::eulerAngles(q);
        outRDeg = glm::degrees(eulerRad);
    }



    // Helper to decompose world matrix back to transform (for gizmos)
    BOOM_INLINE void SetWorldMatrix(entt::registry& reg, entt::entity entity, const glm::mat4& worldMatrix) {
        if (!reg.valid(entity) || !reg.all_of<TransformComponent>(entity)) {
            return;
        }

        auto& transform = reg.get<TransformComponent>(entity);
        entt::entity parent = GetParentEntity(reg, entity);

        // Compute local = inverse(parentWorld) * world
        glm::mat4 localMatrix;
        if (parent == entt::null) {
            localMatrix = worldMatrix;
        }
        else {
            glm::mat4 parentMatrix = GetWorldMatrix(reg, parent);
            localMatrix = glm::inverse(parentMatrix) * worldMatrix;
        }

        glm::vec3 T, RDeg, S;
        DecomposeTRS(localMatrix, T, RDeg, S);

        transform.transform.translate = T;
        transform.transform.rotate = RDeg;
        transform.transform.scale = S;
    }




    // Set parent of an entity using UID (validates to prevent circular references)
// preserveWorldTransform: if true, adjusts local transform to keep same world position (Unity/Unreal behavior)
    BOOM_INLINE bool SetParent(entt::registry& reg,
        entt::entity entity,
        entt::entity newParent,
        bool preserveWorldTransform = true)
    {
        if (!reg.valid(entity) || !reg.all_of<InfoComponent>(entity)) {
            BOOM_WARN("[SetParent] Entity invalid or missing InfoComponent");
            return false;
        }

        // Prevent self-parenting
        if (entity == newParent) {
            BOOM_WARN("[SetParent] Cannot parent entity to itself");
            return false;
        }

        // Prevent circular parenting (entity can't be parent of its ancestor)
        if (newParent != entt::null && HasAncestor(reg, newParent, entity)) {
            BOOM_WARN("[SetParent] Circular reference prevented");
            return false;
        }

        auto& info = reg.get<InfoComponent>(entity);
        AssetID oldParent = info.parent;

        const bool hasTransform = reg.all_of<TransformComponent>(entity);

        // 1) Capture current WORLD matrix and original scale BEFORE changing parent
        glm::mat4 worldMatrix(1.0f);
        glm::vec3 originalLocalScale(1.0f);

        if (preserveWorldTransform && hasTransform) {
            auto& t = reg.get<TransformComponent>(entity);
            originalLocalScale = t.transform.scale;  // SAVE THIS FIRST!
            worldMatrix = GetWorldMatrix(reg, entity);

            BOOM_INFO("[SetParent] BEFORE - Entity '{}' local: pos({}, {}, {}), scale({}, {}, {})",
                info.name,
                t.transform.translate.x, t.transform.translate.y, t.transform.translate.z,
                t.transform.scale.x, t.transform.scale.y, t.transform.scale.z);

            glm::vec3 worldPos = GetWorldPosition(reg, entity);
            BOOM_INFO("[SetParent] World position: ({}, {}, {})", worldPos.x, worldPos.y, worldPos.z);
        }

        // 2) Update parent relationship (InfoComponent.parent stores UID)
        if (newParent == entt::null) {
            info.parent = EMPTY_ASSET;
            BOOM_INFO("[SetParent] Cleared parent: '{}' (UID:{}) is now root (was parent UID:{})",
                info.name, info.uid, oldParent);
        }
        else {
            if (!reg.all_of<InfoComponent>(newParent)) {
                BOOM_WARN("[SetParent] New parent missing InfoComponent");
                return false;
            }

            AssetID newParentUID = reg.get<InfoComponent>(newParent).uid;
            info.parent = newParentUID;

            BOOM_INFO("[SetParent] Set parent: '{}' (UID:{}) -> parent '{}' (UID:{})",
                info.name, info.uid,
                reg.get<InfoComponent>(newParent).name, newParentUID);
        }

        // 3) Recompute LOCAL transform so that WORLD stays the same
        if (preserveWorldTransform && hasTransform) {
            auto& t = reg.get<TransformComponent>(entity);

            // Extract the ACTUAL current world scale from the world matrix
            glm::vec3 worldT, worldRDeg, worldScale;
            DecomposeTRS(worldMatrix, worldT, worldRDeg, worldScale);

            glm::mat4 parentWorld(1.0f);
            glm::vec3 parentScale(1.0f);

            if (newParent != entt::null && reg.all_of<TransformComponent>(newParent)) {
                parentWorld = GetWorldMatrix(reg, newParent);

                // Extract parent's world scale
                glm::vec3 parentT, parentRDeg;
                DecomposeTRS(parentWorld, parentT, parentRDeg, parentScale);
            }

            glm::mat4 localMatrix = glm::inverse(parentWorld) * worldMatrix;

            glm::vec3 T, RDeg, S;
            DecomposeTRS(localMatrix, T, RDeg, S);

            t.transform.translate = T;
            t.transform.rotate = RDeg;

            // Adjust local scale to compensate for parent's scale
            // localScale = worldScale / parentScale (component-wise division)
            t.transform.scale.x = (parentScale.x != 0.0f) ? (worldScale.x / parentScale.x) : worldScale.x;
            t.transform.scale.y = (parentScale.y != 0.0f) ? (worldScale.y / parentScale.y) : worldScale.y;
            t.transform.scale.z = (parentScale.z != 0.0f) ? (worldScale.z / parentScale.z) : worldScale.z;

            BOOM_INFO("[SetParent] AFTER - local: pos({}, {}, {}), scale({}, {}, {})",
                T.x, T.y, T.z,
                t.transform.scale.x, t.transform.scale.y, t.transform.scale.z);
            BOOM_INFO("[SetParent] Parent scale: ({}, {}, {}), World scale preserved: ({}, {}, {})",
                parentScale.x, parentScale.y, parentScale.z,
                worldScale.x, worldScale.y, worldScale.z);
        }

        return true;
    }


    struct DirectLightComponent
    {
        BOOM_INLINE DirectLightComponent(const DirectLightComponent&) = default;
        BOOM_INLINE DirectLightComponent() = default;
        DirectionalLight light;

        XPROPERTY_DEF(
            "DirectLightComponent", DirectLightComponent,
            obj_member<"Light", &DirectLightComponent::light>
        )
    };

    struct PointLightComponent
    {
        BOOM_INLINE PointLightComponent(const PointLightComponent&) = default;
        BOOM_INLINE PointLightComponent() = default;
        PointLight light;

        XPROPERTY_DEF(
            "PointLightComponent", PointLightComponent,
            obj_member<"Light", &PointLightComponent::light>
        )
    };

    struct SpotLightComponent
    {
        BOOM_INLINE SpotLightComponent(const SpotLightComponent&) = default;
        BOOM_INLINE SpotLightComponent() = default;
        SpotLight light;

        XPROPERTY_DEF(
            "SpotLightComponent", SpotLightComponent,
            obj_member<"Light", &SpotLightComponent::light>
        )
    };

    //Chris I have no idea how your sound component works
    struct SoundComponent
    {
        std::string name;      // logical name ("bgm", "jump", etc.)
        std::string filePath;  // actual sound file path
        bool loop = false;
        float volume = 1.0f;
        bool playOnStart = false;

        // --- ADD THIS CODE INSIDE THE STRUCT ---
        void serialize(nlohmann::json& j) const {
            j["name"] = name;
            j["filePath"] = filePath;
            j["loop"] = loop;
            j["volume"] = volume;
            j["playOnStart"] = playOnStart;
        }
        void deserialize(const nlohmann::json& j) {
            if (j.contains("name")) j.at("name").get_to(name);
            if (j.contains("filePath")) j.at("filePath").get_to(filePath);
            if (j.contains("loop")) j.at("loop").get_to(loop);
            if (j.contains("volume")) j.at("volume").get_to(volume);
            if (j.contains("playOnStart")) j.at("playOnStart").get_to(playOnStart);
        }
    };

    struct ScriptComponent
    {
        // Managed type name in C# (e.g., "PhysicsDropDemo" or "MyGame.PlayerController")
        std::string TypeName;

        // Runtime handle returned by script_create_instance(...).
        // Do NOT serialize this; it’s valid only while the game is running.
        uint64_t InstanceId = 0;

        // Allow toggling without removing the component
        bool Enabled = true;

        nlohmann::json Params = nlohmann::json::object();

        // ---- Serialization (save only authoring data) ----
        void serialize(nlohmann::json& j) const {
            j["TypeName"] = TypeName;
            j["Enabled"] = Enabled;
            if (!Params.is_null() && !Params.empty())
                j["Params"] = Params;

            // NOTE: InstanceId is intentionally NOT serialized (runtime only)
        }

        void deserialize(const nlohmann::json& j) {
            if (j.contains("TypeName")) j.at("TypeName").get_to(TypeName);
            if (j.contains("Enabled"))  j.at("Enabled").get_to(Enabled);
            if (j.contains("Params"))   j.at("Params").get_to(Params);

            // Ensure runtime handle starts cleared when loading a scene
            InstanceId = 0;
        }
    };

    struct ThirdPersonCameraComponent {
        AssetID targetUID = 0;       // The UID of the target entity
        glm::vec3 offset = glm::vec3(0.0f, 2.0f, 0.0f);
        float currentDistance = 2.0f;
        float minDistance = 1.0f;
        float maxDistance = 5.0f;
        float currentYaw = 0.0f;
        float currentPitch = 20.0f;
        float mouseSensitivity = 1.0f;
        float scrollSensitivity = 1.0f;

        // Add this back in
        XPROPERTY_DEF(
            "ThirdPersonCameraComponent", ThirdPersonCameraComponent,

            obj_member<"Offset", &ThirdPersonCameraComponent::offset>,
            obj_member<"Current Distance", &ThirdPersonCameraComponent::currentDistance>,
            obj_member<"Min Distance", &ThirdPersonCameraComponent::minDistance>,
            obj_member<"Max Distance", &ThirdPersonCameraComponent::maxDistance>,
            obj_member<"Current Yaw", &ThirdPersonCameraComponent::currentYaw>,
            obj_member<"Current Pitch", &ThirdPersonCameraComponent::currentPitch>,
            obj_member<"Mouse Sensitivity", &ThirdPersonCameraComponent::mouseSensitivity>,
            obj_member<"Scroll Sensitivity", &ThirdPersonCameraComponent::scrollSensitivity>
        )
    };
    struct NavAgentComponent {
        glm::vec3 target{ 0.f };
        std::vector<glm::vec3> path; // straight path
        int   waypoint = 0;
        float speed = 2.5f;  // m/s
        float arrive = 0.15f; // meters
        glm::vec3 velocity = glm::vec3(0.f);
        bool  active = true;
        bool  dirty = false; // set true when target changes
        std::string followName;
        entt::entity follow = entt::null; //this is player entity to follow
        float repathCooldown = 0.25f;     // seconds between path rebuilds
        float retargetDist = 0.5f;      // re-path if player moved this far
        float repathTimer = 0.f;       // internal timer
        XPROPERTY_DEF
        ("NavAgentComponent", NavAgentComponent
            , obj_member<"Target", &NavAgentComponent::target>
            , obj_member<"Speed", &NavAgentComponent::speed>
            , obj_member<"Velocity", &NavAgentComponent::velocity>
            , obj_member<"ArriveRadius", &NavAgentComponent::arrive>
            , obj_member<"Active", &NavAgentComponent::active>
            , obj_member<"RepathCooldown", &NavAgentComponent::repathCooldown>
            , obj_member<"RetargetDistance", &NavAgentComponent::retargetDist>
        )
    };
    struct AIComponent {
        enum class AIMode : int { Auto = 0, Idle = 1, Patrol = 2, Seek = 3 };
        AIMode mode = AIMode::Auto;   // exposed in Inspector
        AIMode lastMode = AIMode::Auto;
        float detectRadius = 8.0f;    // start seeking when within this distance
        float loseRadius = 12.0f;   // stop seeking when beyond this distance
        float idleWait = 1.0f;    // wait at patrol points
        float idleTimer = 0.0f;
        std::string playerName = "Samurai";   // find by name instead of PlayerTag
        entt::entity player = entt::null;     // cached after first successful lookup
        // Patrol
        std::vector<glm::vec3> patrolPoints;
        int patrolIndex = 0;


        // BT root
        BTNodePtr root;

        XPROPERTY_DEF
        ("AIComponent", AIComponent
            , obj_member<"DetectRadius", &AIComponent::detectRadius>
            , obj_member<"LoseRadius", &AIComponent::loseRadius>
            , obj_member<"IdleWait", &AIComponent::idleWait>
            , obj_member<"IdleTimer", &AIComponent::idleTimer>    // include if you want to see the live timer
            , obj_member<"PlayerName", &AIComponent::playerName>

            , obj_member<"PatrolIndex", &AIComponent::patrolIndex>
        )
    };

    struct SpriteComponent {
        AssetID textureID{ EMPTY_ASSET };
        glm::vec4 color{ 1.0f };
        bool uiOverlay{ true };

        XPROPERTY_DEF(
            "SpriteComponent", SpriteComponent,
            obj_member<"textureID", &SpriteComponent::textureID>,
            obj_member<"color", &SpriteComponent::color>,
            obj_member<"uiOverlay", &SpriteComponent::uiOverlay>
        )
    };

    struct PauseMenuTagComponent {
        BOOM_INLINE PauseMenuTagComponent(const PauseMenuTagComponent&) = default;
        BOOM_INLINE PauseMenuTagComponent() = default;

        bool isTag = true;

        XPROPERTY_DEF(
            "PauseMenuTagComponent", PauseMenuTagComponent
        )
    };

    struct Entity
    {
        BOOM_INLINE Entity(EntityRegistry* registry, EntityID entity) :
            m_Registry(registry), m_EnttID(entity)
        {
        }

        BOOM_INLINE Entity(EntityRegistry* registry) :
            m_Registry(registry)
        {
            m_EnttID = m_Registry->create();
        }

        BOOM_INLINE virtual ~Entity() = default;
        BOOM_INLINE Entity() = default;

        BOOM_INLINE operator EntityID ()
        {
            return m_EnttID;
        }

        BOOM_INLINE operator bool()
        {
            return m_Registry != nullptr &&
                m_Registry->valid(m_EnttID);
        }

        BOOM_INLINE EntityID ID()
        {
            return m_EnttID;
        }

        template<typename T, typename... Args>
        BOOM_INLINE T& Attach(Args&&... args)
        {
            return m_Registry->get_or_emplace<T>(m_EnttID, std::forward<Args>(args)...);
        }

        template<typename T>
        BOOM_INLINE void Detach()
        {
            m_Registry->remove<T>(m_EnttID);
        }

        BOOM_INLINE void Destroy()
        {
            if (m_Registry)
            {
                m_Registry->destroy(m_EnttID);
            }
        }

        template<typename T>
        BOOM_INLINE bool Has() const
        {
            return m_Registry != nullptr &&
                m_Registry->all_of<T>(m_EnttID);
        }

        template<typename T>
        BOOM_INLINE T& Get()
        {
            return m_Registry->get<T>(m_EnttID);
        }

    protected:
        EntityRegistry* m_Registry = nullptr;
        EntityID m_EnttID = NENTT;
    };

    // ========== ENTITY MANIPULATION FUNCTIONS ==========
    // NOTE: These are placed at the end of the file after all component declarations

    // Delete entity and all its children recursively (with physics cleanup)
    BOOM_INLINE void DeleteEntityRecursive(entt::registry& reg, entt::entity entity, void* physicsCtx = nullptr) {
        if (!reg.valid(entity)) return;

        // Get entity name for logging
        std::string entityName = "Unknown";
        if (reg.all_of<InfoComponent>(entity)) {
            entityName = reg.get<InfoComponent>(entity).name;
        }

        // First, delete all children recursively
        auto children = GetChildren(reg, entity);
        for (entt::entity child : children) {
            DeleteEntityRecursive(reg, child, physicsCtx);
        }

        // Clean up physics if context provided
        if (physicsCtx != nullptr) {
            // Cast to PhysicsContext and remove rigid body
            // Note: PhysicsContext is forward-declared, so we need to include it where this is called
            // For now, just log
            BOOM_INFO("[DeleteEntity] Physics cleanup for '{}'", entityName);
        }

        // Destroy the entity
        BOOM_INFO("[DeleteEntity] Destroying entity '{}'", entityName);
        reg.destroy(entity);
    }

    // Duplicate entity with all components (optionally with children)
    BOOM_INLINE entt::entity DuplicateEntity(entt::registry& reg, entt::entity source, bool duplicateChildren = true) {
        if (!reg.valid(source)) {
            BOOM_WARN("[DuplicateEntity] Source entity is invalid");
            return entt::null;
        }

        // Create new entity
        entt::entity duplicate = reg.create();

        // Copy InfoComponent with new UID and modified name
        if (reg.all_of<InfoComponent>(source)) {
            auto& srcInfo = reg.get<InfoComponent>(source);
            auto& dstInfo = reg.emplace<InfoComponent>(duplicate);
            dstInfo.name = srcInfo.name + " (Copy)";
            dstInfo.uid = RandomU64(); // New unique ID
            dstInfo.parent = srcInfo.parent; // Keep same parent
        }

        // Copy TransformComponent
        if (reg.all_of<TransformComponent>(source)) {
            auto& srcTrans = reg.get<TransformComponent>(source);
            auto& dstTrans = reg.emplace<TransformComponent>(duplicate);
            dstTrans = srcTrans; // Copy all transform data
        }

        // Copy CameraComponent
        if (reg.all_of<CameraComponent>(source)) {
            auto& srcCam = reg.get<CameraComponent>(source);
            reg.emplace<CameraComponent>(duplicate, srcCam);
        }

        // Copy ModelComponent
        if (reg.all_of<ModelComponent>(source)) {
            auto& srcModel = reg.get<ModelComponent>(source);
            auto& dstModel = reg.emplace<ModelComponent>(duplicate);
            dstModel.modelID = srcModel.modelID;
            dstModel.materialID = srcModel.materialID;
        }

        // Copy AnimatorComponent
        if (reg.all_of<AnimatorComponent>(source)) {
            auto& srcAnim = reg.get<AnimatorComponent>(source);
            auto& dstAnim = reg.emplace<AnimatorComponent>(duplicate);
            dstAnim.animator = srcAnim.animator; // Copy animator data
        }

        // Copy RigidBodyComponent (but NOT the physics actor - that needs special handling)
        if (reg.all_of<RigidBodyComponent>(source)) {
            auto& srcRB = reg.get<RigidBodyComponent>(source);
            auto& dstRB = reg.emplace<RigidBodyComponent>(duplicate);
            dstRB = srcRB;
            // Note: Physics actor will be created by physics system on next update
        }

        // Copy ColliderComponent
        if (reg.all_of<ColliderComponent>(source)) {
            auto& srcCol = reg.get<ColliderComponent>(source);
            reg.emplace<ColliderComponent>(duplicate, srcCol);
        }

        // Copy Light Components
        if (reg.all_of<DirectLightComponent>(source)) {
            auto& srcLight = reg.get<DirectLightComponent>(source);
            reg.emplace<DirectLightComponent>(duplicate, srcLight);
        }
        if (reg.all_of<PointLightComponent>(source)) {
            auto& srcLight = reg.get<PointLightComponent>(source);
            reg.emplace<PointLightComponent>(duplicate, srcLight);
        }
        if (reg.all_of<SpotLightComponent>(source)) {
            auto& srcLight = reg.get<SpotLightComponent>(source);
            reg.emplace<SpotLightComponent>(duplicate, srcLight);
        }

        // Copy SoundComponent
        if (reg.all_of<SoundComponent>(source)) {
            auto& srcSound = reg.get<SoundComponent>(source);
            auto& dstSound = reg.emplace<SoundComponent>(duplicate);
            dstSound.name = srcSound.name;
            dstSound.filePath = srcSound.filePath;
            dstSound.loop = srcSound.loop;
            dstSound.volume = srcSound.volume;
            dstSound.playOnStart = srcSound.playOnStart;
        }

        // Copy ScriptComponent
        if (reg.all_of<ScriptComponent>(source)) {
            auto& srcScript = reg.get<ScriptComponent>(source);
            auto& dstScript = reg.emplace<ScriptComponent>(duplicate);
            dstScript.TypeName = srcScript.TypeName;
            dstScript.Enabled = srcScript.Enabled;
            // Note: InstanceId will be set by scripting system
        }

        // Copy NavAgentComponent
        if (reg.all_of<NavAgentComponent>(source)) {
            auto& srcNav = reg.get<NavAgentComponent>(source);
            reg.emplace<NavAgentComponent>(duplicate, srcNav);
        }

        // Copy AIComponent
        if (reg.all_of<AIComponent>(source)) {
            auto& srcAI = reg.get<AIComponent>(source);
            auto& dstAI = reg.emplace<AIComponent>(duplicate);
            dstAI.detectRadius = srcAI.detectRadius;
            dstAI.loseRadius = srcAI.loseRadius;
            dstAI.idleWait = srcAI.idleWait;
            dstAI.playerName = srcAI.playerName;
            dstAI.patrolPoints = srcAI.patrolPoints;
            // Note: Don't copy runtime state (idleTimer, patrolIndex, etc.)
        }

        // Copy SpriteComponent
        if (reg.all_of<SpriteComponent>(source)) {
            auto& srcSprite = reg.get<SpriteComponent>(source);
            reg.emplace<SpriteComponent>(duplicate, srcSprite);
        }

        // Copy ThirdPersonCameraComponent
        if (reg.all_of<ThirdPersonCameraComponent>(source)) {
            auto& srcCam = reg.get<ThirdPersonCameraComponent>(source);
            reg.emplace<ThirdPersonCameraComponent>(duplicate, srcCam);
        }

        // Copy SkyboxComponent
        if (reg.all_of<SkyboxComponent>(source)) {
            auto& srcSky = reg.get<SkyboxComponent>(source);
            reg.emplace<SkyboxComponent>(duplicate, srcSky);
        }

        // Copy PauseMenuTagComponent
        if (reg.all_of<PauseMenuTagComponent>(source)) {
            reg.emplace<PauseMenuTagComponent>(duplicate);
        }

        BOOM_INFO("[DuplicateEntity] Duplicated '{}' -> '{}'",
                 reg.get<InfoComponent>(source).name,
                 reg.get<InfoComponent>(duplicate).name);

        // Recursively duplicate children if requested
        if (duplicateChildren) {
            auto children = GetChildren(reg, source);
            AssetID duplicateUID = reg.get<InfoComponent>(duplicate).uid;

            for (entt::entity child : children) {
                entt::entity childDuplicate = DuplicateEntity(reg, child, true);
                if (reg.valid(childDuplicate) && reg.all_of<InfoComponent>(childDuplicate)) {
                    // Set duplicated child's parent to the duplicated parent
                    reg.get<InfoComponent>(childDuplicate).parent = duplicateUID;
                }
            }
        }

        return duplicate;
    }

}
